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
This program is used to conduct the emulation with VMs and brite topology
generator. It's similar to brite-for-all.cc except tap bridge instead of
OnOff application is used here for target flows.

Note currently the leaf link should be hardcoded to CSMA for emulation.
*/

#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <ctime>
#include "ns3/ratemonitor-module.h"
#include "ns3/brite-module.h"
#include "ns3/minibox-module.h"

#define vint vector<uint32_t>
#define vdouble vector<double>

using namespace std;
using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("BriteForEmu");


int main (int argc, char* argv[])
{
    uint32_t mid = 1234;
    uint32_t tid = 0;
    uint32_t nFlow = 2;    
    uint32_t crossRate = 10;
    uint32_t edgeRate = 1000;
    double tStop = 30;
    string tapLeft = "tap-left";
    string tapRight = "tap-right";
    string infoFile = "flow_info_1234.txt";
    string confFile = "brite_conf/TD_DefaultWaxman.conf";

    CommandLine cmd;
    cmd.AddValue ("mid", "Run ID (4 digit)", mid);
    cmd.AddValue ("tid", "Random seed for brite topology", tid);
    cmd.AddValue ("nFlow", "Number of target flows", nFlow);
    cmd.AddValue ("cRate", "Rate of cross traffic (in Mbps)", crossRate);
    cmd.AddValue ("eRate", "Rate of edge link (in Mbps)", edgeRate);
    cmd.AddValue ("tStop", "Time duration of simulation (in s)", tStop);
    cmd.AddValue ("tapLeft", "Prefix for left tap", tapLeft);
    cmd.AddValue ("tapRight", "Prefix for right tap", tapRight);
    cmd.AddValue ("infoFile", "Flow info file path", infoFile);
    cmd.AddValue ("confFile", "Conf file for brite generator", confFile);
    cmd.Parse (argc, argv);
    crossRate *= 1000000;
    edgeRate *= 1000000;
    stringstream ss;

    LogComponentEnable ("BriteForEmu", LOG_LEVEL_INFO);
    GlobalValue::Bind ("SimulatorImplementationType", StringValue ("ns3::RealtimeSimulatorImpl"));
    GlobalValue::Bind ("ChecksumEnabled", BooleanValue (true));
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue (1400));
    Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(4096 * 1024));      // 128 (KB) by default, allow at most 85Mbps for 12ms rtt
    Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(4096 * 1024));      // here we use 4096 KB
    NS_LOG_DEBUG ("mid: " << mid << "\ntid: " << tid << "\nnFlow: " << nFlow << "\ncRate: " \
        << crossRate << "\neRate: " << edgeRate << "\ntStop: " << tStop << "\ninfoFile: " << \
        infoFile << "\nconfFile: " << confFile);
    
    // build topology
    BriteTopologyHelper bth (confFile);
    bth.AssignStreams (tid % 100);
    InternetStackHelper stack;
    Ipv4AddressHelper addr ("10.0.0.0", "255.255.255.0");
    bth.BuildBriteTopology (stack);
    bth.SetQueue ();
    bth.AssignIpv4Addresses (addr);
    ss << "-> BRITE topology built. Number of ASes: " << bth.GetNAs () << endl;
    ss << "  Number of routers and leaves: " << endl;
    for (uint32_t i = 0; i < bth.GetNAs (); i ++)
        ss << "  - AS " << i << ": " << bth.GetNNodesForAs (i) << " nodes, " << \
            bth.GetNLeafNodesForAs (i) << " leaves;" << endl;
    NS_LOG_DEBUG (ss.str());
    ss.str("");

    // parse flow info
    NodeContainer txEnd, ctEnd;
    NetDeviceContainer txEndDev, rxEndDev, ctEndDev, crEndDev;
    vint leafBw;
    vector<vint> targetFlow, crossTraffic;       // leafBw: src -> leaf bw
    parseFlowInfo (nFlow, infoFile, targetFlow, leafBw, crossTraffic);
    uint32_t nCross = crossTraffic.size ();

    // set up target flows
    uint32_t txAS = 0, rxAS = 1;
    NodeContainer txEnds, rxEnds;
    NetDeviceContainer txEndDevices, rxEndDevices;
    Ipv4AddressHelper leftAddr ("10.1.0.0", "255.255.255.0");
    Ipv4AddressHelper rightAddr ("10.2.0.0", "255.255.255.0");
    for (uint32_t i = 0; i < nFlow; i ++)
    {
        uint32_t src = targetFlow[i][0];
        uint32_t des = targetFlow[i][1];
        Ptr<Node> txLeaf = bth.GetNodeForAs (txAS, src);
        Ptr<Node> rxLeaf = bth.GetNodeForAs (rxAS, des);
        Flow normalFlow (txLeaf, rxLeaf, leftAddr, rightAddr, 0, {0, tStop});
        normalFlow.build (to_string (edgeRate) + "Mbps", "", true);
        normalFlow.setTapBridge (tapLeft + "-" + to_string(src), \
            tapRight + "-" + to_string(des));
        leftAddr = normalFlow.getLeftAddr ();
        rightAddr = normalFlow.getRightAddr ();
        txEnds.Add (normalFlow.getHost (0));
        rxEnds.Add (normalFlow.getHost (1));
        txEndDevices.Add (normalFlow.getEndDevice (0));
        rxEndDevices.Add (normalFlow.getEndDevice (3));
    }
    NS_LOG_INFO ("-> Target flows set.");

    // set up cross traffic
    for (uint32_t i = 0; i < nCross; i ++)
    {
        uint32_t src = crossTraffic[i][0];
        uint32_t des = crossTraffic[i][1];
        Ptr<Node> txLeaf = bth.GetNodeForAs (txAS, src);
        Ptr<Node> rxLeaf = bth.GetNodeForAs (rxAS, des);
        Flow crossTraffic (txLeaf, rxLeaf, leftAddr, rightAddr, crossRate, {0, tStop});
        crossTraffic.build (to_string (edgeRate) + "Mbps");
        crossTraffic.setPpbp ();
        leftAddr = crossTraffic.getLeftAddr ();
        rightAddr = crossTraffic.getRightAddr ();
        txEnds.Add (crossTraffic.getHost (0));
        txEndDevices.Add (crossTraffic.getEndDevice (0));
        rxEndDevices.Add (crossTraffic.getEndDevice (3));
    }
    NS_LOG_INFO ("   Cross traffic set.");

    // set up rate monitors
    vector<Ptr<RateMonitor>> mons;
    Time t0 (Seconds (0.01)), t1 (Seconds (tStop));
    for (uint32_t i = 0; i < nFlow + nCross; i ++)
    {
        vint id = {mid, i};
        Ptr<RateMonitor> mon = CreateObject <RateMonitor, vint, bool> (id, true);
        mons.push_back (mon);
        mon->install (rxEndDevices.Get (i));
        // mon->install (txEndDevices.Get (i));         // test tx sending rate
        mon->start (t0);
        mon->stop (t1);
    }
    NS_LOG_INFO ("-> MiniBox and RateMonitor set. \n");

    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
    Simulator::Stop (Seconds (tStop));
    Simulator::Run ();
    Simulator::Destroy ();
    return 0;
}