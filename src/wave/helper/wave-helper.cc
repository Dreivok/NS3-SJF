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
#include "ns3/wifi-mac.h"
#include "ns3/wifi-phy.h"
#include "ns3/log.h"
#include "ns3/pointer.h"
#include "ns3/string.h"
#include "ns3/wifi-mode.h"
#include "ns3/nqos-wifi-mac-helper.h"
#include "ns3/wave-net-device.h"
#include "wave-mac-helper.h"
#include "wave-helper.h"

namespace ns3 {

WaveHelper::WaveHelper ()
{
}

WaveHelper::~WaveHelper ()
{
}

WaveHelper
WaveHelper::Default (void)
{
  WaveHelper helper;
  helper.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                  "DataMode", StringValue ("OfdmRate6MbpsBW10MHz"),
                                  "ControlMode",StringValue ("OfdmRate6MbpsBW10MHz"),
                                  "NonUnicastMode",WifiModeValue (WifiMode ("OfdmRate6MbpsBW10MHz")));

  return helper;
}

void
WaveHelper::EnableLogComponents (void)
{
  WifiHelper::EnableLogComponents ();

  LogComponentEnable ("WaveNetDevice", LOG_LEVEL_ALL);
  LogComponentEnable ("ChannelCoordinator", LOG_LEVEL_ALL);
  LogComponentEnable ("ChannelManager", LOG_LEVEL_ALL);
  LogComponentEnable ("ChannelScheduler", LOG_LEVEL_ALL);
  LogComponentEnable ("OcbWifiMac", LOG_LEVEL_ALL);
  LogComponentEnable ("VendorSpecificAction", LOG_LEVEL_ALL);
}

NetDeviceContainer
WaveHelper::Install (const WifiPhyHelper &phyHelper, const WifiMacHelper &macHelper, NodeContainer c) const
{
  bool isNqos = false;
  try
    {
      const NqosWifiMacHelper& nqosMac = dynamic_cast<const NqosWifiMacHelper&> (macHelper);
      isNqos = true;
      if (&nqosMac == 0)
        {
          NS_FATAL_ERROR ("it could never get here");
        }
    }
  catch (const std::bad_cast &)
    {
    }

  if (isNqos)
    {
      NS_FATAL_ERROR ("WaveNetDevice should use qos MAC to support priority required by 1609.4");
    }

  bool isQosWaveMac = false;
  try
    {
      const WaveMacHelper& qosMac = dynamic_cast<const WaveMacHelper&> (macHelper);
      isQosWaveMac = true;
      // below check will never fail, just used for survive from
      // gcc warn "-Wunused-but-set-variable"
      if (&qosMac == 0)
        {
          NS_FATAL_ERROR ("it could never get here");
        }
    }
  catch (const std::bad_cast &)
    {
    }

  if (!isQosWaveMac)
    {
      NS_FATAL_ERROR ("macHelper should be WaveMacHelper or its subclass.");
    }


  NetDeviceContainer devices;
  for (NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i)
    {
      Ptr<Node> node = *i;
      Ptr<WaveNetDevice> device = CreateObject<WaveNetDevice> ();
      Ptr<WifiRemoteStationManager> manager = m_stationManager.Create<WifiRemoteStationManager> ();
      Ptr<WifiPhy> phy = phyHelper.Create (node, device);
      phy->ConfigureStandard (WIFI_PHY_STANDARD_80211_10MHZ);
      phy->SetChannelNumber (CCH);
      const WifiMacHelper & machelp = macHelper;
      Ptr<WifiMac> mac = machelp.Create ();
      mac->ConfigureStandard (WIFI_PHY_STANDARD_80211_10MHZ);
      device->SetMac (mac);
      device->SetAddress (Mac48Address::Allocate ());
      device->SetPhy (phy);
      device->SetRemoteStationManager (manager);
      node->AddDevice (device);
      devices.Add (device);
    }
  return devices;
}


} // namespace ns3
