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
#ifndef WAVE_EDCA_TXOP_N_H
#define WAVE_EDCA_TXOP_N_H


#include "ns3/edca-txop-n.h"
#include "ns3/wifi-mac-queue.h"
#include <map>

namespace ns3 {

class ChannelTag : public Tag
{
public:
  ChannelTag (void) : m_channel (0)
  {
  }
  ChannelTag (uint32_t channel) : m_channel (channel)
  {
  }
  virtual ~ChannelTag (void)
  {
  }

  void SetChannelNumber (uint32_t channel)
  {
    m_channel = channel;
  }
  uint32_t GetChannelNumber (void) const
  {
    return m_channel;
  }

  static TypeId GetTypeId (void)
  {
    static TypeId tid = TypeId ("ns3::ChannelTag")
      .SetParent<Tag> ()
      .AddConstructor<ChannelTag> ()
    ;
    return tid;
  }
  virtual TypeId GetInstanceTypeId (void) const
  {
    return GetTypeId ();
  }
  virtual uint32_t GetSerializedSize (void) const
  {
    return sizeof (uint32_t);
  }
  virtual void Serialize (TagBuffer i) const
  {
    i.WriteU32 (m_channel);
  }
  virtual void Deserialize (TagBuffer i)
  {
    m_channel = i.ReadU32 ();
  }
  virtual void Print (std::ostream &os) const
  {
    os << "channel=" << m_channel;
  }

private:
  uint32_t m_channel;
};

/**
 * This class provides three features required by alternating access
 * 1. chapter 6.2.5 tolerance
 * "at the beginning of a guard interval, the MAC activities on the
 * previous channel may be suspend"
 * the channel switch event notified by phy class will cause mac classes
 * reset, and this class will override to not flush previous mac queue.
 * instead switch to another mac queue for next channel.
 * 2. Annex C: avoiding transmission at scheduled guard intervals
 * "the MAC issues a PLME-TXTIME.request and the PHY returns
 *  a PLME-TXTIME.confire with required transmit time"
 *  A little different from operation declared by the standard, this class
 *  will calculate transmit time by considering many factors like RTS/CTS
 *  time, ACK time rather than the time of one MSDU frame.
 * 3. chapter Channel routing
 * " the MAC shall route the packet to a proper queue corresponding to the
 * Channel Identifier and priority"
 * since wifi module only supports priority queue by QosTag, here we use
 * ChannelTag to find the final queue.
 */
class WaveEdcaTxopN : public EdcaTxopN
{
public:
  static TypeId GetTypeId (void);
  WaveEdcaTxopN ();
  virtual ~WaveEdcaTxopN ();

  virtual void Queue (Ptr<const Packet> packet, const WifiMacHeader &hdr);

  /**
    * When a channel switching occurs, the mac queue will not flush
    * if channel access is alternating access.
    */
  virtual void NotifyChannelSwitching (void);

  void SwitchToChannel (uint32_t channelNumber);

private:
  virtual void DoDispose (void);
  virtual void DoInitialize (void);

  typedef std::map<uint32_t, Ptr<WifiMacQueue> > WaveQueues;
  typedef std::map<uint32_t,Ptr<WifiMacQueue> >::iterator WaveQueuesI;
  WaveQueues m_queues;
};

}  // namespace ns3

#endif /* WAVE_EDCA_TXOP_N_H */

