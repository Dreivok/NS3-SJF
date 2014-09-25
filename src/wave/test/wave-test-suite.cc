/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
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
 * Author: Junling Bu <linlinjavaer@gmail.com>
 */
#include "ns3/wifi-net-device.h"
#include "ns3/adhoc-wifi-mac.h"
#include "ns3/node.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/test.h"
#include "ns3/node-container.h"
#include "ns3/net-device-container.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-helper.h"
#include "ns3/flow-id-tag.h"
#include <iostream>

#include "ns3/channel-coordinator.h"
#include "ns3/channel-manager.h"
#include "ns3/wave-net-device.h"
#include "ns3/wave-mac-helper.h"
#include "ns3/wave-helper.h"

using namespace ns3;

// This test case tests the channel coordination.
// In particular, it checks the following:
// - channel interval calculation including CCH Interval, SCH Interval,
//   Guard Interval and Sync Interval
// - current interval state for current time and future time
// - channel coordination events notified at the correct time.
class ChannelCoordinationTestCase : public TestCase
{
public:
  ChannelCoordinationTestCase (void);
  virtual ~ChannelCoordinationTestCase (void);

  // below three methods are used in CoordinationTestListener
  void NotifyCchStartNow (Time duration);
  void NotifySchStartNow (Time duration);
  void NotifyGuardStartNow (Time duration, bool inCchInterval);
private:
  void TestIntervalAfter (Time duration, bool cchi, bool schi,
                          bool guardi, bool synci, bool switchi);
  virtual void DoRun (void);
  Ptr<ChannelCoordinator> m_coordinator;
};
ChannelCoordinationTestCase::ChannelCoordinationTestCase (void)
  : TestCase ("test channel interval of channel coordination ")
{
}
ChannelCoordinationTestCase::~ChannelCoordinationTestCase (void)
{

}
void
ChannelCoordinationTestCase::TestIntervalAfter (Time duration, bool cchi, bool schi,
                                                bool guardi, bool synci, bool switchi)
{
  int64_t now = Now ().GetMilliSeconds ();
  int64_t after = duration.GetMilliSeconds ();

  NS_TEST_EXPECT_MSG_EQ (m_coordinator->IsCchInterval (duration), cchi, "now is " << now  << "ms "
                         "check whether is CCH interval after duration = " << after << "ms");
  NS_TEST_EXPECT_MSG_EQ (m_coordinator->IsSchInterval (duration), schi, "now is " << now  << "ms "
                         "check whether is SCH interval after duration = " << after << "ms");
  NS_TEST_EXPECT_MSG_EQ (m_coordinator->IsGuardInterval (duration), guardi, "now is " << now  << "ms "
                         "check whether is Guard interval after duration = " << after << "ms");
}
void
ChannelCoordinationTestCase::NotifyCchStartNow (Time duration)
{
  // this method shall be called at 4ms, 104ms, ... synci * n + guardi
  // synci is sync interval with default value 100ms
  // guardi is guard interval with default value 4ms
  // n is sequence number
  int64_t now = Now ().GetMilliSeconds ();
  int64_t synci = m_coordinator->GetSyncInterval ().GetMilliSeconds ();
  int64_t guardi = m_coordinator->GetGuardInterval ().GetMilliSeconds ();
  bool test = (((now - guardi) % synci) == 0);
  NS_TEST_EXPECT_MSG_EQ (test, true, "the time of now shall be synci * n + guardi");

  // besides that, the argument duration shall be cchi - guardi
  Time d = m_coordinator->GetCchInterval () - m_coordinator->GetGuardInterval ();
  NS_TEST_EXPECT_MSG_EQ ((duration == d), true, "the duration shall be cchi - guardi");
}
void
ChannelCoordinationTestCase::NotifySchStartNow (Time duration)
{
  // this method shall be called at 54ms, 154ms, ... synci * n + cchi + guardi
  // synci is sync interval with default value 100ms
  // cchi is CCH interval with default value 50ms
  // guardi is guard interval with default value 4ms
  // n is sequence number
  int64_t now = Now ().GetMilliSeconds ();
  int64_t synci = m_coordinator->GetSyncInterval ().GetMilliSeconds ();
  int64_t cchi = m_coordinator->GetCchInterval ().GetMilliSeconds ();
  int64_t guardi = m_coordinator->GetGuardInterval ().GetMilliSeconds ();
  bool test = ((now - guardi - cchi) % synci == 0);
  NS_TEST_EXPECT_MSG_EQ (test, true, "the time of now shall be synci * n + cchi + guardi");

  // besides that, the argument duration shall be schi - guardi
  Time d = m_coordinator->GetSchInterval () - m_coordinator->GetGuardInterval ();
  NS_TEST_EXPECT_MSG_EQ ((duration == d), true, "the duration shall be schi - guardi");
}
void
ChannelCoordinationTestCase::NotifyGuardStartNow (Time duration, bool inCchInterval)
{
  int64_t now = Now ().GetMilliSeconds ();
  int64_t sync = m_coordinator->GetSyncInterval ().GetMilliSeconds ();
  int64_t cchi = m_coordinator->GetCchInterval ().GetMilliSeconds ();
  bool test = false;
  if (inCchInterval)
    {
      // if cchi, this method will be called at 0ms, 100ms, sync * n
      test = ((now % sync) == 0);
      NS_TEST_EXPECT_MSG_EQ (test, true, "the time of now shall be sync * n");
    }
  else
    {
      // if schi, this method will be called at 50ms, 150ms, sync * n + cchi
      test = (((now - cchi) % sync) == 0);
      NS_TEST_EXPECT_MSG_EQ (test, true, "the time of now shall be sync * n");
    }
  // the duration shall be guardi
  test = (duration == m_coordinator->GetGuardInterval ());
  NS_TEST_EXPECT_MSG_EQ (test, true, "the duration shall be guard interval");
}
void
ChannelCoordinationTestCase::DoRun ()
{
  m_coordinator = CreateObject<ChannelCoordinator> ();

  // first check basic method
  NS_TEST_EXPECT_MSG_EQ (m_coordinator->GetCchInterval (), MilliSeconds (50), "normally CCH interval is 50ms");
  NS_TEST_EXPECT_MSG_EQ (m_coordinator->GetSchInterval (), MilliSeconds (50), "normally SCH interval is 50ms");
  NS_TEST_EXPECT_MSG_EQ (m_coordinator->GetSyncInterval (), MilliSeconds (100), "normally Sync interval is 50ms");
  NS_TEST_EXPECT_MSG_EQ (m_coordinator->GetGuardInterval (), MilliSeconds (4), "normally Guard interval is 50ms");

  // now it is 0s
  // after 0ms, it's CCH Interval, is SyncTolerance Interval, also is Guard Interval
  TestIntervalAfter (MilliSeconds (0), true, false, true, true, false );
  // after 1ms, it's CCH Interval, is MaxChSwitch Interval, Guard Interval
  TestIntervalAfter (MilliSeconds (1), true, false, true, false, true);
  // after 3ms, it's CCH Interval, is SyncTolerance Interval, also Guard Interval
  TestIntervalAfter (MilliSeconds (3), true, false, true, true, false);
  // after 4ms, it's CCH Interval
  TestIntervalAfter (MilliSeconds (4), true, false, false, false, false);
  // after 0ms, it's SCH Interval, is SyncTolerance Interval, also is Guard Interval
  TestIntervalAfter (MilliSeconds (50), false, true, true, true, false );
  // after 51ms, it's SCH Interval, is MaxChSwitch Interval, Guard Interval
  TestIntervalAfter (MilliSeconds (51), false, true, true, false, true);
  // after 53ms, it's SCH Interval, is SyncTolerance Interval, also Guard Interval
  TestIntervalAfter (MilliSeconds (53), false, true, true, true, false);
  // after 54ms, it's SCH Interval
  TestIntervalAfter (MilliSeconds (54), false, true, false, false, false);
  // after 100ms, again it's CCH Interval, is SyncTolerance Interval, also is Guard Interval
  TestIntervalAfter (MilliSeconds (100), true, false, true, true, false );

  Simulator::Stop (Seconds (51.0));
  Simulator::Run ();
  Simulator::Destroy ();

  // refer to the standard, every UTC second shall be an integer number of SyncInterval");
  // if user set cchi = 40 and schi = 70, then will get error for not support
  // and user can specific or dynamic cchi/schi by inherting from ChannelCoordinator.
  m_coordinator->SetCchInterval (MilliSeconds (40));
  m_coordinator->SetSchInterval (MilliSeconds (60));
  Simulator::Stop (Seconds (51.0));
  Simulator::Run ();
  Simulator::Destroy ();
}

