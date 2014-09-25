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
#include "ns3/llc-snap-header.h"
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
		m_sendTime (Seconds (0)),
		m_serviceChannel (0),	// Allocated Channel Number
		m_serviceSize (0)		// Total Service Size
	{
	}
	StatsTag (uint32_t packetId, Time sendTime, uint32_t channel, uint32_t size)
		: m_packetId (packetId),
		m_sendTime (sendTime),
		m_serviceChannel (channel),
		m_serviceSize (size)
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
	uint32_t GetServiceChannel (void)
	{
		return m_serviceChannel;
	}
	uint32_t GetServiceSize (void)
	{
		return m_serviceSize;
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
	uint32_t m_serviceChannel;
	uint32_t m_serviceSize;
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
	return sizeof (uint32_t) + sizeof (uint64_t) + sizeof (uint32_t) + sizeof (uint32_t);
}
void
	StatsTag::Serialize (TagBuffer i) const
{
	i.WriteU32 (m_packetId);
	i.WriteU64 (m_sendTime.GetMicroSeconds ());
	i.WriteU32 (m_serviceChannel);
	i.WriteU32 (m_serviceSize);
}
void
	StatsTag::Deserialize (TagBuffer i)
{
	m_packetId = i.ReadU32 ();
	m_sendTime = MicroSeconds (i.ReadU64 ());
	m_serviceChannel = i.ReadU32 ();
	m_serviceSize = i.ReadU32 ();
}
void
	StatsTag::Print (std::ostream &os) const
{
	os << "packet=" << m_packetId << " sendTime=" << m_sendTime << " serviceChannel" << m_serviceChannel << " serviceSize" << m_serviceSize;
}

/**
Each Service channel store the IP packet Information to each Queue
Store the Serviced Channel Number, and IP Packet Size
and Calculate Service Remain Time
and Scheduling using SJF Scheduler
*/

typedef struct std::pair<Address, uint32_t> SentInfo;
typedef struct std::pair<uint32_t, SentInfo> Items;

typedef struct std::pair<uint32_t, Items> bItems;

class ChannelQueues
{
public:
	ChannelQueues ();
	~ChannelQueues () {}
	bool isEmpty(uint32_t ChannelNum);
	//Packet have Packet size. So Calculate Service time and allocate Service channel
	uint32_t Enqueue(Ptr<const Packet>, bool Scheduling, Address sender);
	Items peek(uint32_t ChannelNum);
	Items Dequeue(uint32_t ChannelNum);
	void Update(uint32_t ChannelNum, uint32_t receiveSize);
	void EnqueueRemains(uint32_t ChannelNum, uint32_t RemainSize, Address sender);
	void PushBackups();
	void DropService(uint32_t ChannelNum, Address sender);

private:
	void EnqueueSwitch(uint32_t Channel, Items item, uint32_t Count, uint32_t RemainTime, bool back);
	uint32_t CalcServiceTime(uint32_t ServiceSize) { return (ServiceSize / 6001) + 1;}
	uint32_t AllocServiceChannel(uint32_t RemainTime,  bool Scheduling);
	bool CheckNodes(uint32_t Channel, Address node);
	std::list<Items> Sch2List;
	std::list<Items> Sch3List;
	std::list<Items> Sch4List;
	std::list<Items> Sch5List;

	std::list<bItems> BackupList;

	uint32_t Counts[4];
	uint32_t Sums[4];
	uint32_t current;
};

ChannelQueues::ChannelQueues(void)
	: current(SCH2)
{
	for(int i = 0; i < 4; i++) {
		Counts[i] = 0;
		Sums[i] = 0;
	}
}

void
	ChannelQueues::EnqueueSwitch(uint32_t ChannelNum, Items item, uint32_t Count, uint32_t RemainTime, bool back)
{
	if(back)
	{
		switch(ChannelNum)
		{
		case SCH2: Sch2List.push_back(item); Counts[0]+=Count; Sums[0]+=RemainTime; break;
		case SCH3: Sch3List.push_back(item); Counts[1]+=Count; Sums[1]+=RemainTime; break;
		case SCH4: Sch4List.push_back(item); Counts[2]+=Count; Sums[2]+=RemainTime; break;
		case SCH5: Sch5List.push_back(item); Counts[3]+=Count; Sums[3]+=RemainTime; break;
		}
	}
	else
	{
		switch(ChannelNum)
		{
		case SCH2: Sch2List.push_front(item); Counts[0]+=Count; Sums[0]+=RemainTime; break;
		case SCH3: Sch3List.push_front(item); Counts[1]+=Count; Sums[1]+=RemainTime; break;
		case SCH4: Sch4List.push_front(item); Counts[2]+=Count; Sums[2]+=RemainTime; break;
		case SCH5: Sch5List.push_front(item); Counts[3]+=Count; Sums[3]+=RemainTime; break;
		}
	}
}

bool
	ChannelQueues::isEmpty(uint32_t ChannelNum)
{
	switch(ChannelNum) {
	case SCH2: return Counts[0] == 0;
	case SCH3: return Counts[1] == 0;
	case SCH4: return Counts[2] == 0;
	case SCH5: return Counts[3] == 0;
	default: return false;
	}
}

uint32_t
	ChannelQueues::Enqueue(Ptr<const Packet> pkt, bool Scheduling, Address sender)
{
	StatsTag tag;
	bool result;
	result = pkt->FindFirstMatchingByteTag (tag);
	if (!result)
	{
		NS_FATAL_ERROR ("the packet here shall have a stats tag");
	}
	uint32_t packetId = tag.GetPacketId ();
	uint32_t ServiceSize = tag.GetServiceSize ();

	for(uint32_t channel = SCH2; channel <= SCH5; channel+=2)
	{
		if(channel == CCH)
			continue;
		if(CheckNodes(channel, sender))
			return -1;
	}

	uint32_t RemainTime = CalcServiceTime(ServiceSize);
	uint32_t ChannelNum = AllocServiceChannel(RemainTime, Scheduling);

	SentInfo info = std::make_pair(sender, 0);
	Items item = std::make_pair(ServiceSize, info);

	EnqueueSwitch(ChannelNum, item, 1, RemainTime, true);

	return ChannelNum;
}

