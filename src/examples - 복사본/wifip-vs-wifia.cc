/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2014 Dalian University of Technology
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR average_throughput PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received average_throughput copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Junling Bu <linlinjavaer@gmail.com>
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
#include <algorithm>
#include "ns3/string.h"
#include "ns3/double.h"
#include "ns3/qos-tag.h"
#include "ns3/wifi-mode.h"
#include "ns3/object-map.h"

#include "ns3/wave-net-device.h"
#include "ns3/wave-mac-helper.h"
#include "ns3/wifi-80211p-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WaveMultipleChannel");

class StatsTag : public Tag
{
public:
  StatsTag (void)
    : m_packetId (0),
      m_sendTime (Seconds (0))
  {
  }
  StatsTag (uint32_t packetId, Time sendTime)
    : m_packetId (packetId),
      m_sendTime (sendTime)
  {
  }
  virtual ~StatsTag (void)
  {
  }

  uint32_t GetPacketId (void)
  {
    return m_packetId;
  }
  Time GetSendTime (void)
  {
    return m_sendTime;
  }

  static TypeId GetTypeId (void);
  virtual TypeId GetInstanceTypeId (void) const;
  virtual uint32_t GetSerializedSize (void) const;
  virtual void Serialize (TagBuffer i) const;
  virtual void Deserialize (TagBuffer i);
  virtual void Print (std::ostream &os) const;

private:
  uint32_t m_packetId;
  Time m_sendTime;
};
TypeId
StatsTag::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::StatsTag")
    .SetParent<Tag> ()
    .AddConstructor<StatsTag> ()
  ;
  return tid;
}
TypeId
StatsTag::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}
uint32_t
StatsTag::GetSerializedSize (void) const
{
  return sizeof (uint32_t) + sizeof (uint64_t);
}
void
StatsTag::Serialize (TagBuffer i) const
{
  i.WriteU32 (m_packetId);
  i.WriteU64 (m_sendTime.GetMicroSeconds ());
}
void
StatsTag::Deserialize (TagBuffer i)
{
  m_packetId = i.ReadU32 ();
  m_sendTime = MicroSeconds (i.ReadU64 ());
}
void
StatsTag::Print (std::ostream &os) const
{
  os << "packet=" << m_packetId << " sendTime=" << m_sendTime;
}

// it is useless here we can use any value we like
// because we only allow to send and receive one type packets
#define Packet_Number 0x8888

// the transmission range of a device should be decided carefully,
// since it will affect the packet delivery ratio
// we suggest users use wave-transmission-range.cc to get this value
#define Device_Transmission_Range 250

class WifipVsWifia
{
public:
  WifipVsWifia (void);

  bool Configure (int argc, char **argv);
  void Usage (void);
  void Run (void);

private:
  void CreateNodes (void);

  void SetupMobility (void);

  void InstallApplication (uint32_t qos);

  void CreateWifi80211pDevices (void);

  void CreateWifi80211aDevices (void);

  void Send (Ptr<WifiNetDevice> sender, uint32_t qos);
  bool Receive (Ptr<NetDevice> dev, Ptr<const Packet> pkt, uint16_t mode, const Address &sender);

  void InitStats (void);
  void Stats (uint32_t randomNumber);
  void StatQueuedPackets (void);

  NodeContainer nodes;
  NetDeviceContainer devices;

  uint32_t nodesNum;          // current nodes in 4-lines and 1km
  uint32_t freq;              // the frequency for sending packets
  uint32_t simulationTime;    // simulation time
  uint32_t size;              // the size of packet
  bool broadcast;

  Ptr<UniformRandomVariable> rng;

  uint32_t sequence;         // used for identifying received packet


  // we will check whether the packet is received by the neighbors
   // in the transmission range
   std::map<uint32_t, std::vector<uint32_t> *> broadcastPackets;

  uint32_t receives;
  uint32_t queues;
  uint64_t delaySum;        // us

