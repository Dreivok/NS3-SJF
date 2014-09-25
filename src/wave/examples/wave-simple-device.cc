/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2013 Dalian University of Technology
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
 * Author: Junling Bu <linlinjavaer@gmail.com>
 */
#include "ns3/node.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/node-container.h"
#include "ns3/net-device-container.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-helper.h"
#include <iostream>
#include "ns3/constant-velocity-mobility-model.h"
#include "ns3/mobility-model.h"

#include "ns3/wave-net-device.h"
#include "ns3/wave-mac-helper.h"
#include "ns3/wave-helper.h"

using namespace ns3;
/**
 * How to send WSMP packets or and IP packets:
 * first have a WaveNetDevice;
 * then assign channel access for sending packet (IP packets should not
 * request CCH channel); Now you can send WSMP packets with SendX method.
 * However before sending IP packets, you should still register a TxProfile
 * to tell which channel will be used and other information,
 * then you can send IP packets with Send method
 */
class WaveNetDeviceExample
{
public:
  void SendWsmpExample ();

  void SendIpExample ();

  void CreateWaveNodes ();

private:
  bool Receive (Ptr<NetDevice> dev, Ptr<const Packet> pkt, uint16_t mode, const Address &sender);
  NodeContainer nodes;
  NodeContainer nodes2;
  NetDeviceContainer devices;
  NetDeviceContainer devices2;
};
void
WaveNetDeviceExample::CreateWaveNodes ()
{
  nodes = NodeContainer ();
  nodes.Create (3);
  nodes2 = NodeContainer ();
  nodes2.Create (5);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator");
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  for (NodeContainer::Iterator i = nodes.Begin (); i != nodes.End (); ++i)
    {
      Ptr<Node> node = (*i);
      Ptr<MobilityModel> model = node->GetObject<MobilityModel> ();
      Vector pos = model->GetPosition ();
    }

  MobilityHelper mobility2;
  mobility2.SetPositionAllocator ("ns3::GridPositionAllocator");
  mobility2.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility2.Install (nodes2);

  for (NodeContainer::Iterator i = nodes2.Begin (); i != nodes2.End (); ++i)
    {
      Ptr<Node> node = (*i);
      Ptr<MobilityModel> model = node->GetObject<MobilityModel> ();
      Vector pos = model->GetPosition ();
    }

  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());
  WaveMacHelper waveMac = WaveMacHelper::Default ();
  WaveHelper waveHelper = WaveHelper::Default ();
  devices = waveHelper.Install (wifiPhy, waveMac, nodes);
  devices2 = waveHelper.Install (wifiPhy, waveMac, nodes2);

  for (uint32_t i = 0; i != devices.GetN (); ++i)
    {
      devices.Get (i)->SetReceiveCallback (MakeCallback (&WaveNetDeviceExample::Receive,this));
    }

  for (uint32_t i = 0; i != devices2.GetN (); ++i)
    {
      devices2.Get (i)->SetReceiveCallback (MakeCallback (&WaveNetDeviceExample::Receive,this));
    }

}
bool
WaveNetDeviceExample::Receive (Ptr<NetDevice> dev, Ptr<const Packet> pkt, uint16_t mode, const Address &sender)
{
  std::cout << "receive packet: Time = " << std::dec << Now ().GetSeconds ()
            << "s protocol = 0x" << std::hex << (int)mode << std::endl;
  return true;
}
void
WaveNetDeviceExample::SendIpExample ()
{
  Ptr<WaveNetDevice>  node1 = DynamicCast<WaveNetDevice> (devices.Get (0));
  Ptr<WaveNetDevice>  node2 = DynamicCast<WaveNetDevice> (devices.Get (1));
  Ptr<WaveNetDevice>  node3 = DynamicCast<WaveNetDevice> (devices.Get (2));

  Ptr<WaveNetDevice>  rsu = DynamicCast<WaveNetDevice> (devices2.Get (0));
  Ptr<WaveNetDevice>  rsuSCH2 = DynamicCast<WaveNetDevice> (devices2.Get (1));
  Ptr<WaveNetDevice>  rsuSCH3 = DynamicCast<WaveNetDevice> (devices2.Get (2));

  // Continuous access without immediate channel switch
  const SchInfo sch1Info = SchInfo (SCH2, false, 0xff);
  const SchInfo sch2Info = SchInfo (SCH3, false, 0xff);
  const TxProfile txProfile1 = TxProfile (SCH2);
  const TxProfile txProfile2 = TxProfile (SCH3);
  Ptr<Packet> packet = Create<Packet> (100);
  const Address dest = rsu->GetAddress ();

  node1->SetSchedule(false);

  //register txprofile
  // IP packets will automatically be sent with txprofile parameter
  Simulator::Schedule (Seconds (1.0),&WaveNetDevice::StartSch,rsuSCH2,sch1Info);
  Simulator::Schedule (Seconds (1.0),&WaveNetDevice::RegisterTxProfile,rsuSCH2,txProfile1);
  Simulator::Schedule (Seconds (1.0),&WaveNetDevice::StartSch,rsuSCH3,sch2Info);
  Simulator::Schedule (Seconds (1.0),&WaveNetDevice::RegisterTxProfile,rsuSCH3,txProfile2);

  // unregister TxProfile or release channel access
  Simulator::Schedule (Seconds (19.0),&WaveNetDevice::UnregisterTxProfile, rsuSCH2,SCH2);
  Simulator::Schedule (Seconds (20.0),&WaveNetDevice::StopSch, rsuSCH2,SCH2);
  Simulator::Schedule (Seconds (19.0),&WaveNetDevice::UnregisterTxProfile, rsuSCH3,SCH3);
  Simulator::Schedule (Seconds (20.0),&WaveNetDevice::StopSch, rsuSCH3,SCH3);

  uint16_t ipv4_protocol = 0x0800, ipv6_protocol = 0x86DD;
  // send IPv4 packet and IPv6 packet successfully
  Simulator::Schedule (Seconds (2.2),&WaveNetDevice::Send, rsuSCH2,packet, node1->GetAddress(), ipv4_protocol);
  Simulator::Schedule (Seconds (5.3),&WaveNetDevice::Send, rsuSCH2,packet, node1->GetAddress(), ipv6_protocol);
  Simulator::Schedule (Seconds (2.8),&WaveNetDevice::Send, rsuSCH3,packet, node2->GetAddress(), ipv4_protocol);
  Simulator::Schedule (Seconds (15.3),&WaveNetDevice::Send, rsuSCH3,packet, node2->GetAddress(), ipv6_protocol);
  Simulator::Schedule (Seconds (14.3),&WaveNetDevice::Send, rsuSCH3,packet, node1->GetAddress(), ipv6_protocol);


  
  // send IP packet again
  // this packet will be dropped, because neither SCH channel has channel access
  // nor has registered a TxProfile
/*  Simulator::Schedule (Seconds (4.0),&WaveNetDevice::StartSch,sender3,sch2Info);
  Simulator::Schedule (Seconds (4.0),&WaveNetDevice::RegisterTxProfile,sender3,txProfile2);

  Simulator::Schedule (Seconds (5.1),&WaveNetDevice::Send, sender3,packet, rsuSCH2->GetAddress(), ipv6_protocol);
  
  Simulator::Schedule (Seconds (6.0),&WaveNetDevice::UnregisterTxProfile, sender3,SCH2);
  Simulator::Schedule (Seconds (7.0),&WaveNetDevice::StopSch, sender3,SCH2);*/

  // sender needs to assign channel access, the receiver also needs to assign channel
  // access to receive packets.
  Simulator::Schedule (Seconds (1.0),&WaveNetDevice::StartSch, node1, sch1Info);
  Simulator::Schedule (Seconds (1.0),&WaveNetDevice::RegisterTxProfile,node1,txProfile1);
  Simulator::Schedule (Seconds (9.0),&WaveNetDevice::UnregisterTxProfile, node1,SCH2);
  Simulator::Schedule (Seconds (9.0),&WaveNetDevice::StopSch, node1, SCH2);

  Simulator::Schedule (Seconds (11.0),&WaveNetDevice::StartSch, node1, sch2Info);
  Simulator::Schedule (Seconds (11.0),&WaveNetDevice::RegisterTxProfile,node1,txProfile2);
  Simulator::Schedule (Seconds (19.0),&WaveNetDevice::UnregisterTxProfile, node1,SCH3);
  Simulator::Schedule (Seconds (19.0),&WaveNetDevice::StopSch, node1, SCH3);

  Simulator::Schedule (Seconds (1.0),&WaveNetDevice::StartSch, node2, sch2Info);
  Simulator::Schedule (Seconds (1.0),&WaveNetDevice::RegisterTxProfile,node2,txProfile2);
  Simulator::Schedule (Seconds (6.0),&WaveNetDevice::UnregisterTxProfile, node2,SCH3);
  Simulator::Schedule (Seconds (6.0),&WaveNetDevice::StopSch, node2, SCH3);


  Simulator::Stop (Seconds (20.0));
  Simulator::Run ();
  Simulator::Destroy ();
}
const static uint16_t WSMP_PROT_NUMBER = 0x88DC;
void
WaveNetDeviceExample::SendWsmpExample ()
{
  Ptr<WaveNetDevice>  sender = DynamicCast<WaveNetDevice> (devices.Get (0));
  Ptr<WaveNetDevice>  receiver = DynamicCast<WaveNetDevice> (devices.Get (1));

  // send a WSMP packet
  const TxInfo txInfo = TxInfo (CCH);
  Ptr<Packet> packet = Create<Packet> (100);
  const Address dest = receiver->GetAddress ();
  // this packet will send successfully
  Simulator::Schedule (Seconds (1.1),&WaveNetDevice::SendX, sender,
                       packet->Copy(), dest, WSMP_PROT_NUMBER, txInfo);
  const TxInfo txInfo2 = TxInfo (SCH1);
  // this packet will be dropped, because SCH1 channel has no channel access
  Simulator::Schedule (Seconds (1.3),&WaveNetDevice::SendX, sender,
		  	  	  	  packet->Copy(), dest, WSMP_PROT_NUMBER, txInfo2);
  // send WSMP packet again
  // this packet will be dropped, because CCH channel has no channel access
  Simulator::Schedule (Seconds (2.1),&WaveNetDevice::SendX, sender,
		  	  	  	  packet->Copy(), dest, WSMP_PROT_NUMBER, txInfo);

  const SchInfo schInfo = SchInfo (CCH, false, 0xff);
  Simulator::Schedule (Seconds (1.0),&WaveNetDevice::StartSch,sender,schInfo);
  // sender needs to assign channel access, the receiver needs to assign channel
  // access to receive packets.
  Simulator::Schedule (Seconds (1.0),&WaveNetDevice::StartSch, receiver, schInfo);
  Simulator::Schedule (Seconds (5.0),&WaveNetDevice::StopSch, receiver, CCH);

  Simulator::Stop (Seconds (5.0));
  Simulator::Run ();
  Simulator::Destroy ();
}

int
main (int argc, char *argv[])
{
  WaveNetDeviceExample example;
//  example.CreateWaveNodes ();
//  example.SendWsmpExample ();
  example.CreateWaveNodes ();
  example.SendIpExample ();
  return 0;
}
