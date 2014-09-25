/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2005,2006,2007 INRIA
 * Copyright (c) 2013 Dalian University of Technology
 *
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
 * Author: Mathieu Lacage <mathieu.lacage@sophia.inria.fr>
 * Author: Junling Bu <linlinjavaer@gmail.com>
 *
 */
/**
 * This example shows basic construction of an 802.11p node.  Two nodes
 * are constructed with 802.11p devices, and by default, one node sends a single
 * packet to another node (the number of packets and interval between
 * them can be configured by command-line arguments).  The example shows
 * typical usage of the helper classes for this mode of WiFi (where "OCB" refers
 * to "Outside the Context of a BSS")."
 */
#include "ns3/node.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/log.h"
#include "ns3/node-container.h"
#include "ns3/net-device-container.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-helper.h"
#include "ns3/rng-seed-manager.h"
#include "ns3/command-line.h"
#include "ns3/mobility-model.h"
#include <iostream>
#include "ns3/string.h"
#include "ns3/double.h"
#include "ns3/object-map.h"
#include "ns3/regular-wifi-mac.h"
#include "ns3/constant-velocity-mobility-model.h"
#include "ns3/wave-net-device.h"
#include "ns3/wave-mac-helper.h"
#include "ns3/wave-helper.h"
#include "ns3/wifi-80211p-helper.h"
#include "ns3/wifi-helper.h"

NS_LOG_COMPONENT_DEFINE ("TransmissionRange");

using namespace ns3;
// how to evaluate the transmission range of wifi device (or wave device)
  // my idea is that
  // first change the position between sender and receiver, then transmit 1 packets, and record;
  // second change the random run number and run simulation again until 10000 times.
  // finally stats in which range that the packet is received with the 95% probability .
  // this range here will be treated as Transmission Range

enum Device
{
  WAVE,
  Wifi80211p,
  Wifi80211a,
};

class TransmissionRangeExperiment
{
public:
  void GetTransmissionRange (enum Device device);

private:
  void Run (void);

  void CreateNodes (void);
  void SetupMobility ();

  void CreateWaveDevice (void);
  void CreateWifipDevice (void);
  void CreateWifiaDevice (void);

  void Send ();
  bool Receive (Ptr<NetDevice> dev, Ptr<const Packet> pkt, uint16_t mode, const Address &sender);

  NodeContainer nodes;
  NetDeviceContainer devices;

  enum Device device;
  double range;
  uint32_t receives;
};

void
TransmissionRangeExperiment::CreateNodes ()
{
  NS_LOG_FUNCTION (this);
  nodes = NodeContainer ();
  nodes.Create (2);
}

void
TransmissionRangeExperiment::CreateWaveDevice (void)
{
  NS_LOG_FUNCTION (this);

  YansWifiChannelHelper wifiChannel;
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss ("ns3::TwoRayGroundPropagationLossModel",
 			  	  	  	  	  	  "Frequency", DoubleValue (5.89e9),
 			  	  	  	  	      "HeightAboveZ", DoubleValue (0.5));
//  wifiChannel.AddPropagationLoss ("ns3::NakagamiPropagationLossModel");
  YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();
  wifiPhy.Set("TxPowerStart",  DoubleValue (16));
  wifiPhy.Set("TxPowerEnd",  DoubleValue (16));
  wifiPhy.Set("ChannelNumber",  UintegerValue (CCH));
  wifiPhy.SetChannel (wifiChannel.Create ());
  WaveMacHelper waveMac = WaveMacHelper::Default ();
  WaveHelper waveHelper = WaveHelper::Default ();
  devices = waveHelper.Install (wifiPhy, waveMac, nodes);

  // Enable WAVE logs.
  // WaveHelper::LogComponentEnable();
  // or waveHelper.LogComponentEnable();

  for (uint32_t i = 0; i != devices.GetN (); ++i)
    {
      devices.Get (i)->SetReceiveCallback (MakeCallback (&TransmissionRangeExperiment::Receive,this));
    }
}

void
TransmissionRangeExperiment::CreateWifipDevice (void)
{
	  NS_LOG_FUNCTION (this);
	  // wifi 802.11p with constant 6Mbps (10MHz channel width)
	  YansWifiChannelHelper wifiChannel;
	  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
	  wifiChannel.AddPropagationLoss ("ns3::TwoRayGroundPropagationLossModel",
			  	  	  	  	  	  	  "Frequency", DoubleValue (5.89e9),
			  	  	  	  	          "HeightAboveZ", DoubleValue (0.5));
//	  wifiChannel.AddPropagationLoss ("ns3::NakagamiPropagationLossModel");
	  YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();
	  wifiPhy.Set("TxPowerStart",  DoubleValue (16));
	  wifiPhy.Set("TxPowerEnd",  DoubleValue (16));
	  wifiPhy.Set("ChannelNumber",  UintegerValue (CCH));
	  wifiPhy.SetChannel (wifiChannel.Create ());
	  NqosWifipMacHelper wifi80211pMac = NqosWifipMacHelper::Default ();
	  Wifi80211pHelper wifi80211p = Wifi80211pHelper::Default ();
	  devices = wifi80211p.Install (wifiPhy, wifi80211pMac, nodes);
	  for (uint32_t i = 0; i != devices.GetN (); ++i)
	    {
	      devices.Get (i)->SetReceiveCallback (MakeCallback (&TransmissionRangeExperiment::Receive,this));
	    }
}

