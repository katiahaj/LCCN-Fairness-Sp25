#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-layout-module.h"
#include "ns3/flow-monitor-module.h"

#include <iostream>
#include <fstream>
#include <map>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpDumbbell");

double CalculateJainsFairnessIndex(double throughput1, double throughput2)
{
    double sum = throughput1 + throughput2;
    double sumOfSquares = throughput1 * throughput1 + throughput2 * throughput2;
    
    if (sumOfSquares == 0.0) return 1.0; // Both zero = perfectly fair
    
    return (sum * sum) / (2.0 * sumOfSquares);
}

Ptr<FlowMonitor> g_monitor;
std::ofstream* g_outputFile;
Ipv4Address g_sender1IpAddress;
Ipv4Address g_sender2IpAddress;
FlowMonitorHelper* g_flowMonitorHelper;

void RecordPeriodicStats(double interval)
{
    static std::map<FlowId, uint64_t> lastReceivedBytes;
    
    g_monitor->CheckForLostPackets();
    auto stats = g_monitor->GetFlowStats();
    auto classifier = DynamicCast<Ipv4FlowClassifier>(g_flowMonitorHelper->GetClassifier());

    double timeInSeconds = Simulator::Now().GetSeconds();
    double flow1ThroughputBytesPerSecond = 0.0;
    double flow2ThroughputBytesPerSecond = 0.0;
    uint32_t flow1PacketLoss = 0;
    uint32_t flow2PacketLoss = 0;
    double flow1PacketLossPercent = 0.0;
    double flow2PacketLossPercent = 0.0;

    for (auto const& [flowId, flowStats] : stats)
    {
        auto fiveTuple = classifier->FindFlow(flowId);
        if (fiveTuple.sourceAddress == g_sender1IpAddress || fiveTuple.sourceAddress == g_sender2IpAddress)
        {
            uint64_t previousBytes = (lastReceivedBytes.count(flowId) > 0) ? lastReceivedBytes[flowId] : 0;
            double throughputBytesPerSecond = (flowStats.rxBytes - previousBytes) / interval;
            
            // Calculate packet loss
            uint32_t packetLoss = flowStats.txPackets - flowStats.rxPackets;
            double packetLossPercent = (flowStats.txPackets > 0) ? 
                (static_cast<double>(packetLoss) / flowStats.txPackets) * 100.0 : 0.0;
            
            if (fiveTuple.sourceAddress == g_sender1IpAddress)
            {
                flow1ThroughputBytesPerSecond = throughputBytesPerSecond;
                flow1PacketLoss = packetLoss;
                flow1PacketLossPercent = packetLossPercent;
            }
            else
            {
                flow2ThroughputBytesPerSecond = throughputBytesPerSecond;
                flow2PacketLoss = packetLoss;
                flow2PacketLossPercent = packetLossPercent;
            }
                
            lastReceivedBytes[flowId] = flowStats.rxBytes;
        }
    }

    double jainsFairnessIndex = CalculateJainsFairnessIndex(flow1ThroughputBytesPerSecond, flow2ThroughputBytesPerSecond);
    *g_outputFile << timeInSeconds << "," << flow1ThroughputBytesPerSecond << "," << flow2ThroughputBytesPerSecond << "," 
                  << flow1PacketLoss << "," << flow1PacketLossPercent << "," 
                  << flow2PacketLoss << "," << flow2PacketLossPercent << "," 
                  << jainsFairnessIndex << std::endl;

    Simulator::Schedule(Seconds(interval), &RecordPeriodicStats, interval);
}

