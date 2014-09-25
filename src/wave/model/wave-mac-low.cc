/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2008 INRIA
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
 *         Junling Bu <linlinjavaer@gmail.com>
 */
#include "ns3/log.h"
#include "wave-mac-low.h"
#include "data-tx-tag.h"
#include "wave-net-device.h"

NS_LOG_COMPONENT_DEFINE ("WaveMacLow");

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (WaveMacLow);

TypeId
WaveMacLow::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::WaveMacLow")
    .SetParent<MacLow> ()
    .AddConstructor<WaveMacLow> ()
  ;
  return tid;
}
WaveMacLow::WaveMacLow ()
{
  NS_LOG_FUNCTION (this);
}
WaveMacLow::~WaveMacLow ()
{
  NS_LOG_FUNCTION (this);
}

void
WaveMacLow::SetWaveDevice (Ptr<NetDevice> device)
{
  Ptr<WaveNetDevice> wave = DynamicCast<WaveNetDevice> (device);
  m_scheduler =  wave->GetChannelScheduler ();
  m_coordinator =  wave->GetChannelCoordinator ();
}

WifiTxVector
WaveMacLow::GetDataTxVector (Ptr<const Packet> packet, const WifiMacHeader *hdr) const
{
  NS_LOG_FUNCTION (this << packet << hdr);
  DataTxVectorTag datatag;
  bool found;
  found = ConstCast<Packet> (packet)->PeekPacketTag (datatag);
  if (!found)
    {
      return MacLow::GetDataTxVector (packet, hdr);
    }

  if (!datatag.IsAdapter ())
    {
      return datatag.GetDataTxVector ();
    }

  WifiTxVector txHigher = datatag.GetDataTxVector ();
  WifiTxVector txMac = MacLow::GetDataTxVector (packet, hdr);
  WifiTxVector txAdapter;
  // if adapter is true, DataRate set by higher layer is the minimum data rate
  // that sets the lower bound for the actual data rate.
  if (txHigher.GetMode ().GetDataRate () > txMac.GetMode ().GetDataRate ())
    {
	  txAdapter.SetMode (txHigher.GetMode ());
    }
  else
    {
	  txAdapter.SetMode (txMac.GetMode ());
    }

  // if adapter is true, TxPwr_Level set by higher layer is the maximum
  // transmit power that sets the upper bound for the actual transmit power;
  txAdapter.SetTxPowerLevel (std::min (txHigher.GetTxPowerLevel (), txMac.GetTxPowerLevel ()));
  return txAdapter;
}

void
WaveMacLow::StartTransmission (Ptr<const Packet> packet,
                        const WifiMacHeader* hdr,
                        MacLowTransmissionParameters params,
                        MacLowTransmissionListener *listener)
{
  NS_LOG_FUNCTION (this << packet << hdr << params << listener);
  // if current channel access is not AlternatingAccess, just do as MacLow.
  if (m_scheduler->GetAccess () != AlternatingAccess)
    {
	  MacLow::StartTransmission (packet, hdr, params, listener);
	  return;
    }

  Time transmissionTime = MacLow::CalculateTransmissionTime (packet, hdr, params);
  Time remainingTime = m_coordinator->NeedTimeToGuardInterval ();

  if (transmissionTime > remainingTime)
    {
	  // this packet will not be delivered to PHY layer
	  // and the data packet and management frame will be re-inserted into the queue by EdcaTxopN class

	  NS_LOG_DEBUG ("transmission time = " << transmissionTime
			  	   << ", remainingTime = " << remainingTime
				   << ", this packet will be queued again.");
	 }
  else
    {
	  MacLow::StartTransmission (packet, hdr, params, listener);
    }
}

} // namespace ns3
