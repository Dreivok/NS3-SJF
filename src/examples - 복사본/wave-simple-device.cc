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
  NetDeviceContainer devices;
};
void
WaveNetDeviceExample::CreateWaveNodes ()
{
  nodes = NodeContainer ();
  nodes.Create (2);

  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 0.0));
  positionAlloc->Add (Vector (5.0, 0.0, 0.0));
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());
  WaveMacHelper waveMac = WaveMacHelper::Default ();
  WaveHelper waveHelper = WaveHelper::Default ();
  devices = waveHelper.Install (wifiPhy, waveMac, nodes);

  for (uint32_t i = 0; i != devices.GetN (); ++i)
    {
      devices.Get (i)->SetReceiveCallback (MakeCallback (&WaveNetDeviceExample::Receive,this));
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
  Ptr<WaveNetDevice>  sender = DynamicCast<WaveNetDevice> (devices.Get (0));
  Ptr<WaveNetDevice>  receiver = DynamicCast<WaveNetDevice> (devices.Get (1));

  // Continuous access without immediate channel switch
  const SchInfo schInfo = SchInfo (SCH1, false, 0xff);
  Ptr<Packet> packet = Create<Packet> (100);
  const Address dest = receiver->GetAddress ();
  Simulator::Schedule (Seconds (1.0),&WaveNetDevice::StartSch,sender,schInfo);
  //register txprofile
  // IP packets will automatically be sent with txprofile parameter
  const TxProfile txProfile = TxProfile (SCH1);
  Simulator::Schedule (Seconds (2.0),&WaveNetDevice::RegisterTxProfile,
                       sender,txProfile);
  uint16_t ipv4_protocol = 0x0800, ipv6_protocol = 0x86DD;
  // send IPv4 packet and IPv6 packet successfully
  Simulator::Schedule (Seconds (2.1),&WaveNetDevice::Send, sender,
                       packet, dest, ipv4_protocol);
  Simulator::Schedule (Seconds (2.3),&WaveNetDevice::Send, sender,
                       packet, dest, ipv6_protocol);
  // unregister TxProfile or release channel access
  Simulator::Schedule (Seconds (3.0),&WaveNetDevice::UnregisterTxProfile, sender,SCH1);
  Simulator::Schedule (Seconds (4.0),&WaveNetDevice::StopSch, sender,SCH1);
  // send IP packet again
  // this packet will be dropped, because neither SCH channel has channel access
  // nor has registered a TxProfile
  Simulator::Schedule (Seconds (5.1),&WaveNetDevice::Send, sender,
                       packet, dest, ipv6_protocol);

  // sender needs to assign channel access, the receiver also needs to assign channel
  // access to receive packets.
  Simulator::Schedule (Seconds (1.0),&WaveNetDevice::StartSch, receiver, schInfo);
  Simulator::Schedule (Seconds (6.0),&WaveNetDevice::StopSch, receiver, SCH1);


  Simulator::Stop (Seconds (6.0));
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
  example.CreateWaveNodes ();
  example.SendWsmpExample ();
  example.CreateWaveNodes ();
  example.SendIpExample ();
  return 0;
}
