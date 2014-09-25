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
#include "channel-manager.h"
#include "ns3/log.h"
#include "ns3/assert.h"

NS_LOG_COMPONENT_DEFINE ("ChannelManager");

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (ChannelManager);

TypeId
ChannelManager::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::ChannelManager")
    .SetParent<Object> ()
    .AddConstructor<ChannelManager> ()
  ;
  return tid;
}

ChannelManager::ChannelManager ()
{
  NS_LOG_FUNCTION (this);
  m_channels.push_back (new WaveChannel(SCH1));
  m_channels.push_back (new WaveChannel(SCH2));
  m_channels.push_back (new WaveChannel(SCH3));
  m_channels.push_back (new WaveChannel(CCH));
  m_channels.push_back (new WaveChannel(SCH4));
  m_channels.push_back (new WaveChannel(SCH5));
  m_channels.push_back (new WaveChannel(SCH6));
}

ChannelManager::~ChannelManager ()
{
  NS_LOG_FUNCTION (this);
  std::vector<WaveChannel *>::iterator i;
  for (i = m_channels.begin (); i != m_channels.end (); ++i)
    {
      delete (*i);
    }
  m_channels.clear ();
}

bool
ChannelManager::IsCch (uint32_t channelNumber)
{
  NS_LOG_FUNCTION_NOARGS ();
  return channelNumber == CCH;
}

bool
ChannelManager::IsSch (uint32_t channelNumber)
{
  NS_LOG_FUNCTION_NOARGS ();
  if (channelNumber < SCH1 || channelNumber > SCH6)
    {
      return false;
    }
  if (channelNumber % 2 == 1)
    {
      return false;
    }
  return (channelNumber != CCH);
}

bool
ChannelManager::IsWaveChannel (uint32_t channelNumber)
{
  NS_LOG_FUNCTION_NOARGS ();
  if (channelNumber < SCH1 || channelNumber > SCH6)
    {
	  return false;
    }
  if (channelNumber % 2 == 1)
    {
	  return false;
    }
  return true;
}

uint32_t
ChannelManager::GetIndex (uint32_t channelNumber)
{
  NS_LOG_FUNCTION (this << channelNumber);
  if (channelNumber < SCH1 || channelNumber > SCH6)
    {
      return CHANNELS_OF_WAVE;
    }
  if (channelNumber % 2 == 1)
    {
      return CHANNELS_OF_WAVE;
    }
  return (channelNumber - SCH1) / 2;
}

enum ChannelState
ChannelManager::GetState (uint32_t channelNumber)
{
  NS_LOG_FUNCTION (this << channelNumber);
  uint32_t index = GetIndex (channelNumber);
  return m_channels[index]->state;
}

void
ChannelManager::SetState (uint32_t channelNumber, enum ChannelState state)
{
  NS_LOG_FUNCTION (this << channelNumber);
  uint32_t index = GetIndex (channelNumber);
  m_channels[index]->state = state;
}

bool
ChannelManager::IsChannelActive (uint32_t channelNumber)
{
  NS_LOG_FUNCTION (this << channelNumber);
  return (GetState (channelNumber)) == CHANNEL_ACTIVE;
}

bool
ChannelManager::IsChannelInactive (uint32_t channelNumber)
{
  NS_LOG_FUNCTION (this << channelNumber);
  return (GetState (channelNumber)) == CHANNEL_INACTIVE;
}

bool
ChannelManager::IsChannelDead (uint32_t channelNumber)
{
  NS_LOG_FUNCTION (this << channelNumber);
  return (GetState (channelNumber)) == CHANNEL_DEAD;
}

uint32_t
ChannelManager::GetOperatingClass (uint32_t channelNumber)
{
  NS_LOG_FUNCTION (this << channelNumber);
  uint32_t index = GetIndex (channelNumber);
  return m_channels[index]->operatingClass;
}

bool
ChannelManager::IsAdapter (uint32_t channelNumber)
{
  NS_LOG_FUNCTION (this << channelNumber);
  uint32_t index = GetIndex (channelNumber);
  return m_channels[index]->adapter;
}

enum WaveDataRate
ChannelManager::GetDataRate (uint32_t channelNumber)
{
  uint32_t index = GetIndex (channelNumber);
  return m_channels[index]->dataRate;
}

uint32_t
ChannelManager::GetTxPowerLevel (uint32_t channelNumber)
{
  uint32_t index = GetIndex (channelNumber);
  return m_channels[index]->txPowerLevel;
}

} // namespace ns3
