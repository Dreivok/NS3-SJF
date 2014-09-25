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
#include "channel-coordinator.h"
#include "ns3/log.h"
#include "ns3/nstime.h"
#include "ns3/simulator.h"

NS_LOG_COMPONENT_DEFINE ("ChannelCoordinator");

namespace ns3 {

/****************************************************************
 *       This destructor is needed.
 ****************************************************************/

ChannelCoordinationListener::~ChannelCoordinationListener (void)
{
}

/****************************************************************/

NS_OBJECT_ENSURE_REGISTERED (ChannelCoordinator);

TypeId
ChannelCoordinator::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::ChannelCoordinator")
    .SetParent<Object> ()
    .AddConstructor<ChannelCoordinator> ()
    .AddAttribute ("CchInterval", "CCH Interval, default value is 50ms.",
                   TimeValue (GetDefaultCchInterval ()),
                   MakeTimeAccessor (&ChannelCoordinator::m_cchi),
                   MakeTimeChecker ())
    .AddAttribute ("SchInterval", "SCH Interval, default value is 50ms.",
                   TimeValue (GetDefaultSchInterval ()),
                   MakeTimeAccessor (&ChannelCoordinator::m_schi),
                   MakeTimeChecker ())
    .AddAttribute ("GuardInterval", "Guard Interval, default value is 4ms.",
    		       TimeValue (GetDefaultGuardInterval ()),
    		       MakeTimeAccessor (&ChannelCoordinator::m_gi),
    		       MakeTimeChecker ())
  ;
  return tid;
}

ChannelCoordinator::ChannelCoordinator ()
  : m_guardCount (0)
{
  NS_LOG_FUNCTION (this);
}

ChannelCoordinator::~ChannelCoordinator ()
{
  NS_LOG_FUNCTION (this);
}

void
ChannelCoordinator::DoInitialize (void)
{
  NS_LOG_FUNCTION (this);
  StartChannelCoordination ();
}

void
ChannelCoordinator::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  StopChannelCoordination ();
  UnregisterAllListeners ();
}


Time
ChannelCoordinator::GetDefaultCchInterval (void)
{
  NS_LOG_FUNCTION_NOARGS ();
  const static uint8_t DEFAULT_CCH_INTERVAL = 50;
  return MilliSeconds (DEFAULT_CCH_INTERVAL);
}

Time
ChannelCoordinator::GetDefaultSchInterval (void)
{
  NS_LOG_FUNCTION_NOARGS ();
  const static uint8_t DEFAULT_SCH_INTERVAL = 50;
  return MilliSeconds (DEFAULT_SCH_INTERVAL);
}

Time
ChannelCoordinator::GetDefaultGuardInterval (void)
{
  NS_LOG_FUNCTION_NOARGS ();
  // refer to Annex H
  const static uint8_t SyncTolerance = 2;
  const static uint8_t MaxChSwitchTime  = 2;
  const static uint8_t DEFAULT_GUARD_INTERVAL = SyncTolerance + MaxChSwitchTime;
  return MilliSeconds (DEFAULT_GUARD_INTERVAL);
}

void
ChannelCoordinator::SetCchInterval (Time cchInterval)
{
  NS_LOG_FUNCTION (this << cchInterval);
  m_cchi = cchInterval;
}

Time
ChannelCoordinator::GetCchInterval (void) const
{
  NS_LOG_FUNCTION (this);
  return m_cchi;
}

void
ChannelCoordinator::SetSchInterval (Time schInterval)
{
  NS_LOG_FUNCTION (this << schInterval);
  m_schi = schInterval;
}

Time
ChannelCoordinator::GetSchInterval (void) const
{
  NS_LOG_FUNCTION (this);
  return m_schi;
}

Time
ChannelCoordinator::GetSyncInterval (void) const
{
  NS_LOG_FUNCTION (this);
  return GetCchInterval () + GetSchInterval ();
}

void
ChannelCoordinator::SetGuardInterval (Time guard)
{
  NS_LOG_FUNCTION (this);
  m_gi =  guard;
}

Time
ChannelCoordinator::GetGuardInterval (void) const
{
  NS_LOG_FUNCTION (this);
  return m_gi;
}


 Time
ChannelCoordinator::GetSchSlot (void) const
{
  NS_LOG_FUNCTION (this);
  return m_schi - m_gi;
}

Time
ChannelCoordinator::GetCchSlot (void) const
{
  NS_LOG_FUNCTION (this);
  return m_cchi - m_gi;
}

bool
ChannelCoordinator::IsCchInterval (Time duration) const
{
  NS_LOG_FUNCTION (this << duration);
  Time future = GetIntervalTime (duration);
  return (future < m_cchi);
}

bool
ChannelCoordinator::IsSchInterval (Time duration) const
{
  NS_LOG_FUNCTION (this << duration);
  return !IsCchInterval (duration);
}

bool
ChannelCoordinator::IsGuardInterval (Time duration) const
{
  NS_LOG_FUNCTION (this << duration);
  Time future = GetIntervalTime (duration);
  // the interval is either in CchInterval or SchInterval
  Time interval = future < m_cchi ? future : future - m_cchi;
  return interval < m_gi;
}

