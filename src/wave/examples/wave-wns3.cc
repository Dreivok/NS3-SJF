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
#include "ns3/object-map.h"
#include "ns3/regular-wifi-mac.h"
#include "ns3/constant-velocity-mobility-model.h"
#include "ns3/wave-net-device.h"
#include "ns3/wave-mac-helper.h"
#include "ns3/wave-helper.h"

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

/**
 * This simulation is mainly used for wns3 to evaluate the performance of IEEE 1609.4
 * with current implementation.
 *
 * This simulation contains many parameters we can configure such as the number of nodes.
 * In our paper,
 * First we study the impact of the standard on safety packets.
 * We use small packet size (default 200) and broadcast transmission mechanism to model safety packets.
 * we consider four cases of the standard: A-not use, B-normal usage, C-best usage, D-worst usage.
 * And we change the number of nodes to study the communication performance.
 * Second we study the impact of the standard on non-safety packets.
 * We use large packet size (default 1500) and unicast transmission mechanism to model non-safety packets.
 * we also consider four cases of the standard: A-not use, B-normal usage, C-best usage, D-worst usage.
 * And we also change the number of nodes to study the communication performance. *
 */
class MultipleChannelsExperiment
{
public:
  MultipleChannelsExperiment (void);

  bool Configure (int argc, char **argv);
  void Usage (void);
  void Run (void);

private:

  void CreateNodes (void);
  void CreateWaveDevice (void);
  void SetupMobility (void);

  void InstallApplicationA (void);
  void InstallApplicationB (void);
  void InstallApplicationC (void);
  void InstallApplicationD (void);

  void Send (Ptr<WaveNetDevice> sender, uint32_t channelNumber);
  bool Receive (Ptr<NetDevice> dev, Ptr<const Packet> pkt, uint16_t mode, const Address &sender);

  void InitStats (void);
  void Stats (uint32_t randomNumber);
  void StatQueuedPackets (void);
  uint32_t GetQueuedSize (Ptr<WaveEdcaTxopN> edca);

  NodeContainer nodes;
  NetDeviceContainer devices;

  uint32_t nodesNum;          // current nodes in 4-lines and 1km
  uint32_t freq;              // the frequency for sending packets
  uint32_t simulationTime;    // simulation time
  uint32_t size;              // the size of packet
  bool broadcast;

  Ptr<UniformRandomVariable> rng;

  // used for identifying received packet
  uint32_t sequence;

  // every received packets will be used for stat the duplicate packet
  // and duplicate packet case will only happen in unicast
  std::vector <uint32_t> receivedPackets;
  // we will check whether the packet is received by the neighbors
  // in the transmission range
  std::map<uint32_t, std::vector<uint32_t> *> broadcastPackets;

  class SendStat
  {
  public:
    uint32_t inCchi;
    uint32_t inCgi;
    uint32_t inSchi;
    uint32_t inSgi;
    SendStat (): inCchi (0), inCgi (0), inSchi (0), inSgi (0){}
  };

  SendStat sends;
  uint32_t receives;
  uint32_t queues;
  uint64_t delaySum;        // us
  uint64_t receiveSum;

  uint32_t run;
  double pdr,delay,system_throughput,average_throughput;
};

MultipleChannelsExperiment::MultipleChannelsExperiment (void)
  : nodesNum (20),           // 20 vehicles in 1km * 4-line
    freq (10),               // 10Hz, 100ms send one non-safety packet
    simulationTime (10),     // make it run 100s
    size (1500),             // packet size
    broadcast (false),
    sequence (0),
    receives (0),
    queues (0),
    delaySum (0),
    receiveSum (0),
    run (1),
    pdr (0),
    delay (0),
    system_throughput (0),
    average_throughput (0)
{
}

bool
MultipleChannelsExperiment::Configure (int argc, char **argv)
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
MultipleChannelsExperiment::Usage (void)
{
  std::cout << "usage:"
		    << "./waf --run=\"wave-multiple-channel --nodes=20 --time=100 --size=200 "
		    		"--frequency=10 --broadcast=false \""
            << std::endl;
}

