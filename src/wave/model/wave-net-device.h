/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2005,2006 INRIA
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
 * Author: Mathieu Lacage <mathieu.lacage@sophia.inria.fr>
 *         Junling Bu <linlinjavaer@gmail.com>
 */
#ifndef WAVE_NET_DEVICE_H
#define WAVE_NET_DEVICE_H

#include "ns3/wifi-net-device.h"
#include "ns3/packet.h"
#include "ns3/traced-callback.h"
#include "ns3/mac48-address.h"
#include "channel-coordinator.h"
#include "channel-manager.h"
#include "channel-scheduler.h"
#include "vsa-repeater.h"
#include "vendor-specific-action.h"
namespace ns3 {

class WifiRemoteStationManager;
class WifiChannel;
class WifiPhy;
class WifiMac;
class ChannelScheduler;
class VsaRepeater;

/**
 * \param channelNumber the specific channel
 * \param priority the priority of packet with range 0-7
 * \param dataRate the transmit data rate of packet
 * \param txPowerLevel the transmit power level
 * \param expireTime indicate how many milliseconds the packet
 * can stay before sent
 *
 * typically these parameters are used by higher layer to control
 * the transmission characteristics of WSMP data.
 * when dataRate is UnknownDataRate and txPowerLevel is 8, it indicates
 * that higher layer will not control, and value will be set by MAC layer.
 */
struct TxInfo
{
  uint32_t channelNumber;
  uint32_t priority;
  enum WaveDataRate dataRate;
  uint32_t txPowerLevel;
  // uint32_t expiryTime;   // unsupport
  TxInfo ()
    : channelNumber (CCH),
      priority (0),
      dataRate (UNKNOWN_DATA_RATE),
      txPowerLevel (8)
  {

  }
  TxInfo (uint32_t channel, uint32_t prio = 0, enum WaveDataRate rate = UNKNOWN_DATA_RATE, uint32_t powerLevel = 8, uint32_t expire = 0)
    : channelNumber (channel),
      priority (prio),
      dataRate (rate),
      txPowerLevel (powerLevel)
  {

  }
};
/**
 * \param channelNumber the channel number for the SCH.
 * \param adaptable if true, the actual power level and data
 * rate for transmission are adaptable. TxPwr_Level is the maximum
 * transmit power that sets the upper bound for the actual transmit
 * power; DataRate is the minimum data rate that sets the lower
 * bound for the actual data rate. If false, the actual power level
 * and data rate for transmission are nonadaptable. TxPwr_Level and
 * DataRate are the actual values to be used for transmission.
 * \param txPowerLevel transmit power level
 * \param dataRate transmit data rate
 *
 * note: although txPowerLevel is supported now, the "TxPowerLevels"
 * attribute of YansWifiPhy is 1 which means phy devices only support 1
 * level. If users want to control txPowerlvel, they should set these
 * attributes in YansWifiPhy by themselves.
 */
struct TxProfile
{
  uint32_t channelNumber;
  bool adaptable;
  uint32_t txPowerLevel;
  enum WaveDataRate dataRate;
  TxProfile (void)
    : channelNumber (SCH1),
      adaptable (false),
      txPowerLevel (8),
      dataRate (UNKNOWN_DATA_RATE)
  {
  }
  TxProfile (uint32_t channel, bool adapt = false, uint32_t powerLevel = 8, enum WaveDataRate rate = UNKNOWN_DATA_RATE)
    : channelNumber (channel),
      adaptable (adapt),
      txPowerLevel (powerLevel),
      dataRate (rate)
  {
  }
};


/**
 * This class holds together ns3::WifiChannel, ns3::WifiPhy,
 * ns3::WifiMac, and ns3::WifiRemoteStationManager.
 * Besides that, to support multiple channel operation this
 * class also holds ns3::ChannelScheduler, ns3::ChannelManager,
 * ns3::Coordinator and ns3::VsaRepeater.
 *
 * these primitives specified in the standard will not be implemented
 * because of limited use in simulation:
 * void StartTimingAdvertisement ();
 * void StopTimingAdvertisement ();
 * UtcTime GetUtcTime ();
 * void SetUtcTime ();
 * void CancelTx (uint32_t channelNumber, AcIndex ac);
 */
class WaveNetDevice : public WifiNetDevice
{
public:
  static TypeId GetTypeId (void);
  WaveNetDevice (void);
  virtual ~WaveNetDevice (void);

  /**
   * \param sch_info the parameters about how to start SCH service
   *
   * This method is used to assign channel access for sending packets.
   */
  bool StartSch (const SchInfo & schInfo);
  /**
   *  \param channelNumber the channel which access resource will be released.
   */
  void StopSch (uint32_t channelNumber);

  /**
   * \param vsa_info the parameters about how to send VSA frame
   *
   * note:
   * (i) channel access should be assigned for VSA transmission.
   * (ii) although this method supports sending management information
   * which is not belong to IEEE 1609 by setting new OrganizationIdentifiers
   * instead of management id, the 1609.4 standard declares that if
   * OrganizationIdentifier not indicates IEEE 1609, the management
   * frames will be not delivered to the IEEE 1609.4 MLME.
   */
  bool StartVsa (const VsaInfo & vsaInfo);
  /**
   * \param channelNumber the channel on which VSA frames send event
   * will be canceled.
   */
  void StopVsa (uint32_t channelNumber);
  /**
   * \param packet the packet is Vendor Specific Action frame.
   * \param address the address of the MAC from which the management frame
   *  was received.
   * \param managementID identify the originator of the data.
   *  Values are specified in IEEE P1609.0 with range 0-15.
   * \param channelNumber the channel on which the frame was received.
   * \returns true if the callback could handle the packet successfully, false
   *          otherwise.
   */
  typedef Callback<bool, Ptr<const Packet>,const Address &, uint32_t, uint32_t> WaveCallback;
  /**
   * \param vsaCallback the VSA receive callback for 1609
   */
  void SetVsaReceiveCallback (WaveCallback vsaCallback);