void 
	ChannelQueues::EnqueueRemains(uint32_t ChannelNum, uint32_t RemainSize, Address sender)
{
	if(isEmpty(ChannelNum)) { // when RSU Send the packet and Finish the Job. but Node can't receive the Packet
		// ADD Node's Info to RSU LIST
		SentInfo info = std::make_pair(sender, 0);
		Items item = std::make_pair(RemainSize, info);

		uint32_t RemainTime = CalcServiceTime(RemainSize);
		EnqueueSwitch(ChannelNum, item, 1, RemainTime, false);
	}
	else { // RSU Send Pakcet But Not Finish the Job. and Node know Job isnt finish
		Items item = peek(ChannelNum);

		if(sender != item.second.first)
		{
			// Check this Sender already exist in the Queue, if not return do nothing
			if(CheckNodes(ChannelNum, sender))
				return;
			
			if(item.second.second != 0) // RSU sent Packet but node doesnt confirm yet.. then Input End of Queue
			{
				SentInfo info = std::make_pair(sender, 0);
				item = std::make_pair(RemainSize, info);
				bItems bItem = std::make_pair(ChannelNum, item);

				BackupList.push_back(bItem);

				//uint32_t RemainTime = CalcServiceTime(RemainSize);
				//EnqueueSwitch(ChannelNum, item, 1, RemainTime, true);
			}
			else // following Job doesnt start or start but partitailly finished
			{ 
				SentInfo info = std::make_pair(sender, 0);
				item = std::make_pair(RemainSize, info);

				uint32_t RemainTime = CalcServiceTime(RemainSize);
				EnqueueSwitch(ChannelNum, item, 1, RemainTime, false);
			}
			return;
		}
		
		item = Dequeue(ChannelNum);

		if(item.first == RemainSize)	// Job is Exellent. Send packet and Receive all Pakcet
		{
			item.second.second = 0;
			EnqueueSwitch(ChannelNum, item, 0, 0, false);
			return;
		}

		// Job is Partitially Complete. Sent packet but receive one less
		uint32_t RemainTime = CalcServiceTime(RemainSize - item.first);
		item.first = RemainSize;

		EnqueueSwitch(ChannelNum, item, 0, RemainTime, false);
	}
}

void
	ChannelQueues::PushBackups()
{
	while(!BackupList.empty())
	{
		bItems bitem = *(BackupList.begin());
		uint32_t RemainTime = CalcServiceTime(bitem.second.first);
		
		EnqueueSwitch(bitem.first, bitem.second, 1, RemainTime, false);
		BackupList.pop_front();
	}
}

void
	ChannelQueues::DropService(uint32_t Channel, Address node)
{
	if(!CheckNodes(Channel, node))	// When RSU know All the Schedule of node is Already Finished
		return;

	// When Services to node Remain in RSU Schedule
	Items item = Dequeue(Channel);
	uint32_t RemainTime = CalcServiceTime(item.first);
	if(item.second.first == node)
	{
		switch(Channel)
		{
		case SCH2: Counts[0]--; Sums[0]-=RemainTime; break;
		case SCH3: Counts[1]--; Sums[1]-=RemainTime; break;
		case SCH4: Counts[2]--; Sums[2]-=RemainTime; break;
		case SCH5: Counts[3]--; Sums[3]-=RemainTime; break;
		}
	}
	else
	{
		Items drop = Dequeue(Channel);
		RemainTime = CalcServiceTime(drop.first);
		switch(Channel)
		{
		case SCH2: Counts[0]--; Sums[0]-=RemainTime; break;
		case SCH3: Counts[1]--; Sums[1]-=RemainTime; break;
		case SCH4: Counts[2]--; Sums[2]-=RemainTime; break;
		case SCH5: Counts[3]--; Sums[3]-=RemainTime; break;
		}

		EnqueueSwitch(Channel, item, 0, 0, false);
	}

}

bool
	ChannelQueues::CheckNodes(uint32_t Channel, Address node)
{
	std::list<Items> SchList;
	Items item;

	switch(Channel)
	{
	case SCH2: SchList = Sch2List; break;
	case SCH3: SchList = Sch3List; break;
	case SCH4: SchList = Sch4List; break;
	case SCH5: SchList = Sch5List; break;
	}

	while(!SchList.empty())
	{
		item = *(SchList.begin());
		SchList.pop_front();
		if(item.second.first == node)
			return true;
	}

	return false;
}

Items
	ChannelQueues::peek(uint32_t ChannelNum)
{
	Items item;

	std::list<Items>::iterator iter;
	switch(ChannelNum)
	{
	case SCH2: item = *(Sch2List.begin()); break;
	case SCH3: item = *(Sch3List.begin()); break;
	case SCH4: item = *(Sch4List.begin()); break;
	case SCH5: item = *(Sch5List.begin()); break;
	}
	return item;
}

Items
	ChannelQueues::Dequeue(uint32_t ChannelNum)
{
	Items item;

	switch(ChannelNum)
	{
	case SCH2: item = *(Sch2List.begin()); Sch2List.pop_front(); break;
	case SCH3: item = *(Sch3List.begin()); Sch3List.pop_front(); break;
	case SCH4: item = *(Sch4List.begin()); Sch4List.pop_front(); break;
	case SCH5: item = *(Sch5List.begin()); Sch5List.pop_front(); break;
	}
	return item;
}

uint32_t
	ChannelQueues::AllocServiceChannel(uint32_t RemainTime, bool Scheduling)
{
	uint32_t selected;

	if(Scheduling) // when using sjf scheduling
	{
		uint32_t min = Sums[0];
		selected = SCH2;

		if(min > Sums[1]) 
		{
			min = Sums[1];
			selected = SCH3;
		}
		if(min > Sums[2]) 
		{
			min = Sums[2];
			selected = SCH4;
		}
		if(min > Sums[3]) 
		{
			min = Sums[3];
			selected = SCH5;
		}
	}
	else
	{
		if(current > SCH5)
			current = SCH2;
		if(current == CCH)
			current+=2;
		selected = current;
		current+=2;
	}

	return selected;
}

