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
#include "ns3/log.h"
#include "ns3/string.h"
#include "ns3/command-line.h"
#include "ns3/mobility-model.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/position-allocator.h"
#include "ns3/mobility-helper.h"
#include "ns3/wifi-net-device.h"
#include <iostream>

#include "ns3/ocb-wifi-mac.h"
#include "ns3/wifi-80211p-helper.h"
#include "ns3/wave-mac-helper.h"
#include "ns3/wave-helper.h"
#include "ns3/channel-manager.h"
#include "ns3/wave-net-device.h"

NS_LOG_COMPONENT_DEFINE ("WaveWifi80211pVsaExample");

using namespace ns3;

/*
 *  Normally the higher layer can send packets in different protocols
 *  via 802.11 data frame, like ipv4 and ipv6.
 *  However, actually wifi know nothing about these differences,
 *  because this work has been done with LlcSnapHeader with protocol field.
 *  So if higher layer wants to send different management informations
 *  over 802.11 management frame (of course nobody can prevent someone just
 *  wants to send management information over 802.11 data frame), how can we do?
 *  The 802.11 and 802.11p can allow the higher layer send different informations
 *  over Vendor Specific Action management frame by using different
 *  OrganizationIdentifier fields to identify differences.
 *  Usage:
 *  1. define an OrganizationIdentifier
 *     uint8_t oi_bytes[5] = {0x00, 0x50, 0xC2, 0x4A, 0x40};
 *     OrganizationIdentifier oi(oi_bytes,5);
 *  2. define a Callback for the defined OrganizationIdentifier
 *     VscCallback vsccall = MakeCallback (&Wifi80211pVsaExample::GetWsaAndOi, this);
 *  3. OcbWifiMac registers this identifier and function
 *     Ptr<WifiNetDevice> device1 = DynamicCast<WifiNetDevice>(nodes.Get (i)->GetDevice (0));
 *     Ptr<OcbWifiMac> ocb1 = DynamicCast<OcbWifiMac>(device->GetMac ());
 *     ocb1->AddReceiveVscCallback (oi, vsccall);
 *  4. now you can send management packet over VSA frame
 *     Ptr<Packet> vsc = Create<Packet> ();
 *     ocb2->SendVsc (vsc, Mac48Address::GetBroadcast (), m_16093oi);
 *  5. then registed callback in other devices will be called.
 */
class Wifi80211pVsaExample
{
public:
  Wifi80211pVsaExample ();
  virtual ~Wifi80211pVsaExample ();
  void DoRun (void);
private:
  void SendWsa (Ptr<OcbWifiMac>);
  void SendAuthen (Ptr<OcbWifiMac>);
  bool GetWsaAndOi (Ptr<WifiMac>, const OrganizationIdentifier &,Ptr<const Packet>,const Address &);
  bool GetAuthenAndOi (Ptr<WifiMac>, const OrganizationIdentifier &,Ptr<const Packet>,const Address &);

  OrganizationIdentifier m_16093oi;
  OrganizationIdentifier m_authenoi;
};

Wifi80211pVsaExample::Wifi80211pVsaExample ()
{
// define two OrganizationIdentifier
// 00-50-C2-4A-4y is used in WAVE
// 00-0F-AC is used in authentication services, see 802.11-2010 chapter 4.10
  uint8_t oi_bytes_16093[5] = {0x00, 0x50, 0xC2, 0x4A, 0x40};
  OrganizationIdentifier oi_16093 (oi_bytes_16093,5);
  m_16093oi = oi_16093;

  uint8_t oi_bytes_authen[5] = {0x00, 0x0F, 0xAC};
  OrganizationIdentifier oi_authen (oi_bytes_authen,3);
  m_authenoi = oi_authen;
}


Wifi80211pVsaExample::~Wifi80211pVsaExample ()
{
}

bool
Wifi80211pVsaExample::GetWsaAndOi (Ptr<WifiMac> mac, const OrganizationIdentifier & oi,Ptr<const Packet> wsa, const Address & peer)
{
  std::cout << mac->GetAddress () << "receive wsa packet" << std::endl;
  return true;
}

bool
Wifi80211pVsaExample::GetAuthenAndOi (Ptr<WifiMac> mac, const OrganizationIdentifier & oi,Ptr<const Packet> auth,const Address & peer)
{
  std::cout << mac->GetAddress () << "receive authen packet" << std::endl;
  return true;
}

void
Wifi80211pVsaExample::SendWsa (Ptr<OcbWifiMac> ocb)
{
  Ptr<Packet> vsc = Create<Packet> ();
  VendorSpecificActionHeader vsa;
  ocb->SendVsc (vsc, Mac48Address::GetBroadcast (), m_16093oi);
  std::cout << ocb->GetAddress() << " broadcast VSA with Wave OrganiaztionIdentifier" << std::endl;
}