  uint32_t run;
  double pdr,delay,system_throughput,average_throughput;
};

WifipVsWifia::WifipVsWifia (void)
  : nodesNum (20),           // 20 vehicles in 1km * 4-line
    freq (10),               // 10Hz, 100ms send one non-safety packet
    simulationTime (20),     // make it run 100s
    size (300),             // packet size
    broadcast (true),
    sequence (0),
    receives (0),
    queues (0),
    delaySum (0),
    run (1),
    pdr (0),
    delay (0),
    system_throughput (0),
    average_throughput (0)
{
}

bool
WifipVsWifia::Configure (int argc, char **argv)
{
  CommandLine cmd;
  cmd.AddValue ("nodes", "Number of nodes.", nodesNum);
  cmd.AddValue ("time", "Simulation time, s.", simulationTime);
  cmd.AddValue ("size", "Size of safety packet, bytes.", size);
  cmd.AddValue ("frequency", "Frequency of sending safety packets, Hz.", freq);
  cmd.AddValue ("broadcast", "transmission mechanism:broadcast (true) or unicast (false), boolean).", broadcast);
  cmd.AddValue ("run", "to make the result more credible, make simulation run many times with different random number).", run);

  cmd.Parse (argc, argv);
  return true;
}

void
WifipVsWifia::Usage (void)
{
  std::cout << "usage:"
		    << "./waf --run=\"wifip-vs-wifia --nodes=20 --time=100 --size=200 "
		    		"--frequency=10 --broadcast=false \""
            << std::endl;
}

void
WifipVsWifia::CreateNodes (void)
{
  NS_LOG_FUNCTION (this);

  nodes = NodeContainer ();
  nodes.Create (nodesNum);
  NS_LOG_DEBUG ("create " << nodesNum << " nodes");

}

void
WifipVsWifia::SetupMobility ()
{
  // Create a simple highway model based on GridPositionAllocator and ConstantPositionMobilityModel
  MobilityHelper mobility;
  // every node is in 1km * 4line. It is 3 meter between neighboring lines.
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
		  	  	  	  	  	  	 "MinX", DoubleValue (0.0),
		  	  	  	  	  	  	 "MinY", DoubleValue (0.0),
		  	  	  	  	  	  	 "DeltaX", DoubleValue (1000.0 / (nodesNum / 4.0)),
		  	  	  	  	  	  	 "DeltaY", DoubleValue (3),
		  	  	  	  	  	  	 "GridWidth", UintegerValue (nodesNum / 4),
		  	  	  	  	  	  	 "LayoutType", StringValue ("RowFirst"));
  // here we use constant velocity mobility model for two reasons
  // (a) since the mobility model of ns3 is mainly useful and enough for MANET, we can not ensure
  // whether they are suitable for VANET. Some special mobility models are required for VANET simulation.
  // (b) the mobility characteristic can cause packets lost which we do not want here. Because we want
  // get the impact of the standard on communication performance.
//  mobility.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);
  for (NodeContainer::Iterator i = nodes.Begin (); i != nodes.End (); ++i)
    {
	  Ptr<Node> node = (*i);
//	  node->GetObject<ConstantVelocityMobilityModel> ()->SetVelocity (Vector (20, 0, 0));

	  Ptr<MobilityModel> model = node->GetObject<MobilityModel> ();
	  Vector pos = model->GetPosition ();
	  NS_LOG_DEBUG ( "node :" << node->GetId() << " position: " << pos);
    }
}

