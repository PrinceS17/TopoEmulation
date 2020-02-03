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
 */

#include <vector>
#include <chrono>
#include <ctime>  
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/tools.h"
#include "ns3/onoff-application.h"
#include "ns3/on-off-helper.h"
#include "ns3/ratemonitor-module.h"

using namespace ns3;
using namespace std;

NS_LOG_COMPONENT_DEFINE ("SimpleTest");


void changeCsmaBandwidth (NetDeviceContainer dev, uint32_t bw)
{
    Ptr<CsmaChannel> channel = DynamicCast<CsmaChannel> (dev.Get (0)->GetChannel ());
    for (uint32_t i:{0, 1})
    {
        Ptr<CsmaNetDevice> cdev = DynamicCast<CsmaNetDevice> (dev.Get (i));
        channel->Detach (cdev);     // cdev detach channel: not work
    }
    channel->SetAttribute ("DataRate", DataRateValue(bw));
    for (uint32_t i:{0, 1})
    {
        Ptr<CsmaNetDevice> cdev = DynamicCast<CsmaNetDevice> (dev.Get (i));
        cdev->Attach (channel);     // channel->Reattach(cdev): only set active, don't change rate!
    }

}

int main (int argc, char *argv[])
{
    LogComponentEnable ("SimpleTest", LOG_INFO);
    LogComponentEnable ("RateMonitor", LOG_INFO);
    double tStop = 10;
    bool useP2p = true;

    NodeContainer nodes;
    nodes.Create (2);
    NetDeviceContainer dev;
    if (useP2p)
    {
        PointToPointHelper p2p;
        p2p.SetDeviceAttribute ("DataRate", StringValue ("1Mbps"));
        p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));
        dev = p2p.Install (nodes);
    }
    else
    {
        CsmaHelper csma;
        csma.SetChannelAttribute ("DataRate", StringValue ("5Mbps"));
        dev = csma.Install (nodes);
    }
    

    InternetStackHelper stack;
    stack.Install (nodes);
    Ipv4AddressHelper address("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer ifc = address.Assign (dev);

    // set on off application
    uint32_t port = 8080;
    string typeId = "ns3::TcpSocketFactory";
    PacketSinkHelper sink (typeId, InetSocketAddress (Ipv4Address::GetAny (), port));
    ApplicationContainer sinkApp = sink.Install (nodes.Get (1));
    sinkApp.Start (Seconds (0));
    sinkApp.Stop (Seconds (tStop));
    NS_LOG_INFO ("Sink app started.");

    OnOffHelper src (typeId, InetSocketAddress (ifc.GetAddress (1), port));
    src.SetConstantRate (DataRate(10000000));
    ApplicationContainer srcApp = src.Install (nodes.Get (0));
    srcApp.Start (Seconds (0));
    srcApp.Stop (Seconds (tStop));
    NS_LOG_INFO ("On off started.");

    // set up rate monitor
    RateMonitor mon({1111, 0}, true);
    mon.install (dev.Get (1));
    mon.start (Seconds (0));
    mon.stop (Seconds (tStop));
    NS_LOG_INFO ("Rate Monitor set up.");

    // test change rate functionality
    // Simulator::Schedule (Seconds (5), &changeLinkBandwidth, dev, 200000);
    Simulator::Schedule (Seconds (5), &changeLinkBandwidth, dev, 2000000, true);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
    Simulator::Stop (Seconds (10));
    Simulator::Run ();
    Simulator::Destroy ();

    return 0;
}