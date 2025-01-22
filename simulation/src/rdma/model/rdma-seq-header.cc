/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2009 INRIA
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
 */

#include "ns3/assert.h"
#include "ns3/log.h"
#include "ns3/header.h"
#include "ns3/simulator.h"
#include "ns3/rdma-seq-header.h"

NS_LOG_COMPONENT_DEFINE ("RdmaSeqHeader");

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (RdmaSeqHeader);

RdmaSeqHeader::RdmaSeqHeader ()
  : m_seq (0)
{
	if (IntHeader::mode == 1)
		ih.ts = Simulator::Now().GetTimeStep();
}

void
RdmaSeqHeader::SetSeq (uint32_t seq)
{
  m_seq = seq;
}
uint32_t
RdmaSeqHeader::GetSeq (void) const
{
  return m_seq;
}

void
RdmaSeqHeader::SetPG (uint16_t pg)
{
	m_pg = pg;
}
uint16_t
RdmaSeqHeader::GetPG (void) const
{
	return m_pg;
}

Time
RdmaSeqHeader::GetTs (void) const
{
	NS_ASSERT_MSG(IntHeader::mode == 1, "RdmaSeqHeader cannot GetTs when IntHeader::mode != 1");
	return TimeStep (ih.ts);
}

TypeId
RdmaSeqHeader::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::RdmaSeqHeader")
    .SetParent<Header> ()
    .AddConstructor<RdmaSeqHeader> ()
  ;
  return tid;
}
TypeId
RdmaSeqHeader::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}
void
RdmaSeqHeader::Print (std::ostream &os) const
{
  //os << "(seq=" << m_seq << " time=" << TimeStep (m_ts).GetSeconds () << ")";
	//os << m_seq << " " << TimeStep (m_ts).GetSeconds () << " " << m_pg;
	os << m_seq << " " << m_pg;
}
uint32_t
RdmaSeqHeader::GetSerializedSize (void) const
{
	return GetHeaderSize();
}
uint32_t RdmaSeqHeader::GetHeaderSize(void){
	return 6 + IntHeader::GetStaticSize();
}

void
RdmaSeqHeader::Serialize (Buffer::Iterator start) const
{
  Buffer::Iterator i = start;
  i.WriteHtonU32 (m_seq);
  i.WriteHtonU16 (m_pg);

  // write IntHeader
  ih.Serialize(i);
}
uint32_t
RdmaSeqHeader::Deserialize (Buffer::Iterator start)
{
  Buffer::Iterator i = start;
  m_seq = i.ReadNtohU32 ();
  m_pg =  i.ReadNtohU16 ();

  // read IntHeader
  ih.Deserialize(i);
  return GetSerializedSize ();
}

} // namespace ns3