void
	ChannelQueues::Update(uint32_t channelNum, uint32_t size)
{
	Items item;
	bool boolean = false;

	item = Dequeue(channelNum);
	item.first-=size;
	item.second.second+=size;

	if(item.first == 0)
		boolean = true;

	switch(channelNum) {
	case SCH2:
		if(boolean)
			Counts[0]--;
		else
			Sch2List.push_front(item);
		Sums[0]--;
		break;
	case SCH3: 
		if(boolean)
			Counts[1]--;
		else
			Sch3List.push_front(item);
		Sums[1]--; 
		break;
	case SCH4: 
		if(boolean)
			Counts[2]--;
		else
			Sch4List.push_front(item);
		Sums[2]--; 
		break;
	case SCH5:
		if(boolean)
			Counts[3]--;
		else
			Sch5List.push_front(item); 
		Sums[3]--; 
		break;
	}
}


/**
* (1) some nodes are in constant speed mobility, every node sends
* two types of packets.
* the first type is a small size with 200 bytes which models beacon
* and safety message, this packet is broadcasted to neighbor nodes
* (there will no ACK and retransmission);
* the second type is a big size with 1500 bytes which models information
* or entertainment message, this packet is sent to individual destination
* node (this may cause ACK and retransmission).
*
* (2) Here are four configurations:
* a - the first is every node sends packets randomly in SCH1 channel with continuous access.
* b - the second is every node sends packets randomly with alternating access.
*   Safety packets are sent to CCH channel, and non-safety packets are sent to SCH1 channel.
* c - the third is similar to the second. But Safety packets will be sent randomly in CCH interval,
*   non-safety packets will be sent randomly in SCH interval.
*   This is the best situation of configuration 2 which models higher layer be aware of lower layer.
* d - the fourth is also similar to the second. But Safety packets will be sent randomly in SCH interval,
*   non-safety packets will be sent randomly in CCH interval.
*   This is the worst situation of configuration 2 which makes packets get high queue delay.
*
* (3) Besides (2), users can also configures send frequency and nodes number.
*
* (4) The output is the delay of safety packets, the delay of non-safety packets, the throughput of
* safety packets, and the throughput of safety packets.
*
* REDO: the packets are sent from second 0 to second 99, and the simulation will be stopped at second 100.
* But when simulation is stopped, some packets may be queued in wave mac queue. These queued packets should
* not be used for stats, but here they will be treated as packet loss.
*/
const static uint16_t IPv4_PROT_NUMBER = 0x0800;
const static uint16_t WSMP_PROT_NUMBER = 0x88DC;

const static uint16_t ACK_PROT_NUMBER = 0x88DD;

// the transmission range of a device should be decided carefully,
// since it will affect the packet delivery ratio
// we suggest users use wave-transmission-range.cc to get this value
#define Device_Transmission_Range 250

class MultipleChannelExperiment
{
public:
	MultipleChannelExperiment (void);

	bool Configure (int argc, char **argv);
	void Usage (void);
	void Run (void);
	void Stats (void);

private:
	void CreateWaveNodes (void);
	void CheckRemainService (Ptr<WaveNetDevice> node);
	void ChangeToServiceChannel (Ptr<WaveNetDevice> node);
	void RSUSendPacket(Ptr<WaveNetDevice> node, uint32_t Num);

	// we treat WSMP packets as safety message
	//void SendWsmpPackets (Ptr<WaveNetDevice> sender, uint32_t channelNumber);
	// we treat IP packets as non-safety message
	void SendIpPackets (Address dest, uint32_t channelNum, uint32_t serviceSize);
	// WSAs Packets
	void SendWsaPackets(Ptr<WaveNetDevice> sender, uint32_t channelNumber);
	void SendAckPackets (Ptr<WaveNetDevice> sender, uint32_t channelNumber, Address receiver, uint32_t serviceChannel, uint32_t serviceSize);
	bool Receive (Ptr<NetDevice> dev, Ptr<const Packet> pkt, uint16_t mode, const Address &sender);

	void ConfigurationA (void);
	void ConfigurationB (void);

	void InitStats (void);
	void Stats (uint32_t randomNumber);
	void StatQueuedPackets (void);
	void StatSingleQueue (Ptr<WaveEdcaTxopN> edca);

	NodeContainer nodes;
	NodeContainer RSUs;
	NetDeviceContainer devices;
	NetDeviceContainer RSUdevices;

	ChannelQueues ChannelQueue;

	uint32_t nodesNumber;
	uint32_t channelNow;
	uint32_t frequencySafety;
	uint32_t frequencyNonSafety;
	uint32_t simulationTime;
	uint32_t sizeSafety;
	uint32_t sizeNonSafety;

	Ptr<UniformRandomVariable> rngSafety;
	Ptr<UniformRandomVariable> rngNonSafety;
	Ptr<UniformRandomVariable> rngOther;

	uint32_t safetyPacketID;
	// we will check whether the packet is received by the neighbors
	// in the transmission range
	std::map<uint32_t, std::vector<uint32_t> *> broadcastPackets;

	uint32_t nonSafetyPacketID;

	struct SendIntervalStat
	{
		uint32_t        sendInCchi;
		uint32_t        sendInCguardi;
		uint32_t    sendInSchi;
		uint32_t    sendInSguardi;
	};

	SendIntervalStat sendSafety;
	SendIntervalStat sendNonSafety;
	uint32_t receiveSafety;     // the number of packets
	uint32_t receiveNonSafety;  // the number of packets
	uint32_t queueSafety;
	uint32_t queueNonSafety;
	uint64_t timeSafety;        // us
	uint64_t timeNonSafety;     // us

	uint32_t SCHsends[4];
	uint32_t SCHreceives[4];

	uint32_t ServiceCall;
	uint32_t ServiceFinish;