void
MultipleChannelsExperiment::CreateNodes ()
{
  NS_LOG_FUNCTION (this);
  NS_ASSERT (nodesNum != 0);
  nodes = NodeContainer ();
  nodes.Create (nodesNum);
  NS_LOG_DEBUG ("create " << nodesNum << " nodes");
}

void
MultipleChannelsExperiment::CreateWaveDevice (void)
{
  NS_LOG_FUNCTION (this);
  // the transmission range is about 250m refer to "wave-transmission-range.cc"
  YansWifiChannelHelper wifiChannel;
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss ("ns3::TwoRayGroundPropagationLossModel",
 			  	  	  	  	  	  "Frequency", DoubleValue (5.89e9),
 			  	  	  	  	      "HeightAboveZ", DoubleValue (0.5));
  YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();
  wifiPhy.Set("TxPowerStart",  DoubleValue (16));
  wifiPhy.Set("TxPowerEnd",  DoubleValue (16));
  wifiPhy.SetChannel (wifiChannel.Create ());
  WaveMacHelper waveMac = WaveMacHelper::Default ();
  WaveHelper waveHelper = WaveHelper::Default ();
  devices = waveHelper.Install (wifiPhy, waveMac, nodes);

  // Enable WAVE logs.
  // WaveHelper::LogComponentEnable();
  // or waveHelper.LogComponentEnable();

  for (uint32_t i = 0; i != devices.GetN (); ++i)
    {
      devices.Get (i)->SetReceiveCallback (MakeCallback (&MultipleChannelsExperiment::Receive,this));
    }
}

void
MultipleChannelsExperiment::SetupMobility ()
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
MultipleChannelsExperiment::Receive (Ptr<NetDevice> dev, Ptr<const Packet> pkt, uint16_t mode, const Address &sender)
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

  // for average delay (including broadcast and unicast packets)
  if (find(receivedPackets.begin(), receivedPackets.end(), packetId) == receivedPackets.end())
    {
	  delaySum += (now - sendTime).GetMicroSeconds ();
	  receiveSum++;
      receivedPackets.push_back(packetId);
    }

  // for pdr of unicast packets
  if (!broadcast)
    {
      receives++;
      return true;
    }

  // for pdr of broadcast packets
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
  // decease the current neighbors.
  (*neighbors)[0]--;
  // if becomes 0, it indicates all neighbors receive packets successfully
  // so this packet will be taken as successful receive.
  if ((*neighbors)[0] == 0)
  {
	 receives++;
	 (*neighbors).clear();
	 delete neighbors;
	 broadcastPackets.erase(i);
  }

  return true;
}

