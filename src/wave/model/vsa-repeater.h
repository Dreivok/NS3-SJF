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
#ifndef VSA_REPEATER_H
#define VSA_REPEATER_H
#include <vector>
#include <stdint.h>
#include "vendor-specific-action.h"

namespace ns3 {
/**
 * indicate which interval the VSA frames will be transmitted in.
 * VSA_IN_CCHI will only allow in CCH Interval;
 * VSA_IN_SCHI will only allow in SCH Interval;
 * VSA_IN_ANYI will allow anytime.
 *
 * note: now we only support VSA_IN_CCHI ad VSA_IN_SCHI when current
 * channel access is alternating access, and support VSA_IN_ANYI when
 * current channel access is continuous access or extended access.
 */
enum VsaSentInterval
{
  VSA_IN_CCHI = 1,
  VSA_IN_SCHI = 2,
  VSA_IN_ANYI = 3,
};


/**
 * \param peer The address of the peer MAC entity to which the
 * VSA is sent.
 * \param oi Identifies the source of the data when the source
 * is not an IEEE 1609 entity. See IEEE Std 802.11p.
 * \param managementId Identifies the source of the data when the source
 * is an IEEE 1609 entity. Values are specified in IEEE P1609.0.
 * Valid range: 0-15
 * \param vsc pointer to Information that will be sent as vendor specific content.
 * \param vscLength the length of vendor specific content
 * \param channelNumber The channel on which the transmissions are to occur
 * While section 7.2 of the standard specifies that channel identification
 * comprises Country String, Operating Class, and Channel Number, channel
 * number is all that is needed for simulation.
 * \param repeatRate The number of Vendor Specific Action frames to
 * be transmitted per 5 s. A value of 0 indicates a single message is to be sent.
 * If Destination MAC Address is an individual address, Repeat Rate is ignored.
 * \param channelInterval The channel interval in which the transmissions
 * are to occur. Note: because of current limited implementation, we suggest users
 * assign alternating access for sending VSAs in SCH Interval or CCH Interval, and
 * assign continuous access or extended access for sending VSAs in both interval.
 */
struct VsaInfo
{
  Mac48Address peer;
  OrganizationIdentifier oi;
  uint8_t managementId;
  Ptr<Packet> vsc;
  uint32_t channelNumber;
  uint8_t repeatRate;

  enum VsaSentInterval sendInterval;

  VsaInfo (Mac48Address peer, OrganizationIdentifier identifier, uint8_t manageId, Ptr<Packet> vscPacket,
           uint32_t channel, uint8_t repeat, enum VsaSentInterval interval)
    : peer (peer),
      oi (identifier),
      managementId (manageId),
      vsc (vscPacket),
      channelNumber (channel),
      repeatRate (repeat),
      sendInterval (interval)
  {

  }
};

/**
 * refer to 1609.4-2010 chapter 6.4
 * Vendor Specific Action (VSA) frames transmission
 */
class VsaRepeater : public Object
{
public:
  static TypeId GetTypeId (void);
  VsaRepeater (void);
  virtual ~VsaRepeater (void);

  void SetWaveDevice (Ptr<NetDevice> device);

  void SendVsa (const VsaInfo &vsaInfo);
  void RemoveAll (void);
  void RemoveByChannel (uint32_t channelNumber);
  void RemoveByOrganizationIdentifier (OrganizationIdentifier oi);
private:
  void DoDispose (void);

  // the Vendor Specific Action frames will
  // be transmitted repeatedly during the period of 5s.
  const static uint32_t VSA_REPEAT_PERIOD = 5;

  struct VsaWork
  {
    Mac48Address peer;
    OrganizationIdentifier oi;
    Ptr<Packet> vsc;
    uint32_t channelNumber;
    Time repeatPeriod;
    uint32_t sentInterval;
    EventId repeat;
  };

  void DoRepeat (VsaWork *vsa);
  void DoSendVsa (uint32_t interval, uint32_t channel, Ptr<Packet> p,
		          OrganizationIdentifier oi, Mac48Address peer);

  std::vector<VsaWork *> m_vsas;
  Ptr<NetDevice> m_device;
};

}
#endif /* VSA_REPEATER_H */
