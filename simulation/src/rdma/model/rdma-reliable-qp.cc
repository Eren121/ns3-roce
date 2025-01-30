#include <ns3/rdma-reliable-qp.h>
#include <ns3/qbb-net-device.h>
#include <ns3/rdma-hw.h>
#include <ns3/rdma-bth.h>
#include <ns3/qbb-header.h>
#include <ns3/simulator.h>
#include <ns3/ppp-header.h>
#include <ns3/rdma-seq-header.h>

namespace ns3 {

RdmaReliableSQ::RdmaReliableSQ(Ptr<Node> node, uint16_t pg, Ipv4Address sip, uint16_t sport, Ipv4Address dip, uint16_t dport)
    : RdmaTxQueuePair(node, pg, sip, sport),
      m_dip(dip),
      m_dport(dport)
{
}

void RdmaReliableSQ::Acknowledge(uint64_t ack)
{
	if(ack > m_snd_una) {
		m_snd_una = ack;

		// Notify the sender packets have been received
		while(m_ack_cbs.front().seq_no <= ack) {
			m_ack_cbs.front().on_send();
			m_ack_cbs.pop();
		}
	}
}

uint64_t RdmaReliableSQ::GetOnTheFly() const
{
	return m_snd_nxt - m_snd_una;
}

bool RdmaReliableSQ::IsWinBound() const
{
	const uint64_t w = GetWin();
	return w != 0 && GetOnTheFly() >= w;
}

bool RdmaReliableSQ::IsReadyToSend() const
{
	const bool timerReady = Simulator::Now().GetTimeStep() <= m_nextAvail.GetTimeStep();
	return !m_to_send.empty() && !IsWinBound() && timerReady;
}

void RdmaReliableSQ::PostSend(SendRequest sr)
{
	m_to_send.push(std::move(sr));
}

Ptr<Packet> RdmaReliableSQ::GetNextPacket()
{
	if(m_to_send.empty()) {
		return nullptr;
	}
	SendRequest sr = m_to_send.front();
	RdmaBTH bth;
	bth.SetReliable(true);
	bth.SetMulticast(false);

	if (sr.payload_size > m_mtu) {
		// Split the Write Request in multiple MTU-sized packets.
		// Only the last packet will trigger the completion event
		sr.payload_size = m_mtu;
		bth.SetNotif(false);
		bth.SetAckReq(ShouldReqAck(sr.payload_size));
		// Don't push again a packet, but resize the next packet in the queue
		m_to_send.front().payload_size -= m_mtu;
	}
	else {
		bth.SetNotif(true); // Last packet, generate a notif on the RX
		bth.SetAckReq(true);
		bth.SetImm(sr.imm);
		m_to_send.pop();

		// When the ACK is received, call this
		if(sr.on_send) {
			AckCallback cb;
			cb.seq_no = m_snd_nxt;
			cb.on_send = std::move(sr.on_send);
			m_ack_cbs.push(cb);
		}
	}
	
	Ptr<Packet> p = Create<Packet>(sr.payload_size);
	
	// Add RdmaSeqHeader
	RdmaSeqHeader seqTs;
	seqTs.SetSeq(m_snd_nxt);
	seqTs.SetPG(m_pg);
	p->AddHeader(seqTs);
	
	// Add UDP header
	UdpHeader udpHeader;
	udpHeader.SetDestinationPort(m_dport);
	udpHeader.SetSourcePort(m_sport);
	p->AddHeader(udpHeader);

	// Add IPv4 header
	Ipv4Header ipHeader;
	ipHeader.SetSource(m_sip);
	ipHeader.SetDestination(m_dip);
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

void RdmaReliableSQ::RecoverNack()
{
	m_snd_nxt = m_snd_una;
}

uint64_t RdmaReliableSQ::GetWin() const
{
	if (m_win == 0)
		return 0;
	uint64_t w;
	if (m_var_win){
		w = m_win * m_rate.GetBitRate() / m_max_rate.GetBitRate();
		if (w == 0)
			w = 1; // must > 0
	}else{
		w = m_win;
	}
	return w;
}

bool RdmaReliableRQ::Receive(Ptr<Packet> p, const CustomHeader& ch)
{
	if(ch.l3Prot == 0xFD) { // NACK
		ReceiveAck(p, ch);
	}
	else if(ch.l3Prot == 0xFC) { // ACK
		ReceiveAck(p, ch);
	}

	return RdmaRxQueuePair::Receive(p, ch);
}


int RdmaReliableRQ::ReceiverCheckSeq(uint32_t seq, uint32_t size)
{
	// returns:
	//   - 1: Generate ACK (success)
	//   - 2: Generate NAK (failure, missing seqno)
	//   - 3: Duplicate, already ACKed (failure, do nothing)
	//   - 4: Error but NAK timer not yet elapsed (failure, do nothing)
	//   - 5: Success, don't send ACK
	uint32_t expected = ReceiverNextExpectedSeq;
	if (seq == expected){
		ReceiverNextExpectedSeq = expected + size;
		return 5;
	} else if (seq > expected) {
		// Generate NACK
		if (Simulator::Now() >= m_nackTimer || m_lastNACK != expected){
			m_nackTimer = Simulator::Now() + MicroSeconds(m_nack_interval);
			m_lastNACK = expected;
			if (m_backto0){
				ReceiverNextExpectedSeq = ReceiverNextExpectedSeq / m_chunk*m_chunk;
			}
			return 2;
		}else
			return 4;
	}else {
		// Duplicate. 
		return 3;
	}
}

/**
 * \return true If the interval between the two values spans on more than one chunk.
 */
static bool CrossBoundary(uint64_t chunksize, uint64_t oldval, uint64_t newval) {
	return (oldval / chunksize) != (newval / chunksize);
}

bool RdmaReliableSQ::ShouldReqAck(uint64_t payload_size) const
{
	// This is a change from the original project.
	// Previously, it was to the discretion of the receiver to choose when to send back ACKs.
	// Now, the sender can request for an ACK.
	// If the packet ReqAck bit is set, the receiver should to send back an ACK on success.
	// If the packet ReqAck bit is not set, it's to the discretion of the receiver to generate an ACK on success.
	// The logic is similar to the original `ReceiverCheckSeq()`, but with the following changes:
	//   - The last packet always generates an ACK. Previously, the QP would not properly shutdown if the last packet PSN was not multiple of the ACK interval.
	//   - Even if ACK is disabled, chunks are always ACKed (`m_chunk` and `m_ack_interval` are independant).

	const uint64_t psn = m_snd_nxt;
	
	// Care of Arithmetic Exception (% by zero).
	if(m_chunk != 0 && CrossBoundary(m_chunk, psn, psn + payload_size)) {
		return true; // End of chunk
	}
	if(m_ack_interval != 0 && CrossBoundary(m_ack_interval, psn, psn + payload_size)) {
		return true; // Milestone reached
	}

	return false;
}

void RdmaReliableRQ::ReceiveUdp(Ptr<Packet> p, const CustomHeader &ch)
{
	RdmaRxQueuePair::ReceiveUdp(p, ch);

	RdmaBTH bth;
	NS_ASSERT(p->PeekPacketTag(bth));
	if(!bth.GetReliable()) {
		return;
	}
	
	const uint8_t ecnbits = ch.GetIpv4EcnBits();
	const uint32_t payload_size = p->GetSize() - ch.GetSerializedSize();
	int x = ReceiverCheckSeq(ch.udp.seq, payload_size);
	
	if(x != 2 && bth.GetAckReq()) {
		x = 1; // If no NAK and ACK is requested, force an ACK
	}

	if (x == 1 || x == 2) { //generate ACK or NACK
		qbbHeader seqh;
		seqh.SetSeq(ReceiverNextExpectedSeq);
		seqh.SetPG(ch.udp.pg);
		seqh.SetSport(ch.udp.dport);
		seqh.SetDport(ch.udp.sport);
		seqh.SetIntHeader(ch.udp.ih);
		if (ecnbits)
			seqh.SetCnp();

		Ptr<Packet> newp = Create<Packet>(std::max(60-14-20-(int)seqh.GetSerializedSize(), 0));
		newp->AddHeader(seqh);

		Ipv4Header head;	// Prepare IPv4 header
		head.SetDestination(Ipv4Address(ch.sip));
		head.SetSource(Ipv4Address(ch.dip));
		head.SetProtocol(x == 1 ? 0xFC : 0xFD); //ack=0xFC nack=0xFD
		head.SetTtl(64);
		head.SetPayloadSize(newp->GetSize());
		head.SetIdentification(m_ipid++);

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
		p->AddHeader (ppp);

		// send
		Ptr<QbbNetDevice> dev = m_tx->GetDevice();
		dev->RdmaEnqueueHighPrioQ(newp);
		dev->TriggerTransmit();
	}
}


void RdmaReliableRQ::ReceiveAck(Ptr<Packet> p, const CustomHeader &ch){

	RdmaBTH bth;
	NS_ASSERT(p->PeekPacketTag(bth));
	const bool reliable = bth.GetReliable();
	NS_ASSERT(reliable);

	uint16_t qIndex = ch.ack.pg;
	uint16_t port = ch.ack.dport;
	uint32_t seq = ch.ack.seq;
	uint8_t cnp = (ch.ack.flags >> qbbHeader::FLAG_CNP) & 1;
	const bool nack = (ch.l3Prot == 0xFD);
	int i;
	
	Ptr<RdmaReliableSQ> tx = DynamicCast<RdmaReliableSQ>(m_tx);
	NS_ASSERT(tx);

	if (!m_backto0) {
		tx->Acknowledge(seq);
	}
	else {
		uint32_t goback_seq = seq / m_chunk * m_chunk;
		tx->Acknowledge(goback_seq);
	}
	
	if (nack) {
		tx->RecoverNack();
	}
	
	Ptr<RdmaHw> rdma = m_tx->GetNode()->GetObject<RdmaHw>();
	const uint32_t cc = rdma->GetCC();
	NS_ASSERT(cc == 1);
	
	if (cnp){
		if (cc == 1) { // mlx version
			//TODO
			//cnp_received_mlx(qp);
		} 
	}
	
	// ACK may advance the on-the-fly window, allowing more packets to send
	Ptr<QbbNetDevice> dev = m_tx->GetDevice();
	dev->TriggerTransmit();
}

}