bool
WifipVsWifia::Receive (Ptr<NetDevice> dev, Ptr<const Packet> pkt, uint16_t mode, const Address &sender)
{
  NS_LOG_FUNCTION (this << dev << pkt << mode << sender);
  NS_ASSERT (mode == Packet_Number);

  StatsTag tag;
  bool result;
  result = pkt->FindFirstMatchingByteTag (tag);
  if (!result)
    {
      NS_FATAL_ERROR ("the packet here shall have a stats tag");
    }
  Time now = Now ();
  Time sendTime = tag.GetSendTime ();
  uint32_t packetId = tag.GetPacketId ();

  if (!broadcast)
    {
      receives++;
      delaySum += (now - sendTime).GetMicroSeconds ();
      return true;
    }

  // get current node id
  Ptr<Node> node = dev->GetNode();
  uint32_t nodeId = node->GetId();

  std::map<uint32_t, std::vector<uint32_t> *>::iterator i;
  i = broadcastPackets.find(packetId);
  if (i == broadcastPackets.end())
  {
	  // why this packet is received but cannot be stat
	  // two reason:
	  // (i) the time the source node has no neighbors indeed, the received time
	  //     the source node has neighbors again because of mobility characteristic
	  // (ii) the transmission range we used is the range where the receivers can
	  //      receive a packet with 95% probability. So there will be a case that
	  //      in given transmission range source node has neighbors, but this packet
	  //      can still be received by nodes out of the transmission range.
	  // However, here we will take this packet not received.
	  NS_LOG_DEBUG ("the node [" << nodeId << " out of source node's transmission range"
	 			    " receive the packet [" << packetId << "].");
		 return true;
  }
  std::vector<uint32_t> * neighbors = i->second;
  std::vector<uint32_t>::iterator j;
  j = std::find((*neighbors).begin()+1, (*neighbors).end(), nodeId);
  if (j == (*neighbors).end())
  {
	  NS_LOG_DEBUG ("the node [" << nodeId << " out of source node's transmission range"
	 	 			    " receive the packet [" << packetId << "].");
		 return true;
  }

  NS_ASSERT ((*neighbors).size() > 1 && (*neighbors)[0] != 0);
  // since the packet is received by one neighbor successfully
  // decease the current size.
  (*neighbors)[0]--;
  // if becomes 0, it indicates all neighbors receive packets successfully
  if ((*neighbors)[0] == 0)
  {
	 receives++;
	 delaySum += (now - sendTime).GetMicroSeconds ();
	 (*neighbors).clear();
	 delete neighbors;
	 broadcastPackets.erase(i);
	 return true;
  }

  return true;
}

void
WifipVsWifia::Send (Ptr<WifiNetDevice> sender, uint32_t qos)
{
  NS_LOG_FUNCTION (this << sender << qos);
  Time now = Now ();
  Ptr<Packet> packet = Create<Packet> (size);
  StatsTag tag = StatsTag (++sequence, now);
  packet->AddByteTag (tag);

  QosTag qosTag = QosTag (qos);
  packet->AddPacketTag(qosTag);
  Address dest;
  Ptr<Node> src = sender->GetNode();
  Ptr<MobilityModel> model_src = src->GetObject<MobilityModel> ();
  Vector pos_src = model_src->GetPosition ();

  if (broadcast)
  {
	  dest = Mac48Address::GetBroadcast ();
	  NodeContainer::Iterator i = nodes.Begin ();
	  std::vector<uint32_t> * neighbors = new std::vector<uint32_t>;
	  // the first elem is used to set the neighbors size
	  // when it is received by one neighbor, it will decrease.
	  // if it becomes 0 finally, it indicates the packet is broadcasted successfully.
	  // otherwise it indicates that the packet is only received by some neighbors unsuccessfully
	  (*neighbors).push_back (0);
	  for (; i != nodes.End (); ++i)
	    {
		  if ((*i) == src)
			  continue;
		  Ptr<Node> d = (*i);
		  Ptr<MobilityModel> model_d = d->GetObject<MobilityModel> ();
		  Vector pos_d = model_d->GetPosition ();

		  if (CalculateDistance (pos_d, pos_src) < Device_Transmission_Range )
		    {
			  (*neighbors).push_back (d->GetId());
			  continue;
		    }
	    }
	  if (neighbors->size() == 1)
	    {
		  NS_LOG_WARN ("it may be strange that this node[" << src->GetId()
				       <<"] has no neighbors when it is sending a packet[" <<
				       tag.GetPacketId() << "].");
	    }
	  (*neighbors)[0] =  (*neighbors).size() - 1;
	  broadcastPackets.insert(std::make_pair(tag.GetPacketId(), neighbors));
  }
  else
  {
	  // if unitcast packet, we will select a destination in transmission range
	  NodeContainer::Iterator i = nodes.Begin ();
	  for (; i != nodes.End (); ++i)
	    {
		  if ((*i) == src)
			  continue;
		  Ptr<Node> d = (*i);
		  Ptr<MobilityModel> model_d = d->GetObject<MobilityModel> ();
		  Vector pos_d = model_d->GetPosition ();

		  if (CalculateDistance (pos_d, pos_src) < Device_Transmission_Range/2 )
		    {
			  dest = d->GetDevice(0)->GetAddress();
			  break;
		    }
	    }
	  // we are sure that there is at least one node is in transmission range (it is on the up lane or below lane).
	  NS_ASSERT (i != nodes.End());
  }

  bool result = false;
  result = sender->Send (packet, dest, Packet_Number);
     if (result)
       NS_LOG_DEBUG ("Time = " << now.GetMicroSeconds () << "us, transmit packet: ID = " << tag.GetPacketId ());
     else
       NS_LOG_DEBUG ( "transmit packet fail");
}