	bool usingSJF;

	bool createTraceFile;
	std::ofstream outfile;
};

MultipleChannelExperiment::MultipleChannelExperiment (void)
	: nodesNumber (15),           // 20 nodes
	channelNow (0),
	frequencySafety (5),       // 10Hz, 1s send n safety packet
	frequencyNonSafety (0),    // 10Hz, 100ms send one non-safety packet
	simulationTime (30),       // make it run 100s
	sizeSafety (200),           // 100 bytes small size
	sizeNonSafety (1500),       // 1500 bytes big size
	safetyPacketID (0),
	nonSafetyPacketID (0),
	receiveSafety (0),
	receiveNonSafety (0),
	queueSafety (0),
	queueNonSafety (0),
	timeSafety (0),
	timeNonSafety (0),
	ServiceCall (0),
	ServiceFinish (0),
	usingSJF(false),
	createTraceFile (true)
{
	for(int i = 0; i<4; i++)
	{
		SCHsends[i] = 0;
		SCHreceives[i] = 0;
	}
}

bool
	MultipleChannelExperiment::Configure (int argc, char **argv)
{
	CommandLine cmd;
	cmd.AddValue ("nodes", "Number of nodes.", nodesNumber);
	cmd.AddValue ("time", "Simulation time, s.", simulationTime);
	cmd.AddValue ("sizeSafety", "Size of safety packet, bytes.", sizeSafety);
	cmd.AddValue ("sizeNonSafety", "Size of non-safety packet, bytes.", sizeNonSafety);
	cmd.AddValue ("frequencySafety", "Frequency of sending safety packets, Hz.", frequencySafety);
	cmd.AddValue ("frequencyNonSafety", "Frequency of sending non-safety packets, Hz.", frequencyNonSafety);
	cmd.AddValue ("createTraceFile", "create trace file with 4 different configuration", createTraceFile);

	cmd.Parse (argc, argv);
	return true;
}
void
	MultipleChannelExperiment::Usage (void)
{
	std::cout << "usage:"
		<< "./waf --run=\"wave-multiple-channel --nodes=20 --time=100 --sizeSafety=200 "
		"--sizeNonSafety=1500 --frequencySafety=10 --frequencyNonSafety=10 \""
		<< std::endl;
}

void
	MultipleChannelExperiment::CreateWaveNodes (void)
{
	NS_LOG_FUNCTION (this);

	RngSeedManager::SetSeed (1);
	RngSeedManager::SetRun (191);

	nodes = NodeContainer ();
	nodes.Create (nodesNumber);

	RSUs = NodeContainer ();
	RSUs.Create (5);	// CCH and Each 4 SCH

	// Create static grid
	// here we use static mobility model for two reasons
	// (a) since the mobility model of ns3 is mainly used for MANET, we can not ensure
	// whether they are suitable for VANET. From some papers, considering real traffic
	// pattern is required for VANET simulation.
	// (b) Here is no network layer protocol used to avoid causing the impact on MAC layer,
	// which will cause packets can not be routed. So if two nodes is far from each other,
	// the packets will not be received by phy layer, but here we want to show the impact of
	// MAC layer of 1609.4, like low PDR caused by contending at the guard interval of channel.
	// So we need every node is in transmission range.
	MobilityHelper mobility;
	mobility.SetPositionAllocator ("ns3::GridPositionAllocator");
	mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
	mobility.Install (nodes);
	mobility.Install (RSUs);

	for (NodeContainer::Iterator i = nodes.Begin (); i != nodes.End (); ++i)
	{
		Ptr<Node> node = (*i);
		Ptr<MobilityModel> model = node->GetObject<MobilityModel> ();
		Vector pos = model->GetPosition ();
		NS_LOG_DEBUG ( "position: " << pos);
	}

	for (NodeContainer::Iterator i = RSUs.Begin (); i != RSUs.End (); ++i)
	{
		Ptr<Node> node = (*i);
		Ptr<MobilityModel> model = node->GetObject<MobilityModel> ();
		Vector pos = model->GetPosition ();
		NS_LOG_DEBUG ( "position: " << pos);
	}

	YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
	YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();
	wifiPhy.SetChannel (wifiChannel.Create ());
	WaveMacHelper waveMac = WaveMacHelper::Default ();
	WaveHelper waveHelper = WaveHelper::Default ();
	devices = waveHelper.Install (wifiPhy, waveMac, nodes);
	RSUdevices = waveHelper.Install (wifiPhy, waveMac, RSUs);

	// Enable WAVE logs.
	// WaveHelper::LogComponentEnable();
	// or waveHelper.LogComponentEnable();

	for (uint32_t i = 0; i != devices.GetN (); ++i)
	{
		devices.Get (i)->SetReceiveCallback (MakeCallback (&MultipleChannelExperiment::Receive,this));
	}

	for (uint32_t i = 0; i != RSUdevices.GetN (); ++i)
	{
		RSUdevices.Get (i)->SetReceiveCallback (MakeCallback (&MultipleChannelExperiment::Receive,this));
	}

	NetDeviceContainer::Iterator i;
	int channel;
	for (i = RSUdevices.Begin(), channel=SCH2; i != RSUdevices.End (); ++i, channel+=2)
	{
		Ptr<WaveNetDevice> sender = DynamicCast<WaveNetDevice> (*i);
		SchInfo schInfo;
		TxProfile profile;

		if(channel == CCH)
		{
			schInfo = SchInfo (CCH, false, 0xff);
			Simulator::Schedule (Seconds (0.0),&WaveNetDevice::StartSch, sender, schInfo);
			continue;
		}

		schInfo = SchInfo (channel, false, 0xff);
		profile = TxProfile (channel, false, 8, OFDM_6M);

		/*		switch (j) {
		case 1: 
		schInfo = SchInfo (SCH2, false, 0xff);
		profile = TxProfile (SCH2, false, 8, OFDM_6M);
		break;
		case 2: 
		schInfo = SchInfo (SCH3, false, 0xff);
		profile = TxProfile (SCH3, false, 8, OFDM_6M);
		break;
		case 3: 
		schInfo = SchInfo (SCH4, false, 0xff);
		profile = TxProfile (SCH4, false, 8, OFDM_6M);
		break;
		case 4: 
		schInfo = SchInfo (SCH5, false, 0xff);
		profile = TxProfile (SCH5, false, 8, OFDM_6M);
		break;
		}*/

		Simulator::Schedule (Seconds (0.0), &WaveNetDevice::StartSch, sender, schInfo);
		Simulator::Schedule (Seconds (0.0), &WaveNetDevice::RegisterTxProfile, sender, profile);
	}
}

