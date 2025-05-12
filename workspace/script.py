from ns import ns

ns.LogComponentEnable("UdpEchoClientApplication", ns.LOG_LEVEL_INFO)
ns.LogComponentEnable("UdpEchoServerApplication", ns.LOG_LEVEL_INFO)

print("Creating nodes...")
leftLeafNodes = ns.NodeContainer()
leftLeafNodes.Create(2)

rightLeafNodes = ns.NodeContainer()
rightLeafNodes.Create(2)

routerNodes = ns.NodeContainer()
routerNodes.Create(2)

print("Connecting left leaf nodes to left router...")
leftDevices = []
for i in range(2):
    p2p = ns.PointToPointHelper()
    p2p.SetDeviceAttribute("DataRate", ns.StringValue("5Mbps"))
    p2p.SetChannelAttribute("Delay", ns.StringValue("2ms"))
    nc = ns.NodeContainer()
    nc.Add(leftLeafNodes.Get(i))
    nc.Add(routerNodes.Get(0))
    leftDevices.append(p2p.Install(nc))

print("Connecting right leaf nodes to right router...")
rightDevices = []
for i in range(2):
    p2p = ns.PointToPointHelper()
    p2p.SetDeviceAttribute("DataRate", ns.StringValue("5Mbps"))
    p2p.SetChannelAttribute("Delay", ns.StringValue("2ms"))
    nc = ns.NodeContainer()
    nc.Add(rightLeafNodes.Get(i))
    nc.Add(routerNodes.Get(1))
    rightDevices.append(p2p.Install(nc))

print("Connecting the two routers (the dumbbell bar)...")
p2p = ns.PointToPointHelper()
p2p.SetDeviceAttribute("DataRate", ns.StringValue("10Mbps"))
p2p.SetChannelAttribute("Delay", ns.StringValue("1ms"))
routerLink = p2p.Install(routerNodes)

print("Installing Internet stack...")
stack = ns.InternetStackHelper()
for i in range(2):
    stack.Install(leftLeafNodes.Get(i))
    stack.Install(rightLeafNodes.Get(i))
for i in range(2):
    stack.Install(routerNodes.Get(i))

print("Assigning IP addresses...")
address = ns.Ipv4AddressHelper()

leftInterfaces = []
for i, dev in enumerate(leftDevices):
    base = "10.1.%d.0" % (i+1)
    address.SetBase(ns.Ipv4Address(base), ns.Ipv4Mask("255.255.255.0"))
    leftInterfaces.append(address.Assign(dev))
    print(f"Assigned {base} to left leaf {i}")

rightInterfaces = []
for i, dev in enumerate(rightDevices):
    base = "10.2.%d.0" % (i+1)
    address.SetBase(ns.Ipv4Address(base), ns.Ipv4Mask("255.255.255.0"))
    rightInterfaces.append(address.Assign(dev))
    print(f"Assigned {base} to right leaf {i}")

address.SetBase(ns.Ipv4Address("10.3.1.0"), ns.Ipv4Mask("255.255.255.0"))
routerInterfaces = address.Assign(routerLink)
print("Assigned 10.3.1.0 to router-router link")

print("Setting up UDP echo server on rightmost leaf node...")
echoServer = ns.UdpEchoServerHelper(9)
serverApps = echoServer.Install(rightLeafNodes.Get(1))
serverApps.Start(ns.Seconds(1))
serverApps.Stop(ns.Seconds(10))

print("Setting up UDP echo client on leftmost leaf node, targeting the server...")
serverAddress = rightInterfaces[1].GetAddress(0).ConvertTo()
print(f"Server address is {serverAddress}")
echoClient = ns.UdpEchoClientHelper(serverAddress, 9)
echoClient.SetAttribute("MaxPackets", ns.UintegerValue(1))
echoClient.SetAttribute("Interval", ns.TimeValue(ns.Seconds(1)))
echoClient.SetAttribute("PacketSize", ns.UintegerValue(1024))

clientApps = echoClient.Install(leftLeafNodes.Get(0))
clientApps.Start(ns.Seconds(2))
clientApps.Stop(ns.Seconds(10))

print("Starting simulation...")
ns.Simulator.Run()
print("Simulation finished.")
ns.Simulator.Destroy()