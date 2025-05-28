#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-layout-module.h"
#include "ns3/flow-monitor-module.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SimpleTcpDumbbell");

// Global variables for monitoring
Ptr<FlowMonitor> g_monitor;
std::ofstream g_packetLossFile;

void PrintPacketLossStats(double interval) {
    static uint32_t lastTotalLostPackets = 0;
    
    g_monitor->CheckForLostPackets();
    
    uint32_t totalLostPackets = 0;
    uint32_t totalTxPackets = 0;
    
    FlowMonitor::FlowStatsContainer stats = g_monitor->GetFlowStats();
    for (auto& flow : stats) {
        totalLostPackets += flow.second.lostPackets;
        totalTxPackets += flow.second.txPackets;
    }
    
    uint32_t newLostPackets = totalLostPackets - lastTotalLostPackets;
    double currentTime = Simulator::Now().GetSeconds();
    double packetLossRate = totalTxPackets > 0 ? (double)totalLostPackets / totalTxPackets * 100.0 : 0.0;
    
    // Output to console
    std::cout << "Time: " << currentTime << "s, "
              << "Total Lost: " << totalLostPackets << ", "
              << "New Lost: " << newLostPackets << ", "
              << "Loss Rate: " << packetLossRate << "%" << std::endl;
    
    // Output to file
    g_packetLossFile << currentTime << "," << totalLostPackets << "," 
                     << newLostPackets << "," << packetLossRate << std::endl;
    
    lastTotalLostPackets = totalLostPackets;
    
    Simulator::Schedule(Seconds(interval), &PrintPacketLossStats, interval);
}

int main(int argc, char* argv[]) {
    LogComponentEnable("SimpleTcpDumbbell", LOG_LEVEL_INFO);

    // === Setup network topology ===
    PointToPointHelper p2pLeaf;
    p2pLeaf.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    p2pLeaf.SetChannelAttribute("Delay", StringValue("20ms"));

    PointToPointHelper p2pRouter;
    p2pRouter.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2pRouter.SetChannelAttribute("Delay", StringValue("50ms"));

    PointToPointDumbbellHelper dumbbell(2, p2pLeaf, 2, p2pLeaf, p2pRouter);

    InternetStackHelper stack;
    dumbbell.InstallStack(stack);

    // Reset IP address generator right before assignment
    Ipv4AddressGenerator::Reset();
    
    dumbbell.AssignIpv4Addresses(Ipv4AddressHelper("10.1.0.0", "255.255.255.0"),
                                 Ipv4AddressHelper("10.2.0.0", "255.255.255.0"),
                                 Ipv4AddressHelper("10.3.0.0", "255.255.255.0"));

    // === Setup applications ===
    uint16_t sinkPort = 8080;
    Time startTime = Seconds(1.0);
    Time stopTime = Seconds(10.0);

    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));

    ApplicationContainer sinkApp1 = packetSinkHelper.Install(dumbbell.GetRight(0));
    sinkApp1.Start(Seconds(0.0));
    sinkApp1.Stop(stopTime + Seconds(1.0));

    ApplicationContainer sinkApp2 = packetSinkHelper.Install(dumbbell.GetRight(1));
    sinkApp2.Start(Seconds(0.0));
    sinkApp2.Stop(stopTime + Seconds(1.0));

    InetSocketAddress remote1(dumbbell.GetRightIpv4Address(0), sinkPort);
    BulkSendHelper bulkSendClient1("ns3::TcpSocketFactory", remote1);
    bulkSendClient1.SetAttribute("MaxBytes", UintegerValue(0));
    ApplicationContainer clientApp1 = bulkSendClient1.Install(dumbbell.GetLeft(0));
    clientApp1.Start(startTime);
    clientApp1.Stop(stopTime);

    InetSocketAddress remote2(dumbbell.GetRightIpv4Address(1), sinkPort);
    BulkSendHelper bulkSendClient2("ns3::TcpSocketFactory", remote2);
    bulkSendClient2.SetAttribute("MaxBytes", UintegerValue(0));
    ApplicationContainer clientApp2 = bulkSendClient2.Install(dumbbell.GetLeft(1));
    clientApp2.Start(startTime);
    clientApp2.Stop(stopTime);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // === Setup monitoring ===
    p2pRouter.EnablePcapAll("simple-tcp-dumbbell-router");

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();
    
    // Initialize global monitor and output file
    g_monitor = monitor;
    g_packetLossFile.open("./scratch/workspace/packet-loss-over-time.csv");
    g_packetLossFile << "Time(s),TotalLostPackets,NewLostPackets,LossRate(%)" << std::endl;
    
    // Schedule periodic packet loss reporting every 0.5 seconds
    Simulator::Schedule(Seconds(0.5), &PrintPacketLossStats, 0.5);

    // === Run simulation ===
    NS_LOG_INFO("Running Simulation.");
    Simulator::Stop(stopTime + Seconds(5.0));
    Simulator::Run();

    // === Save results ===
    monitor->CheckForLostPackets();
    monitor->SerializeToXmlFile("./scratch/workspace/simple-tcp-dumbbell-flowmon.xml", true, true);
    
    // Close the packet loss file
    g_packetLossFile.close();

    NS_LOG_INFO("Simulation Finished.");
    Simulator::Destroy();

    return 0;
}