// when simulation is stopped, we need to stats the queued packets
// so the real transmitted packets will be (sends - queues).
void
WifipVsWifia::StatQueuedPackets ()
{
  NS_LOG_FUNCTION (this);
  NetDeviceContainer::Iterator i;
  for (i = devices.Begin (); i != devices.End (); ++i)
    {
	  Ptr<WifiNetDevice> device = DynamicCast<WifiNetDevice>(*i);
	  Ptr<RegularWifiMac> rmac = DynamicCast<RegularWifiMac>(device->GetMac ());

	  PointerValue ptr;

	  rmac->GetAttribute ("DcaTxop", ptr);
	  Ptr<DcaTxop> dcaTxop = ptr.Get<DcaTxop> ();
	  queues += dcaTxop->GetQueue()->GetSize();

	  rmac->GetAttribute ("VO_EdcaTxopN", ptr);
	  Ptr<EdcaTxopN> vo_edcaTxopN = ptr.Get<EdcaTxopN> ();
	  queues += vo_edcaTxopN->GetQueue()->GetSize();

	  rmac->GetAttribute ("VI_EdcaTxopN", ptr);
	  Ptr<EdcaTxopN> vi_edcaTxopN = ptr.Get<EdcaTxopN> ();
	  queues += vi_edcaTxopN->GetQueue()->GetSize();


	  rmac->GetAttribute ("BE_EdcaTxopN", ptr);
	  Ptr<EdcaTxopN> be_edcaTxopN = ptr.Get<EdcaTxopN> ();
	  queues += be_edcaTxopN->GetQueue()->GetSize();

	  rmac->GetAttribute ("BK_EdcaTxopN", ptr);
	  Ptr<EdcaTxopN> bk_edcaTxopN = ptr.Get<EdcaTxopN> ();
	  queues += bk_edcaTxopN->GetQueue()->GetSize();

  }
}

