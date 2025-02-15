#include <ns3/rdma-unreliable-qp.h>
#include <ns3/qbb-net-device.h>
#include <ns3/rdma-bth.h>
#include <ns3/rdma-seq-header.h>
#include <ns3/cn-header.h>
#include <ns3/ppp-header.h>
#include <ns3/qbb-header.h>
#include <ns3/rdma-seq-header.h>
#include <ns3/simulator.h>
#include <ns3/rdma-hw.h>
#include <ns3/log.h>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("RdmaUnreliableQP");

void RdmaUnreliableSQ::PostSend(RdmaTxQueuePair::SendRequest sr)
{
  NS_LOG_FUNCTION(this);

	NS_ASSERT_MSG(sr.payload_size <= m_mtu, "UD QP max. message size is MTU");

	m_to_send.push(std::move(sr));	
	TriggerDevTransmit();
}

bool RdmaUnreliableSQ::HasDataToSend() const
{
	return !m_to_send.empty();
}

bool RdmaUnreliableSQ::IsReadyToSend() const
{
  NS_LOG_FUNCTION(this);

	const bool timer_ready = Simulator::Now().GetTimeStep() >= m_nextAvail.GetTimeStep();
	const bool ready = HasDataToSend() && timer_ready;
	
	NS_LOG_LOGIC(this << " {to_send.size=" << m_to_send.size() << ",timer_ready=" << timer_ready << "}");

	return ready;
}

Ptr<Packet> RdmaUnreliableSQ::GetNextPacket()
{
  NS_LOG_FUNCTION(this);
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
  NS_LOG_FUNCTION(this);
	
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

	// Even if UD QP has no ACK, we use ACK to manage the congestion control
	const uint8_t ecnbits = ch.GetIpv4EcnBits();
	
	// If any node in the path encountered congestion, send back an ECN notification (but not too often)
	// UD QP SHOULD NOT SEND ACK
	// Reverse multicast, it can overflow the sender
#if 0
	if(ecnbits == Ipv4Header::ECN_CE && Simulator::Now() >= m_ecn_next_avail) {
		m_ecn_next_avail = Simulator::Now() + m_ecn_delay;
		SendEcn(ch);
	}
#endif
}

void RdmaUnreliableRQ::SendEcn(const CustomHeader& recv)
{
	NS_LOG_FUNCTION(this);

	CustomHeader ch;

	// We don't really care of the content, we just want to send the packet
	CnHeader cn(0, 0, 0, 0, 0);

	Ptr<Packet> newp = Create<Packet>(1);
	newp->AddHeader(cn);

	Ipv4Header head;	// Prepare IPv4 header
	head.SetDestination(Ipv4Address(recv.sip));
	head.SetSource(Ipv4Address(recv.dip));
	head.SetProtocol(0xFF); // CNP
	head.SetTtl(64);
	head.SetPayloadSize(newp->GetSize());
	head.SetIdentification(0);

	newp->AddHeader(head);
	
	const auto EtherToPpp = [](uint16_t proto) -> uint16_t {
		switch(proto){
			case 0x0800: return 0x0021;   //IPv4
			case 0x86DD: return 0x0057;   //IPv6
			default: NS_ASSERT_MSG (false, "PPP Protocol number not defined!");
		}
		return 0;
	};

	PppHeader ppp;
	ppp.SetProtocol(EtherToPpp (0x800));
	newp->AddHeader (ppp);

	RdmaBTH bth;
	bth.SetDestQpKey(recv.udp.sport);
	newp->AddPacketTag(bth);

	// send
	Ptr<QbbNetDevice> dev = m_tx->GetDevice();
	dev->RdmaEnqueueHighPrioQ(newp);
	dev->TriggerTransmit();
}

}