int main(int argc, char* argv[])
{
    // --- Simulation Parameters ---
    std::string tcpCongAlg1 = "NewReno";
    std::string tcpCongAlg2 = "NewReno";
    double stopTimeSecs = 10.0;

    // --- Command Line Parsing ---
    CommandLine cmd;
    cmd.AddValue("tcpCongAlg1", "TCP algorithm for sender 1", tcpCongAlg1);
    cmd.AddValue("tcpCongAlg2", "TCP algorithm for sender 2", tcpCongAlg2);
    cmd.AddValue("stopTime", "Stop time for applications", stopTimeSecs);
    cmd.Parse(argc, argv);

    // Simple algorithm mapping
    std::string tcpTypeId1 = "ns3::Tcp" + tcpCongAlg1;
    std::string tcpTypeId2 = "ns3::Tcp" + tcpCongAlg2;



    // --- Network Topology Setup ---
    // Create initial symmetric topology
    PointToPointHelper p2pLeaf;
    p2pLeaf.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2pLeaf.SetChannelAttribute("Delay", StringValue("10ms"));

    PointToPointHelper p2pRouter;
    p2pRouter.SetDeviceAttribute("DataRate", StringValue("5Mbps"));      // Shared bottleneck
    p2pRouter.SetChannelAttribute("Delay", StringValue("50ms"));
    p2pRouter.SetQueue("ns3::DropTailQueue", "MaxSize", StringValue("50p"));

    PointToPointDumbbellHelper dumbbell(2, p2pLeaf, 2, p2pLeaf, p2pRouter);

    // --- Extract Node References ---
    Ptr<Node> sender1 = dumbbell.GetLeft(0);
    Ptr<Node> sender2 = dumbbell.GetLeft(1);
    Ptr<Node> receiver1 = dumbbell.GetRight(0);
    Ptr<Node> receiver2 = dumbbell.GetRight(1);

    // --- Modify Individual Sender Link Characteristics for Asymmetry ---
    // Get the network devices for each sender
    Ptr<NetDevice> sender1Device = sender1->GetDevice(0);
    Ptr<NetDevice> sender2Device = sender2->GetDevice(0);
    
    // Cast to PointToPointNetDevice to modify attributes
    Ptr<PointToPointNetDevice> p2pSender1 = DynamicCast<PointToPointNetDevice>(sender1Device);
    Ptr<PointToPointNetDevice> p2pSender2 = DynamicCast<PointToPointNetDevice>(sender2Device);
    
    // Create ONLY RTT asymmetry - remove bandwidth asymmetry to isolate algorithm differences
    // Both senders get same high bandwidth, but different RTTs
    p2pSender1->SetAttribute("DataRate", DataRateValue(DataRate("100Mbps")));  // High bandwidth
    p2pSender1->GetChannel()->SetAttribute("Delay", TimeValue(Time("5ms")));   // Low RTT
    
    p2pSender2->SetAttribute("DataRate", DataRateValue(DataRate("100Mbps")));  // Same high bandwidth
    p2pSender2->GetChannel()->SetAttribute("Delay", TimeValue(Time("100ms"))); // High RTT (20x difference)

    // --- Install Internet Stack ---
    InternetStackHelper stack;
    dumbbell.InstallStack(stack);

    // --- Configure TCP Algorithms AFTER Installing Stack ---
    TypeId tid1 = TypeId::LookupByName(tcpTypeId1);
    TypeId tid2 = TypeId::LookupByName(tcpTypeId2);
    
    // Configure TCP algorithms directly on nodes
    Ptr<TcpL4Protocol> tcp1 = sender1->GetObject<TcpL4Protocol>();
    Ptr<TcpL4Protocol> tcp2 = sender2->GetObject<TcpL4Protocol>();
    
    tcp1->SetAttribute("SocketType", TypeIdValue(tid1));
    tcp2->SetAttribute("SocketType", TypeIdValue(tid2));

    // --- IP Address Assignment ---
    Ipv4AddressGenerator::Reset();
    dumbbell.AssignIpv4Addresses(Ipv4AddressHelper("10.1.0.0", "255.255.255.0"),
                                 Ipv4AddressHelper("10.2.0.0", "255.255.255.0"),
                                 Ipv4AddressHelper("10.3.0.0", "255.255.255.0"));

    // --- Extract IP Address References ---
    Ipv4Address sender1IpAddress = dumbbell.GetLeftIpv4Address(0);
    Ipv4Address sender2IpAddress = dumbbell.GetLeftIpv4Address(1);
    Ipv4Address receiver1IpAddress = dumbbell.GetRightIpv4Address(0);
    Ipv4Address receiver2IpAddress = dumbbell.GetRightIpv4Address(1);

    // --- Application Setup ---
    uint16_t sinkPort = 8080;

    // Setup sink applications (one per receiver)
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sink1 = sinkHelper.Install(receiver1);
    ApplicationContainer sink2 = sinkHelper.Install(receiver2);
    sink1.Start(Seconds(0.0));
    sink1.Stop(Seconds(stopTimeSecs));
    sink2.Start(Seconds(0.0));
    sink2.Stop(Seconds(stopTimeSecs));

    // Setup sender applications (continuous transmission), we can apply delay to ".Start" to have offsets
    BulkSendHelper sender1Helper("ns3::TcpSocketFactory", InetSocketAddress(receiver1IpAddress, sinkPort));
    sender1Helper.SetAttribute("MaxBytes", UintegerValue(0)); // unlimited
    ApplicationContainer clientApp1 = sender1Helper.Install(sender1);
    clientApp1.Start(Seconds(0.0));
    clientApp1.Stop(Seconds(stopTimeSecs));

    BulkSendHelper sender2Helper("ns3::TcpSocketFactory", InetSocketAddress(receiver2IpAddress, sinkPort));
    sender2Helper.SetAttribute("MaxBytes", UintegerValue(0)); // unlimited
    ApplicationContainer clientApp2 = sender2Helper.Install(sender2);
    clientApp2.Start(Seconds(0.0));
    clientApp2.Stop(Seconds(stopTimeSecs));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // --- Setup Flow Monitor and Periodic Measurements ---
    FlowMonitorHelper flowMonitor;
    g_monitor = flowMonitor.InstallAll();
    g_flowMonitorHelper = &flowMonitor;
    g_sender1IpAddress = sender1IpAddress;
    g_sender2IpAddress = sender2IpAddress;

    // Setup output file
    std::string filename = "scratch/workspace/results-" + tcpCongAlg1 + "-vs-" + tcpCongAlg2 + ".csv";
    std::ofstream outputFile(filename);
    outputFile << std::fixed; // Use fixed-point notation, no scientific notation
    g_outputFile = &outputFile;
    
    outputFile << "# TCP Comparison: " << tcpCongAlg1 << " vs " << tcpCongAlg2 << std::endl;
    outputFile << "# Simulation time: " << stopTimeSecs << " seconds" << std::endl;
    outputFile << "TimeInSeconds,Flow1ThroughputBytesPerSecond,Flow2ThroughputBytesPerSecond,Flow1PacketLoss,Flow1PacketLossPercent,Flow2PacketLoss,Flow2PacketLossPercent,JainsFairnessIndex" << std::endl;
    
    // Schedule periodic measurements 
    double measurementInterval = 0.1;
    Simulator::Schedule(Seconds(measurementInterval), &RecordPeriodicStats, measurementInterval);

    // --- Run Simulation ---
    Simulator::Stop(Seconds(stopTimeSecs));
    Simulator::Run();

    outputFile.close();
    std::cout << "Results saved to " << filename << std::endl;

    Simulator::Destroy();
    return 0;
}
