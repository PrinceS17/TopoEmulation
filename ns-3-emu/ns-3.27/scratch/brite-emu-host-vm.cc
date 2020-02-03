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
enables our emulation with remote machine like Google Cloud Platfrom. Since
the flows go from host, we use ConfigureLocal mode on TX side. Besides, this
program also enables time scheduling of switching our bottleneck by changing
main or downstream link capacity, which is cool!
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

NS_LOG_COMPONENT_DEFINE ("BriteEmuHostVm");


// parse the flow info file, store target flow, leaf BW and cross traffic
void parseFlowInfo (uint32_t nFlow, string infoFile, vector<vint>& targetFlow, vint& leafBw, vector<vint>& crossTraffic)
{
    ifstream fin (infoFile, ios::in);
    uint32_t src, des;
    double bw;
    vector<vint> tFlow, cTraffic;
    vint lBw, tmp;
    for (uint32_t i = 0; i < nFlow; i ++)
    {
        fin >> src >> des >> bw;
        tmp = {src, des};
        tFlow.push_back (tmp);
        lBw.push_back (bw);
    }
    while (!fin.eof ())
    {
        src = -1;
        des = -1;
        fin >> src >> des;
        if (src == 4294967295 || des == 4294967295) break;
        tmp = {src, des};
        cTraffic.push_back (tmp);
    }
    fin.close ();
    targetFlow = tFlow;
    leafBw = lBw;
    crossTraffic = cTraffic;
    NS_LOG_DEBUG (" - Flow info parsed, target flows:");
    for (uint32_t i = 0; i < nFlow; i ++)
        NS_LOG_DEBUG (" -- " << targetFlow[i][0] << " -> " << targetFlow[i][1] << ", " 
            << leafBw[i] << " bps");
    NS_LOG_DEBUG (" - Cross traffic, size of " << crossTraffic.size ());
    for (uint32_t i = 0; i < crossTraffic.size (); i ++)
        NS_LOG_DEBUG (" -- " << crossTraffic[i][0] << " -> " << crossTraffic[i][1]);
}


void parseBandwidth (string bwFile, vint& bws)
{
    ifstream fin (bwFile, ios::in);
    vint tmp_bw;
    int val;
    while (!fin.eof ())
    {
        val = -1;
        fin >> val;
        if (val > 0) tmp_bw.push_back (uint32_t(val));
        else break;
    }
    fin.close ();
    bws = tmp_bw;
    NS_LOG_DEBUG (" - BW info parsed.");
    for (uint32_t i = 0; i < bws.size (); i ++)
        NS_LOG_DEBUG (" -- " << bws[i] << " bps");   
}


void testLinkBandwidth (NetDeviceContainer dev, uint32_t bw, bool useP2p = false)
{
    NS_LOG_DEBUG ("-- " << Simulator::Now ().GetSeconds () << "s : Testing link bandwidth: " \
        << bw << " bps expected..");
    if (!useP2p)
    {
        Ptr<CsmaChannel> channel = DynamicCast<CsmaChannel> (dev.Get (0)->GetChannel ());
        NS_ASSERT_MSG (channel->GetDataRate () == DataRate (bw), "Wrong channel rate!");
    }
    else
    {
        NS_LOG_DEBUG (" Note: expect BW of a point-to-point link: " << bw << " bps");
    }
}


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
        double offset = 2;              // offset at the beginning
        Simulator::Schedule (Seconds(offset), &LinkChangeTool::startLinkVariation, &LinkChangeTool0);
        // LinkChangeTool0.startLinkVariation ();
        started = true;
    }
}