void
WifipVsWifia::WifipVsWifia::CreateWifi80211pDevices (void)
{
	  NS_LOG_FUNCTION (this);
	  // wifi 802.11p with constant 6Mbps (10MHz channel width)
	  YansWifiChannelHelper wifiChannel;
	  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
	  wifiChannel.AddPropagationLoss ("ns3::TwoRayGroundPropagationLossModel",
	 			  	  	  	  	  	  "Frequency", DoubleValue (5.89e9),
	 			  	  	  	  	      "HeightAboveZ", DoubleValue (0.5));
	  YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();
	  wifiPhy.Set("TxPowerStart",  DoubleValue (16));
	  wifiPhy.Set("TxPowerEnd",  DoubleValue (16));
	  wifiPhy.Set("ChannelNumber",  UintegerValue (CCH));
	  wifiPhy.SetChannel (wifiChannel.Create ());
	  QosWifipMacHelper wifi80211pMac = QosWifipMacHelper::Default ();
	  Wifi80211pHelper wifi80211p = Wifi80211pHelper::Default ();
	  devices = wifi80211p.Install (wifiPhy, wifi80211pMac, nodes);
	  for (uint32_t i = 0; i != devices.GetN (); ++i)
	    {
	      devices.Get (i)->SetReceiveCallback (MakeCallback (&WifipVsWifia::Receive,this));
	    }
}

void
WifipVsWifia::CreateWifi80211aDevices (void)
{
	  NS_LOG_FUNCTION (this);

	  // wifi 802.11a with constant 6Mbps (20MHz channel width)
	  YansWifiChannelHelper wifiChannel;
	  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
	  wifiChannel.AddPropagationLoss ("ns3::TwoRayGroundPropagationLossModel",
	 			  	  	  	  	      "HeightAboveZ", DoubleValue (0.5));
	  YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();
	  wifiPhy.Set("TxPowerStart",  DoubleValue (16));
	  wifiPhy.Set("TxPowerEnd",  DoubleValue (16));
	  wifiPhy.Set("ChannelNumber",  UintegerValue (CCH));
	  wifiPhy.SetChannel (wifiChannel.Create ());
	  QosWifiMacHelper wifiMac = QosWifiMacHelper::Default ();
	  wifiMac.SetType ("ns3::AdhocWifiMac");
	  WifiHelper wifi80211aHelper;
	  wifi80211aHelper.SetStandard (WIFI_PHY_STANDARD_80211a);
	  wifi80211aHelper.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
	                                  "DataMode", StringValue ("OfdmRate6Mbps"),
	                                  "ControlMode",StringValue ("OfdmRate6Mbps"),
	                                  "NonUnicastMode", WifiModeValue (WifiMode("OfdmRate6Mbps")));
	  devices = wifi80211aHelper.Install (wifiPhy, wifiMac, nodes);
	  for (uint32_t i = 0; i != devices.GetN (); ++i)
	    {
	      devices.Get (i)->SetReceiveCallback (MakeCallback (&WifipVsWifia::Receive,this));
	    }
}

void
WifipVsWifia::InstallApplication (uint32_t qos)
{
  NS_LOG_FUNCTION (this);
  NetDeviceContainer::Iterator i;
  for (i = devices.Begin (); i != devices.End (); ++i)
    {
      Ptr<WifiNetDevice> sender = DynamicCast<WifiNetDevice> (*i);

      for (uint32_t time = 0; time != simulationTime; ++time)
        {
          for (uint32_t sends = 0; sends != freq; ++sends)
            {
              Simulator::Schedule (Seconds (rng->GetValue (time, time + 1)), &WifipVsWifia::Send, this, sender, qos);
            }
        }
    }
}

