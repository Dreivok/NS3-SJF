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
#include "ns3/assert.h"
#include "ns3/simulator.h"
#include "ns3/qos-tag.h"

#include "vsa-repeater.h"
#include "ocb-wifi-mac.h"
#include "data-tx-tag.h"
#include "wave-edca-txop-n.h"
#include "wave-net-device.h"
#include "channel-coordinator.h"
#include "channel-manager.h"

NS_LOG_COMPONENT_DEFINE ("VsaRepeater");

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (VsaRepeater);

TypeId
VsaRepeater::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::VsaRepeater")
    .SetParent<Object> ()
    .AddConstructor<VsaRepeater> ()
  ;
  return tid;
}

VsaRepeater::VsaRepeater (void)
  : m_device (0)
{
}

VsaRepeater::~VsaRepeater (void)
{

}

void
VsaRepeater::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  RemoveAll ();
}

void
VsaRepeater::SetWaveDevice (Ptr<NetDevice> device)
{
  NS_LOG_FUNCTION (this << device);
  m_device = device;
}

void
VsaRepeater::SendVsa (const VsaInfo & vsaInfo)
{
  NS_LOG_FUNCTION (this << &vsaInfo);
  OrganizationIdentifier oi;
  if (vsaInfo.oi.IsNull ())
    {
	  // refer to 1609.4-2010 chapter 6.4.1.1
      uint8_t oibytes[5] = {0x00, 0x50, 0xC2, 0x4A, 0x40};
      oibytes[4] |= (vsaInfo.managementId & 0x0f);
      oi = OrganizationIdentifier (oibytes, 5);
    }
  else
    {
      oi = vsaInfo.oi;
    }

  Ptr<Packet> p = vsaInfo.vsc;

  // refer to 1609.4-2010 chapter 5.4.1
  // Management frames are assigned the highest AC (AC_VO).
  QosTag qosTag (7);
  p->AddPacketTag (qosTag);
  ChannelTag channelTag (vsaInfo.channelNumber);
  p->AddPacketTag (channelTag);

  // if destination MAC address indicates a unicast address,
  // only single VSA frame is sent, and the repeat rate is ignored;
  // or repeat rate is 0, the VSA frame is sent once.
  if (vsaInfo.peer.IsGroup () && (vsaInfo.repeatRate != 0))
    {
      VsaWork *vsa = new VsaWork ();
      // XXX REDO, it is a bad implementation to change enum to uint32_t,
      // however the compiler that not supports c++11 cannot forward declare enum
      // I need to find a better way.
      vsa->sentInterval = vsaInfo.sendInterval;
      vsa->channelNumber = vsaInfo.channelNumber;
      vsa->peer = vsaInfo.peer;
      vsa->repeatPeriod = MilliSeconds (VSA_REPEAT_PERIOD * 1000 / vsaInfo.repeatRate);
      vsa->vsc = vsaInfo.vsc->Copy ();
      vsa->oi = oi;
      vsa->repeat =  Simulator::Schedule (vsa->repeatPeriod, &VsaRepeater::DoRepeat, this, vsa);
      m_vsas.push_back (vsa);
    }
  DoSendVsa (vsaInfo.sendInterval, vsaInfo.channelNumber, p, oi, vsaInfo.peer);
}

void
VsaRepeater::DoRepeat (VsaWork *vsa)
{
  NS_LOG_FUNCTION (this << vsa);
  vsa->repeat =  Simulator::Schedule (vsa->repeatPeriod, &VsaRepeater::DoRepeat, this, vsa);
  DoSendVsa (vsa->sentInterval, vsa->channelNumber, vsa->vsc->Copy (), vsa->oi, vsa->peer);
}

/**
 * The Channel Interval indicating which channel interval the transmission will occur on.
 * However making the VSA transmitted strictly in required channel interval is hard to achieve.
 * The current channel access type, current channel internal and the VSA transmit request should
 * be considered. For example, if current channel is assigned “Alternating Access” and high layer
 * wants VSA transmitted in CCHI, but when packet is dequeued from the internal queue of MAC layer
 * in SCHI and can contend for channel access, how to deal with this case? Reinserting into the
 * head of the queue and dequeuing the second packet is not a good choice, because it will require
 * queue traversal. Reinserting the packet into the tail of the queue is worse, because it will
 * make queue not in order. And both solutions may affect many MAC classes of Wifi module.
 * So current design is to guarantee that packets can be inserted into the queue strictly in
 * channel interval required by high layer. This solution will result in high probability that
 * packets will be sent in channel interval as high layer wants, while some packet may be sent
 * practically in other channel interval that not follows the requirement of high layer
 * due to queue delay of MAC layer.
 */