bool
	MultipleChannelExperiment::Receive (Ptr<NetDevice> dev, Ptr<const Packet> pkt, uint16_t mode, const Address &sender)
{
	NS_LOG_FUNCTION (this << dev << pkt << mode << sender);

	// when RSU receive WSA packet
	//   Allocate Channel to Sender, calculate the time to be alloc
	//   send ACK packet to sender
	// when NODE receive ACK packet
	//   change state to scheduled
	// when NODE receive IP packet
	//   It's Success..... make Result

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
	uint32_t serviceSize = tag.GetServiceSize ();
	uint32_t serviceChannel = tag.GetServiceChannel ();

	Ptr<WaveNetDevice> receiver = DynamicCast<WaveNetDevice> (dev);

	if (createTraceFile)
		outfile << "Time = " << std::dec << now.GetMicroSeconds () << "us, receive packet: "
		<< " protocol = 0x" << std::hex << mode << std::dec
		<< " id = " << packetId << " sendTime = " << sendTime.GetMicroSeconds ()
		<< " type = " << (mode == IPv4_PROT_NUMBER ? "NonSafetyPacket" : "SafetyPacket")
		<< " ReceiveChannel = " << serviceChannel
		<< " ServiceSize = " << serviceSize
		<< std::endl;

	if (mode == IPv4_PROT_NUMBER) // When Vehicles(receiver) receive IP Packet from RSU(sender)
	{
		receiver->UpdateSchedule(serviceSize);
		//ChannelQueue.Update(serviceChannel, serviceSize);
		if(!receiver->IsScheduled()) ServiceFinish++;

		switch(serviceChannel)
		{
		case SCH2: SCHreceives[0]++; break;
		case SCH3: SCHreceives[1]++; break;
		case SCH4: SCHreceives[2]++; break;
		case SCH5: SCHreceives[3]++; break;
		}

		receiveNonSafety++;
		timeNonSafety += (now - sendTime).GetMicroSeconds ();
	}
	else if(mode == WSMP_PROT_NUMBER) // When RSU(receiver) receive WSA Message from Vehicle(sender)
	{
		// Allocate Channel and Send ACK Packet
		int channelNum;

		channelNum = ChannelQueue.Enqueue(pkt, usingSJF, sender);


		// Send ACK Packet to Vehicle + NOT SCHEDULE
		SendAckPackets(receiver, CCH, sender, channelNum, serviceSize);

		receiveSafety++;
		timeSafety += (now - sendTime).GetMicroSeconds ();
	}
	else if(mode == ACK_PROT_NUMBER) // When Vehiclues(receiver) receive ACK Packet from RSU(sender)
	{
		receiver->SetSchedule(serviceChannel, serviceSize);
		receiveSafety++;
		timeSafety += (now - sendTime).GetMicroSeconds ();
	}

	return true;
}
// although WAVE devices support send IP-based packets, here we
// simplify ip routing protocol and application. Actually they will
// make delay and through of safety message more serious.
void
	MultipleChannelExperiment::SendIpPackets (Address dest, uint32_t channelNum, uint32_t serviceSize)
{
	NS_LOG_FUNCTION (this << dest);

	Time now = Now ();
	Ptr<Packet> packet = Create<Packet> (serviceSize);
	StatsTag tag = StatsTag (nonSafetyPacketID++, now, channelNum, serviceSize);
	packet->AddByteTag (tag);


	//Address dest = receiver->GetAddress ();
	Ptr<WaveNetDevice> sender;

	switch(channelNum)
	{
	case SCH2: sender = DynamicCast<WaveNetDevice> (RSUdevices.Get(0)); SCHsends[0]++; break;
	case SCH3: sender = DynamicCast<WaveNetDevice> (RSUdevices.Get(1)); SCHsends[1]++; break;
	case SCH4: sender = DynamicCast<WaveNetDevice> (RSUdevices.Get(3)); SCHsends[2]++; break;
	case SCH5: sender = DynamicCast<WaveNetDevice> (RSUdevices.Get(4)); SCHsends[3]++; break;
	}

	bool result = false;
	result = sender->Send (packet, dest, IPv4_PROT_NUMBER);
	if (createTraceFile)
	{
		if (result)
			outfile << "Time = " << now.GetMicroSeconds () << "us,"
			<< " unicast IP packet:  ID = " << tag.GetPacketId ()
			<< ", dest = " << dest << ", channel = " << channelNum << ", size = " << serviceSize
			<< std::endl;
		else
			outfile << "unicast IP packet fail" << std::endl;
	}

	Ptr<ChannelCoordinator> coordinator = sender->GetChannelCoordinator ();
	if (coordinator->IsCchInterval ())
	{
		if (coordinator->IsGuardInterval ())
		{
			sendNonSafety.sendInCguardi++;
		}
		else
		{
			sendNonSafety.sendInCchi++;
		}
	}
	else
	{
		if (coordinator->IsGuardInterval ())
		{
			sendNonSafety.sendInSguardi++;
		}
		else
		{
			sendNonSafety.sendInSchi++;
		}
	}
}

