/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

/*
This program is used to do the emulation with flows from host to VM , which 
enables our emulation with remote machine like Google Cloud Platfrom. The 
flow goes from VM to host (opposite to that of brite-emu-host-vm) since it's 
what we used for emulation. And the tap bridge configuration is munually done
here instead of using Flow's API since the downstream links are needed to add
cross traffic.

Topology:

    VM - tapbridge - vm leaf - [upstream - access link - downstream] - host leaf - taps
                               [           Brite topology          ]
term     tapVm                    tx                        rx                    tapHost
ip                              10.2.0.0    10.0.0.0      10.1.0.0
cross                                     cross flow      ds cross

*/

#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <ctime>
#include "ns3/ratemonitor-module.h"
#include "ns3/brite-module.h"

#define vint vector<uint32_t>
#define vdouble vector<double>

using namespace std;
using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("BriteEmuCross");


struct LinkChangeTool
{
    uint32_t nFlow;
    uint32_t nShared;
    double T;
    vint vbw;
    NetDeviceContainer rxLeafDevices;
    NetDeviceContainer rxEndDevices;

    LinkChangeTool () {};
    LinkChangeTool (uint32_t nFlow, uint32_t nShared, double T, vint vbw, NetDeviceContainer rxLeafDevices, NetDeviceContainer rxEndDevices): \
        nFlow(nFlow), nShared(nShared), T(T), vbw(vbw), rxLeafDevices(rxLeafDevices), rxEndDevices(rxEndDevices)
    {}

    void startLinkVariation ()
    {
        if (nShared > rxLeafDevices.GetN () || nShared > rxEndDevices.GetN ())
        {
            cout << "Error: devices out of boundary!" << endl;
            exit(1);
        }
        for (uint32_t i = 1; i <= vbw.size(); i ++)
        // for (uint32_t i = 0; i < vbw.size (); i ++)
        {
            for (uint32_t j = 0; j < nShared; j ++)
            {
                NetDeviceContainer nc;
                nc.Add (rxLeafDevices.Get (j));
                nc.Add (rxEndDevices.Get (j));
                bool useP2p = (j >= nFlow);
                double ts = T * (double) i;
                Simulator::Schedule(Seconds (ts), &changeLinkBandwidth, nc, vbw[i - 1], useP2p);
                
                // test
                Simulator::Schedule(Seconds (ts + 0.02), &testLinkBandwidth, nc, vbw[i - 1], useP2p);
            }
        }
    }

} LinkChangeTool0;


bool started = false;

void flowStartSink (bool vOld, bool vNew)
{
    if (!started)
    {
        double offset = 0.2;              // offset at the beginning
        Simulator::Schedule (Seconds(offset), &LinkChangeTool::startLinkVariation, &LinkChangeTool0);
        // LinkChangeTool0.startLinkVariation ();
        started = true;
    }
}