int main (int argc, char* argv[])
{
    uint32_t mid = 1234;
    uint32_t tid = 0;
    uint32_t nFlow = 2;    
    uint32_t crossRate = 100;
    uint32_t edgeRate = 1000;
    uint32_t offset = 0;
    double tStop = 20;
    uint32_t nSmall = 3;
    string tapLeft = "tap-left";
    string tapRight = "tap-right";
    string infoFile = "flow_info_1234.txt";
    string confFile = "brite_conf/TD_DefaultWaxman.conf";       // 100Mbps
    // string confFile = "brite_conf/TD_inter=220M_Waxman.conf";
    string bwFile = "bw_info.txt";
    bool changeBw = false;
    bool testFlow = false;

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
    cmd.AddValue ("bwFile", "Conf file for bw varying", bwFile);
    cmd.AddValue ("changeBw", "If change the bandwidth along time", changeBw);
    cmd.AddValue ("offset", "Off set of tap index", offset);
    cmd.AddValue ("nSmall", "Number of small flows", nSmall);
    cmd.AddValue ("testFlow", "Test mode of flow (ns3 flow instead of iperf)", testFlow);
    cmd.Parse (argc, argv);
    crossRate *= 1000000;
    edgeRate *= 1000000;
    stringstream ss;

    LogComponentEnable ("BriteEmuHostVm", LOG_LEVEL_INFO);
    // LogComponentEnable ("RateMonitor", LOG_INFO);
    GlobalValue::Bind ("SimulatorImplementationType", StringValue ("ns3::RealtimeSimulatorImpl"));
    GlobalValue::Bind ("ChecksumEnabled", BooleanValue (true));
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue (1400));
    Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(4096 * 1024));      // 128 (KB) by default, allow at most 85Mbps for 12ms rtt
    Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(4096 * 1024));      // here we use 4096 KB
    NS_LOG_DEBUG ("mid: " << mid << "\ntid: " << tid << "\nnFlow: " << nFlow << "\ncRate: " \
        << crossRate << "\neRate: " << edgeRate << "\ntStop: " << tStop << "\ninfoFile: " << \
        infoFile << "\nconfFile: " << confFile << "\nchangeBw: " << changeBw << "\nnSmall: " << nSmall);
    
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
    NetDeviceContainer txEndDevices, rxEndDevices, rxLeafDevices;
    string leftStr = "10.1." + to_string(offset) + ".0";
    string rightStr = "10.2." + to_string(offset) + ".0";
    Ipv4AddressHelper leftAddr (leftStr.c_str(), "255.255.255.0");
    Ipv4AddressHelper rightAddr (rightStr.c_str(), "255.255.255.0");
    for (uint32_t i = 0; i < nFlow; i ++)
    {
        uint32_t src = targetFlow[i][0];
        uint32_t des = targetFlow[i][1];
        Ptr<Node> txLeaf = bth.GetNodeForAs (txAS, src);
        Ptr<Node> rxLeaf = bth.GetNodeForAs (rxAS, des);
        uint32_t normalRate = testFlow? 100000000 : 0;
        Flow normalFlow (txLeaf, rxLeaf, leftAddr, rightAddr, normalRate, {0, tStop});
        NS_LOG_INFO (i << ": " << to_string (edgeRate) + "bps" << ", " << to_string(leafBw[i]) + "bps");
        normalFlow.build (to_string (edgeRate) + "bps", to_string(leafBw[i]) + "bps",\
            true);
        
        if (testFlow) normalFlow.setOnoff ();
        else normalFlow.setLocalBridge (tapLeft + "-" + to_string(src), \
        tapRight + "-" + to_string(des));       // note: src != i

        leftAddr = normalFlow.getLeftAddr ();
        rightAddr = normalFlow.getRightAddr ();
        txEnds.Add (normalFlow.getHost (0));
        rxEnds.Add (normalFlow.getHost (1));
        txEndDevices.Add (normalFlow.getEndDevice (0));
        rxLeafDevices.Add (normalFlow.getEndDevice (2));
        rxEndDevices.Add (normalFlow.getEndDevice (3));
    }
    NS_LOG_INFO ("-> Target flows set.");

    // set up cross traffic: mainly not needed in host-vm scenario
    for (uint32_t i = 0; i < nCross; i ++)
    {
        uint32_t src = crossTraffic[i][0];
        uint32_t des = crossTraffic[i][1];
        Ptr<Node> txLeaf = bth.GetNodeForAs (txAS, src);
        Ptr<Node> rxLeaf = bth.GetNodeForAs (rxAS, des);
        Flow crossTraffic (txLeaf, rxLeaf, leftAddr, rightAddr, crossRate, {0, tStop});
        crossTraffic.build (to_string (edgeRate) + "bps");
        crossTraffic.setPpbp ();
        leftAddr = crossTraffic.getLeftAddr ();
        rightAddr = crossTraffic.getRightAddr ();
        txEnds.Add (crossTraffic.getHost (0));
        txEndDevices.Add (crossTraffic.getEndDevice (0));
        rxLeafDevices.Add (crossTraffic.getEndDevice (2));
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
        // Ptr<RateMonitor> mon = CreateObject <RateMonitor, vint, bool> (id, false);      // use phyRx here
        
        mons.push_back (mon);
        mon->install (rxEndDevices.Get (i));
        // mon->install (txEndDevices.Get (i));         // test tx sending rate
        mon->start (t0);
        mon->stop (t1);
        
    }
    NS_LOG_INFO ("-> MiniBox and RateMonitor set. \n");


    // change bandwidth given schedule, bandwidth
    vint vbw;
    parseBandwidth (bwFile, vbw);
    double T = 20;
    LinkChangeTool0 = LinkChangeTool (nFlow, nFlow + nCross - nSmall, T, vbw, rxLeafDevices, rxEndDevices);    
    if (changeBw) flowStartSink (true, true);


    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
    Simulator::Stop (Seconds (tStop));
    Simulator::Run ();
    Simulator::Destroy ();
    return 0;
}