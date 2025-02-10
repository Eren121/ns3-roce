#include <ns3/rdma-reliable-qp.h>
#include <ns3/qbb-net-device.h>
#include <ns3/rdma-hw.h>
#include <ns3/rdma-bth.h>
#include <ns3/qbb-header.h>
#include <ns3/simulator.h>
#include <ns3/ppp-header.h>
#include <ns3/rdma-seq-header.h>
#include <ns3/log.h>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("RdmaReliableQP");

RdmaReliableSQ::RdmaReliableSQ(Ptr<Node> node, uint16_t pg, Ipv4Address sip, uint16_t sport, Ipv4Address dip, uint16_t dport)
    : RdmaTxQueuePair(node, pg, sip, sport),
      m_dip(dip),
      m_dport(dport)
{
	NS_LOG_FUNCTION(this);
}

RdmaReliableSQ::~RdmaReliableSQ() = default;

void RdmaReliableSQ::SetAckInterval(uint64_t chunk, uint64_t ack_interval)
{
	NS_LOG_FUNCTION(this << chunk << ack_interval);

	m_chunk = chunk;
	m_ack_interval = ack_interval;
}

void RdmaReliableSQ::Acknowledge(uint64_t next_psn_expected)
{
	NS_LOG_FUNCTION(this << next_psn_expected);
	
	if(next_psn_expected > m_snd_una) {
		m_snd_una = next_psn_expected;
		NotifyPendingCompEvents();
	}
	
	ScheduleRetrTimeout();
}

void RdmaReliableSQ::ScheduleRetrTimeout()
{
	if(m_retr_to.IsRunning()) {
		m_retr_to.Cancel();
	}
	
	const bool infly_acks = (highest_ack_psn > m_snd_una); 
	if(!infly_acks) {
		return;
	}

	// https://www.rdmamojo.com/2013/01/12/ibv_modify_qp/
	const Time retr_timeout = MicroSeconds(65.556);
	m_retr_to = Simulator::Schedule(retr_timeout, &RdmaReliableSQ::OnRetrTimeout, this);
}

void RdmaReliableSQ::OnRetrTimeout()
{
	NS_LOG_FUNCTION(this);
	RecoverNack(m_snd_una);
}

