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
#include "data-tx-tag.h"
#include "ns3/tag.h"
#include "ns3/log.h"
#include "ns3/uinteger.h"

NS_LOG_COMPONENT_DEFINE ("DataTxVectorTag");

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (DataTxVectorTag);

TypeId
DataTxVectorTag::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::DataTxVectorTag")
    .SetParent<Tag> ()
    .AddConstructor<DataTxVectorTag> ()
  ;
  return tid;
}
DataTxVectorTag::DataTxVectorTag ()
  : m_adapter (false)
{
  NS_LOG_FUNCTION (this);
}
DataTxVectorTag::DataTxVectorTag (WifiTxVector dataTxVector, bool adapter)
  : m_dataTxVector (dataTxVector),
    m_adapter (adapter)
{
  NS_LOG_FUNCTION (this);
}
DataTxVectorTag::~DataTxVectorTag ()
{
  NS_LOG_FUNCTION (this);
}
TypeId
DataTxVectorTag::GetInstanceTypeId (void) const
{
  NS_LOG_FUNCTION (this);
  return GetTypeId ();
}

WifiTxVector
DataTxVectorTag::GetDataTxVector (void) const
{
  NS_LOG_FUNCTION (this);
  return m_dataTxVector;
}
bool
DataTxVectorTag::IsAdapter (void) const
{
  NS_LOG_FUNCTION (this);
  return m_adapter;
}

uint32_t
DataTxVectorTag::GetSerializedSize (void) const
{
  NS_LOG_FUNCTION (this);
  return (sizeof (WifiTxVector) + 1);
}
void
DataTxVectorTag::Serialize (TagBuffer i) const
{
  NS_LOG_FUNCTION (this << &i);
  i.Write ((uint8_t *)&m_dataTxVector, sizeof (WifiTxVector));
  i.WriteU8 (static_cast<uint8_t> (m_adapter));
}
void
DataTxVectorTag::Deserialize (TagBuffer i)
{
  NS_LOG_FUNCTION (this << &i);
  i.Read ((uint8_t *)&m_dataTxVector, sizeof (WifiTxVector));
  m_adapter = i.ReadU8 ();
}
void
DataTxVectorTag::Print (std::ostream &os) const
{
  NS_LOG_FUNCTION (this << &os);
  os << " Data=" << m_dataTxVector << " Adapter=" << m_adapter;
}

} // namespace ns3
