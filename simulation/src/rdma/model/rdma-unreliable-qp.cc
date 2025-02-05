#include <ns3/rdma-unreliable-qp.h>
#include <ns3/qbb-net-device.h>
#include <ns3/rdma-bth.h>
#include <ns3/rdma-seq-header.h>
#include <ns3/ppp-header.h>
#include <ns3/simulator.h>
#include <ns3/log.h>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("RdmaUnreliableSQ");

void RdmaUnreliableSQ::PostSend(RdmaTxQueuePair::SendRequest sr)
{
    NS_ASSERT_MSG(sr.payload_size <= m_mtu, "UD QP max. message size is MTU");

	m_to_send.push(std::move(sr));	
	
	// Trigger to possibly send
	if(m_nextAvail <= Simulator::Now()) {
		Simulator::Schedule(Seconds(0), &QbbNetDevice::TriggerTransmit, GetDevice());
	}
	else {
		Simulator::Schedule(m_nextAvail - Simulator::Now(), &QbbNetDevice::TriggerTransmit, GetDevice());
	}
}

bool RdmaUnreliableSQ::IsReadyToSend() const
{
	const bool timer_ready = Simulator::Now().GetTimeStep() >= m_nextAvail.GetTimeStep();
	const bool ready = !m_to_send.empty() && timer_ready;
	
	NS_LOG_LOGIC(this << " {to_send.size=" << m_to_send.size() << ",timer_ready=" << timer_ready << "}");

	return ready;
}

Ptr<Packet> RdmaUnreliableSQ::GetNextPacket()
{
	if(m_to_send.empty()) {
		NS_ASSERT(false);
		return nullptr;
	}

	SendRequest sr = m_to_send.front();
	NS_ASSERT_MSG(sr.payload_size <= m_mtu, "UD QP max. message size is MTU");

	RdmaBTH bth;
	bth.SetReliable(false);
	bth.SetAckReq(false);
	bth.SetMulticast(sr.multicast);
	bth.SetDestQpKey(sr.dport);
	bth.SetNotif(true);
	bth.SetImm(sr.imm);
	m_to_send.pop();

	// UD QP notification is when sent, not when ACKed, because there is no ACK
	// In fact, the packet still neds to be transmited, but maybe it is enough to call it here
	if(sr.on_send) {
			sr.on_send();
	}
	
	Ptr<Packet> p = Create<Packet>(sr.payload_size);
	
	// Add RdmaSeqHeader
	RdmaSeqHeader seqTs;
	seqTs.SetSeq(m_snd_nxt);
	seqTs.SetPG(m_pg);
	p->AddHeader(seqTs);
	
	// Add UDP header
	UdpHeader udpHeader;
	udpHeader.SetDestinationPort(sr.dport);
	udpHeader.SetSourcePort(m_sport);
	p->AddHeader(udpHeader);

	// Add IPv4 header
	Ipv4Header ipHeader;
	ipHeader.SetSource(m_sip);
	ipHeader.SetDestination(sr.dip);
	ipHeader.SetProtocol(0x11);
	ipHeader.SetPayloadSize(p->GetSize());
	ipHeader.SetTtl(64);
	ipHeader.SetTos(0);
	ipHeader.SetIdentification(m_ipid);
	p->AddHeader(ipHeader);

	// Add PPP header
	PppHeader ppp;
	ppp.SetProtocol(0x0021); // EtherToPpp(0x800), see point-to-point-net-device.cc
	p->AddHeader(ppp);

	// Add BTH header
	p->AddPacketTag(bth);

	// Update state
	m_snd_nxt += sr.payload_size;
	m_ipid++;

	// return
	return p;
}


void RdmaUnreliableRQ::ReceiveUdp(Ptr<Packet> p, const CustomHeader &ch)
{
	RdmaRxQueuePair::ReceiveUdp(p, ch);

	RdmaBTH bth;
	NS_ABORT_UNLESS(p->PeekPacketTag(bth));
	NS_ASSERT(!bth.GetReliable());

	// If no error, call completion event
	if(m_onRecv && bth.GetNotif()) {
		RecvNotif notif;
		notif.packet = p;
		notif.ch = &ch;
		notif.has_imm = true;
		notif.imm = bth.GetImm();
		m_onRecv(notif);
	}
}

}