#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-layout-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/bulk-send-application.h"
#include "ns3/tcp-dctcp.h"
#include "ns3/traffic-control-module.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpExperimentFramework");

// Global Variables
uint32_t g_nFlows;
Ptr<FlowMonitor> g_monitor;
std::ofstream* g_outputFile;
FlowMonitorHelper* g_flowMonitorHelper;
NodeContainer g_senderNodes;
Ipv4InterfaceContainer g_senderInterfaces;
std::vector<uint32_t> g_cwnd;

// Helper Functions
double CalculateJainsFairnessIndex(const std::vector<double>& throughputs)
{
    double sum = 0, sumOfSquares = 0;
    for (const auto& x : throughputs) { sum += x; sumOfSquares += x * x; }
    if (sumOfSquares == 0) return 1.0;
    return (sum * sum) / (throughputs.size() * sumOfSquares);
}

// Statistics and Tracing
void RecordPeriodicStats(double interval)
{
    static std::map<FlowId, uint64_t> lastReceivedBytes;
    g_monitor->CheckForLostPackets();
    auto stats = g_monitor->GetFlowStats();
    auto classifier = DynamicCast<Ipv4FlowClassifier>(g_flowMonitorHelper->GetClassifier());
    double timeInSeconds = Simulator::Now().GetSeconds();
    std::vector<double> throughputs(g_nFlows, 0.0);
    std::vector<uint32_t> losses(g_nFlows, 0);
    for (auto const& [flowId, flowStats] : stats)
    {
        auto fiveTuple = classifier->FindFlow(flowId);
        for (uint32_t i = 0; i < g_nFlows; ++i)
        {
            if (g_senderInterfaces.GetAddress(i) == fiveTuple.sourceAddress)
            {
                throughputs[i] = (flowStats.rxBytes - (lastReceivedBytes.count(flowId) ? lastReceivedBytes[flowId] : 0)) / interval;
                losses[i] = flowStats.txPackets - flowStats.rxPackets;
                lastReceivedBytes[flowId] = flowStats.rxBytes;
                break;
            }
        }
    }
    double fairness = CalculateJainsFairnessIndex(throughputs);
    *g_outputFile << timeInSeconds;
    for(const auto& val : throughputs) *g_outputFile << "," << val;
    for(const auto& val : losses) *g_outputFile << "," << val;
    for(const auto& val : g_cwnd) *g_outputFile << "," << val;
    *g_outputFile << "," << fairness << std::endl;
    Simulator::Schedule(Seconds(interval), &RecordPeriodicStats, interval);
}

void CwndChangeCallback0(uint32_t oldCwnd, uint32_t newCwnd) { if (!g_cwnd.empty()) g_cwnd[0] = newCwnd; }
void CwndChangeCallback1(uint32_t oldCwnd, uint32_t newCwnd) { if (g_cwnd.size() > 1) g_cwnd[1] = newCwnd; }
void CwndChangeCallback2(uint32_t oldCwnd, uint32_t newCwnd) { if (g_cwnd.size() > 2) g_cwnd[2] = newCwnd; }
void CwndChangeCallback3(uint32_t oldCwnd, uint32_t newCwnd) { if (g_cwnd.size() > 3) g_cwnd[3] = newCwnd; }

void ConnectCwndTraces()
{
    if (g_nFlows > 0) Config::ConnectWithoutContext("/NodeList/" + std::to_string(g_senderNodes.Get(0)->GetId()) + "/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow", MakeCallback(&CwndChangeCallback0));
    if (g_nFlows > 1) Config::ConnectWithoutContext("/NodeList/" + std::to_string(g_senderNodes.Get(1)->GetId()) + "/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow", MakeCallback(&CwndChangeCallback1));
    if (g_nFlows > 2) Config::ConnectWithoutContext("/NodeList/" + std::to_string(g_senderNodes.Get(2)->GetId()) + "/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow", MakeCallback(&CwndChangeCallback2));
    if (g_nFlows > 3) Config::ConnectWithoutContext("/NodeList/" + std::to_string(g_senderNodes.Get(3)->GetId()) + "/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow", MakeCallback(&CwndChangeCallback3));
}

