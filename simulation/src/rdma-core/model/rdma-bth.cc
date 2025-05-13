#include "rdma-bth.h"
#include <ns3/uinteger.h>
#include "ns3/boolean.h"

namespace ns3 {

TypeId RdmaBTH::GetTypeId()
{
	static TypeId tid = TypeId("ns3::RdmaBTH")
		.SetParent<Object>()
		.AddAttribute("Reliable",
				"Whether the RQ destination is UD or RC",
				BooleanValue(true),
				MakeBooleanAccessor(&RdmaBTH::m_reliable),
				MakeBooleanChecker())
		.AddAttribute("Multicast",
				"Whether the destination is a multicast address",
				BooleanValue(false),
				MakeBooleanAccessor(&RdmaBTH::m_multicast),
				MakeBooleanChecker())
		.AddAttribute("AckReq",
				"Whether to request for an ACK.",
				BooleanValue(false),
				MakeBooleanAccessor(&RdmaBTH::m_ack_req),
				MakeBooleanChecker())
		;
	return tid;
}

TypeId RdmaBTH::GetInstanceTypeId() const
{
	return GetTypeId();
}

void RdmaBTH::Print(std::ostream &os) const
{
	os << "m_reliable=" << m_reliable;
}

uint32_t RdmaBTH::GetSerializedSize() const
{
	return 7;
}

void RdmaBTH::Serialize(TagBuffer start) const
{
	const uint8_t payload = m_reliable
		| (m_multicast << 1)
		| (m_ack_req << 2)
		| (m_notif << 3);

	start.WriteU8(payload);
	start.WriteU32(m_imm);
	start.WriteU16(m_key);
}

void RdmaBTH::Deserialize(TagBuffer start)
{
	const uint8_t payload = start.ReadU8();
	m_reliable  = payload & 1;
	m_multicast = payload & 2;
	m_ack_req   = payload & 4;
	m_notif     = payload & 8;
	m_imm       = start.ReadU32();
	m_key       = start.ReadU16();
}

bool RdmaBTH::GetReliable() const
{
	return m_reliable;
}

void RdmaBTH::SetReliable(bool reliable)
{
	m_reliable = reliable;
}	

bool RdmaBTH::GetAckReq() const
{
	return m_ack_req;
}

void RdmaBTH::SetAckReq(bool ack_req)
{
	m_ack_req = ack_req;
}

bool RdmaBTH::GetMulticast() const
{
	return m_multicast;
}

void RdmaBTH::SetMulticast(bool multicast)
{
	m_multicast = multicast;
}

bool RdmaBTH::GetNotif() const
{
	return m_notif;
}

void RdmaBTH::SetNotif(bool notif)
{
	m_notif = notif;
}

void RdmaBTH::SetImm(uint32_t imm)
{
	m_imm = imm;
}

uint32_t RdmaBTH::GetImm() const
{
	return m_imm;
}

}