void
MultipleChannelsExperiment::Send (Ptr<WaveNetDevice> sender, uint32_t channelNumber)
{
  NS_LOG_FUNCTION (this << sender << channelNumber);
  Time now = Now ();
  Ptr<Packet> packet = Create<Packet> (size);
  StatsTag tag = StatsTag (++sequence, now);
  packet->AddByteTag (tag);
   Address dest;
  TxInfo info = TxInfo (channelNumber);
  Ptr<Node> src = sender->GetNode();
  Ptr<MobilityModel> model_src = src->GetObject<MobilityModel> ();
  Vector pos_src = model_src->GetPosition ();

  if (broadcast)
  {
	  dest = Mac48Address::GetBroadcast ();
	  NodeContainer::Iterator i ;
	  std::vector<uint32_t> * neighbors = new std::vector<uint32_t>;
	  // the first elem is used to set the neighbors size
	  // when it is received by one neighbor, it will decrease.
	  // if it becomes 0 finally, it indicates the packet is broadcasted successfully.
	  // otherwise it indicates that the packet is only received by some neighbors unsuccessfully
	  (*neighbors).push_back (0);
	  for (i = nodes.Begin (); i != nodes.End (); ++i)
	    {
		  if ((*i)->GetId() == src->GetId())
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
	  NodeContainer::Iterator i;
	  for (i = nodes.Begin (); i != nodes.End (); ++i)
	    {
		  if ((*i)->GetId() == src->GetId())
			  continue;
		  Ptr<Node> d = (*i);
		  Ptr<MobilityModel> model_d = d->GetObject<MobilityModel> ();
		  Vector pos_d = model_d->GetPosition ();

		  // here we choose a destination node not too far
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
  result = sender->SendX (packet, dest, Packet_Number, info);
     if (result)
       NS_LOG_DEBUG ("Time = " << now.GetMicroSeconds () << "us, transmit packet: ID = " << tag.GetPacketId ());
     else
       NS_LOG_DEBUG ( "transmit packet fail");

  Ptr<ChannelCoordinator> coordinator = sender->GetChannelCoordinator ();
  if (coordinator->IsCchInterval ())
    {
      if (coordinator->IsGuardInterval ())
        {
          sends.inCgi++;
        }
      else
        {
          sends.inCchi++;
        }
    }
  else
    {
      if (coordinator->IsGuardInterval ())
        {
          sends.inSgi++;
        }
      else
        {
          sends.inSchi++;
        }
    }
}

uint32_t
MultipleChannelsExperiment::GetQueuedSize (Ptr<WaveEdcaTxopN> edca)
{
  uint32_t queueSize = 0,  duplicate = 0;
  ObjectMapValue map;

  edca->GetAttribute ("Queues", map);
  for (ObjectPtrContainerValue::Iterator i = map.Begin(); i != map.End(); ++i)
    {
	  Ptr<WifiMacQueue> queue = DynamicCast<WifiMacQueue> (i->second);
	  queueSize += queue->GetSize();

	  if (broadcast)
		  continue;
	  // the duplicate packet problem can happen in unicast case
	  // here exist an interesting point that we may receive a duplicate packet, one
	  // is in received side of receiver, the second is in the queue side of sender.
	  // the reason is that (i) the sender unicasts a packet to the receiver, then the
	  // received received this packets successfully, but the ACK is lost,
	  // (ii) currently the sender need to switch to another channel because of alternating access,
	  // resulting the retransmitted packet queued again.
	  // (iii) suddenly the simulation is stopped, so this packet will become two, one in sender
	  // and one in receiver.
	  Ptr<const Packet> pkt;
	  WifiMacHeader hd;
	  while ((pkt = queue->Dequeue (&hd)))
	    {
		  StatsTag tag;
		  bool result;
		  result = pkt->FindFirstMatchingByteTag (tag);
		  if (!result)
		    {
			  NS_FATAL_ERROR ("the packet here shall have a stats tag");
			}
		  uint32_t packetId = tag.GetPacketId ();
		  if (find(receivedPackets.begin(), receivedPackets.end(), packetId) != receivedPackets.end())
		    {
			  duplicate++;
		    }
		 }
	  }
  return (queueSize - duplicate);
}

// when simulation is stopped, we need to stats the queued packets
// so the real transmitted packets will be (sends - queues).
void
MultipleChannelsExperiment::StatQueuedPackets ()
{
  NS_LOG_FUNCTION (this);
  NetDeviceContainer::Iterator i;
  for (i = devices.Begin (); i != devices.End (); ++i)
    {
	  Ptr<WaveNetDevice> device = DynamicCast<WaveNetDevice>(*i);
	  Ptr<RegularWifiMac> rmac = DynamicCast<RegularWifiMac>(device->GetMac ());

	  PointerValue ptr;

	  // for WAVE devices, the DcaTxop will not be used.
	  // rmac->GetAttribute ("DcaTxop", ptr);
	  // Ptr<DcaTxop> dcaTxop = ptr.Get<DcaTxop> ();

	  rmac->GetAttribute ("VO_EdcaTxopN", ptr);
	  Ptr<EdcaTxopN> vo_edcaTxopN = ptr.Get<EdcaTxopN> ();
	  Ptr<WaveEdcaTxopN> wave_vo = DynamicCast<WaveEdcaTxopN>(vo_edcaTxopN);
	  queues += GetQueuedSize (wave_vo);

	  rmac->GetAttribute ("VI_EdcaTxopN", ptr);
	  Ptr<EdcaTxopN> vi_edcaTxopN = ptr.Get<EdcaTxopN> ();
	  Ptr<WaveEdcaTxopN> wave_vi = DynamicCast<WaveEdcaTxopN>(vi_edcaTxopN);
	  queues += GetQueuedSize (wave_vi);

	  rmac->GetAttribute ("BE_EdcaTxopN", ptr);
	  Ptr<EdcaTxopN> be_edcaTxopN = ptr.Get<EdcaTxopN> ();
	  Ptr<WaveEdcaTxopN> wave_be = DynamicCast<WaveEdcaTxopN>(be_edcaTxopN);
	  queues += GetQueuedSize (wave_be);

	  rmac->GetAttribute ("BK_EdcaTxopN", ptr);
	  Ptr<EdcaTxopN> bk_edcaTxopN = ptr.Get<EdcaTxopN> ();
	  Ptr<WaveEdcaTxopN> wave_bk = DynamicCast<WaveEdcaTxopN>(bk_edcaTxopN);
	  queues += GetQueuedSize (wave_bk);
  }
}

void
MultipleChannelsExperiment::InstallApplicationA (void)
{
  NS_LOG_FUNCTION (this);
  NetDeviceContainer::Iterator i;
  for (i = devices.Begin (); i != devices.End (); ++i)
    {
      Ptr<WaveNetDevice> sender = DynamicCast<WaveNetDevice> (*i);

      // to get continuous access for CCH, we do not need to call assignment method

      for (uint32_t time = 0; time != simulationTime; ++time)
        {
          for (uint32_t sends = 0; sends != freq; ++sends)
            {
              Simulator::Schedule (Seconds (rng->GetValue (time, time + 1)), &MultipleChannelsExperiment::Send, this, sender, CCH);
            }
        }

  // or we can call assignment method to get continuous access for other SCH,
  // then send packet in the SCH
  //      SchInfo schInfo = SchInfo (SCH1, false, 0xff);
  //      Simulator::Schedule (Seconds (0.0), &WaveNetDevice::StartSch, sender, schInfo);
  //      Simulator::Schedule (Seconds (rng->GetValue (time, time + 1)), &MultipleChannelsExperiment::Send, this, sender, SCH1);
    }
}

void
MultipleChannelsExperiment::InstallApplicationB (void)
{
  NS_LOG_FUNCTION (this);
  NetDeviceContainer::Iterator i;
  for (i = devices.Begin (); i != devices.End (); ++i)
    {
      Ptr<WaveNetDevice> sender = DynamicCast<WaveNetDevice> (*i);

      // to get alternating access, we need to call assignment method
      SchInfo schInfo = SchInfo (SCH1, false, 0x0);
      Simulator::Schedule (Seconds (0.0), &WaveNetDevice::StartSch, sender, schInfo);

      for (uint32_t time = 0; time != simulationTime; ++time)
        {
          for (uint32_t sends = 0; sends != freq; ++sends)
            {
              Simulator::Schedule (Seconds (rng->GetValue (time, time + 1)), &MultipleChannelsExperiment::Send, this, sender, CCH);
            }
        }
    }
}

void
MultipleChannelsExperiment::InstallApplicationC (void)
{
  NS_LOG_FUNCTION (this);
  NetDeviceContainer::Iterator i;
  for (i = devices.Begin (); i != devices.End (); ++i)
    {
      Ptr<WaveNetDevice> sender = DynamicCast<WaveNetDevice> (*i);
      SchInfo schInfo = SchInfo (SCH1, false, 0x0);
      Simulator::Schedule (Seconds (0.0),&WaveNetDevice::StartSch,sender,schInfo);

      Ptr<ChannelCoordinator> coordinator = sender->GetChannelCoordinator ();
      for (uint32_t time = 0; time != simulationTime; ++time)
        {
          for (uint32_t sends = 0; sends != freq; ++sends)
            {
              Time system_throughput = Seconds (rng->GetValue (time, time + 1));
              // if the send time is not in CCHI, we will add a duration, then the send time will be in CCHI.
              // note that: this simple mechanism can be used when CCHI is equal to SCHI. If SCHI is 60ms and CCHI is 40ms,
              // this mechanism could be broken. So we need a better approach to enable packets sent in CCHI.
              if (!coordinator->IsCchInterval (system_throughput))
                {
                  system_throughput += coordinator->GetCchInterval ();
                }
              Simulator::Schedule (system_throughput, &MultipleChannelsExperiment::Send, this, sender, CCH);
            }
        }
    }
}

void
MultipleChannelsExperiment::InstallApplicationD (void)
{
  NS_LOG_FUNCTION (this);
  NetDeviceContainer::Iterator i;
  for (i = devices.Begin (); i != devices.End (); ++i)
    {
      Ptr<WaveNetDevice> sender = DynamicCast<WaveNetDevice> (*i);
      SchInfo schInfo = SchInfo (SCH1, false, 0x0);
      Simulator::Schedule (Seconds (0.0), &WaveNetDevice::StartSch, sender, schInfo);

      Ptr<ChannelCoordinator> coordinator = sender->GetChannelCoordinator ();
      for (uint32_t time = 0; time != simulationTime; ++time)
        {
          for (uint32_t sends = 0; sends != freq; ++sends)
            {
              Time system_throughput = Seconds (rng->GetValue (time, time + 1));
              if (!coordinator->IsSchInterval (system_throughput))
                {
                  system_throughput += coordinator->GetSchInterval ();
                }
              Simulator::Schedule (system_throughput, &MultipleChannelsExperiment::Send, this, sender, CCH);
            }
        }
    }
}

void
MultipleChannelsExperiment::Run (void)
{
  NS_LOG_FUNCTION (this);
  NS_LOG_DEBUG ("simulation configuration arguments: ");
  NS_LOG_UNCOND ("      PDR  AverageDelay SystemThroughput  AverageThroughput");

  {
    NS_LOG_DEBUG ("configuration A:");
    {
    pdr = delay = system_throughput = average_throughput = 0.0;
    for (uint32_t r = 0; r != run; r++)
    {
    	RngSeedManager::SetSeed (1);
    	RngSeedManager::SetRun (17+r);
    	InitStats ();
    	CreateNodes ();
    	SetupMobility ();
    	CreateWaveDevice ();
    	InstallApplicationA ();
    	Simulator::Stop (Seconds (simulationTime));
    	Simulator::Run ();
    	Simulator::Destroy ();
    	Stats (17+r);
    }
    NS_LOG_UNCOND ("A :  " << pdr/run<< " " << delay/run << " " << system_throughput/run << " " << average_throughput/run);
    }
  }
  {
    NS_LOG_DEBUG ("configuration B:");
    {
    pdr = delay = system_throughput = average_throughput = 0.0;
    for (uint32_t r = 0; r != run; r++)
    {
    	RngSeedManager::SetSeed (1);
    	RngSeedManager::SetRun (17+r);
    	InitStats ();
    	CreateNodes ();
    	SetupMobility ();
    	CreateWaveDevice ();
    	InstallApplicationB ();
    	Simulator::Stop (Seconds (simulationTime));
    	Simulator::Run ();
    	Simulator::Destroy ();
    	Stats (17+r);
    }
    NS_LOG_UNCOND ("B :  " << pdr/run << " " << delay/run << " " << system_throughput/run << " " << average_throughput/run);
    }
  }

  {
    NS_LOG_DEBUG ("configuration C:");
    {
    pdr = delay = system_throughput = average_throughput = 0.0;
    for (uint32_t r = 0; r != run; r++)
    {
    	RngSeedManager::SetSeed (1);
    	RngSeedManager::SetRun (17+r);
    	InitStats ();
    	CreateNodes ();
    	SetupMobility ();
    	CreateWaveDevice ();
    	InstallApplicationC ();
    	Simulator::Stop (Seconds (simulationTime));
    	Simulator::Run ();
    	Simulator::Destroy ();
    	Stats (17+r);
    }
    NS_LOG_UNCOND("C :  " << pdr/run << " " << delay/run << " " << system_throughput/run << " " << average_throughput/run);
    }
  }
  {
    NS_LOG_DEBUG ("configuration delay:");
    {
    pdr = delay = system_throughput = average_throughput = 0.0;
    for (uint32_t r = 0; r != run; r++)
    {
    	RngSeedManager::SetSeed (1);
    	RngSeedManager::SetRun (17+r);
    	InitStats ();
    	CreateNodes ();
    	SetupMobility ();
    	CreateWaveDevice ();
    	InstallApplicationD ();
    	Simulator::Stop (Seconds (simulationTime));
    	Simulator::Run ();
    	Simulator::Destroy ();
    	Stats (17+r);
    }
    NS_LOG_UNCOND ("D :  " << pdr/run << " " << delay/run << " " << system_throughput/run << " " << average_throughput/run);
    }
  }
}

void
MultipleChannelsExperiment::InitStats (void)
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
  receivedPackets.clear();
  sends = SendStat ();
  sequence = 0;
  receives = 0;
  queues = 0;
  delaySum = 0;
  receiveSum = 0;

  Simulator::ScheduleDestroy (&MultipleChannelsExperiment::StatQueuedPackets, this);
}

void
MultipleChannelsExperiment::Stats (uint32_t randomNumber)
{
  NS_LOG_FUNCTION (this);
  NS_ASSERT (receives != 0);
  // first show stats records.
  NS_LOG_DEBUG (" simulation result:(run number = " << randomNumber << ")");
  NS_LOG_DEBUG (" nodes = " << nodesNum);
  NS_LOG_DEBUG (" simulation time = " << simulationTime << "s");
  NS_LOG_DEBUG (" send packets = " << sequence);
  NS_LOG_DEBUG (" receives packets = " << receives);
  NS_LOG_DEBUG (" queues packets = " << queues);
  NS_LOG_DEBUG (" lost packets = " << (sequence - queues - receives));
  NS_LOG_DEBUG (" the sum delay of all received packets = " << delaySum << "micros");

  if (broadcast)
	NS_LOG_UNCOND (" the partly received broadcast packets = " << broadcastPackets.size ());

  // second show performance result
  // stats PDR (packet delivery ratio)
  double PDR = receives / (double)(sequence - queues);
  // stats average delay
  double AverageDelay = delaySum / receiveSum / 1000.0;
  // stats system throughput, Mbps
  double Throughput = receives * size * 8 / simulationTime / 1000.0 / 1000.0;
  // stats average throughput, kbps
  double AverageThroughput = Throughput / (double)nodesNum * 1000;

  NS_LOG_DEBUG (" PDR = " << PDR);
  NS_LOG_DEBUG (" AverageDelay = " << AverageDelay << "ms");
  NS_LOG_DEBUG (" Throughput = " << Throughput << "Mbps");
  NS_LOG_DEBUG (" AverageThroughput = " << AverageThroughput << "Kbps");

  pdr+=PDR;
  delay+=AverageDelay;
  system_throughput+=Throughput;
  average_throughput+=AverageThroughput;
}

int
main (int argc, char *argv[])
{
//  LogComponentEnable ("WaveMultipleChannel", LOG_DEBUG);

  MultipleChannelsExperiment experiment;
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