void
VsaRepeater::DoSendVsa (uint32_t interval, uint32_t channel,
                                  Ptr<Packet> packet, OrganizationIdentifier oi, Mac48Address peer)
{
  NS_LOG_FUNCTION (this << interval << channel << packet << oi << peer);
  NS_ASSERT (interval < 4);
  NS_ASSERT (m_device != 0);
  Ptr<WaveNetDevice> wave = DynamicCast<WaveNetDevice>(m_device);
  Ptr<ChannelCoordinator> coordinator = wave->GetChannelCoordinator ();
  Ptr<ChannelManager> manager = wave->GetChannelManager ();

  if (manager->IsChannelDead (channel))
    {
	  NS_LOG_DEBUG ("no channel access is assigned for channel " << channel << ", so VSA cannot be sent");
      return;
    }
  // if request is for transmitting in SCH Interval (or CCH Interval),
  // but now is not in SCH Interval (or CCH Interval) and , then we
  // will wait some time to send this vendor specific content packet.
  // if request is for transmitting in any channel interval, then we
  // send packets immediately.
  if (interval == VSA_IN_SCHI)    // SCH Interval
    {
      Time wait = coordinator->NeedTimeToSchInterval ();
      if (wait == Seconds (0))
        {
          Simulator::Schedule (wait, &VsaRepeater::DoSendVsa, this,
                               interval, channel, packet, oi, peer);
          NS_LOG_DEBUG ("since VSA is sent in SCHI and current is not in SCHI, the transmission will wait " << wait.GetMilliSeconds() << "ms");
          return;
        }
      NS_LOG_DEBUG ("since VSA is sent in SCHI and current is in SCHI, the VSA will be queued immediately");
    }
  else if (interval == VSA_IN_CCHI)  // CCH Interval
    {
      Time wait = coordinator->NeedTimeToCchInterval ();
      if (wait == Seconds (0))
        {
          Simulator::Schedule (wait, &VsaRepeater::DoSendVsa, this,
                               interval, channel, packet, oi, peer);
          NS_LOG_DEBUG ("since VSA is sent in CCHI and current is not in CCHI, the transmission will wait " << wait.GetMilliSeconds() << "ms");
          return;
        }
      NS_LOG_DEBUG ("since VSA is sent in CCHI and current is in CCHI, the VSA will be queued immediately");
    }
  else if (interval == VSA_IN_ANYI)  // both channel interval
    {
	  NS_LOG_DEBUG ("since VSA is can be sent in both channel interval, the VSA will be queue immediately");
    }

  WifiMode datarate = wave->GetPhy ()->GetMode (manager->GetDataRate (channel));
  WifiTxVector txVector;
  txVector.SetTxPowerLevel (manager->GetTxPowerLevel (channel));
  txVector.SetMode (datarate);
  DataTxVectorTag tag = DataTxVectorTag (txVector, manager->IsAdapter (channel));
  packet->AddPacketTag (tag);

  Ptr<WifiMac> mac = wave->GetMac ();
  Ptr<OcbWifiMac> ocbmac = DynamicCast<OcbWifiMac> (mac);
  NS_ASSERT (ocbmac != 0);
  ocbmac->SendVsc (packet, peer, oi);
}

void
VsaRepeater::RemoveAll (void)
{
  NS_LOG_FUNCTION (this);
  for (std::vector<VsaWork *>::iterator i = m_vsas.begin ();
	    i != m_vsas.end (); ++i)
	 {
	  if (!(*i)->repeat.IsExpired ())
	    {
		  (*i)->repeat.Cancel ();
	    }
      (*i)->vsc = 0;
      delete (*i);
	 }
  m_vsas.clear ();
}

void
VsaRepeater::RemoveByChannel (uint32_t channelNumber)
{
  NS_LOG_FUNCTION (this << channelNumber);
  for (std::vector<VsaWork *>::iterator i = m_vsas.begin ();
       i != m_vsas.end (); )
    {
      if ((*i)->channelNumber == channelNumber)
        {
          if (!(*i)->repeat.IsExpired ())
            {
              (*i)->repeat.Cancel ();
            }
          (*i)->vsc = 0;
          delete (*i);
          i = m_vsas.erase (i);
        }
      else
        {
          ++i;
        }
    }
}


void
VsaRepeater::RemoveByOrganizationIdentifier (OrganizationIdentifier oi)
{
  NS_LOG_FUNCTION (this << oi);
  for (std::vector<VsaWork *>::iterator i = m_vsas.begin ();
		  i != m_vsas.end (); )
	 {
	  if ((*i)->oi == oi)
	    {
		  if (!(*i)->repeat.IsExpired ())
		    {
			  (*i)->repeat.Cancel ();
		    }
		  	  (*i)->vsc = 0;
		  delete (*i);
		  i = m_vsas.erase (i);
	  }
	  else
	  {
		  ++i;
	  }
	 }
}

} // namespace ns3