void
	MultipleChannelExperiment::SendWsaPackets (Ptr<WaveNetDevice> sender, uint32_t channelNum)
{
	NS_LOG_FUNCTION (this << sender << channelNum);

	if(sender->IsScheduled())
		return ;

	Time now = Now ();
	Ptr<Packet> packet = Create<Packet> (sizeSafety);
	uint32_t serviceSize = rngOther->GetInteger (1000, 20000);	// 1Mb ~ 20Mb
	StatsTag tag = StatsTag (safetyPacketID++, now, channelNum, serviceSize);
	packet->AddByteTag (tag);

	Address dest = RSUdevices.Get (2)->GetAddress ();

	TxInfo info = TxInfo (channelNum);

	bool result = false;
	result = sender->SendX (packet, dest, WSMP_PROT_NUMBER, info);
	sender->SetSent();
	if (createTraceFile)
	{
		if (result)
			outfile << "Time = " << now.GetMicroSeconds () << "us, Send to RSU WSA packet: ID = " << tag.GetPacketId () << std::endl;
		else
			outfile << "Send to RSU WSA packet fail" << std::endl;
	}

	Ptr<ChannelCoordinator> coordinator = sender->GetChannelCoordinator ();
	if(result) {
		if (coordinator->IsCchInterval ())
		{
			if (coordinator->IsGuardInterval ())
			{
				sendSafety.sendInCguardi++;
			}
			else
			{
				sendSafety.sendInCchi++;
			}

		}
		else
		{
			if (coordinator->IsGuardInterval ())
			{
				sendSafety.sendInSguardi++;
			}
			else
			{
				sendSafety.sendInSchi++;
			}
		}

		ServiceCall++;
	}
}

void
	MultipleChannelExperiment::SendAckPackets (Ptr<WaveNetDevice> sender, uint32_t channelNumber, Address receiver, uint32_t serviceChannel, uint32_t serviceSize)
{
	NS_LOG_FUNCTION (this << sender << channelNumber);
	Time now = Now ();
	Ptr<Packet> packet = Create<Packet> (sizeSafety);
	StatsTag tag = StatsTag (safetyPacketID++, now, serviceChannel, serviceSize);
	packet->AddByteTag (tag);

	Address dest = receiver;

	TxInfo info = TxInfo (channelNumber);

	bool result = false;
	result = sender->SendX (packet, dest, ACK_PROT_NUMBER, info);
	if (createTraceFile)
	{
		if (result)
			outfile << "Time = " << now.GetMicroSeconds () << "us, Send ACK packet: ID = " << tag.GetPacketId () << std::endl;
		else
			outfile << "Send to ACK packet fail" << std::endl;
	}

	Ptr<ChannelCoordinator> coordinator = sender->GetChannelCoordinator ();
	if(result) {
		if (coordinator->IsCchInterval ())
		{
			if (coordinator->IsGuardInterval ())
			{
				sendSafety.sendInCguardi++;
			}
			else
			{
				sendSafety.sendInCchi++;
			}

		}
		else
		{
			if (coordinator->IsGuardInterval ())
			{
				sendSafety.sendInSguardi++;
			}
			else
			{
				sendSafety.sendInSchi++;
			}
		}
	}
}

void
	MultipleChannelExperiment::CheckRemainService(Ptr<WaveNetDevice> node)
{
	NS_LOG_FUNCTION (this << node);

	Time now = Now ();

	if(!(node->IsScheduled()))	// have No Schedule or Finish the Job
		return;

	uint32_t channel = node->GetScheduleChannel();
	uint32_t size = node->GetScheduleSize();

	if(node->DuplicationCounts()) {
		ChannelQueue.EnqueueRemains(channel, size, node->GetAddress());
		ChannelQueue.PushBackups();
	}
	else
		ChannelQueue.DropService(channel, node->GetAddress());
}

void
	MultipleChannelExperiment::ChangeToServiceChannel(Ptr<WaveNetDevice> node)
{
	NS_LOG_FUNCTION (this << node);

	Time now = Now ();

	// check node have Scheduled
	if(!node->IsScheduled())
		return;

	Ptr<ChannelCoordinator> coordinator = node->GetChannelCoordinator ();

	uint32_t channelNumber = node->GetScheduleChannel();
	SchInfo schInfo = SchInfo (channelNumber, false, 0x0);
	Simulator::Schedule (Seconds(0.001),&WaveNetDevice::StartSch,node,schInfo);
	TxProfile profile = TxProfile (channelNumber);
	Simulator::Schedule (Seconds(0.001), &WaveNetDevice::RegisterTxProfile, node, profile);

	// check Channel is ready for this node
	//if(!ChannelQueue.isReady(node->GetSchedule()))
	//	return;

	Simulator::Schedule (Seconds (0.05),&WaveNetDevice::UnregisterTxProfile, node,node->GetScheduleChannel());
	Simulator::Schedule (Seconds (0.05),&WaveNetDevice::StopSch, node, channelNumber);
}

void
	MultipleChannelExperiment::RSUSendPacket(Ptr<WaveNetDevice> node, uint32_t channel)
{
	NS_LOG_FUNCTION (this << node << channel);

	Time now = Now ();
	if(channel == CCH)
		return;

	//while(!ChannelQueue.isEmpty(channel))
	for(uint32_t i=0; i<2; i++)
	{
		if(ChannelQueue.isEmpty(channel))
			break;

		Items item = ChannelQueue.peek(channel);

		uint32_t size = item.first;
		if(size > 6000)
			size = 6000;

		Address address = item.second.first;

		Ptr<ChannelCoordinator> coordinator = node->GetChannelCoordinator ();
		Time t = Seconds (rngSafety->GetValue (0, 0.04));
		/*if (!coordinator->IsSchInterval (t))
		{
		t =  t + coordinator->NeedTimeToSchInterval (t)
		+  MicroSeconds (rngOther->GetInteger (0, coordinator->GetSchInterval ().GetMicroSeconds () - 1));
		}*/

		Simulator::Schedule (t, &MultipleChannelExperiment::SendIpPackets, this, address, channel, size);
		ChannelQueue.Update(channel, size);
	}

}