// CoordinationTestListener is used to test channel coordination events
class CoordinationTestListener : public ChannelCoordinationListener
{
public:
  CoordinationTestListener (ChannelCoordinationTestCase *coordinatorTest)
    : m_coordinatorTest (coordinatorTest)
  {

  }
  virtual ~CoordinationTestListener (void)
  {
  }

  virtual void NotifyCchSlotStart (Time duration)
  {
    m_coordinatorTest->NotifyCchStartNow (duration);
  }

  virtual void NotifySchSlotStart (Time duration)
  {
    m_coordinatorTest->NotifySchStartNow (duration);
  }

  virtual void NotifyGuardSlotStart (Time duration, bool cchi)
  {
    m_coordinatorTest->NotifyGuardStartNow (duration, cchi);
  }
  ChannelCoordinationTestCase *m_coordinatorTest;
};
/**
 *  route different packets or frames
 *  see 1609.4-2010 chapter 5.3.4
 *
 */
class ChannelRoutingTestCase : public TestCase
{
public:
  ChannelRoutingTestCase (void);
  virtual ~ChannelRoutingTestCase (void);

  // send IP-based packets
  // shouldSuccess is used to check whether packet sent should be successful.
  void Send (bool shouldSucceed);
  // send WSMP or other packets
  // shouldSuccess is used to check whether packet sent should be successful.
  void SendX (bool shouldSucceed, const TxInfo &txInfo);
  // send VSA management frames
  // shouldSuccess is used to check whether packet sent should be successful.
  void SendVsa (bool shouldSucceed);
private:
  virtual void DoRun (void);
  void CreatWaveDevice ();
  Ptr<WaveNetDevice>  m_sender;
};
ChannelRoutingTestCase::ChannelRoutingTestCase (void)
  : TestCase ("test channel routing for packets with different send methods of WaveNetDevice")
{

}
ChannelRoutingTestCase::~ChannelRoutingTestCase (void)
{

}
void
ChannelRoutingTestCase::SendX (bool shouldSucceed, const TxInfo &txInfo)
{
  Ptr<Packet> packet = Create<Packet> (100);
  const Address dest = Mac48Address::GetBroadcast ();
  uint16_t protocol = 0x80dd; // any number is OK
  bool result = m_sender->SendX (packet, dest, protocol, txInfo);
  NS_TEST_EXPECT_MSG_EQ (result, shouldSucceed, "test SendX method error");
}
void
ChannelRoutingTestCase::Send (bool shouldSucceed)
{
  Ptr<Packet> packet = Create<Packet> (100);
  const Address dest = Mac48Address::GetBroadcast ();
  uint16_t protocol = 0x80dd; // any number is OK
  bool result = m_sender->Send (packet, dest, protocol);
  NS_TEST_EXPECT_MSG_EQ (result, shouldSucceed, "test Send method error");
}
void
ChannelRoutingTestCase::SendVsa (bool shouldSucceed)
{

}
void
ChannelRoutingTestCase::CreatWaveDevice ()
{
  NodeContainer nodes;
  nodes.Create (1);

  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 0.0));
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());
  QosWifipMacHelper waveMac = QosWifipMacHelper::Default ();
  WaveHelper waveHelper = WaveHelper::Default ();
  NetDeviceContainer devices = waveHelper.Install (wifiPhy, waveMac, nodes);

  m_sender = DynamicCast<WaveNetDevice> (devices.Get (0));
}
void
ChannelRoutingTestCase::DoRun ()
{
  //  check SendX method for WSMP packets
  {
    // first create a device
    CreatWaveDevice ();

    TxInfo txInfo = TxInfo (CCH);
    // the packet will be dropped because of channel access is not assigned
    Simulator::Schedule (Seconds (0.1), &ChannelRoutingTestCase::SendX, this, false, txInfo);

    // assign channel access at 0.2s
    const SchInfo schInfo = SchInfo (CCH, false, 0xff);
    Simulator::Schedule (Seconds (0.2), &WaveNetDevice::StartSch, m_sender, schInfo);

    // the packet will succeed
    Simulator::Schedule (Seconds (0.3), &ChannelRoutingTestCase::SendX, this, true, txInfo);
    txInfo.channelNumber = SCH1;

    // the packet will be dropped because access of request channel is not assigned
    Simulator::Schedule (Seconds (0.31), &ChannelRoutingTestCase::SendX, this, false, txInfo);
    txInfo.channelNumber = SCH2;
    Simulator::Schedule (Seconds (0.32), &ChannelRoutingTestCase::SendX, this, false, txInfo);
    txInfo.channelNumber = SCH3;
    Simulator::Schedule (Seconds (0.33), &ChannelRoutingTestCase::SendX, this, false, txInfo);
    txInfo.channelNumber = SCH4;
    Simulator::Schedule (Seconds (0.34), &ChannelRoutingTestCase::SendX, this, false, txInfo);
    txInfo.channelNumber = SCH5;
    Simulator::Schedule (Seconds (0.35), &ChannelRoutingTestCase::SendX, this, false, txInfo);
    txInfo.channelNumber = SCH6;
    Simulator::Schedule (Seconds (0.36), &ChannelRoutingTestCase::SendX, this, false, txInfo);

    // the packet will be dropped because the channel is invalid WAVE channel
    txInfo.channelNumber = 0;
    Simulator::Schedule (Seconds (0.41), &ChannelRoutingTestCase::SendX, this, false, txInfo);
    txInfo.channelNumber = 200;
    Simulator::Schedule (Seconds (0.42), &ChannelRoutingTestCase::SendX, this, false, txInfo);

    // the packet will be dropped because the priority is invalid
    txInfo = TxInfo (CCH, 8);
    Simulator::Schedule (Seconds (0.51), &ChannelRoutingTestCase::SendX, this, false, txInfo);

    // release channel access at 0.6s
    Simulator::Schedule (Seconds (0.6), &WaveNetDevice::StopSch, m_sender, CCH);

    // the packet will be dropped because channel access is not assigned again
    txInfo = TxInfo (CCH);
    Simulator::Schedule (Seconds (0.7), &ChannelRoutingTestCase::SendX, this, false, txInfo);

    Simulator::Stop (Seconds (1.0));
    Simulator::Run ();
    Simulator::Destroy ();
  }

  // check Send method for IP-based packets
  {
    // second create a device
    CreatWaveDevice ();

    // the packet will be dropped because of channel access is not assigned
    Simulator::Schedule (Seconds (0.1), &ChannelRoutingTestCase::Send, this, false);

    // assign channel access at 0.2s
    // note: here should not assign for CCH, because RegisterTxProfile will return error
    // and we want to test Send method here rather than RegisterTxProfile method.
    const SchInfo schInfo = SchInfo (SCH1, false, 0xff);
    Simulator::Schedule (Seconds (0.2), &WaveNetDevice::StartSch, m_sender, schInfo);

    // the packet will be dropped because here is no txprofile registered.
    Simulator::Schedule (Seconds (0.3), &ChannelRoutingTestCase::Send, this, false);

    TxProfile txProfile = TxProfile (SCH1);
    Simulator::Schedule (Seconds (0.4), &WaveNetDevice::RegisterTxProfile, m_sender, txProfile);

    // the packet will succeed to be sent.
    Simulator::Schedule (Seconds (0.51), &ChannelRoutingTestCase::Send, this, true);
    // send again
    Simulator::Schedule (Seconds (0.52), &ChannelRoutingTestCase::Send, this, true);

    // unregister txprofile
    Simulator::Schedule (Seconds (0.6), &WaveNetDevice::UnregisterTxProfile, m_sender,SCH1);
    // fail because no txprofile, so Send method will fail
    Simulator::Schedule (Seconds (0.71), &ChannelRoutingTestCase::Send, this, false);
    TxInfo txInfo = TxInfo (SCH1);
    // although IP-based packets cannot be sent,
    // WSMP packets can be sent successfully because channel is assigned
    Simulator::Schedule (Seconds (0.72), &ChannelRoutingTestCase::SendX, this, true, txInfo);

    // release channel access
    // mac entities have no channel resource even phy device has ability to send
    Simulator::Schedule (Seconds (0.8),&WaveNetDevice::StopSch, m_sender, SCH1);
    // IP-based packets is sent fail
    Simulator::Schedule (Seconds (0.9), &ChannelRoutingTestCase::Send, this, false);

    Simulator::Stop (Seconds (1.0));
    Simulator::Run ();
    Simulator::Destroy ();
  }

  // check StartVsa method for management frames
  // now not implemented
  {

  }
}
// this test case tests channel access assignments which is done by
// StartSch and StopSch method of WaveNetDevice.
// channel access assignments include ContinuousAccess, ExtendedAccess,
// and AlternatingAccess. The assignment can also set or unset immediate
// choice which is useful to select proper interval.
class ChannelAccessTestCase : public TestCase
{
public:
  ChannelAccessTestCase (void);
  virtual ~ChannelAccessTestCase (void);
private:
  void CreatWaveDevice (void);
  void TestContinuous (SchInfo &info, bool shouldSuccceed);
  void TestExtended (SchInfo &info, bool shouldSuccceed);
  void TestAlternating (SchInfo &info, bool shouldSuccceed);