bool
ChannelCoordinator::IsValidConfig (void) const
{
  NS_LOG_FUNCTION (this);
  Time sync = GetSyncInterval ();
  // 1000 is 1000ms which is one UTC second
  if ((1000 % sync.GetMilliSeconds ()) != 0)
    {
	  NS_LOG_WARN ("every UTC second shall be an integer number of SyncInterval");
	  return false;
    }
  if ( GetCchInterval() <= GetGuardInterval ())
    {
	  NS_LOG_WARN ("CCH Interval should be large than GuardInterval");
	  return false;
    }
  if ( GetSchInterval() <= GetGuardInterval ())
    {
	  NS_LOG_WARN ("SCH Interval should be large than GuardInterval");
	  return false;
    }
  // here we may need check whether CCHI, SCHI and GI is 0,
  // however maybe users want to set them 0 in some cases.
  // at least, GI should be larger than real channel switch time of PHY layer.
  return true;
}

Time
ChannelCoordinator::NeedTimeToCchInterval (Time duration) const
{
  NS_LOG_FUNCTION (this << duration);
  if (IsCchInterval (duration))
    {
      return MilliSeconds (0);
    }
  return GetSyncInterval () - GetIntervalTime (duration);
}

Time
ChannelCoordinator::NeedTimeToSchInterval (Time duration) const
{
  NS_LOG_FUNCTION (this << duration);
  if (IsSchInterval (duration))
    {
      return MilliSeconds (0);
    }
  return GetCchInterval () - GetIntervalTime (duration);
}

Time
ChannelCoordinator::NeedTimeToGuardInterval (Time duration) const
{
  NS_LOG_FUNCTION (this << duration);
  if (IsGuardInterval (duration))
    {
	  return MilliSeconds (0);
    }
  if (IsCchInterval (duration))
    {
      return (GetCchInterval () - GetIntervalTime (duration));
    }
  return (GetSyncInterval () - GetIntervalTime (duration));
}

Time
ChannelCoordinator::GetIntervalTime (Time duration) const
{
  NS_LOG_FUNCTION (this << duration);
  Time future = Now () + duration;
  Time sync = GetSyncInterval ();
  uint32_t n = future.GetMilliSeconds () / sync.GetMilliSeconds ();
  return future - MilliSeconds (n * sync.GetMilliSeconds ());
}

Time
ChannelCoordinator::GetRemainTime (Time duration) const
{
  NS_LOG_FUNCTION (this << duration);
  return GetSyncInterval () - GetIntervalTime (duration);
}

void
ChannelCoordinator::RegisterListener (ChannelCoordinationListener *listener)
{
  NS_LOG_FUNCTION (this << listener);
  NS_ASSERT (listener != 0);
  m_listeners.push_back (listener);
}

void
ChannelCoordinator::UnregisterListener (ChannelCoordinationListener *listener)
{
  NS_LOG_FUNCTION (this << listener);
  for (ListenersI i = m_listeners.begin (); i != m_listeners.end (); ++i)
    {
	  if ((*i) == listener)
	    {
		  m_listeners.erase (i);
		  break;
	    }
    }
}

void
ChannelCoordinator::UnregisterAllListeners (void)
{
  NS_LOG_FUNCTION (this);
  m_listeners.clear ();
}

void
ChannelCoordinator::StartChannelCoordination (void)
{
  NS_LOG_FUNCTION (this);
  Time now = GetIntervalTime ();
  if (now != MilliSeconds (0))
    {
	  // although we can make events started from other time such as SshGI,
	  // we recommend strongly the CchGI is better and required
	  // and it could also follow the standard better
	  NS_FATAL_ERROR ("we should start from CchGI,(CCHI-CchGI), SshGI, (SCHI-SshGI)");
    }

  m_guardCount = 0;
  NotifyGuardSlot ();
}

void
ChannelCoordinator::StopChannelCoordination (void)
{
  if (!m_coordination.IsExpired ())
    {
	  m_coordination.Cancel ();
    }
  m_guardCount = 0;
}

void
ChannelCoordinator::NotifySchSlot (void)
{
  NS_LOG_FUNCTION (this);
  m_coordination = Simulator::Schedule (GetSchSlot (), &ChannelCoordinator::NotifyGuardSlot, this);
  for (ListenersI i = m_listeners.begin (); i != m_listeners.end (); ++i)
    {
      (*i)->NotifySchSlotStart (GetSchSlot ());
    }
}

void
ChannelCoordinator::NotifyCchSlot (void)
{
  NS_LOG_FUNCTION (this);
  m_coordination = Simulator::Schedule (GetCchSlot (), &ChannelCoordinator::NotifyGuardSlot, this);
  for (ListenersI i = m_listeners.begin (); i != m_listeners.end (); ++i)
    {
      (*i)->NotifyCchSlotStart (GetCchSlot ());
    }
}

void
ChannelCoordinator::NotifyGuardSlot (void)
{
  NS_LOG_FUNCTION (this);
  Time guardSlot = GetGuardInterval ();
  bool inCchi = ((m_guardCount % 2) == 0);
  if (inCchi)
    {
      m_coordination = Simulator::Schedule (guardSlot, &ChannelCoordinator::NotifyCchSlot, this);
    }
  else
    {
      m_coordination = Simulator::Schedule (guardSlot, &ChannelCoordinator::NotifySchSlot, this);
    }
  for (ListenersI i = m_listeners.begin (); i != m_listeners.end (); ++i)
    {
      (*i)->NotifyGuardSlotStart (guardSlot, inCchi);
    }
  m_guardCount++;
}

} // namespace ns3
