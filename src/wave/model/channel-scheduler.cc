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
#include "channel-scheduler.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/wifi-mac-queue.h"
#include "ns3/wifi-mac-trailer.h"
#include "ns3/mac-low.h"
#include "ocb-wifi-mac.h"
#include "wave-net-device.h"

NS_LOG_COMPONENT_DEFINE ("ChannelScheduler");

namespace ns3 {

class CoordinationListener : public ChannelCoordinationListener
{
public:
  CoordinationListener (ChannelScheduler * scheduler)
    : m_scheduler (scheduler)
  {
  }
  virtual ~CoordinationListener ()
  {
  }
  virtual void NotifyCchSlotStart (Time duration)
  {
    m_scheduler->NotifyCchSlotStart (duration);
  }
  virtual void NotifySchSlotStart (Time duration)
  {
    m_scheduler->NotifySchSlotStart (duration);
  }
  virtual void NotifyGuardSlotStart (Time duration, bool cchi)
  {
    m_scheduler->NotifyGuardSlotStart (duration, cchi);
  }
private:
  ChannelScheduler * m_scheduler;
};

NS_OBJECT_ENSURE_REGISTERED (ChannelScheduler);

TypeId
ChannelScheduler::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::ChannelScheduler")
    .SetParent<Object> ()
    .AddConstructor<ChannelScheduler> ()
  ;
  return tid;
}

ChannelScheduler::ChannelScheduler ()
  : m_device (0),
    m_channelNumber (CCH),
    m_extend (255),
    m_channelAccess (DefaultCchAccess),
    m_coordinationListener (0)
{
  NS_LOG_FUNCTION (this);
}
ChannelScheduler::~ChannelScheduler ()
{
  NS_LOG_FUNCTION (this);
}

void
ChannelScheduler::DoInitialize (void)
{
  NS_LOG_FUNCTION (this);
  Ptr<WaveNetDevice> wave = DynamicCast<WaveNetDevice>(m_device);
  m_mac = DynamicCast<OcbWifiMac> (wave->GetMac ());
  m_phy = wave->GetPhy ();
  m_mac->SetWaveDevice (wave);
  AssignDefaultCchAccess ();
}

void
ChannelScheduler::DoDispose (void)
{
  NS_LOG_FUNCTION (this);

  // we just need set to 0,
  // WaveNetDevice will call DoDispose of m_manager and m_coordinator
  m_manager = 0;
  m_coordinator = 0;
  m_device = 0;
  delete m_coordinationListener;
  m_coordinationListener = 0;
}

void
ChannelScheduler::SetWaveDevice (Ptr<NetDevice> device)
{
  NS_LOG_FUNCTION (this << device);
  m_device = device;
}

void
ChannelScheduler::SetChannelManager (Ptr<ChannelManager> manager)
{
  NS_LOG_FUNCTION (this << manager);
  m_manager = manager;
}

void
ChannelScheduler::SetChannelCoodinator (Ptr<ChannelCoordinator> coordinator)
{
  NS_LOG_FUNCTION (this << coordinator);
  m_coordinator = coordinator;
  m_coordinationListener = new CoordinationListener (this);
  m_coordinator->RegisterListener (m_coordinationListener);
}

bool
ChannelScheduler::IsAccessAssigned (uint32_t channelNumber) const
{
  NS_LOG_FUNCTION (this << channelNumber);
  switch (m_channelAccess)
    {
    // continuous access is similar to extended access except extend operation
    case ContinuousAccess:
    case ExtendedAccess:
      return m_channelNumber == channelNumber;
    case AlternatingAccess:
      return (channelNumber == CCH) || (m_channelNumber == channelNumber);
    case DefaultCchAccess:
      return false;
    default:
      NS_FATAL_ERROR ("we could not get here");
      return false;
    }
}

bool
ChannelScheduler::IsAccessAssigned (void) const
{
  NS_LOG_FUNCTION (this);
  return (GetAccess () != DefaultCchAccess);
}

enum ChannelAccess
ChannelScheduler::GetAccess (void) const
{
  NS_LOG_FUNCTION (this);
  return m_channelAccess;
}

uint32_t
ChannelScheduler::GetChannel (void) const
{
  NS_LOG_FUNCTION (this);
  return m_channelNumber;
}

