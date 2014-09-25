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
#ifndef CHANNEL_MANAGER_H
#define CHANNEL_MANAGER_H
#include <vector>
#include <stdint.h>
#include "ns3/ptr.h"
#include "ns3/wifi-mac.h"

namespace ns3 {

/**
 * see 1609.4-2010 Annex H
 * default System characteristic values
 */
const uint32_t DEFAULT_CCH_OPERATING_CLASS = 17;
const uint32_t DEFAULT_CCH_CHANNEL_NUMBER = 178;

/**
 * WAVE channels
 * channel number  | 172 | 174 | 176 | 178 | 180 | 182 | 184 |
 * channel bandwidth  10MHz 10MHz 10MHz 10MHz 10MHz 10MHz 10MHz
 * channel name      SCH1  SCH2  SCH3  CCH   SCH4  SCH5  SCH6
 *
 * now we not support 20MHz channel number:
 * channel 175 : combine channel 174 and 176
 * channel 181 : combine channel 180 and 182
 */
#define CH172 172
#define CH174 174
#define CH176 176
#define CH178 178
#define CH180 180
#define CH182 182
#define CH184 184
#define CHANNELS_OF_WAVE 7

#define SCH1 CH172
#define SCH2 CH174
#define SCH3 CH176
#define CCH  CH178
#define SCH4 CH180
#define SCH5 CH182
#define SCH6 CH184

/**
 * data rates of a WAVE device.
 * since we only support 802.11a with 10MHz
 */
enum WaveDataRate
{
  OFDM_3M  = 0,      // 3Mbps
  OFDM_4_5M = 1,     // 4.5Mbps
  OFDM_6M  = 2,      // 6Mbps
  OFDM_9M  = 3,      // 9Mbps
  OFDM_12M = 4,      // 12Mbps
  OFDM_18M = 5,      // 28Mbps
  OFDM_24M = 6,      // 24Mbps
  OFDM_27M = 7,      // 27Mbps
  UNKNOWN_DATA_RATE = 8,
};

/**
  * enum ChannelState - Channel State indicate channel access assignment
  * CHANNEL_DEAD, no channel access has been assigned for this
  * channel, so this channel cannot send and receive packets;
  * CHANNEL_ACTIVE, channel access is assigned, so packets will
  * be queued and ready to be sent whenever PHY permits.
  * CHANNEL_INACTIVE, the channel access will be assigned in
  * next interval but now has no channel access. Packets will be queued
  * but cannot be sent until ChannelScheduler assigns channel access for
  * this channel in next channel interval controlled by ChannelCoordinator.
  */
 enum ChannelState
 {
   CHANNEL_DEAD,
   CHANNEL_ACTIVE,
   CHANNEL_INACTIVE,
 };

/**
 * manage 7 WaveChannels and their status information.
 * 1. Although 802.11p standard supports 10MHz and 20MHz channel bandwidth,
 * and 1609.4 should support too, now we just support 7 channels
 * with 10MHz channel bandwidth.
 * 2. 1609.4 says CCH should be put first in Channel List.
 * but here we insert into vector according to channel number, so we
 * can map channel number to the index of vector immediately and fast.
 */
class ChannelManager : public Object
{
public:
  static TypeId GetTypeId (void);
  ChannelManager ();
  virtual ~ChannelManager ();

  /**
   * \param channelNumber the specific channel
   * \return whether channel is valid CCH channel
   */
  static bool IsCch (uint32_t channelNumber);
  /**
   * \param channelNumber the specific channel
   * \return whether channel is valid SCH channel
   */
  static bool IsSch (uint32_t channelNumber);
  /**
   * \param channelNumber the specific channel
   * \return whether channel is valid WAVE channel
   */
  static bool IsWaveChannel (uint32_t channelNumber);
  /**
   * \param channelNumber the specific channel
   * \return current state of the channel
   */
  enum ChannelState GetState (uint32_t channelNumber);
  /**
   * \param channelNumber the specific channel
   * \param state the specific channel
   * set current state of the channe
   */
  void SetState (uint32_t channelNumber, enum ChannelState state);
  /**
   * \param channelNumber the specific channel
   * \return whether channel is active now
   */
  bool IsChannelActive (uint32_t channelNumber);
  /**
   * \param channelNumber the specific channel
   * \return whether channel is inactive now
   */
  bool IsChannelInactive (uint32_t channelNumber);
  /**
   * \param channelNumber the specific channel
   * \return whether channel is dead now
   */
  bool IsChannelDead (uint32_t channelNumber);

  /**
   * Get channel information according to channel number.
   * The information is used to send VSA management frame.
   * 1) the data rate and txpower of IP-based packet are controlled
   * by registered txprofile;
   * 2) the data rate and txpower of WSMP packets are controlled by
   * higher layers on a per-message basis;
   * 3) the data rate and txpower of management frames are controlled
   * by default channel table here.
   */
  uint32_t GetOperatingClass (uint32_t channelNumber);
  bool IsAdapter (uint32_t channelNumber);
  enum WaveDataRate GetDataRate (uint32_t channelNumber);
  uint32_t GetTxPowerLevel (uint32_t channelNumber);

private:
  uint32_t GetIndex (uint32_t channelNumber);
  struct WaveChannel
  {
    uint32_t channelNumber;
    uint32_t operatingClass;
    bool adapter;
    enum WaveDataRate dataRate;
    uint32_t txPowerLevel;

    enum ChannelState state;

    WaveChannel (uint32_t channel)
      : channelNumber (channel),
        operatingClass (DEFAULT_CCH_OPERATING_CLASS),
        adapter (true),
        dataRate (OFDM_6M),
        txPowerLevel (4),
        state (CHANNEL_DEAD)
      {
      }
  };
  std::vector<WaveChannel *> m_channels;
};

}
#endif /* CHANNEL_MANAGER_H */