  /**
   * \param txprofile transmit profile for IP-based data
   * register a transmitter profile in the MLME before
   * the associated IP-based data transfer starts.
   * So normally this method should be called before Send
   * method is called.
   * And txprofile can only be registered once unless
   * users unregister.
   */
  bool RegisterTxProfile (const TxProfile &txprofile);
  /**
   * \param channelNumber the specific channel number
   * delete a registered transmitter profile in the MLME
   * after the associated IP-based data transfer is complete
   */
  void UnregisterTxProfile (uint32_t channelNumber);

  /**
   * \param packet packet sent from above down to Network Device
   * \param dest mac address of the destination (already resolved)
   * \param protocol identifies the type of payload contained in the packet.
   *  Used to call the right L3Protocol when the packet is received.
   *  note: although this method is mainly used for WSMP packets, users
   *  can send any packets they like even this protocol is IP-based.
   * \param txInfo WSMP or other packets parameters for sending
   * \return whether the SendX operation succeeded
   *
   * SendX method implements MA-UNITDATAX.request primitive.
   * comparing to WaveNetDevice::Send, here is two differences
   * 1) WaveNetDevice::Send cannot control phy parameters for generality,
   *  but WaveNetDevice::SendX can allow user control packets on a per-message basis;
   * 2) If users want to use priority in WifiNetDevice, they should insert
   * a QosTag into Packet before Send method, however with SendX method,
   * users should set the priority in TxInfo.
   * Normally this method is called by high 1609.3 standard to send WSMP packets.
   *
   * SendX method for WSMP packets and Send method for IP-based packets,
   * Here is to use SetReceiveCallback (NetDevice::ReceiveCallback cb) to forward
   * received packets to higher layer regardless of whether the frame contains a WSMP
   * or an IPv6 packet.
   */
  bool SendX (Ptr<Packet> packet, const Address& dest, uint32_t protocol, const TxInfo & txInfo);
  /**
   * \param newAddress
   * an immediate MAC-layer address change is required
   * note: this function is useful in real environment,
   * but seems useless in simulation.
   * And this method is similar with SetAddress method, but
   * SetAddress is suggested when initialize a device, and this method
   * is used when change address, a addressChange TracedCallback
   * will be called.
   */
  void ChangeAddress (Address newAddress);

  /**
   * Different from WifiNetDevice::IsLinkUp, a WaveNetDevice device
   * is always link up so the m_linkup variable is true forever,
   * even the device is in channel switch state, packets still can be queued
   */
  virtual bool IsLinkUp (void) const;
  virtual void AddLinkChangeCallback (Callback<void> callback);

  virtual bool Send (Ptr<Packet> packet, const Address& dest, uint16_t protocol);
  virtual bool SendFrom (Ptr<Packet> packet, const Address& source, const Address& dest, uint16_t protocol);
  virtual bool SupportsSendFrom (void) const;

  /**
   * Whether NeedsArp or not?
   * For IP-based Packets , yes;
   * For WSMP packets, no;
   * so now return true always.
   */
  virtual bool NeedsArp (void) const;

  /**
   * see 5.2.3 Transmit restrictions on CCH and SCH
   * "Frames carrying IP packets shall not be transmitted on the CCH"
   *
   * If user really want to send ip packets on CCH, they can use this method
   */
  void SetIpOnCchSupported (bool enable);
  bool GetIpOnCchSupported (void) const;

  void SetChannelManager (Ptr<ChannelManager> channelManager);
  Ptr<ChannelManager> GetChannelManager (void) const;
  void SetChannelScheduler (Ptr<ChannelScheduler> channelScheduler);
  Ptr<ChannelScheduler> GetChannelScheduler (void) const;
  void SetChannelCoordinator (Ptr<ChannelCoordinator> channelCoordinator);
  Ptr<ChannelCoordinator> GetChannelCoordinator (void) const;

  bool IsScheduled(void) const;
  void SetSchedule(uint32_t channelNumber, uint32_t serviceSize);
  void SetSent(void);
  uint32_t GetScheduleChannel(void) const;
  uint32_t GetScheduleSize(void) const;
  void UpdateSchedule(uint32_t receiveSize);
  bool DuplicationCounts();

private:
  virtual void DoDispose (void);
  virtual void DoInitialize (void);
  void WaveForwardUp (Ptr<Packet> packet, Mac48Address from, Mac48Address to);

  bool DoReceiveVsc (Ptr<WifiMac> , const OrganizationIdentifier &, Ptr<const Packet>, const Address &);

  WaveCallback m_waveVscReceived;

  Ptr<ChannelManager> m_channelManager;
  Ptr<ChannelScheduler> m_channelScheduler;
  Ptr<ChannelCoordinator> m_channelCoordinator;
  Ptr<VsaRepeater> m_vsaRepeater;
  TxProfile *m_txProfile;

  bool m_ipOnCch;
  uint32_t m_Scheduled;
  uint32_t m_Size;
  uint32_t m_Sent;

  uint32_t prev_Size;
  uint32_t prev_Channel;
  uint32_t prev_Count;

  TracedCallback<Address, Address> m_addressChange;
};

} // namespace ns3

#endif /* WAVE_NET_DEVICE_H */