bool
ChannelScheduler::AssignAlternatingAccess (uint32_t channelNumber, bool immediate)
{
  NS_LOG_FUNCTION (this << channelNumber << immediate);
  uint32_t cn = channelNumber;
  if (cn == CCH)
    {
      return false;
    }

  // if channel access is already assigned for the same channel ,
  // we just need to return successfully
  if (m_channelAccess == AlternatingAccess && channelNumber == m_channelNumber)
    {
      return true;
    }

  // channel access is already assigned for other channel
  if (m_channelNumber != CCH)
    {
      return false;
    }

  // if we need immediately switch SCH in CCHI, or we are in SCHI now,
  // we switch to CCH channel.
  if ((immediate || m_coordinator->IsSchInterval ()))
    {
      m_manager->SetState (cn, CHANNEL_ACTIVE);
      m_manager->SetState (CCH, CHANNEL_INACTIVE);
      m_phy->SetChannelNumber (cn);
      m_mac->SwitchQueueToChannel (cn);

    }
  else
    {
      m_manager->SetState (CCH, CHANNEL_ACTIVE);
      m_manager->SetState (cn, CHANNEL_INACTIVE);
    }

  // when Alternating Access is assigned initially and current time is in Guard Interval,
  // the GuardSlotStart coordination event will miss unfortunately
  // so we will make medium busy for remain access time by hand.
  if (m_coordinator->IsGuardInterval ())
  {
	  Time remainSlot = m_coordinator->GetGuardInterval () - m_coordinator->GetIntervalTime ();
	  // see chapter 6.2.5 Sync tolerance
	  // a medium busy shall be declared during the guard interval.
	  m_mac->NotifyBusy (remainSlot);
  }

  m_channelNumber = cn;
  m_channelAccess = AlternatingAccess;

  return true;
}

bool
ChannelScheduler::AssignContinuousAccess (uint32_t channelNumber, bool immediate)
{
  NS_LOG_FUNCTION (this << channelNumber << immediate);
  uint32_t cn = channelNumber;
  // if channel access is already assigned for the same channel ,
  // we just need to return successfully
  if (m_channelAccess == ContinuousAccess && channelNumber == m_channelNumber)
    {
      return true;
    }

  // channel access is already assigned for other channel
  if (m_channelNumber != CCH)
    {
      return false;
    }

  bool switchNow = (ChannelManager::IsCch (cn) && m_coordinator->IsCchInterval ())
    || (ChannelManager::IsSch (cn) && m_coordinator->IsSchInterval ());
  if (immediate || switchNow)
    {
      m_phy->SetChannelNumber (cn);
      m_mac->SwitchQueueToChannel (cn);
      m_manager->SetState (CCH, CHANNEL_DEAD);
      m_manager->SetState (cn, CHANNEL_ACTIVE);
      m_channelNumber = cn;
      m_channelAccess = ContinuousAccess;
    }
  else
    {
      Time wait = ChannelManager::IsCch (cn) ? m_coordinator->NeedTimeToCchInterval () : m_coordinator->NeedTimeToSchInterval ();
      m_waitEvent = Simulator::Schedule (wait, &ChannelScheduler::AssignContinuousAccess, this, cn, false);
    }

  return true;
}

bool
ChannelScheduler::AssignExtendedAccess (uint32_t channelNumber, uint32_t extends, bool immediate)
{
  NS_LOG_FUNCTION (this << channelNumber << extends << immediate);
  uint32_t cn = channelNumber;

  // channel access is already for the same channel
  if ((m_channelAccess == ExtendedAccess) && (m_channelNumber == channelNumber) && (extends <= m_extend))
    {
      return true;
    }

  // channel access is already assigned for other channel
  if (m_channelNumber != CCH)
    {
      return false;
    }

  bool switchNow = (ChannelManager::IsCch (cn) && m_coordinator->IsCchInterval ())
    || (ChannelManager::IsSch (cn) && m_coordinator->IsSchInterval ());
  Time wait = ChannelManager::IsCch (cn) ? m_coordinator->NeedTimeToCchInterval () : m_coordinator->NeedTimeToSchInterval ();

  if (immediate || switchNow)
    {
      m_extend = extends;
      m_phy->SetChannelNumber (cn);
      m_mac->SwitchQueueToChannel (cn);
      m_manager->SetState (CCH, CHANNEL_DEAD);
      m_manager->SetState (cn, CHANNEL_ACTIVE);
      m_channelNumber = cn;
      m_channelAccess = ExtendedAccess;

      Time sync = m_coordinator->GetSyncInterval ();
      NS_ASSERT (extends != 0 && extends < 0xff);
      // the wait time to proper interval will not be calculated as extended time.
      Time extendedDuration = wait + MilliSeconds (extends * sync.GetMilliSeconds ());
      // after end_duration time, ChannelScheduler will release channel access automatically
      m_extendEvent = Simulator::Schedule (extendedDuration, &ChannelScheduler::Release, this, cn);
    }
  else
    {
      m_waitEvent = Simulator::Schedule (wait, &ChannelScheduler::AssignExtendedAccess, this, cn, extends, false);
    }
  return true;
}

