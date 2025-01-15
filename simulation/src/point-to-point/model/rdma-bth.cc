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
	return 2;
}

void RdmaBTH::Serialize(TagBuffer start) const
{
	start.WriteU8(m_reliable);
	start.WriteU8(m_ack_req);
}

void RdmaBTH::Deserialize(TagBuffer start)
{
	m_reliable = start.ReadU8();
	m_ack_req = start.ReadU8();
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

}