  virtual void DoRun (void);
  Ptr<WaveNetDevice>  m_sender;
};
ChannelAccessTestCase::ChannelAccessTestCase (void)
  : TestCase ("test StartSch method with different channel access assignments")
{
}
ChannelAccessTestCase::~ChannelAccessTestCase (void)
{

}
void
ChannelAccessTestCase::TestContinuous (SchInfo &info, bool shouldSuccceed)
{
  bool result = m_sender->StartSch (info);
  NS_TEST_EXPECT_MSG_EQ (result, shouldSuccceed, "TestContinuous fail");
}
void
ChannelAccessTestCase::TestExtended (SchInfo &info, bool shouldSuccceed)
{
  bool result = m_sender->StartSch (info);
  NS_TEST_EXPECT_MSG_EQ (result, shouldSuccceed, "TestExtended fail");
}
void
ChannelAccessTestCase::TestAlternating (SchInfo &info, bool shouldSuccceed)
{
  bool result = m_sender->StartSch (info);
  NS_TEST_EXPECT_MSG_EQ (result, shouldSuccceed, "TestAlternating fail");
}

void
ChannelAccessTestCase::CreatWaveDevice (void)
{
  NodeContainer nodes;
  nodes.Create (1);

  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 0.0));
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());
  QosWifipMacHelper waveMac = QosWifipMacHelper::Default ();
  WaveHelper waveHelper = WaveHelper::Default ();
  NetDeviceContainer devices = waveHelper.Install (wifiPhy, waveMac, nodes);

  m_sender = DynamicCast<WaveNetDevice> (devices.Get (0));
}
void
ChannelAccessTestCase::DoRun ()
{
  // test ContinuousAccess with immediate
  {
    CreatWaveDevice ();

    // when the extended value of SchInfo is 0xff, it means continuous access.
    SchInfo info = SchInfo (CCH, false, 0xff);
    // assign channel access successfully
    Simulator::Schedule (Seconds (1.0), &ChannelAccessTestCase::TestContinuous, this, info, true);
    // request again for the same channel, that's OK, however is meaningless.
    Simulator::Schedule (Seconds (1.2), &ChannelAccessTestCase::TestContinuous, this, info, true);
    info = SchInfo (SCH1, false, 0xff);
    // fail, you have to release channel access first
    Simulator::Schedule (Seconds (1.4), &ChannelAccessTestCase::TestContinuous, this, info, false);

    // then we release channel access at 2.0s
    Simulator::Schedule (Seconds (2.0), &WaveNetDevice::StopSch, m_sender, CCH);

    info = SchInfo (SCH2, false, 0xff);
    // assign channel access for other channel successfully
    Simulator::Schedule (Seconds (2.2), &ChannelAccessTestCase::TestContinuous, this, info, true);

    Simulator::Stop (Seconds (4.0));
    Simulator::Run ();
    Simulator::Destroy ();
  }

  // test ExtendedAccess with immediate
  {
    CreatWaveDevice ();

    // when the extended value of SchInfo is less than 0xff and nonzero, it means extended access.
    SchInfo info = SchInfo (SCH1, false, 5);
    // assign channel access successfully
    Simulator::Schedule (Seconds (1.0), &ChannelAccessTestCase::TestExtended, this, info, true);
    // request again for the same channel, that's OK, however is meaningless.
    Simulator::Schedule (Seconds (1.0), &ChannelAccessTestCase::TestExtended, this, info, true);
    info = SchInfo (SCH1, false, 4);
    // succeed and extends of channel access is still 5
    Simulator::Schedule (Seconds (1.0), &ChannelAccessTestCase::TestExtended, this, info, true);
    info = SchInfo (SCH2, false, 6);
    // at 1.3s fail, at 1.6s succeed, why?
    // because at 1.3s, the extended access for SCH1 still exists until 1.5s
    // at 1.5s, the channel access has already been released automatically,
    // so at 1.6s, the channel resource is idle.
    Simulator::Schedule (Seconds (1.3), &ChannelAccessTestCase::TestExtended, this, info, false);
    Simulator::Schedule (Seconds (1.6), &ChannelAccessTestCase::TestExtended, this, info, true);

    info = SchInfo (SCH3, false, 6);
    // channel access successfully for SCH3, the channel access for SCH1 is released at 2.2s
    Simulator::Schedule (Seconds (2.3), &ChannelAccessTestCase::TestExtended, this, info, true);

    Simulator::Stop (Seconds (4.0));
    Simulator::Run ();
    Simulator::Destroy ();
  }

  // test AlternatingAccess with immediate
  {
    CreatWaveDevice ();
    // when the extended value of SchInfo is zero, it means alternating access.
    SchInfo info = SchInfo (CCH, false, 0);
    //fail, we shall not request with CCH for alternating access
    Simulator::Schedule (Seconds (1.0), &ChannelAccessTestCase::TestAlternating, this, info, false);

    info = SchInfo (SCH1, false, 0);
    // assign channel access successfully
    Simulator::Schedule (Seconds (1.0), &ChannelAccessTestCase::TestAlternating, this, info, true);
    // request again for the same channel, that's OK, however is meaningless.
    Simulator::Schedule (Seconds (1.0), &ChannelAccessTestCase::TestAlternating, this, info, true);

    info = SchInfo (SCH1, false, 2);
    // fail, we cannot request for extened access
    Simulator::Schedule (Seconds (2.3), &ChannelAccessTestCase::TestAlternating, this, info, false);
    info = SchInfo (SCH1, false, 0xff);
    // fail, we cannot request for continuous access
    Simulator::Schedule (Seconds (2.4), &ChannelAccessTestCase::TestAlternating, this, info, false);

    info = SchInfo (SCH2, false, 0);
    // fail, since now channel is switching SCH1 with CCH, the we can not support SCH2
    Simulator::Schedule (Seconds (3.0), &ChannelAccessTestCase::TestAlternating, this, info, false);


    Simulator::Stop (Seconds (4.0));
    Simulator::Run ();
    Simulator::Destroy ();
  }
}
// this test case tests the AlternatingAccess.
// Now here is one sender and three receivers.
//            |  CCHI  | SCHI |  CCHI | SCHI |.....
// sender:       CCH     SCH1    CCH1   SCH1
// receiver1:    CCH     SCH1    CCH    SCH1
// receiver2:    CCH     CCH     CCH    CCH
// receiver3:    SCH1    SCH1    SCH1   SCH1
// The sender and receiver1 are AlternatingAccess, receiver2
// and receiver3 are ContinusAccess that one is CCH and another is SCH1.
// sender sends 4 packets per 100ms for 10s, and receiver1 receives almost
// all packets, receive2 receives about half of packets at CCH interval,
// receive3 receives about half of packets at SCH1 interval.
class AlternatingAccessTestCase : public TestCase
{
public:
  AlternatingAccessTestCase ();
  virtual ~AlternatingAccessTestCase ();
private:
  void CreatWaveDevice (void);
  void Send (void);
  bool Receive (Ptr<NetDevice> dev, Ptr<const Packet> pkt, uint16_t mode, const Address &sender);
  bool ReceiveInCch (Ptr<NetDevice> dev, Ptr<const Packet> pkt, uint16_t mode, const Address &sender);
  bool ReceiveInSch (Ptr<NetDevice> dev, Ptr<const Packet> pkt, uint16_t mode, const Address &sender);