void
Wifi80211pVsaExample::SendAuthen (Ptr<OcbWifiMac> ocb)
{
  Ptr<Packet> auth = Create<Packet> ();
  VendorSpecificActionHeader vsa;
  ocb->SendVsc (auth, Mac48Address::GetBroadcast (),m_authenoi);
  std::cout << ocb->GetAddress() << "broadcast VSA with Authen OrganiaztionIdentifier" << std::endl;
}


void
Wifi80211pVsaExample::DoRun (void)
{
  VscCallback vsccall = MakeCallback (&Wifi80211pVsaExample::GetWsaAndOi, this);
  VscCallback authencall = MakeCallback (&Wifi80211pVsaExample::GetAuthenAndOi, this);

  NodeContainer nodes;
  nodes.Create (2);
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());
  NqosWifipMacHelper wifi80211pMac = NqosWifipMacHelper::Default ();

  Wifi80211pHelper wifi80211p = Wifi80211pHelper::Default ();
  wifi80211p.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                      "DataMode", StringValue ("OfdmRate6MbpsBW10MHz"),
                                      "ControlMode",StringValue ("OfdmRate6MbpsBW10MHz"));
  wifi80211p.Install (wifiPhy, wifi80211pMac, nodes);

  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 0.0));
  positionAlloc->Add (Vector (5.0, 0.0, 0.0));
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  for (uint32_t i = 0; i < nodes.GetN (); i++)
    {
      Ptr<WifiNetDevice> device = DynamicCast<WifiNetDevice> (nodes.Get (i)->GetDevice (0));
      Ptr<OcbWifiMac> ocb = DynamicCast<OcbWifiMac> (device->GetMac ());
      ocb->AddReceiveVscCallback (m_16093oi, vsccall);
      ocb->AddReceiveVscCallback (m_authenoi, authencall);
      Simulator::Schedule (Seconds (i), &Wifi80211pVsaExample::SendWsa, this, ocb);
      Simulator::Schedule (Seconds (i + 0.5), &Wifi80211pVsaExample::SendAuthen, this, ocb);
    }
  Simulator::Stop (Seconds (10));
  Simulator::Run ();
  Simulator::Destroy ();
}

/**
 * In WaveNetDevice, users can only use StartVsa to send VSAs with different
 * Organization Identifiers, however according to the 1609.4 standard,
 * WaveNetDevice do not support receive  VSAs which Organization Identifier not
 * indicate WAVE. If users want to receive VSAs with other Organization Identifier
 * they should get MAC entities and register Organization Identifier by themselves.
 * Usage:
 * 1. create WaveNetDevice with wave helpers
 * 2. assign channel access for sending VSAs and receiver should also need to
 *    assign access for receiving VSAs.
 * 3. receiver should register WaveCallback by SetVsaReceiveCallback method
 * 4. sender will call StartVsa method of WaveNetDevice.
 *    then the registered callback of receiver will be called.
 *
 * note: if users still want to use WaveNetDevice to send and receive specific VSAs that
 * not belong to WAVE frames, he can (1) GetMac , (2) dynamic cast to OcbWifiMac,
 * (3) register specific OrganiaztionIdentifier, (4) send VSAs with this OrganiaztionIdentifier.
 */