int main(int argc, char* argv[])
{
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1448));
    Config::SetDefault("ns3::TcpSocket::InitialSlowStartThreshold", UintegerValue(65535));
    GlobalValue::Bind("ChecksumEnabled", BooleanValue(false));

    std::string scenario = "AllCubic", outputFile = "scratch/workspace/results.csv";
    bool asymmetricRtt = false;
    double stopTimeSecs = 200.0;
    uint32_t bottleneckQueueSize = 10;
    uint32_t seed = 1;

    CommandLine cmd;
    cmd.AddValue("scenario", "TCP scenario", scenario);
    cmd.AddValue("asymmetricRtt", "Enable RTT asymmetry", asymmetricRtt);
    cmd.AddValue("stopTime", "Stop time for applications", stopTimeSecs);
    cmd.AddValue("queueSize", "Bottleneck queue size", bottleneckQueueSize);
    cmd.AddValue("outputFile", "File path to save results", outputFile);
    cmd.AddValue("seed", "Random seed for simulation", seed);
    cmd.Parse(argc, argv);

    // Set the random seed
    RngSeedManager::SetSeed(seed);

    if (scenario == "AllMixed") {
        g_nFlows = 4;
    } else {
        g_nFlows = 2;
    }
    g_cwnd.resize(g_nFlows, 0);

    std::vector<std::string> tcpAlgorithms(g_nFlows);
    bool ecnEnabled = false;
    
    if (scenario == "AllNewReno")   tcpAlgorithms.assign(g_nFlows, "NewReno");
    else if (scenario == "AllCubic")    tcpAlgorithms.assign(g_nFlows, "Cubic");
    else if (scenario == "AllBbr")      tcpAlgorithms.assign(g_nFlows, "Bbr");
    else if (scenario == "AllDctcp")    { tcpAlgorithms.assign(g_nFlows, "Dctcp"); ecnEnabled = true; }
    else if (scenario == "RenoVsCubic") { tcpAlgorithms = {"NewReno", "Cubic"}; }
    else if (scenario == "RenoVsBbr")   { tcpAlgorithms = {"NewReno", "Bbr"}; }
    else if (scenario == "BbrVsCubic")  { tcpAlgorithms = {"Bbr", "Cubic"}; }
    else if (scenario == "AllMixed")    { tcpAlgorithms = {"NewReno", "Cubic", "Bbr", "Dctcp"}; ecnEnabled = true; }
    else { NS_LOG_ERROR("Invalid scenario!"); return 1; }
    
    if (ecnEnabled) {
        Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpDctcp"));
        Config::SetDefault("ns3::RedQueueDisc::UseEcn", BooleanValue(true));
    }

    NodeContainer receiverNodes, routerNodes;
    g_senderNodes.Create(g_nFlows);
    receiverNodes.Create(g_nFlows);
    routerNodes.Create(2);
    Ptr<Node> leftRouter = routerNodes.Get(0);
    Ptr<Node> rightRouter = routerNodes.Get(1);

    PointToPointHelper p2pLeaf, p2pRouter;
    p2pLeaf.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    
    p2pRouter.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2pRouter.SetChannelAttribute("Delay", StringValue("20ms"));
    
    NetDeviceContainer senderDevices, receiverDevices, routerDevices;
    for(uint32_t i = 0; i < g_nFlows; ++i) {
        senderDevices.Add(p2pLeaf.Install(g_senderNodes.Get(i), leftRouter).Get(0));
    }
    for(uint32_t i = 0; i < g_nFlows; ++i) {
        receiverDevices.Add(p2pLeaf.Install(receiverNodes.Get(i), rightRouter).Get(0));
    }
    routerDevices = p2pRouter.Install(leftRouter, rightRouter);

    InternetStackHelper stack;
    stack.Install(g_senderNodes);
    stack.Install(receiverNodes);
    stack.Install(routerNodes);

    TrafficControlHelper tch;
    if (ecnEnabled) {
        tch.SetRootQueueDisc("ns3::RedQueueDisc", "MaxSize", StringValue(std::to_string(bottleneckQueueSize) + "p"));
    } else {
        tch.SetRootQueueDisc("ns3::FifoQueueDisc", "MaxSize", StringValue(std::to_string(bottleneckQueueSize) + "p"));
    }
    tch.Install(routerDevices);

    if (!ecnEnabled) {
        for (uint32_t i = 0; i < g_nFlows; ++i) {
            TypeId tcpTid = TypeId::LookupByName("ns3::Tcp" + tcpAlgorithms[i]);
            g_senderNodes.Get(i)->GetObject<TcpL4Protocol>()->SetAttribute("SocketType", TypeIdValue(tcpTid));
        }
    }
    
    for (uint32_t i = 0; i < g_nFlows; ++i) {
        if (asymmetricRtt) {
            uint32_t delay_ms = 5;
            if (g_nFlows == 2) {
                delay_ms = 5 + (i * 45);
            } else { 
                delay_ms = 5 + (i * 15);
            }
            DynamicCast<PointToPointNetDevice>(senderDevices.Get(i))->GetChannel()->SetAttribute("Delay", TimeValue(Time(std::to_string(delay_ms) + "ms")));
        } else {
             DynamicCast<PointToPointNetDevice>(senderDevices.Get(i))->GetChannel()->SetAttribute("Delay", StringValue("5ms"));
        }
    }

    Ipv4AddressHelper leftIp, rightIp, routerIp;
    leftIp.SetBase("10.1.0.0", "255.255.255.0");
    rightIp.SetBase("10.2.0.0", "255.255.255.0");
    routerIp.SetBase("10.3.1.0", "255.255.255.0");

    Ipv4InterfaceContainer receiverInterfaces;
    for(uint32_t i=0; i < g_nFlows; ++i)
    {
        NetDeviceContainer link(senderDevices.Get(i), leftRouter->GetDevice(i));
        Ipv4InterfaceContainer ifaces = leftIp.Assign(link);
        g_senderInterfaces.Add(ifaces.Get(0));
        leftIp.NewNetwork(); 
    }
    for(uint32_t i=0; i < g_nFlows; ++i)
    {
        NetDeviceContainer link(receiverDevices.Get(i), rightRouter->GetDevice(i));
        Ipv4InterfaceContainer ifaces = rightIp.Assign(link);
        receiverInterfaces.Add(ifaces.Get(0));
        rightIp.NewNetwork();
    }
    routerIp.Assign(routerDevices);
    
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t sinkPort = 8080;
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    sinkHelper.Install(receiverNodes).Start(Seconds(0.2));

    for (uint32_t i = 0; i < g_nFlows; ++i) {
        BulkSendHelper senderHelper("ns3::TcpSocketFactory", InetSocketAddress(receiverInterfaces.GetAddress(i), sinkPort));
        senderHelper.SetAttribute("MaxBytes", UintegerValue(0)); 
        senderHelper.Install(g_senderNodes.Get(i)).Start(Seconds(0.2));
    }

    FlowMonitorHelper flowmonHelper;
    g_monitor = flowmonHelper.InstallAll();
    g_flowMonitorHelper = &flowmonHelper;

    std::ofstream outputFileStream(outputFile);
    g_outputFile = &outputFileStream;
    *g_outputFile << "Time";
    for(uint32_t i=0; i<g_nFlows; ++i) *g_outputFile << ",Flow" << i+1 << "_" << tcpAlgorithms[i] << "_Bps";
    for(uint32_t i=0; i<g_nFlows; ++i) *g_outputFile << ",Flow" << i+1 << "_PktLoss";
    for(uint32_t i=0; i<g_nFlows; ++i) *g_outputFile << ",Flow" << i+1 << "_Cwnd";
    *g_outputFile << ",JainsFairnessIndex" << std::endl;
    
    Simulator::Schedule(Seconds(0.3), &ConnectCwndTraces);
    Simulator::Schedule(Seconds(0.4), &RecordPeriodicStats, 1);

    Simulator::Stop(Seconds(stopTimeSecs));
    Simulator::Run();
    Simulator::Destroy();

    outputFileStream.close();
    std::cout << "Simulation finished. Results saved to " << outputFile << std::endl;
    return 0;
}
