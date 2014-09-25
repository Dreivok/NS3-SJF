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
#ifndef CHANNEL_COORDINATOR_H
#define CHANNEL_COORDINATOR_H
#include "ns3/nstime.h"
#include "ns3/object.h"
#include "ns3/event-id.h"
#include "ns3/simulator.h"

namespace ns3 {

/**
 * \brief receive notifications about channel coordination events.
 */
class ChannelCoordinationListener
{
public:
  virtual ~ChannelCoordinationListener (void);
  /**
   * \param duration the CCH access time (CCHI - GI) continues,
   * which is normally 46ms.
   */
  virtual void NotifyCchSlotStart (Time duration) = 0;
  /**
   * \param duration the SCH access time (SCHI - GI) continues,
   * which is normally 46ms.
   */
  virtual void NotifySchSlotStart (Time duration) = 0;
  /**
   * \param duration the time Guard access time (GI) continues, normally is 4ms
   * \param cchi indicate whether the guard slot is in the GI of CCHI or SCHI.
   */
  virtual void NotifyGuardSlotStart (Time duration, bool cchi) = 0;
};
/**
 *  ChannelCoordinator deals with channel coordination in data
 *  plane (see 1609.4 chapter 5.2)
 *
 *      <-----------SYNCI-----------> <-----------SYNCI----------->
 *          CCHI          SCHI             CCHI           SCHI
 *     |..************|..************|..************|..************|
 *      GI             GI             GI             GI
 *
 *  The relation among CCHI(CCH Interval), SCHI(SCH Interval), GI(Guard Interval), SYNCI(Sync Interval):
 *  1. they are all time durations, normally default CCHI is 50ms,
 *  default SCHI is 50ms, default GI is 4ms, default SYNCI is 100ms.
 *  2. Every UTC second shall be an integer number of sync interval, and the
 *  beginning of a sync interval shall align with beginning of UTC second;
 *  3. SYNCIis the sum of CCHI and SCHI.
 *  GI is the sum of SyncTolerance and MaxSwitchTime defined for
 *  multi-channel synchronization (see see 1609.4 chapter 6.2). And since the
 *  devices in simulation could be supposed to synchronize by "perfect GPS",
 *  here the synchronization mechanism will be not implemented.
 *  4. Although the real channel switch time of wifi phy layer is very fast, and the "ChannelSwitchDelay"
 *  of YansWifiPhy is 250 microseconds, here GI will be taken as virtual channel
 *  switch time of wave mac extension layer, so the wave device will be in busy state
 *  and could neither send nor receive.
 */
class ChannelCoordinator : public Object
{
public:
  static TypeId GetTypeId (void);
  ChannelCoordinator ();
  virtual ~ChannelCoordinator ();

  static Time GetDefaultCchInterval (void);
  static Time GetDefaultSchInterval (void);
  static Time GetDefaultSyncInterval (void);
  static Time GetDefaultGuardInterval (void);

  void SetCchInterval (Time cch);
  Time GetCchInterval (void) const;
  void SetSchInterval (Time sch);
  Time GetSchInterval (void) const;
  Time GetSyncInterval (void) const;
  void SetGuardInterval (Time guard);
  Time GetGuardInterval (void) const;

  /**
   * \param duration the future time after duration
   * \returns whether future time is in SCH Interval
   */
  bool IsSchInterval (Time duration = Seconds (0.0)) const;
  /**
   * \param duration the future time after duration
   * \returns whether future time is in CCH Interval
   */
  bool IsCchInterval (Time duration = Seconds (0.0)) const;
  /**
   * \param duration the future time after duration
   * \returns whether future time is in Guard Interval
   */
  bool IsGuardInterval (Time duration = Seconds (0.0)) const;
  /**
   * check whether current channel interval configuration is valid.
   */
  bool IsValidConfig (void) const;
  /**
   * \param duration the future time after duration
   * \returns the duration time to the next CCH interval
   * If current time is already in CCH interval, return 0;
   * note that the next CCH interval actually is the begin of Guard Interval
   */
  Time NeedTimeToCchInterval (Time duration = Seconds (0.0)) const;
  /**
   * \param duration the future time after duration
   * \returns the duration time to the next SCH interval
   * If current time is already in SCH interval, return 0;
   * note that the next SCH interval actually is the begin of Guard Interval
   */
  Time NeedTimeToSchInterval (Time duration = Seconds (0.0)) const;
  /**
   * \param duration the future time after duration
   * \returns the duration time to next Guard interval
   * If current time is already in Guard interval, return 0;
   */
  Time NeedTimeToGuardInterval (Time duration = Seconds (0.0)) const;
  /**
   * \param duration the future time after duration
   * \return the time in a Sync Interval of future time
   *  for example:
   *  SyncInterval = 100ms;
   *  Now = 5s20ms;
   *  duration = 50ms;
   *  then GetIntervalTime (duration) = 70ms.
   */
  Time GetIntervalTime (Time duration = Seconds (0.0)) const;
  /**
   * \param duration the future time after duration
   * \return the remain time in a Sync Interval of future time
   *  for example:
   *  SyncInterval = 100ms;
   *  Now = 5s20ms;
   *  duration = 50ms;
   *  then GetIntervalTime (duration) = 30ms.
   */
  Time GetRemainTime (Time duration = Seconds (0.0)) const;
  /**
   * \param listener the new listener pointer
   *
   * Add the input listener to the list of objects to be notified of
   * channel coordination events.
   * The listener object will be deleted and cleaned automatically
   * when ChannelCoordinator is released, so the caller should not delete
   * listener by itself.
   */
  void RegisterListener (ChannelCoordinationListener *listener);
  void UnregisterListener (ChannelCoordinationListener *listener);
  void UnregisterAllListeners (void);

private:
  virtual void DoDispose (void);
  virtual void DoInitialize (void);

  void StartChannelCoordination (void);
  void StopChannelCoordination (void);

  // when channel coordination is started by Start method, then NotifySchSlot,
  // NotifyCchSlot, and NotifyGuardSlot will be called repeatedly to generate
  // channel coordination events.
  void NotifySchSlot (void);
  void NotifyCchSlot (void);
  void NotifyGuardSlot (void);
  /**
   * get SCH channel access time which is SchInterval - GuardInterval, default 46ms
   */
  Time GetSchSlot (void) const;
  /**
   * get CCH channel access time which is SchInterval - GuardInterval, default 46ms
   */
  Time GetCchSlot (void) const;

  Time m_cchi;  // CchInterval
  Time m_schi;  // SchInterval
  Time m_gi;    // GuardInterval

  typedef std::vector<ChannelCoordinationListener *> Listeners;
  typedef std::vector<ChannelCoordinationListener *>::iterator ListenersI;

  Listeners m_listeners;

  uint32_t m_guardCount;
  EventId m_coordination;
};

}
#endif /* CHANNEL_COORDINATOR_H */