void
	MultipleChannelExperiment::InitStats (void)
{
	// used for sending packets randomly
	rngSafety = CreateObject<UniformRandomVariable> ();
	rngSafety->SetStream (1);
	rngNonSafety = CreateObject<UniformRandomVariable> ();
	rngNonSafety->SetStream (2);
	rngOther = CreateObject<UniformRandomVariable> ();
	rngOther->SetStream (3);

	broadcastPackets.clear ();
	safetyPacketID = 0;
	nonSafetyPacketID = 0;
	receiveSafety = 0;
	receiveNonSafety = 0;
	queueSafety = 0;
	queueNonSafety = 0;
	timeSafety = 0;
	timeNonSafety = 0;
	sendSafety.sendInCchi = 0;
	sendSafety.sendInCguardi = 0;
	sendSafety.sendInSchi = 0;
	sendSafety.sendInSguardi = 0;
	sendNonSafety.sendInCchi = 0;
	sendNonSafety.sendInCguardi = 0;
	sendNonSafety.sendInSchi = 0;
	sendNonSafety.sendInSguardi = 0;
	
	ServiceCall = 0;
	ServiceFinish = 0;
	for(int i = 0; i<4; i++)
	{
		SCHsends[i] = 0;
		SCHreceives[i] = 0;
	}

	std::map<uint32_t, std::vector<uint32_t> *>::iterator i;
	for (i = broadcastPackets.begin(); i != broadcastPackets.end();++i)
	{
		i->second->clear();
		delete i->second;
		i->second = 0;
	}
	broadcastPackets.clear();

	Simulator::ScheduleDestroy (&MultipleChannelExperiment::StatQueuedPackets, this);
}

void
	MultipleChannelExperiment::StatSingleQueue (Ptr<WaveEdcaTxopN> edca)
{
	WifiMacHeader hdr;
	ObjectMapValue map;

	edca->GetAttribute ("Queues", map);
	for (ObjectPtrContainerValue::Iterator i = map.Begin(); i != map.End(); ++i)
	{
		Ptr<WifiMacQueue> queue = DynamicCast<WifiMacQueue> (i->second);
		Ptr<const Packet> pkt;
		while (pkt = queue->Dequeue (&hdr))
		{
			LlcSnapHeader llc;
			ConstCast<Packet>(pkt)->RemoveHeader (llc);

			if (llc.GetType () == WSMP_PROT_NUMBER)
			{
				queueSafety ++;
			}
			else if (llc.GetType () == IPv4_PROT_NUMBER)
			{
				queueNonSafety ++;
			}
		}
	}
}

// when simulation is stopped, we need to stats the queued packets
// so the real transmitted packets will be (sends - queues).
void
	MultipleChannelExperiment::StatQueuedPackets ()
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
		StatSingleQueue (wave_vo);

		rmac->GetAttribute ("VI_EdcaTxopN", ptr);
		Ptr<EdcaTxopN> vi_edcaTxopN = ptr.Get<EdcaTxopN> ();
		Ptr<WaveEdcaTxopN> wave_vi = DynamicCast<WaveEdcaTxopN>(vi_edcaTxopN);
		StatSingleQueue (wave_vi);

		rmac->GetAttribute ("BE_EdcaTxopN", ptr);
		Ptr<EdcaTxopN> be_edcaTxopN = ptr.Get<EdcaTxopN> ();
		Ptr<WaveEdcaTxopN> wave_be = DynamicCast<WaveEdcaTxopN>(be_edcaTxopN);
		StatSingleQueue (wave_be);

		rmac->GetAttribute ("BK_EdcaTxopN", ptr);
		Ptr<EdcaTxopN> bk_edcaTxopN = ptr.Get<EdcaTxopN> ();
		Ptr<WaveEdcaTxopN> wave_bk = DynamicCast<WaveEdcaTxopN>(bk_edcaTxopN);
		StatSingleQueue (wave_bk);
	}
}

void
	MultipleChannelExperiment::ConfigurationA (void)
{
	NS_LOG_FUNCTION (this);
	for (uint32_t time = 0; time != simulationTime; ++time)	// simulation time : 10, each time is 1 Second
	{

		for (uint32_t sends = 0; sends != frequencySafety; ++sends)	// fequency : 10, each sends is 100ms
		{
			NetDeviceContainer::Iterator i;
			uint32_t channel;
			for (i = devices.Begin (); i != devices.End (); ++i)
			{
				Ptr<WaveNetDevice> node = DynamicCast<WaveNetDevice> (*i);
				Ptr<ChannelCoordinator> coordinator = node->GetChannelCoordinator ();

				Simulator::Schedule(Seconds(time + (0.1 * sends)), &MultipleChannelExperiment::CheckRemainService, this, node);

				Time t = Seconds (rngSafety->GetValue (time, time + 1));
				//Time t = Seconds (rngSafety->GetValue (0, 0.05));
				// if the send time is not at CCHI, we will calculate a new time
				if (!coordinator->IsCchInterval (t))
				{
					t = t + coordinator->NeedTimeToCchInterval (t)
						+  MicroSeconds (rngOther->GetInteger (0, coordinator->GetCchInterval ().GetMicroSeconds () - 1));
				}

				Simulator::Schedule (t, &MultipleChannelExperiment::SendWsaPackets, this, node, CCH);
				//Simulator::Schedule (Seconds(time + (0.1 * sends)) + t, &MultipleChannelExperiment::SendWsaPackets, this, node, CCH);
				Simulator::Schedule (Seconds(time + (0.1 * sends) + 0.05), &MultipleChannelExperiment::ChangeToServiceChannel, this, node);
			}

			for (i = RSUdevices.Begin(), channel = SCH2; i != RSUdevices.End(); ++i, channel+=2)
			{
				Ptr<WaveNetDevice> node = DynamicCast<WaveNetDevice> (*i);

				if(channel == CCH)
					continue;	// skip Basic RSU looking CCH

				Simulator::Schedule (Seconds(time + (0.1 * sends) + 0.05), &MultipleChannelExperiment::RSUSendPacket, this, node, channel);
			}
			/*
			{ // when it is service interval
			uint32_t channelNumber = node->GetSchedule();
			SchInfo schInfo = SchInfo (channelNumber, false, 0x0);
			Simulator::Schedule (Seconds (Seconds(time)),&WaveNetDevice::StartSch,node,schInfo);
			TxProfile profile = TxProfile (channelNumber);
			Simulator::Schedule (Seconds (Seconds(time)), &WaveNetDevice::RegisterTxProfile, node, profile);

			for (uint32_t sends = 0; sends != frequencyNonSafety; ++sends)
			{
			Time t = Seconds (rngNonSafety->GetValue (time, time + 1));
			// if the send time is not at CCHI, we will calculate a new time
			if (!coordinator->IsSchInterval (t))
			{
			t =  t
			+ coordinator->NeedTimeToSchInterval (t)
			+  MicroSeconds (rngOther->GetInteger (0, coordinator->GetSchInterval ().GetMicroSeconds () - 1));
			}
			Simulator::Schedule (t, &MultipleChannelExperiment::SendIpPackets, this, node, channelNumber);
			}
			Simulator::Schedule (Seconds (Seconds(time + 1)),&WaveNetDevice::UnregisterTxProfile, node,node->GetSchedule());
			Simulator::Schedule (Seconds (Seconds(time + 1)),&WaveNetDevice::StopSch, node, channelNumber);
			}*/
		}
	}
}