int main(int argc, char * argv[])
{
    uint32_t mid = 1234;
    uint32_t tid = 0;
    uint32_t nFlow = 1;                     // debug by default
    uint32_t nCross = 0;                    // # cross traffic through access link
    uint32_t largeRate = 500;               // rate that is large enough
    string tapVm = "tap-right";             // tap of VM side
    string tapHost = "tap-left";            // tap of host side
    string infoFile = "flow_info_0707.txt";
    string confFile = "brite_conf/TD_DefaultWaxman.conf";
    string bwFile = "bw_info.txt";
    double tStop = 30;
    bool changeBw = false;

    CommandLine cmd;
    cmd.AddValue ("mid", "Run ID (4 digit) ", mid);
    cmd.AddValue ("tid", "Random seed for brite topology ", tid);
    cmd.AddValue ("nFlow", "Number target flows ", nFlow);
    // cmd.AddValue ("nCross", "Number of cross traffic along acess link ", nCross);
    cmd.AddValue ("tapVm", "Prefix of VM tap ", tapVm);
    cmd.AddValue ("tapHost", "Prefix of host tap", tapHost);
    cmd.AddValue ("infoFile", "Flow info file path ", infoFile);
    cmd.AddValue ("confFile", "Conf file for Brite topology ", confFile);
    cmd.AddValue ("bwFile", "Conf file of varying bandwidth ", bwFile);
    cmd.AddValue ("tStop", "Time to stop the simulation ", tStop);
    cmd.AddValue ("changeBw", "If vary the bandwidth along time ", changeBw);
    cmd.Parse (argc, argv);
    largeRate *= 1000000;
    string largeRateStr = to_string(largeRate) + "bps";

    LogComponentEnable ("BriteEmuCross", LOG_LEVEL_DEBUG);
    GlobalValue::Bind ("SimulatorImplementationType", StringValue ("ns3::RealtimeSimulatorImpl"));
    GlobalValue::Bind ("ChecksumEnabled", BooleanValue (true));
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue (1400));
    Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(4096 * 1024));      // 128 (KB) by default, allow at most 85Mbps for 12ms rtt
    Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(4096 * 1024));      // here we use 4096 KB
    NS_LOG_DEBUG ("mid: " << mid << "\ntid: " << tid << "\nnFlow: " << nFlow << "\ntStop: " \
        << tStop << "\ninfoFile: " << infoFile << "\nconfFile: " << confFile << "\nchangeBw: " << changeBw);
    stringstream ss;

    // parse flow info
    uint32_t txAS = 0, rxAS = 1;
    NodeContainer vmLeaf, hostLeaf;
    NetDeviceContainer vmLeafDev, hostRxDev, hostLeafDev;
    vint leafBw, nDsCross;
    vector<vint> targetFlow, crossTraffic;
    nDsCross = parseFlowInfo (nFlow, infoFile, targetFlow, leafBw, crossTraffic, true);
    nCross = crossTraffic.size ();
    for (uint32_t i = 0; i < nCross; i ++)
        cout << "       " << crossTraffic[i][0] << " -> " << crossTraffic[i][1] << endl;

    Ipv4AddressHelper vmAddr ("10.4.0.0", "255.255.255.0");
    Ipv4AddressHelper hostAddr ("10.3.0.0", "255.255.255.0");
    
    // build central brite topology, target flow and main cross traffic
    BriteTopologyHelper bth (confFile);
    bth.AssignStreams (tid % 100);
    InternetStackHelper stack;
    Ipv4AddressHelper addr ("10.0.0.0", "255.255.255.252");
    bth.BuildBriteTopology (stack);
    bth.SetQueue ();
    bth.AssignIpv4Addresses (addr);
    for (uint32_t i = 0; i < nFlow; i ++)      // target flows & ds cross traffic
    {
        uint32_t src = targetFlow[i][0];
        uint32_t des = targetFlow[i][1];
        Ptr<Node> tx = bth.GetNodeForAs (txAS, src);
        Ptr<Node> rx = bth.GetNodeForAs (rxAS, des);
        Flow normalFlow (tx, rx, vmAddr, hostAddr, largeRate, {0, tStop});
        normalFlow.build (largeRateStr, to_string(leafBw[i]) + "bps", false);
        
        for (uint32_t j = 0; j < nDsCross[i]; j ++)
            normalFlow.setOnoffByLink ("ds");           // test needed
        
        vmAddr = normalFlow.getLeftAddr ();
        hostAddr = normalFlow.getRightAddr ();
        vmLeaf.Add (normalFlow.getHost (0));
        hostLeaf.Add (normalFlow.getHost (1));
        vmLeafDev.Add (normalFlow.getEndDevice (0));
        hostRxDev.Add (normalFlow.getEndDevice (2));
        hostLeafDev.Add (normalFlow.getEndDevice (3));
    }
    NS_LOG_DEBUG ("->   Target flows set.");
    for (uint32_t i = 0; i < nCross; i ++)      // cross traffic
    {
        uint32_t src = crossTraffic[i][0];
        uint32_t des = crossTraffic[i][1];
        Ptr<Node> tx = bth.GetNodeForAs (txAS, src);
        Ptr<Node> rx = bth.GetNodeForAs (rxAS, des);
        Flow crossFlow (tx, rx, vmAddr, hostAddr, largeRate, {0, tStop});
        crossFlow.build ();
        crossFlow.setOnoff ();
        vmAddr = crossFlow.getLeftAddr ();
        hostAddr = crossFlow.getRightAddr ();
        hostLeafDev.Add (crossFlow.getEndDevice (3));   // support rate monitor
    }
    NS_LOG_DEBUG ("     Cross traffic for access link set.\n");

    // prepare end devices
    NodeContainer vmEnd, hostEnd;
    vmEnd.Create (nFlow);
    hostEnd.Create (nFlow);
    NetDeviceContainer vmLeafLink, hostLeafLink;
    InternetStackHelper stackEnd;
    Ipv4AddressHelper vmEndAddr ("10.2.0.0", "255.255.255.0");
    Ipv4AddressHelper hostEndAddr ("10.1.0.0", "255.255.255.0");
    CsmaHelper vmCsma, hostCsma;
    vmCsma.SetChannelAttribute ("DataRate", DataRateValue (largeRate));
    hostCsma.SetChannelAttribute ("DataRate", DataRateValue (largeRate));
    Ipv4InterfaceContainer vmIfc, hostIfc;
    
    for (uint32_t i = 0; i < nFlow; i ++)
    {
        NodeContainer pair, tmp;
        NetDeviceContainer pairDev, tmpDev;
        Ipv4InterfaceContainer pairIfc, tmpIfc;
        TrafficControlHelper tch;
        tch.SetRootQueueDisc ("ns3::RedQueueDisc",
                              "MinTh", DoubleValue (5),
                              "MaxTh", DoubleValue (15),
                              "QueueLimit", UintegerValue (60),
                              "LinkBandwidth", DataRateValue (leafBw[i]),
                              "LinkDelay", StringValue ("2ms"));

        // VM side leaf links & VM side tapbridge
        pair.Add (vmLeaf.Get (i));
        pair.Add (vmEnd.Get (i));
        pairDev = vmCsma.Install (pair);
        stackEnd.Install (vmEnd.Get (i));
        pairIfc = vmEndAddr.Assign (pairDev);
        vmEndAddr.NewNetwork ();
        vmLeafLink.Add (pairDev);       // needed later or not?
        vmIfc.Add (pairIfc);

        string vmTapName = tapVm + "-" + to_string (i);
        TapBridgeHelper bridge;
        bridge.SetAttribute ("Mode", StringValue ("UseBridge"));
        bridge.SetAttribute ("DeviceName", StringValue (vmTapName));
        bridge.Install (vmEnd.Get (i), pairDev.Get (1));

        // host side leaf and taps
        tmp.Add (hostEnd.Get (i));
        tmp.Add (hostLeaf.Get (i));
        tmpDev = hostCsma.Install (tmp);
        stackEnd.Install (hostEnd.Get (i));
        // tch.Install (tmpDev.Get (1));               // try solving the broken leaf link
        
        tmpIfc = hostEndAddr.Assign (tmpDev);
        hostEndAddr.NewNetwork ();
        hostLeafLink.Add (tmpDev);
        hostIfc.Add (tmpIfc);

        string hostTapName = tapHost + "-" + to_string (i);
        TapBridgeHelper localTap (hostIfc.GetAddress (1));
        localTap.SetAttribute ("Mode", StringValue ("ConfigureLocal"));
        localTap.SetAttribute ("DeviceName", StringValue (hostTapName));
        localTap.Install (hostEnd.Get (i), tmpDev.Get (0));
        ss << "     Tap bridge No. " << i << ": " << vmTapName << ": " << pairIfc.GetAddress (1) << " -> " \
            << pairIfc.GetAddress (0) << " --[access link]-- " << tmpIfc.GetAddress (1) \
            << " -> " << tmpIfc.GetAddress (0) << " - " << hostTapName << endl;
    }
    ss << "-> Tap bridge set up." << endl;
    NS_LOG_DEBUG (ss.str ());
    ss.str("");

    // calculate ground truth of bottleneck
    /* Conditions given: 
        access link bw (default 100M), nFlow, nCross;
        leaf (ds) bw, nDs;
    */

    vector<vint> bottlenecks;
    double mainBw = 100e6;                              // hardcode
    double mainShare = mainBw / (nFlow + nCross);
    string fname = "ec-bottlenecks_" + to_string(mid) + ".txt";
    ofstream bout (fname.c_str (), ios::out);
    ss << "\n-> Bottleneck ground truth: main share = " << mainShare / 1e3 << " kbps" << endl;
    for (uint32_t i = 0; i < nFlow; i ++)
    {
        double dsShare = (double) leafBw[i] / (nDsCross[i] + 1);
        uint32_t isMainBtnk = dsShare > mainShare? 1 : 0;
        bottlenecks.push_back ({i, isMainBtnk});        // redundent
        bout << i << " " << isMainBtnk << endl;
        string btnk = isMainBtnk? "main" : "downstream";
        ss << "     Flow " << i << ": " << btnk << " ( ds: " << dsShare/1e3 << " kbps)" << endl;
    }
    bout.close ();
    NS_LOG_DEBUG (ss.str ());

    // rate monitors
    vector<Ptr<RateMonitor>> mons;
    Time t0 (Seconds (0.01)), t1 (Seconds (tStop));
    for (uint32_t i = 0; i < nFlow + nCross; i ++)
    {
        vint id = {mid, i};
        Ptr<RateMonitor> mon = CreateObject <RateMonitor, vint, bool> (id, true);
        mons.push_back (mon);
        mon->install (hostLeafDev.Get (i));
        mon->setPpp ();
        mon->start (t0);
        mon->stop (t1);
    }
    NS_LOG_DEBUG ("-> Rate monitors set up.");

    // change bandwidth
    vint vbw;
    parseBandwidth (bwFile, vbw);
    double T = 120;                     // hardcode 
        // now without nSmall setting
    LinkChangeTool0 = LinkChangeTool (nFlow, nFlow + nCross, T, vbw, hostRxDev, hostLeafDev);
    // ! to avoid the bug, use nFlow for 2nd argument
    if (changeBw) flowStartSink (1, 1);


    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
    Simulator::Stop (t1);
    Simulator::Run ();
    Simulator::Destroy ();
    return 0;
}