class WaveVsaExample
{
public:
  WaveVsaExample (void);
  virtual ~WaveVsaExample (void);
  void DoRun (void);
private:
  void CreateWaveNodes (void);
  bool ReceiveVsa (Ptr<const Packet>,const Address &, uint32_t, uint32_t);
  NodeContainer nodes;
  NetDeviceContainer devices;
  Time firstVsaReceived;
  Time lastVsaReceived;
  uint32_t vsasReceived;
};
WaveVsaExample::WaveVsaExample (void)
  :  firstVsaReceived (),
    lastVsaReceived (),
    vsasReceived (0)
{

}
WaveVsaExample::~WaveVsaExample (void)
{

}
void
WaveVsaExample::CreateWaveNodes (void)
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

  Ptr<WaveNetDevice>  receiver = DynamicCast<WaveNetDevice> (devices.Get (1));
  receiver->SetVsaReceiveCallback (MakeCallback (&WaveVsaExample::ReceiveVsa, this));

  firstVsaReceived = lastVsaReceived = Time (0);
  vsasReceived = 0;
}
bool
WaveVsaExample::ReceiveVsa (Ptr<const Packet>,const Address &, uint32_t, uint32_t)
{
  if (vsasReceived == 0)
    {
      firstVsaReceived = Now ();
    }
  lastVsaReceived = Now ();
  vsasReceived++;
  return true;
}
void
WaveVsaExample::DoRun (void)
{
  // the peer address of VSA is receiver's unicast address, so even the repeat rate
  // of VsaInfo is 100 per 5s, the VSA frame is only sent once.
  {
    std::cout << "send VSAs with unicast address:" << std::endl;
    CreateWaveNodes ();
    Ptr<WaveNetDevice>  sender = DynamicCast<WaveNetDevice> (devices.Get (0));
    Ptr<WaveNetDevice>  receiver = DynamicCast<WaveNetDevice> (devices.Get (1));

    // Continuous access without immediate channel switch
    const SchInfo schInfo = SchInfo (SCH1, false, 0xff);

    Simulator::Schedule (Seconds (1.0),&WaveNetDevice::StartSch, sender, schInfo);
    Simulator::Schedule (Seconds (1.0),&WaveNetDevice::StartSch, receiver, schInfo);

    Ptr<Packet> packet = Create<Packet> (100);
    const Mac48Address dest = Mac48Address::ConvertFrom (receiver->GetAddress ());
    const VsaInfo vsaInfo = VsaInfo (dest, OrganizationIdentifier (), 0, packet, SCH1, 100, VSA_IN_ANYI);
    Simulator::Schedule (Seconds (1.1),&WaveNetDevice::StartVsa, sender, vsaInfo);

    Simulator::Stop (Seconds (10));
    Simulator::Run ();
    Simulator::Destroy ();

    std::cout << "first VSA is receive at time = " << firstVsaReceived.GetMilliSeconds () << "ms" << std::endl;
    std::cout << "last VSA is receive at time = " << lastVsaReceived.GetMilliSeconds () << "ms" << std::endl;
    std::cout << "the number of VSAs received is " << vsasReceived << std::endl;
    NS_ASSERT (firstVsaReceived == lastVsaReceived);
    NS_ASSERT (vsasReceived == 1);
    std::cout << "because the peer address of VSA is receiver's unicast address,"
              << "the VSA frame is only sent once."
              << std::endl;
  }

  // the peer address of VSA is broadcast address, and the repeat rate
  // of VsaInfo is 100 per 5s, the VSA frame will be sent repeatedly.
  {
    std::cout << "send VSAs with broadcast address:" << std::endl;

    CreateWaveNodes ();
    Ptr<WaveNetDevice>  sender = DynamicCast<WaveNetDevice> (devices.Get (0));
    Ptr<WaveNetDevice>  receiver = DynamicCast<WaveNetDevice> (devices.Get (1));

    // Continuous access without immediate channel switch
    const SchInfo schInfo = SchInfo (SCH1, false, 0xff);
    Simulator::Schedule (Seconds (1.0),&WaveNetDevice::StartSch, sender, schInfo);
    Simulator::Schedule (Seconds (1.0),&WaveNetDevice::StartSch, receiver, schInfo);

    Ptr<Packet> packet = Create<Packet> (100);
    const VsaInfo vsaInfo = VsaInfo (Mac48Address::GetBroadcast (), OrganizationIdentifier (), 0, packet, SCH1, 100, VSA_IN_ANYI);
    Simulator::Schedule (Seconds (1.1),&WaveNetDevice::StartVsa, sender, vsaInfo);

    Simulator::Schedule (Seconds (5.1),&WaveNetDevice::StopVsa, sender, SCH1);

    Simulator::Stop (Seconds (10));
    Simulator::Run ();
    Simulator::Destroy ();

    std::cout << "first VSA is receive at time = " << firstVsaReceived.GetMilliSeconds () << "ms" << std::endl;
    std::cout << "last VSA is receive at time = " << lastVsaReceived.GetMilliSeconds () << "ms" << std::endl;
    std::cout << "the number of VSAs received is " << vsasReceived << std::endl;
    uint32_t repeat = (vsasReceived + 0.0) / ((lastVsaReceived - firstVsaReceived).GetMilliSeconds ()) * 5000;
    std::cout << "the repeat rate of VSAs is " << repeat << " per 5s" << std::endl;
  }
}

int main (int argc, char *argv[])
{
  std::cout << "send VSAs with 802.11p device" << std::endl;
  Wifi80211pVsaExample wifi80211pSendVsa;
  wifi80211pSendVsa.DoRun ();
  std::cout << std::endl;
  std::cout << "send VSAs with wave device" << std::endl;
  WaveVsaExample waveSendVsa;
  waveSendVsa.DoRun ();
  return 0;
}
