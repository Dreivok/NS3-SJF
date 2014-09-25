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
#ifndef WAVE_HELPER_H
#define WAVE_HELPER_H

#include "ns3/wifi-helper.h"
namespace ns3 {

/**
 * \brief helps to create WaveNetDevice objects
 *
 * This class can help to create a large set of similar
 * WaveNetDevice objects and to configure a large set of
 * their attributes during creation.
 *
 * WifiHelper which configures default MAC and PHY for 802.11a;
 * Wifi80211pHelper shall configure 802.11a with 10MHz or 20MHz
 * channel bandwidth;
 * WaveHelper must configure 802.11a with 10MHz channel
 * bandwidth (20MHz is not support).
 */
class WaveHelper : public WifiHelper
{
public:
  WaveHelper ();
  virtual ~WaveHelper ();

  /**
   * default WaveNetDevice is constructed with ConstanRate with OfdmRate6MbpsBW10MHz.
   */
  static WaveHelper Default (void);

  /**
   * since WAVE shall support user priority defined in 1609.4, NqosWaveMacHelper will
   * not be allowed.
   */
  virtual NetDeviceContainer Install (const WifiPhyHelper &phy, const WifiMacHelper &macHelper, NodeContainer c) const;

  static void EnableLogComponents (void);
};
}
#endif /* WAVE_HELPER_H */