  virtual void DoRun (void);
  Ptr<WaveNetDevice>  m_sender;
  Ptr<WaveNetDevice>  m_receiver;
  Ptr<WaveNetDevice> m_cchReceiver;
  Ptr<WaveNetDevice> m_schReceiver;
  int m_sends;
  int m_receives;
  int m_cchReceives;
  int m_schReceives;
};
AlternatingAccessTestCase::AlternatingAccessTestCase ()
  : TestCase ("test alternating access that packets will send in diffrent interval"),
    m_sends (0),
    m_receives (0),
    m_cchReceives (0),
    m_schReceives (0)
{

}
AlternatingAccessTestCase::~AlternatingAccessTestCase ()
{

}
void
AlternatingAccessTestCase::CreatWaveDevice (void)
{
  NodeContainer nodes;
  nodes.Create (4);

  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 0.0));
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());
  QosWifipMacHelper waveMac = QosWifipMacHelper::Default ();
  WaveHelper waveHelper = WaveHelper::Default ();
  NetDeviceContainer devices = waveHelper.Install (wifiPhy, waveMac, nodes);

  m_sender = DynamicCast<WaveNetDevice> (devices.Get (0));
  m_receiver = DynamicCast<WaveNetDevice> (devices.Get (1));
  m_receiver->SetReceiveCallback (MakeCallback (&AlternatingAccessTestCase::Receive,this));
  m_cchReceiver = DynamicCast<WaveNetDevice> (devices.Get (2));
  m_cchReceiver->SetReceiveCallback (MakeCallback (&AlternatingAccessTestCase::ReceiveInCch,this));
  m_schReceiver = DynamicCast<WaveNetDevice> (devices.Get (3));
  m_schReceiver->SetReceiveCallback (MakeCallback (&AlternatingAccessTestCase::ReceiveInSch,this));

}
#define CCHI 0
#define SCHI 1
void
AlternatingAccessTestCase::Send ()
{
  Ptr<Packet> p = Create<Packet> (1000);
  TxInfo info;
  Ptr<ChannelCoordinator> coordinator = m_sender->GetChannelCoordinator ();
  if (coordinator->IsCchInterval ())
    {
	  info = TxInfo (CCH);
      p->AddByteTag (FlowIdTag (CCHI));
    }
 else
   {
      info = TxInfo (SCH1);
      p->AddByteTag (FlowIdTag (SCHI));
   }
  std::cout << " send time = " << Now ().GetMilliSeconds () << std::endl;

  m_sender->SendX (p, Mac48Address::GetBroadcast (), 0x00, info);
  m_sends++;
}
bool
AlternatingAccessTestCase::Receive (Ptr<NetDevice> dev, Ptr<const Packet> pkt, uint16_t mode, const Address &sender)
{
  std::cout << " Receive time = " << Now ().GetMilliSeconds () << std::endl;
  Ptr<ChannelCoordinator> coordinator = m_receiver->GetChannelCoordinator ();
  // although in the standard, the first 1ms and last 1ms of guard interval, the device can
  // only receive packet for the fact that devices may not be precisely aligned in time.
  // However we assume the device in simulation is precisely aligned in time which means
  // devices will not send at the guard interval, so the device shall not receive any packet.
  NS_TEST_EXPECT_MSG_EQ (coordinator->IsGuardInterval (), false, "we cannot receive any packet at guard interval");
  m_receives++;

  FlowIdTag tag;
  if (pkt->FindFirstMatchingByteTag (tag))
    {
      if (coordinator->IsCchInterval ())
        {
          NS_TEST_EXPECT_MSG_EQ (tag.GetFlowId (), CCHI, "receive a packet in CCH interval which is sent in SCH interval");
        }
      else if (coordinator->IsSchInterval ())
        {
          NS_TEST_EXPECT_MSG_EQ (tag.GetFlowId (), SCHI, "receive a packet in SCH interval which is sent in CCH interval");
        }
      else
        {
          NS_FATAL_ERROR ("cannot get here");
        }
    }
  return true;
}