void
	MultipleChannelExperiment::ConfigurationB (void)
{
	NS_LOG_FUNCTION (this);

}

void
	MultipleChannelExperiment::Run (void)
{
	NS_LOG_FUNCTION (this);
	NS_LOG_DEBUG ("simulation configuration arguments: ");

	{
		NS_LOG_UNCOND ("configuration A:");
		if (createTraceFile)
			outfile.open ("config-a");
		InitStats ();

		CreateWaveNodes ();

		//CreateRSUNodes ();
		//CreateWsaSchedules();

		ConfigurationA ();
		Simulator::Stop (Seconds (simulationTime));
		Simulator::Run ();
		Simulator::Destroy ();
		Stats ();
		if (createTraceFile)
			outfile.close ();
	}
	{/*
	 NS_LOG_UNCOND ("configuration B:");
	 if (createTraceFile)
	 outfile.open ("config-b");
	 InitStats ();
	 CreateWaveNodes ();

	 usingSJF = false;
	 ConfigurationA ();

	 Simulator::Stop (Seconds (simulationTime));
	 Simulator::Run ();
	 Simulator::Destroy ();
	 Stats ();
	 if (createTraceFile)
	 outfile.close ();
	 */}
}
void
	MultipleChannelExperiment::Stats (void)
{
	NS_LOG_FUNCTION (this);
	// first show stats information
	NS_LOG_UNCOND (" safety packet: ");
	NS_LOG_UNCOND ("  sends = " << safetyPacketID);
	NS_LOG_UNCOND ( "  CGuardI CCHI SGuardI SCHI " );
	NS_LOG_UNCOND ("  " << sendSafety.sendInCguardi  << " "
		<< sendSafety.sendInCchi  << " "
		<< sendSafety.sendInSguardi  << " "
		<< sendSafety.sendInSchi);
	NS_LOG_UNCOND ("  receives = " << receiveSafety);
	NS_LOG_UNCOND ("  queues = " << queueSafety);

	NS_LOG_UNCOND (" non-safety packet: ");
	NS_LOG_UNCOND ("  sends = " << nonSafetyPacketID);
	NS_LOG_UNCOND ("  CGuardI CCHI SGuardI SCHI ");
	NS_LOG_UNCOND ("  " << sendNonSafety.sendInCguardi  << " "
		<< sendNonSafety.sendInCchi  << " "
		<< sendNonSafety.sendInSguardi  << " "
		<< sendNonSafety.sendInSchi);
	NS_LOG_UNCOND ("  receives = " << receiveNonSafety);
	NS_LOG_UNCOND ("  queues = " << queueNonSafety);

	// second show performance result
	NS_LOG_UNCOND (" performance result:");
	// stats PDR (packet delivery ratio)
	double safetyPDR = receiveSafety / (double)(safetyPacketID - queueSafety + 1);
	double nonSafetyPDR = receiveNonSafety / (double)(nonSafetyPacketID - queueNonSafety + 1);
	NS_LOG_UNCOND ("  safetyPDR = " << safetyPDR << " , nonSafetyPDR = " << nonSafetyPDR);
	// stats average delay
	double delaySafety = timeSafety / (receiveSafety + 1) / 1000.0;
	double delayNonSafety = timeNonSafety / (receiveNonSafety + 1) / 1000.0;
	NS_LOG_UNCOND ("  delaySafety = " << delaySafety << "ms"
		<< " , delayNonSafety = " << delayNonSafety << "ms");
	// stats average throughout
	double throughoutSafety = receiveSafety * sizeSafety * 8 / simulationTime / 1000.0;
	double throughoutNonSafety = receiveNonSafety * sizeNonSafety * 8 / simulationTime / 1000.0;
	NS_LOG_UNCOND ("  throughoutSafety = " << throughoutSafety << "kbps"
		<< " , throughoutNonSafety = " << throughoutNonSafety << "kbps");
	NS_LOG_UNCOND ("  serviceCallCounts = " << ServiceCall
		<< " , serviceFinshCouts = " << ServiceFinish);
	NS_LOG_UNCOND ("  SCH2: sends = " << SCHsends[0] << " , receives = " << SCHreceives[0]);
	NS_LOG_UNCOND ("  SCH3: sends = " << SCHsends[1] << " , receives = " << SCHreceives[1]);
	NS_LOG_UNCOND ("  SCH4: sends = " << SCHsends[2] << " , receives = " << SCHreceives[2]);
	NS_LOG_UNCOND ("  SCH5: sends = " << SCHsends[3] << " , receives = " << SCHreceives[3]);
}

int
	main (int argc, char *argv[])
{
	//LogComponentEnable ("WaveMultipleChannel", LOG_LEVEL_DEBUG);

	MultipleChannelExperiment experiment;
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
