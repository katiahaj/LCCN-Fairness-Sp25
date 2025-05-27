#include <iostream>
#include <fstream>
#include <string>
#include <vector>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/tcp-header.h"

using namespace ns3;

class DumbbellNetwork {
public:
    DumbbellNetwork();
    
private:
    NodeContainer leftHosts;
    NodeContainer rightHosts;
    NodeContainer routers;
    void CreateNodes();
    void InstallInternetStack();
    void SetupNetworkDevices();
    void InstallApplications();
};

DumbbellNetwork::DumbbellNetwork() {
    // Create nodes
    CreateNodes();

    // Install internet stack
    InstallInternetStack();

    // Setup network devices
    SetupNetworkDevices();

    // Install applications
    InstallApplications();
}

void DumbbellNetwork::CreateNodes() {
    // Create nodes
    leftHosts.Create(2);
    rightHosts.Create(2);
    routers.Create(2);

    // Set TCP variant to NewReno
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", 
                      StringValue("ns3::TcpNewReno"));

    std::cout << "Created nodes successfully." << std::endl;
}

void DumbbellNetwork::InstallInternetStack() {
    InternetStackHelper stack;
    stack.Install(leftHosts);
    stack.Install(rightHosts);
    stack.Install(routers);

    std::cout << "Installed internet stack successfully." << std::endl;
}

void DumbbellNetwork::SetupNetworkDevices() {
    // Create links between hosts and routers
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("1ms"));

    // Install host-router links
    NetDeviceContainer leftDevices, rightDevices;
    for(uint32_t i = 0; i < 2; ++i) {
        NetDeviceContainer temp = p2p.Install(leftHosts.Get(i), routers.Get(0));
        leftDevices.Add(temp.Get(1));

        temp = p2p.Install(rightHosts.Get(i), routers.Get(1));
        rightDevices.Add(temp.Get(0));
    }

    // Configure bottleneck link
    PointToPointHelper bottleneckLink;
    bottleneckLink.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    bottleneckLink.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer bottleneckDevices = bottleneckLink.Install(routers.Get(0), routers.Get(1));

    std::cout << "Network devices installed successfully." << std::endl;
}

void DumbbellNetwork::InstallApplications() {
    uint16_t port = 50000;

    // Install TCP applications
    for(uint32_t i = 0; i < 2; ++i) {
        BulkSendHelper source("ns3::TcpSocketFactory",
                            Address(InetSocketAddress(Ipv4Address("10.1.2.1"), port)));
        source.SetAttribute("MaxBytes", UintegerValue(0));
        ApplicationContainer sourceApps = source.Install(leftHosts.Get(i));

        PacketSinkHelper sink("ns3::TcpSocketFactory",
                            Address(InetSocketAddress(Ipv4Address("10.1.2.1"), port)));
        ApplicationContainer sinkApps = sink.Install(rightHosts.Get(i));

        sourceApps.Start(Seconds(1.0));
        sourceApps.Stop(Seconds(10.0));

        sinkApps.Start(Seconds(1.0));
        sinkApps.Stop(Seconds(10.0));

        port++;
    }

    std::cout << "Applications installed successfully." << std::endl;
}

int main(int argc, char *argv[]) {

    // Create simulation
    DumbbellNetwork dumbbell;

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}