/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2006, 2009 INRIA
 * Copyright (c) 2009 MIRKO BANCHI
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
 * Author: Mirko Banchi <mk.banchi@gmail.com>
 * Author: Junling Bu <linlinjavaer@gmail.com>
 */
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/wifi-mac-queue.h"
#include "ns3/wifi-tx-vector.h"
#include "ns3/wifi-mac-trailer.h"
#include "ns3/object-map.h"
#include <map>
#include "channel-manager.h"
#include "wave-edca-txop-n.h"

NS_LOG_COMPONENT_DEFINE ("WaveEdcaTxopN");

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (WaveEdcaTxopN);

TypeId
WaveEdcaTxopN::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::WaveEdcaTxopN")
    .SetParent<EdcaTxopN> ()
    .AddConstructor<WaveEdcaTxopN> ()
    .AddAttribute ("Queues", "The WifiMacQueue object",
    				ObjectMapValue (),
    				MakeObjectMapAccessor (&WaveEdcaTxopN::m_queues),
    				MakeObjectMapChecker<WifiMacQueue> ())
  ;
  return tid;
}

WaveEdcaTxopN::WaveEdcaTxopN ()
{
  NS_LOG_FUNCTION (this);
}

WaveEdcaTxopN::~WaveEdcaTxopN ()
{
  NS_LOG_FUNCTION (this);
}
void
WaveEdcaTxopN::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  m_queues.clear ();
  EdcaTxopN::DoDispose ();
}
void
WaveEdcaTxopN::DoInitialize (void)
{
  NS_LOG_FUNCTION (this);
  EdcaTxopN::DoInitialize ();
  m_queues.insert (std::make_pair (CCH, CreateObject<WifiMacQueue> ()));
  m_queue = m_queues.find (CCH)->second;
  m_baManager->SetQueue (m_queue);
  m_baManager->SetMaxPacketDelay (m_queue->GetMaxDelay ());
}

void
WaveEdcaTxopN::NotifyChannelSwitching (void)
{
  NS_LOG_FUNCTION (this);

  // In EdcaTxopN of WifiNetDevice, when channel begins to switch,
  // the internal MAC queue and current pending packet will be released.
  // Here in WaveEdcaTxopN of WaveNetDevice, when channel begins
  // to switch, the current internal MAC queue will not be released.
  // And the pending packet will be reinserted into the front of the queue
  // no matter currently which channel access is.
  if (m_currentPacket != 0)
  {
	  EdcaTxopN::GetQueue()->PushFront (m_currentPacket, m_currentHdr);
	  m_currentPacket = 0;
  }
}

void
WaveEdcaTxopN::Queue (Ptr<const Packet> packet, const WifiMacHeader &hdr)
{
  NS_LOG_FUNCTION (this << packet << hdr);
  ChannelTag tag;
  bool result;
  uint32_t channelNumber;
  result = ConstCast<Packet> (packet)->RemovePacketTag (tag);
  if (result)
    {
      channelNumber = tag.GetChannelNumber ();
    }
  else
    {
      NS_FATAL_ERROR ("In WAVE, we should queue packet by qos and channel");
    }

  WaveQueuesI i = m_queues.find (channelNumber);
  if (i == m_queues.end ())
    {
	  std::pair<WaveQueuesI, bool> r = m_queues.insert (std::make_pair (channelNumber, CreateObject<WifiMacQueue> ()));
	  NS_ASSERT (r.second == true);
      i = r.first;
    }

  WifiMacTrailer fcs;
  uint32_t fullPacketSize = hdr.GetSerializedSize () + packet->GetSize () + fcs.GetSerializedSize ();
  m_stationManager->PrepareForQueue (hdr.GetAddr1 (), &hdr, packet, fullPacketSize);
  i->second->Enqueue (packet, hdr);

  if (m_queue->GetSize() != 0)
    {
      StartAccessIfNeeded ();
    }
}

void
WaveEdcaTxopN::SwitchToChannel (uint32_t channelNumber)
{
  NS_LOG_FUNCTION (this);
  std::map<uint32_t,Ptr<WifiMacQueue> >::iterator i = m_queues.find (channelNumber);
  if (i == m_queues.end ())
    {
      m_queues.insert (std::make_pair (channelNumber, CreateObject<WifiMacQueue> ()));
      i = m_queues.find (channelNumber);
    }
  NS_ASSERT (i != m_queues.end ());
  m_queue = i->second;
  m_baManager->SetQueue (m_queue);
  m_baManager->SetMaxPacketDelay (m_queue->GetMaxDelay ());

  m_currentPacket = 0;
}

} // namespace ns3