void
TransmissionRangeExperiment::CreateWifiaDevice (void)
{
	  NS_LOG_FUNCTION (this);

	  // wifi 802.11a with constant 6Mbps (20MHz channel width)
	  YansWifiChannelHelper wifiChannel;
	  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
	  wifiChannel.AddPropagationLoss ("ns3::TwoRayGroundPropagationLossModel",
			  	  	  	  	  	  	  "Frequency", DoubleValue (5.825e9),
			  	  	  	  	          "HeightAboveZ", DoubleValue (0.5));
//	  wifiChannel.AddPropagationLoss ("ns3::NakagamiPropagationLossModel");
	  YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();
	  wifiPhy.Set("TxPowerStart",  DoubleValue (15));
	  wifiPhy.Set("TxPowerEnd",  DoubleValue (15));
	  wifiPhy.SetChannel (wifiChannel.Create ());
	  NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default ();
	  wifiMac.SetType ("ns3::AdhocWifiMac");
	  WifiHelper wifi80211aHelper = WifiHelper::Default ();
	  wifi80211aHelper.SetStandard (WIFI_PHY_STANDARD_80211a);
	  wifi80211aHelper.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
	                                  "DataMode", StringValue ("OfdmRate6Mbps"),
	                                  "ControlMode",StringValue ("OfdmRate6Mbps"),
	                                  "NonUnicastMode", WifiModeValue (WifiMode("OfdmRate6Mbps")));
	  devices = wifi80211aHelper.Install (wifiPhy, wifiMac, nodes);
	  for (uint32_t i = 0; i != devices.GetN (); ++i)
	    {
	      devices.Get (i)->SetReceiveCallback (MakeCallback (&TransmissionRangeExperiment::Receive,this));
	    }
}

void
TransmissionRangeExperiment::GetTransmissionRange (enum Device dev)
{
  bool stop = false;
  range = 100;
  device = dev;
  while (!stop)
  {
    range += 10.0;
	receives = 0;

	for (uint32_t r = 0; r != 1000; r++)
	    {
	    	RngSeedManager::SetSeed (1);
	    	RngSeedManager::SetRun (17+r);
	    	Run ();
	    }
	if (receives / 1000.0 < 0.95)
		{
//			std::cout<< range << " " << receives << std::endl;
		  stop = true;
		}
	else
		std::cout<< range << " " << receives << std::endl;

  }
  std::cout<< range << std::endl;
}

void
TransmissionRangeExperiment::Run ()
{
  CreateNodes ();
  SetupMobility ();
  if (device == WAVE)
    CreateWaveDevice ();
  else if (device == Wifi80211p)
	CreateWifipDevice ();
  else
	CreateWifiaDevice ();
  Simulator::Schedule (Seconds (1), &TransmissionRangeExperiment::Send, this);
  Simulator::Stop (Seconds (2));
  Simulator::Run ();
  Simulator::Destroy ();
}

#define PACKET_NUMBER 0x8888

void
TransmissionRangeExperiment::Send ()
{
  Address dest = Mac48Address::GetBroadcast ();
  Ptr<Packet> packet = Create<Packet> (200);

  if (device == WAVE)
  {
	  TxInfo info = TxInfo (CCH);
	  Ptr<WaveNetDevice> sender = DynamicCast<WaveNetDevice> (devices.Get(0));
	  sender->SendX (packet, dest, PACKET_NUMBER, info);
  }
  else
  {
	  Ptr<WifiNetDevice> sender = DynamicCast<WifiNetDevice> (devices.Get(0));
	  sender->Send (packet, dest, PACKET_NUMBER);
  }
}
bool
TransmissionRangeExperiment::Receive (Ptr<NetDevice> dev, Ptr<const Packet> pkt, uint16_t mode, const Address &sender)
{
  NS_LOG_FUNCTION (this << dev << pkt << mode << sender);
  receives++;
  return true;
}

void
TransmissionRangeExperiment::SetupMobility ()
{
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 0.0));
  positionAlloc->Add (Vector (range, 0.0, 0.0));
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);
}

int main (int argc, char *argv[])
{
  TransmissionRangeExperiment experiment = TransmissionRangeExperiment ();
  experiment.GetTransmissionRange(WAVE);
  experiment = TransmissionRangeExperiment ();
  experiment.GetTransmissionRange(Wifi80211p);
  experiment = TransmissionRangeExperiment ();
  experiment.GetTransmissionRange(Wifi80211a);

  return 0;
}
