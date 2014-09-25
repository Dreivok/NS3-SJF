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
#ifndef DATA_TXVECTOR_TAG_H
#define DATA_TXVECTOR_TAG_H

#include "ns3/packet.h"
#include "ns3/wifi-tx-vector.h"

namespace ns3 {
class Tag;

/**
 * This tag will be used to support higher layer control data rate
 * and tx power level.
 *
 * some discussions about this feature
 * https://www.nsnam.org/bugzilla/show_bug.cgi?id=917
 */
class DataTxVectorTag : public Tag
{
public:
  DataTxVectorTag ();
  DataTxVectorTag (WifiTxVector dataTxVector, bool adapter);
  virtual ~DataTxVectorTag ();

  WifiTxVector GetDataTxVector (void) const;
  bool IsAdapter (void) const;

  static TypeId GetTypeId (void);
  virtual TypeId GetInstanceTypeId (void) const;
  virtual uint32_t GetSerializedSize (void) const;
  virtual void Serialize (TagBuffer i) const;
  virtual void Deserialize (TagBuffer i);
  virtual void Print (std::ostream &os) const;

private:
  WifiTxVector m_dataTxVector;
  bool m_adapter;
};

} // namespace ns3

#endif /* DATA_TXVECTOR_TAG_H*/