bool
AlternatingAccessTestCase::ReceiveInCch (Ptr<NetDevice> dev, Ptr<const Packet> pkt, uint16_t mode, const Address &sender)
{
  std::cout << " ReceiveInCch time = " << Now ().GetMilliSeconds () << std::endl;
  Ptr<ChannelCoordinator> coordinator = m_cchReceiver->GetChannelCoordinator ();
  // actually we can receive packet at the guard interval of CCHI when channel access is ContinuoisAccess,
  // but since here is only one sender with AlternatingAccess which will not send packets at guard interval,
  // so here receive with CCH ContinuousAccess should not receive packets at guard interval.
  NS_TEST_EXPECT_MSG_EQ (coordinator->IsGuardInterval (), false,"we cannot receive any packet at guard interval");

  NS_TEST_EXPECT_MSG_EQ (coordinator->IsSchInterval (), false, "we cannot receive any packet at SCH interval");
  m_cchReceives++;

  FlowIdTag tag;
  if (pkt->FindFirstMatchingByteTag (tag))
    {
      if (coordinator->IsCchInterval ())
        {
          NS_TEST_EXPECT_MSG_EQ (tag.GetFlowId (), CCHI, "receive a packet in CCH interval which is sent in SCH interval");
        }
    }
  return true;
}
bool
AlternatingAccessTestCase::ReceiveInSch (Ptr<NetDevice> dev, Ptr<const Packet> pkt, uint16_t mode, const Address &sender)
{
  std::cout << " ReceiveInSch time = " << Now ().GetMilliSeconds () << std::endl;
  Ptr<ChannelCoordinator> coordinator = m_schReceiver->GetChannelCoordinator ();
  // actually we can receive packet at the guard interval of SCHI when channel access is ContinuoisAccess,
  // but since here is only one sender with AlternatingAccess which will not send packets at guard interval,
  // so here receive with SCH1 ContinuousAccess should not receive packets at guard interval.
  NS_TEST_EXPECT_MSG_EQ (coordinator->IsGuardInterval (), false,"we cannot receive any packet at guard interval");

  NS_TEST_EXPECT_MSG_EQ (coordinator->IsCchInterval (), false, "we cannot receive any packet at CCH interval");

  m_schReceives++;

  FlowIdTag tag;
  if (pkt->FindFirstMatchingByteTag (tag))
    {
      if (coordinator->IsSchInterval ())
        {
          NS_TEST_EXPECT_MSG_EQ (tag.GetFlowId (), SCHI, "receive a packet in SCH interval which is sent in CCH interval");
        }
    }
  return true;
}
void
AlternatingAccessTestCase::DoRun ()
{
  CreatWaveDevice ();
  // asign alternating access for sender and receiver
  SchInfo info = SchInfo (SCH1, false, 0);
  Simulator::Schedule (Seconds (0), &WaveNetDevice::StartSch, m_sender, info);
  Simulator::Schedule (Seconds (0), &WaveNetDevice::StartSch, m_receiver, info);
  info = SchInfo (CCH, false, 0xff);
  Simulator::Schedule (Seconds (0), &WaveNetDevice::StartSch, m_cchReceiver, info);
  info = SchInfo (SCH1, false, 0xff);
  Simulator::Schedule (Seconds (0), &WaveNetDevice::StartSch, m_schReceiver, info);

  Ptr<UniformRandomVariable> random = CreateObject<UniformRandomVariable> ();
  int time = 0;
  while (time < 10) // run 10s
    {
      // normally send 40 packets per 1s, which means 2 packets in every SCH interval
      // and 2 packets every CCH interval.
      for (int i = 0; i < 40; ++i)
        {
          Time sendTime = Seconds (random->GetValue (time, time + 1));
          Simulator::Schedule (sendTime, &AlternatingAccessTestCase::Send, this);
        }
      time++;
    }

  Simulator::Stop (Seconds (time));
  Simulator::Run ();
  Simulator::Destroy ();

  std::cout <<  "sends = " << m_sends
            << " receive = " << m_receives
            << " cch receive = " << m_cchReceives
            << " sch receive = " << m_schReceives
            << std::endl;
}

class WaveTestSuite : public TestSuite
{
public:
  WaveTestSuite ();
};

WaveTestSuite::WaveTestSuite ()
  : TestSuite ("wave", UNIT)
{
  // TestDuration for TestCase can be QUICK, EXTENSIVE or TAKES_FOREVER
  AddTestCase (new ChannelCoordinationTestCase, TestCase::QUICK);
  AddTestCase (new ChannelRoutingTestCase, TestCase::QUICK);
  AddTestCase (new ChannelAccessTestCase, TestCase::QUICK);
  AddTestCase (new AlternatingAccessTestCase, TestCase::QUICK);
}

// Do not forget to allocate an instance of this TestSuite
static WaveTestSuite waveTestSuite;
