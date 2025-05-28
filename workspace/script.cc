#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-layout-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SimpleTcpDumbbell");

int main(int argc, char* argv[]) {
    LogComponentEnable("SimpleTcpDumbbell", LOG_LEVEL_INFO);

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

    p2pRouter.EnablePcapAll("simple-tcp-dumbbell-router");

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    NS_LOG_INFO("Running Simulation.");
    Simulator::Stop(stopTime + Seconds(5.0));
    Simulator::Run();

    monitor->CheckForLostPackets();
    monitor->SerializeToXmlFile("./scratch/workspace/simple-tcp-dumbbell-flowmon.xml", true, true);

    NS_LOG_INFO("Simulation Finished.");
    Simulator::Destroy();

    return 0;
}