void
ChannelScheduler::AssignDefaultCchAccess (void)
{
   NS_LOG_FUNCTION (this);
   m_manager->SetState (CCH, CHANNEL_ACTIVE);
   m_phy->SetChannelNumber (CCH);
   m_mac->SwitchQueueToChannel (CCH);
   m_channelNumber = CCH;
}

bool
ChannelScheduler::StartSch (const SchInfo & schInfo)
{
  NS_LOG_FUNCTION (this << &schInfo);

  uint32_t cn = schInfo.channelNumber;
  uint8_t extends = schInfo.extendedAccess;
  // now only support channel continuous access and extended access
  if (extends == EXTENDS_CONTINUOUS)
    {
      return AssignContinuousAccess (cn, schInfo.immediateAccess);
    }
  else if (extends == EXTENDS_ALTERNATING)
    {
      return AssignAlternatingAccess (cn, schInfo.immediateAccess);
    }
  else
    {
      return AssignExtendedAccess (cn, schInfo.extendedAccess, schInfo.immediateAccess);
    }
}

void
ChannelScheduler::Release (uint32_t channelNumber)
{
  NS_LOG_FUNCTION (this << channelNumber);
  uint32_t cn = channelNumber;

  if (!IsAccessAssigned (cn))
    {
      NS_LOG_DEBUG ("the channel access of CH =" << cn << " "
                    " has already been released.");
      return;
    }

  switch (m_channelAccess)
    {
    // continuous access is similar to extended access except extend operation
    // here we can release continuous access as releasing extended access
    case ContinuousAccess:
    case ExtendedAccess:
      NS_ASSERT (m_channelNumber != CCH);
      m_manager->SetState (m_channelNumber, CHANNEL_DEAD);
      m_extend = 0;
      m_waitEvent.Cancel ();
      m_extendEvent.Cancel ();
      break;
    case AlternatingAccess:
      NS_ASSERT (m_channelNumber != CCH);
      m_manager->SetState (m_channelNumber, CHANNEL_DEAD);
      break;
    case DefaultCchAccess:
      break;
    default:
      NS_FATAL_ERROR ("cannot get here");
      break;
    }

  AssignDefaultCchAccess();
}


void
ChannelScheduler::NotifyCchSlotStart (Time duration)
{
  NS_LOG_FUNCTION (this << duration);

  if (m_channelAccess != AlternatingAccess)
    return;
  m_manager->SetState (m_channelNumber, CHANNEL_INACTIVE);
  m_manager->SetState (CCH, CHANNEL_ACTIVE);
  m_mac->QueueStartAccess ();
}

void
ChannelScheduler::NotifySchSlotStart (Time duration)
{
  NS_LOG_FUNCTION (this << duration);

  if (m_channelAccess != AlternatingAccess)
    return;
  m_manager->SetState (m_channelNumber, CHANNEL_ACTIVE);
  m_manager->SetState (CCH, CHANNEL_INACTIVE);
  m_mac->QueueStartAccess ();
}

void
ChannelScheduler::NotifyGuardSlotStart (Time duration, bool cchi)
{
  NS_LOG_FUNCTION (this << duration << cchi);

  if (m_channelAccess != AlternatingAccess)
    return;

  if (cchi)
    {
      NS_ASSERT (m_coordinator->IsCchInterval () && m_coordinator->IsGuardInterval ());
      m_phy->SetChannelNumber (CCH);
      m_mac->SwitchQueueToChannel (CCH);
    }
  else
    {
      m_phy->SetChannelNumber (m_channelNumber);
      m_mac->SwitchQueueToChannel (m_channelNumber);
    }
  // see chapter 6.2.5 Sync tolerance
  // a medium busy shall be declared during the guard interval.
  m_mac->NotifyBusy (duration);
}
} // namespace ns3