void
WifipVsWifia::Run (void)
{
  NS_LOG_FUNCTION (this);
  NS_LOG_DEBUG ("simulation configuration arguments: ");

  {
    NS_LOG_UNCOND ("configuration wifi (or wave) 80211p:");
    NS_LOG_UNCOND ("      PDR  AverageDelay SystemThroughput  AverageThroughput");

    {
    pdr = delay = system_throughput = average_throughput = 0.0;
    for (uint32_t r = 0; r != run; r++)
    {
    	RngSeedManager::SetSeed (1);
    	RngSeedManager::SetRun (17+r);
    	InitStats ();
    	CreateNodes ();
      	SetupMobility ();
    	CreateWifi80211pDevices();
    	InstallApplication (6);  // AC_VO: 6, 7
    	Simulator::Stop (Seconds (simulationTime+10));
    	Simulator::Run ();
    	Simulator::Destroy ();
    	Stats (17+r);
    }
    NS_LOG_UNCOND ("AC_VO:  " << pdr/run<< " " << delay/run << " " << system_throughput/run << " " << average_throughput/run);
    }
    {
     pdr = delay = system_throughput = average_throughput = 0.0;
     for (uint32_t r = 0; r != run; r++)
     {
     	RngSeedManager::SetSeed (1);
     	RngSeedManager::SetRun (17+r);
    	InitStats ();
     	CreateNodes ();
      	SetupMobility ();
     	CreateWifi80211pDevices();
     	InstallApplication (4);  // AC_VI: 4, 5
     	Simulator::Stop (Seconds (simulationTime+10));
     	Simulator::Run ();
     	Simulator::Destroy ();
     	Stats (17+r);
     }
     NS_LOG_UNCOND ("AC_VI:  " << pdr/run<< " " << delay/run << " " << system_throughput/run << " " << average_throughput/run);
     }
    {
    pdr = delay = system_throughput = average_throughput = 0.0;
    for (uint32_t r = 0; r != run; r++)
    {
    	RngSeedManager::SetSeed (1);
    	RngSeedManager::SetRun (17+r);
    	InitStats ();
    	CreateNodes ();
      	SetupMobility ();
    	CreateWifi80211pDevices();
    	InstallApplication (0);  // AC_BE: 0, 3
    	Simulator::Stop (Seconds (simulationTime+10));
    	Simulator::Run ();
    	Simulator::Destroy ();
    	Stats (17+r);
    }
    NS_LOG_UNCOND ("AC_BE:  " << pdr/run<< " " << delay/run << " " << system_throughput/run << " " << average_throughput/run);
    }
    {
     pdr = delay = system_throughput = average_throughput = 0.0;
     for (uint32_t r = 0; r != run; r++)
     {
     	RngSeedManager::SetSeed (1);
     	RngSeedManager::SetRun (17+r);
    	InitStats ();
     	CreateNodes ();
      	SetupMobility ();
     	CreateWifi80211pDevices();
     	InstallApplication (1);  // AC_BK: 1, 2
     	Simulator::Stop (Seconds (simulationTime+10));
     	Simulator::Run ();
     	Simulator::Destroy ();
     	Stats (17+r);
     }
     NS_LOG_UNCOND ("AC_BK:  " << pdr/run<< " " << delay/run << " " << system_throughput/run << " " << average_throughput/run);
     }
  }

  {
  NS_LOG_UNCOND ("configuration wifi 80211a:");
  NS_LOG_UNCOND ("      PDR  AverageDelay SystemThroughput  AverageThroughput");
  {
   pdr = delay = system_throughput = average_throughput = 0.0;
   for (uint32_t r = 0; r != run; r++)
   {
   	RngSeedManager::SetSeed (1);
   	RngSeedManager::SetRun (17+r);
	InitStats ();
   	CreateNodes ();
  	SetupMobility ();
   	CreateWifi80211aDevices();
   	InstallApplication (6);  // AC_VO: 6, 7
   	Simulator::Stop (Seconds (simulationTime+10));
   	Simulator::Run ();
   	Simulator::Destroy ();
   	Stats (17+r);
   }
   NS_LOG_UNCOND ("AC_VO:  " << pdr/run<< " " << delay/run << " " << system_throughput/run << " " << average_throughput/run);
   }
  {
   pdr = delay = system_throughput = average_throughput = 0.0;
   for (uint32_t r = 0; r != run; r++)
   {
   	RngSeedManager::SetSeed (1);
   	RngSeedManager::SetRun (17+r);
	InitStats ();
   	CreateNodes ();
  	SetupMobility ();
   	CreateWifi80211aDevices();
   	InstallApplication (4);  // AC_VI: 4, 5
   	Simulator::Stop (Seconds (simulationTime+10));
   	Simulator::Run ();
   	Simulator::Destroy ();
   	Stats (17+r);
   }
   NS_LOG_UNCOND ("AC_VI:  " << pdr/run<< " " << delay/run << " " << system_throughput/run << " " << average_throughput/run);
   }
  {
  pdr = delay = system_throughput = average_throughput = 0.0;
  for (uint32_t r = 0; r != run; r++)
  {
  	RngSeedManager::SetSeed (1);
  	RngSeedManager::SetRun (17+r);
	InitStats ();
  	CreateNodes ();
  	SetupMobility ();
  	CreateWifi80211aDevices();
  	InstallApplication (0);  // AC_BE: 0, 3
  	Simulator::Stop (Seconds (simulationTime+10));
  	Simulator::Run ();
  	Simulator::Destroy ();
  	Stats (17+r);
  }
  NS_LOG_UNCOND ("AC_BE:  " << pdr/run<< " " << delay/run << " " << system_throughput/run << " " << average_throughput/run);
  }
  {
   pdr = delay = system_throughput = average_throughput = 0.0;
   for (uint32_t r = 0; r != run; r++)
   {
   	RngSeedManager::SetSeed (1);
   	RngSeedManager::SetRun (17+r);
	InitStats ();
   	CreateNodes ();
  	SetupMobility ();
   	CreateWifi80211aDevices();
   	InstallApplication (1);  // AC_BK: 1, 2
   	Simulator::Stop (Seconds (simulationTime+10));
   	Simulator::Run ();
   	Simulator::Destroy ();
   	Stats (17+r);
   }
   NS_LOG_UNCOND ("AC_BK:  " << pdr/run<< " " << delay/run << " " << system_throughput/run << " " << average_throughput/run);
   }

}
}