void RdmaReliableSQ::NotifyPendingCompEvents()
{
	while(!m_to_send.empty()) {
		auto it{m_to_send.begin()};
		const SendRequest& next{it->second};
		const psn_t sr_end_psn{next.GetEndPSN()};
		if(m_snd_una < sr_end_psn) {
			break;
		}
		const OnSendCallback& on_ack{next.on_send};
		if(on_ack) { on_ack(); }
		m_to_send.erase(it);

		// This may unblock, because next op could have been blocked until ACK is received
		TriggerDevTransmit();
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
	const bool timer_ready = Simulator::Now().GetTimeStep() >= m_nextAvail.GetTimeStep();
	if(!timer_ready) {
		return false;
	}

	if(IsWinBound()) {
		return false;
	}

	const bool has_data_to_send = (m_next_op_first_psn > m_snd_nxt);
	if(!has_data_to_send) {
		return false;
	}

	return true;
}

void RdmaReliableSQ::PostSend(SendRequest sr)
{
	NS_LOG_FUNCTION(this);
	
	if(sr.payload_size == 0) {
		// Zero payload size not supported,
		// because PSN should be incremented
		sr.payload_size = 1;
	}

	sr.first_psn = m_next_op_first_psn;
	m_next_op_first_psn += sr.payload_size;

	m_to_send[sr.first_psn] = sr;

	TriggerDevTransmit();
}

void RdmaReliableSQ::TriggerDevTransmit()
{
	// Trigger to possibly send
	if(m_nextAvail <= Simulator::Now()) {
		Simulator::Schedule(Seconds(0), &QbbNetDevice::TriggerTransmit, GetDevice());
	}
	else {
		Simulator::Schedule(m_nextAvail - Simulator::Now(), &QbbNetDevice::TriggerTransmit, GetDevice());
	}
}

Ptr<Packet> RdmaReliableSQ::GetNextPacket()
{
	NS_LOG_FUNCTION(this);

	if(m_to_send.empty()) {
		NS_ASSERT(false);
		return nullptr;
	}

	const SendRequest& sr{(--m_to_send.upper_bound(m_snd_nxt))->second};
	const psn_t sr_already_sent_payload{m_snd_nxt - sr.first_psn};
	const psn_t sr_rem_to_send{sr.payload_size - sr_already_sent_payload};

	NS_ASSERT(sr_rem_to_send > 0);
	const psn_t packet_size = std::min<psn_t>(m_mtu, sr_rem_to_send);

	RdmaBTH bth;
	bth.SetReliable(true);
	bth.SetMulticast(false);
	bth.SetDestQpKey(m_dport);

	const bool op_last_pkt = (m_snd_nxt + packet_size == sr.GetEndPSN());
	
	if (!op_last_pkt) {
		bth.SetNotif(false);
		bth.SetAckReq(ShouldReqAck(packet_size));
	}
	else {
		bth.SetNotif(true); // Last packet, generate a notif on the RX
		bth.SetAckReq(true);
		bth.SetImm(sr.imm);
	}
	
	NS_LOG_INFO("Sending psn=" << m_snd_nxt << ",payload_size=" << packet_size);
	Ptr<Packet> p = Create<Packet>(packet_size);

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
	m_snd_nxt += packet_size;
	m_ipid++;

	if(bth.GetAckReq()) {
		NS_ASSERT(m_snd_nxt > highest_ack_psn);
		highest_ack_psn = m_snd_nxt;
		if(!m_retr_to.IsRunning()) {
			ScheduleRetrTimeout();
		}
	}

	// return
	return p;
}

void RdmaReliableSQ::RecoverNack(uint64_t next_psn_expected)
{
	NS_LOG_FUNCTION(this);
	NS_ASSERT(next_psn_expected >= m_snd_una);

	Acknowledge(next_psn_expected);
	Rollback();
	TriggerDevTransmit();
}

void RdmaReliableSQ::Rollback()
{
	NS_ASSERT(!m_to_send.empty());
	m_snd_nxt = m_snd_una;
	highest_ack_psn = m_snd_una;

	ScheduleRetrTimeout();
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
	NS_LOG_FUNCTION(this);

	if(ch.l3Prot == 0xFD) { // NACK
		ReceiveAck(p, ch);
	}
	else if(ch.l3Prot == 0xFC) { // ACK
		ReceiveAck(p, ch);
	}

	return RdmaRxQueuePair::Receive(p, ch);
}

RdmaReliableRQ::RdmaReliableRQ(Ptr<RdmaReliableSQ> sq)
	: RdmaRxQueuePair{sq}
{
}

int RdmaReliableRQ::ReceiverCheckSeq(uint32_t seq, uint32_t size)
{
	NS_LOG_FUNCTION(this << seq << size);

	// returns:
	//   - 1: Generate ACK (success)
	//   - 2: Generate NAK (failure, missing seqno)
	//   - 3: Duplicate, already ACKed (failure, do nothing)
	//   - 4: Error but NAK timer not yet elapsed (failure, do nothing)
	//   - 5: Success, don't send ACK
	uint32_t expected = ReceiverNextExpectedSeq;
	if (seq == expected){
		NS_LOG_LOGIC("Received data {expected=" << expected << ",size=" << size << "}");
		ReceiverNextExpectedSeq = expected + size;
		return 5;
	} else if (seq > expected) {
		NS_LOG_LOGIC("Seq. received is higher than expected (" << seq << ">" << expected << ")");
		// Generate NACK
		if (Simulator::Now() >= m_nackTimer || m_lastNACK != expected){
			NS_LOG_LOGIC("Send NACK");	
			m_nackTimer = Simulator::Now() + MicroSeconds(m_nack_interval);
			m_lastNACK = expected;
			if (m_backto0){
				ReceiverNextExpectedSeq = ReceiverNextExpectedSeq / GetChunk() * GetChunk();
			}
			return 2;
		}else
			return 4;
	}else {
		NS_LOG_LOGIC("Duplicate received {expected=" << expected << ",recv=" << seq << "}");
		return 3;
	}
}

/**
 * \return true If the interval between the two values spans on more than one chunk.
 */
static bool CrossBoundary(uint64_t chunksize, uint64_t oldval, uint64_t newval)
{
	return (oldval / chunksize) != (newval / chunksize);
}

bool RdmaReliableSQ::ShouldReqAck(uint64_t payload_size) const
{
	NS_LOG_FUNCTION(this << payload_size);

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
	NS_LOG_FUNCTION(this);

	RdmaRxQueuePair::ReceiveUdp(p, ch);

	RdmaBTH bth;
	NS_ABORT_UNLESS(p->PeekPacketTag(bth));
	NS_ASSERT(bth.GetReliable());
	NS_ASSERT(ch.dip == m_local_ip);
	NS_ASSERT(ch.sip == DynamicCast<RdmaReliableSQ>(m_tx)->GetDestIP());
	NS_ASSERT(ch.udp.sport == DynamicCast<RdmaReliableSQ>(m_tx)->GetDestPort());
	NS_ASSERT(ch.udp.dport == m_local_port);
	
	const uint8_t ecnbits = ch.GetIpv4EcnBits();
	const uint32_t payload_size = p->GetSize() - ch.GetSerializedSize();
	int x = ReceiverCheckSeq(ch.udp.seq, payload_size);
	const bool success{x == 1 || x == 5};

	if(bth.GetAckReq()) {
			NS_LOG_LOGIC("ACK requested, sending back ACK");
			if(x == 5 || x == 3) {
				x = 1;
			}
			else if(x == 4) {
				x = 2;
			}
	}

	if (x == 1 || x == 2) { //generate ACK or NACK
		
		NS_LOG_LOGIC("Send back " << (x == 1 ? "ACK" : "NACK"));

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
		newp->AddHeader (ppp);

		RdmaBTH bth;
		bth.SetDestQpKey(ch.udp.sport);
		newp->AddPacketTag(bth);

		// send
		Ptr<QbbNetDevice> dev = m_tx->GetDevice();
		dev->RdmaEnqueueHighPrioQ(newp);
		dev->TriggerTransmit();
	}

	// If no error, call completion event
	if(m_onRecv && success && bth.GetNotif()) {
		RecvNotif notif;
		notif.has_imm = true;
		notif.imm = bth.GetImm();
		m_onRecv(notif);
	}
}

void RdmaReliableRQ::ReceiveAck(Ptr<Packet> p, const CustomHeader &ch){

	NS_LOG_FUNCTION(this);

	RdmaBTH bth;
	NS_ABORT_UNLESS(p->PeekPacketTag(bth));
	const bool reliable = bth.GetReliable();
	NS_ASSERT(reliable);

	const uint16_t qIndex = ch.ack.pg;
	const uint16_t port = ch.ack.dport;
	const uint32_t seq = ch.ack.seq;
	const uint8_t cnp = (ch.ack.flags >> qbbHeader::FLAG_CNP) & 1;
	const bool nack = (ch.l3Prot == 0xFD);
	int i;
	
	Ptr<RdmaReliableSQ> tx = DynamicCast<RdmaReliableSQ>(m_tx);
	NS_ASSERT(tx);

	if (!m_backto0) {
		tx->Acknowledge(seq);
	}
	else {
		uint32_t goback_seq = seq / GetChunk() * GetChunk();
		tx->Acknowledge(goback_seq);
	}
	
	if (nack) {
		tx->RecoverNack(seq);
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