void
WifipVsWifia::InitStats (void)
{
  // used for sending packets randomly
  rng = CreateObject<UniformRandomVariable> ();
  rng->SetStream (1);

  std::map<uint32_t, std::vector<uint32_t> *>::iterator i;
  for (i = broadcastPackets.begin(); i != broadcastPackets.end();++i)
  {
	  i->second->clear();
	  delete i->second;
	  i->second = 0;
  }
  broadcastPackets.clear();
  sequence = 0;
  receives = 0;
  queues = 0;
  delaySum = 0;

  Simulator::ScheduleDestroy (&WifipVsWifia::StatQueuedPackets, this);
}

void
WifipVsWifia::Stats (uint32_t randomNumber)
{
  NS_LOG_FUNCTION (this);
  NS_ASSERT (receives != 0);
  NS_LOG_DEBUG (" simulation result:(run number = " << randomNumber << ")");
  NS_LOG_DEBUG (" send packets = " << sequence);
  NS_LOG_DEBUG (" receives packets = " << receives);

  // second show performance result
  NS_LOG_DEBUG (" performance result:(run number = " << randomNumber << ")");
  // stats PDR (packet delivery ratio)
  double PDR = receives / (double)sequence;

  // stats average delay
  double AverageDelay = delaySum / receives / 1000.0;

  // stats average throughput
  double Throughput = receives * size * 8 / simulationTime / 1000.0 / 1000.0;

  double AverageThroughput = Throughput / (nodesNum + 0.0);

  NS_LOG_DEBUG (PDR << " " << AverageDelay << " " << Throughput << " " << AverageThroughput);

  pdr+=PDR;
  delay+=AverageDelay;
  system_throughput+=Throughput;
  average_throughput+=AverageThroughput;
}

int
main (int argc, char *argv[])
{
//  LogComponentEnable ("WifipVsWifia", LOG_DEBUG);

  WifipVsWifia experiment;
  if (experiment.Configure (argc, argv))
    {
      experiment.Run ();
    }
  else
    {
      experiment.Usage ();
    }

  return 0;
}
