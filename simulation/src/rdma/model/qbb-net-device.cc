/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
* Copyright (c) 2006 Georgia Tech Research Corporation, INRIA
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
* Author: Yuliang Li <yuliangli@g.harvard.com>
*/

#define __STDC_LIMIT_MACROS 1
#include <stdint.h>
#include <stdio.h>
#include "ns3/rdma-random.h"
#include "ns3/qbb-net-device.h"
#include "ns3/log.h"
#include "ns3/boolean.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "ns3/data-rate.h"
#include "ns3/object-vector.h"
#include "ns3/pause-header.h"
#include "ns3/drop-tail-queue.h"
#include "ns3/assert.h"
#include "ns3/abort.h"
#include "ns3/ipv4.h"
#include "ns3/ipv4-header.h"
#include "ns3/simulator.h"
#include "ns3/point-to-point-channel.h"
#include "ns3/qbb-channel.h"
#include "ns3/flow-id-tag.h"
#include "ns3/qbb-header.h"
#include "ns3/error-model.h"
#include "ns3/cn-header.h"
#include "ns3/ppp-header.h"
#include "ns3/udp-header.h"
#include "ns3/rdma-seq-header.h"
#include "ns3/pointer.h"
#include "ns3/custom-header.h"
#include "ns3/assert.h"
#include "ns3/switch-node.h"
#include "ns3/rdma-reliable-qp.h"
#include "ns3/rdma-hw.h"
#include <iostream>

namespace ns3 {
	
NS_LOG_COMPONENT_DEFINE("QbbNetDevice");
	
	uint32_t RdmaEgressQueue::ack_q_idx = 3;
	// RdmaEgressQueue
	TypeId RdmaEgressQueue::GetTypeId (void)
	{
		static TypeId tid = TypeId ("ns3::RdmaEgressQueue")
			.SetParent<Object> ()
			.AddTraceSource ("RdmaEnqueue", "Enqueue a packet in the RdmaEgressQueue.",
					MakeTraceSourceAccessor (&RdmaEgressQueue::m_traceRdmaEnqueue),
					"ns3::RdmaEgressQueue::TraceRdmaEnqueueCallback")
			.AddTraceSource ("RdmaDequeue", "Dequeue a packet in the RdmaEgressQueue.",
					MakeTraceSourceAccessor (&RdmaEgressQueue::m_traceRdmaDequeue),
					"ns3::RdmaEgressQueue::TraceRdmaDequeueCallback")
			;
		return tid;
	}

	RdmaEgressQueue::RdmaEgressQueue(){
		m_rrlast = 0;
		m_qlast = 0;
		m_ackQ = CreateObject<DropTailQueue<Packet>>();
		m_ackQ->SetAttribute("MaxSize", QueueSizeValue(QueueSize(BYTES, 0xffffffff))); // queue limit is on a higher level, not here
	}

	Ptr<Packet> RdmaEgressQueue::DequeueQindex(int qIndex){
		if (qIndex == -1){ // high prio
			Ptr<Packet> p = m_ackQ->Dequeue();
			m_qlast = -1;
			m_traceRdmaDequeue(p, 0);
			return p;
		}
		if (qIndex >= 0){ // qp
			Ptr<Packet> p = m_rdmaGetNxtPkt(m_qpGrp->Get(qIndex));
			m_rrlast = qIndex;
			m_qlast = qIndex;
			m_traceRdmaDequeue(p, m_qpGrp->Get(qIndex)->GetPG());
			return p;
		}
		return 0;
	}
	int RdmaEgressQueue::GetNextQindex(bool paused[])
	{
		NS_LOG_FUNCTION(this);

		bool found = false;
		uint32_t qIndex;
		if (!paused[ack_q_idx] && m_ackQ->GetNPackets() > 0) {
			NS_LOG_LOGIC("Next packet is an ACK");
			return -1;
		}

		// no pkt in highest priority queue, do rr for each qp
		int res = -1024;
		uint32_t fcount = m_qpGrp->GetN();
		uint32_t min_finish_id = 0xffffffff;
		for (qIndex = 1; qIndex <= fcount; qIndex++){
			uint32_t idx = (qIndex + m_rrlast) % fcount;
			Ptr<RdmaTxQueuePair> qp = m_qpGrp->Get(idx);
			if (!paused[qp->GetPG()] && qp->IsReadyToSend()){
				res = idx;
				break;
			}else if (qp->IsFinished()){
				min_finish_id = idx < min_finish_id ? idx : min_finish_id;
			}
		}

		// clear the finished qp
		if (min_finish_id < 0xffffffff) {
			m_qpGrp->RemoveFinished(min_finish_id, fcount, res);
		}

		return res;
	}

	int RdmaEgressQueue::GetLastQueue(){
		return m_qlast;
	}

	uint32_t RdmaEgressQueue::GetNBytes(uint32_t qIndex){
		#if RAF_WAITS_REFACTORING
			NS_ASSERT_MSG(qIndex < m_qpGrp->GetN(), "RdmaEgressQueue::GetNBytes: qIndex >= m_qpGrp->GetN()");
			return m_qpGrp->Get(qIndex)->GetBytesLeft();
		#else
			NS_ABORT_IF(true);
			return 0;
		#endif
	}

	uint32_t RdmaEgressQueue::GetFlowCount(void){
		return m_qpGrp->GetN();
	}

	Ptr<RdmaTxQueuePair> RdmaEgressQueue::GetQp(uint32_t i){
		return m_qpGrp->Get(i);
	}

	void RdmaEgressQueue::EnqueueHighPrioQ(Ptr<Packet> p){
		m_traceRdmaEnqueue(p, 0);
		m_ackQ->Enqueue(p);
	}

	void RdmaEgressQueue::CleanHighPrio(TracedCallback<Ptr<const Packet>, uint32_t> dropCb){
		while (m_ackQ->GetNPackets() > 0){
			Ptr<Packet> p = m_ackQ->Dequeue();
			dropCb(p, 0);
		}
	}

	/******************
	 * QbbNetDevice
	 *****************/
	NS_OBJECT_ENSURE_REGISTERED(QbbNetDevice);

	TypeId
		QbbNetDevice::GetTypeId(void)
	{
		static TypeId tid = TypeId("ns3::QbbNetDevice")
			.SetParent<PointToPointNetDevice>()
			.AddConstructor<QbbNetDevice>()
			.AddAttribute("QbbEnabled",
				"Enable the generation of PAUSE packet.",
				BooleanValue(false),
				MakeBooleanAccessor(&QbbNetDevice::m_qbbEnabled),
				MakeBooleanChecker())
			.AddAttribute("DynamicThreshold",
				"Enable dynamic threshold.",
				BooleanValue(false),
				MakeBooleanAccessor(&QbbNetDevice::m_dynamicth),
				MakeBooleanChecker())
			.AddAttribute("PauseTime",
				"Time to pause upon congestion",
				TimeValue(MicroSeconds(5)),
				MakeTimeAccessor(&QbbNetDevice::m_pausetime),
				MakeTimeChecker())
			.AddAttribute ("TxBeQueue", 
					"A queue to use as the transmit queue in the device.",
					PointerValue (),
					MakePointerAccessor (&QbbNetDevice::m_queue),
					MakePointerChecker<BEgressQueue> ())
			.AddAttribute ("RdmaEgressQueue", 
					"A queue to use as the transmit queue in the device.",
					PointerValue (),
					MakePointerAccessor (&QbbNetDevice::m_rdmaEQ),
					MakePointerChecker<Object> ())
			.AddTraceSource ("QbbEnqueue", "Enqueue a packet in the QbbNetDevice.",
					MakeTraceSourceAccessor (&QbbNetDevice::m_traceEnqueue),
					"ns3::QbbNetDevice::TraceEnqueueCallback")
			.AddTraceSource ("QbbDequeue", "Dequeue a packet in the QbbNetDevice.",
					MakeTraceSourceAccessor (&QbbNetDevice::m_traceDequeue),
					"ns3::QbbNetDevice::TraceDequeueCallback")
			.AddTraceSource ("QbbDrop", "Drop a packet in the QbbNetDevice.",
					MakeTraceSourceAccessor (&QbbNetDevice::m_traceDrop),
					"ns3::QbbNetDevice::TraceDropCallback")
			.AddTraceSource ("RdmaQpDequeue", "A qp dequeue a packet.",
					MakeTraceSourceAccessor (&QbbNetDevice::m_traceQpDequeue),
					"ns3::QbbNetDevice::TraceQpDequeueCallback")
			.AddTraceSource ("QbbPfc", "get a PFC packet. 0: resume, 1: pause",
					MakeTraceSourceAccessor (&QbbNetDevice::m_tracePfc),
					"ns3::QbbNetDevice::TracePfcCallback")
			;

		return tid;
	}

	QbbNetDevice::QbbNetDevice()
	{
		NS_LOG_FUNCTION(this);
		for (uint32_t i = 0; i < qCnt; i++){
			m_paused[i] = false;
		}

		m_rdmaEQ = CreateObject<RdmaEgressQueue>();
	}

	QbbNetDevice::~QbbNetDevice()
	{
		NS_LOG_FUNCTION(this);
	}

	DataRate QbbNetDevice::GetDataRate() const
	{
		return m_bps;
	}

	void
		QbbNetDevice::TransmitComplete(void)
	{
		NS_LOG_FUNCTION(this);
		NS_ASSERT_MSG(m_txMachineState == BUSY, "Must be BUSY if transmitting");
		m_txMachineState = READY;
		NS_ASSERT_MSG(m_currentPkt != 0, "QbbNetDevice::TransmitComplete(): m_currentPkt zero");
		m_phyTxEndTrace(m_currentPkt);
		m_currentPkt = 0;
		DequeueAndTransmit();
	}

	static std::string BoolsToStr(bool arr[], int size)
	{
		std::ostringstream ss;
		for(int i = 0; i < size; i++) {
			ss << (arr[i] ? "1" : "0");
		}
		return ss.str();
	}

	static bool IsAnyTrue(bool arr[], int size)
	{
		for(int i = 0; i < size; i++) {
			if(arr[i]) { return true; }
		}
		return false;
	}

	void
		QbbNetDevice::DequeueAndTransmit(void)
	{
		NS_LOG_FUNCTION(this);
		if (!m_linkUp) return; // if link is down, return
		if (m_txMachineState == BUSY) return;	// Quit if channel busy
		Ptr<Packet> p;
		if (!IsSwitchNode(m_node)) {
			int qIndex = m_rdmaEQ->GetNextQindex(m_paused);
			if (qIndex != -1024) {
				if (qIndex == -1){ // high prio
					p = m_rdmaEQ->DequeueQindex(qIndex);
					m_traceDequeue(p, 0);
					TransmitStart(p);
					return;
				}

				// a qp dequeue a packet
				Ptr<RdmaTxQueuePair> lastQp = m_rdmaEQ->GetQp(qIndex);
				p = m_rdmaEQ->DequeueQindex(qIndex);

				// transmit
				m_traceQpDequeue(p, lastQp);

				TransmitStart(p);

				// update for the next avail time
				m_rdmaPktSent(lastQp, p, m_tInterframeGap);
			}
			else { // no packet to send

				if(IsAnyTrue(m_paused, 8)) {			
					NS_LOG_INFO("PAUSE " << BoolsToStr(m_paused, 8) << " prohibits send at node " << m_node->GetId() << " (or no data to send)");
				}

				Time t = Simulator::GetMaximumSimulationTime();
				for (uint32_t i = 0; i < m_rdmaEQ->GetFlowCount(); i++){
					Ptr<RdmaTxQueuePair> qp = m_rdmaEQ->GetQp(i);
					if(qp->HasDataToSend() && !m_paused[qp->GetPG()]) {
						t = Min(qp->GetNextAvailTime(), t);
					}
				}

				if(t < Simulator::Now()) { t = Simulator::Now(); }
				// if(t < Simulator::GetMaximumSimulationTime()) { t += MicroSeconds(1); }
				
				// Multiple QPs with different rate share the same RdmaHW.
				// Thus, we wan to make sure that a low rate QP will not
				// limit the rate of a high rate QP.
				
				if (t < Simulator::GetMaximumSimulationTime()){
					UpdateNextAvail(t);
				}
			}
			return;
		}
		else {   //switch, doesn't care about qcn, just send
			p = GetQueue()->DequeueRR(m_paused);		//this is round-robin
			if (p != 0){
				m_snifferTrace(p);
				m_promiscSnifferTrace(p);
				Ipv4Header h;
				Ptr<Packet> packet = p->Copy();
				uint16_t protocol = 0;
				ProcessHeader(packet, protocol);
				packet->RemoveHeader(h);
				FlowIdTag t;
				uint32_t qIndex = GetQueue()->GetLastQueue();
				if (qIndex == 0){//this is a pause or cnp, send it immediately!
					SwitchNotifyDequeue(m_node, m_ifIndex, qIndex, p);
					p->RemovePacketTag(t);
				}else{
					SwitchNotifyDequeue(m_node, m_ifIndex, qIndex, p);
					p->RemovePacketTag(t);
				}
				m_traceDequeue(p, qIndex);
				TransmitStart(p);
				return;
			}
			else { //No queue can deliver any packet

				if(IsAnyTrue(m_paused, 8)) {		
					NS_LOG_INFO("PAUSE " << BoolsToStr(m_paused, 8) << " prohibits switch send at node " << m_node->GetId() << " (or no data to send)");
				}
			}
		}
		return;
	}

	void
		QbbNetDevice::Resume(unsigned qIndex)
	{
		NS_LOG_FUNCTION(this << qIndex);
		NS_ASSERT_MSG(m_paused[qIndex], "Must be PAUSEd");
		m_paused[qIndex] = false;
		NS_LOG_INFO("Node " << m_node->GetId() << " dev " << m_ifIndex << " queue " << qIndex <<
			" resumed at " << Simulator::Now().GetSeconds());
		DequeueAndTransmit();
	}

	void
		QbbNetDevice::Receive(Ptr<Packet> packet)
	{
		NS_LOG_FUNCTION(this << packet);
		if (!m_linkUp){
			NS_LOG_LOGIC("Drop (link down)");
			m_traceDrop(packet, 0);
			return;
		}

		if (m_receiveErrorModel && m_receiveErrorModel->IsCorrupt(packet))
		{
			// 
			// If we have an error model and it indicates that it is time to lose a
			// corrupted packet, don't forward this packet up, let it go.
			//
			NS_LOG_LOGIC("Drop (error model)");
			m_phyRxDropTrace(packet);
			return;
		}

		m_macRxTrace(packet);
		CustomHeader ch(CustomHeader::L2_Header | CustomHeader::L3_Header | CustomHeader::L4_Header);
		ch.getInt = 1; // parse INT header
		packet->PeekHeader(ch);
		
		if (ch.l3Prot == 0xFE){ // PFC
			if (!m_qbbEnabled) {
				NS_LOG_LOGIC("Drop (PFC packet; QBB disabled)");
				return;
			}

			unsigned qIndex = ch.pfc.qIndex;
			if (ch.pfc.time > 0){
				NS_LOG_LOGIC("PFC: Pause priority=" << qIndex << " time=" << ch.pfc.time);
				m_tracePfc(1);
				m_paused[qIndex] = true;
			}else{
				NS_LOG_LOGIC("PFC: Resume priority=" << qIndex);
				m_tracePfc(0);
				Resume(qIndex);
			}
		}else { // non-PFC packets (data, ACK, NACK, CNP...)
			if (IsSwitchNode(m_node)){ // switch
				packet->AddPacketTag(FlowIdTag(m_ifIndex));
				SwitchReceiveFromDevice(m_node, this, packet, ch);
			}else { // NIC
				// send to RdmaHw
				int ret = m_rdmaReceiveCb(packet, ch);
				// TODO we may based on the ret do something
			}
		}
		return;
	}

	bool QbbNetDevice::Send(Ptr<Packet> packet, const Address &dest, uint16_t protocolNumber)
	{
		NS_ASSERT_MSG(false, "QbbNetDevice::Send not implemented yet\n");
		return false;
	}

	bool QbbNetDevice::SwitchSend (uint32_t qIndex, Ptr<Packet> packet){
		m_macTxTrace(packet);
		m_traceEnqueue(packet, qIndex);
		
		if(!GetQueue()->Enqueue(packet, qIndex)) {
			NS_LOG_LOGIC("Drop: recv queue cannot enqueue");
		}

		DequeueAndTransmit();
		return true;
	}

	void QbbNetDevice::SendPfc(uint32_t qIndex, uint32_t type)
	{
		NS_LOG_FUNCTION(this);

		Ptr<Packet> p = Create<Packet>(0);

		// TODO Not implemented!!
		PauseHeader pauseh((type == 0 ? m_pausetime.GetMicroSeconds() : 0), qIndex);
		p->AddHeader(pauseh);
		Ipv4Header ipv4h;  // Prepare IPv4 header
		ipv4h.SetProtocol(0xFE);
		ipv4h.SetSource(m_node->GetObject<Ipv4>()->GetAddress(m_ifIndex, 0).GetLocal());
		ipv4h.SetDestination(Ipv4Address("255.255.255.255"));
		ipv4h.SetPayloadSize(p->GetSize());
		ipv4h.SetTtl(1);
		ipv4h.SetIdentification(GenRandomInt(65536));
		p->AddHeader(ipv4h);
		AddHeader(p, 0x800);
		CustomHeader ch(CustomHeader::L2_Header | CustomHeader::L3_Header | CustomHeader::L4_Header);
		p->PeekHeader(ch);
		SwitchSend(0, p);
	}

	bool
		QbbNetDevice::Attach(Ptr<QbbChannel> ch)
	{
		NS_LOG_FUNCTION(this << &ch);
		m_channel = ch;
		m_channel->Attach(this);
		NotifyLinkUp();
		return true;
	}

	bool
		QbbNetDevice::TransmitStart(Ptr<Packet> p)
	{
		NS_LOG_FUNCTION(this << p);
		NS_LOG_LOGIC("UID is " << p->GetUid() << ")");
		//
		// This function is called to start the process of transmitting a packet.
		// We need to tell the channel that we've started wiggling the wire and
		// schedule an event that will be executed when the transmission is complete.
		//
		NS_ASSERT_MSG(m_txMachineState == READY, "Must be READY to transmit");
		m_txMachineState = BUSY;
		m_currentPkt = p;
		m_phyTxBeginTrace(m_currentPkt);
		Time txTime = m_bps.CalculateBytesTxTime(p->GetSize());
		Time txCompleteTime = txTime + m_tInterframeGap;
		// NS_LOG_LOGIC("Schedule TransmitCompleteEvent in " << txCompleteTime.GetSeconds() << "sec");
		Simulator::Schedule(txCompleteTime, &QbbNetDevice::TransmitComplete, this);

		bool result = m_channel->TransmitStart(p, this, txTime);
		if (result == false)
		{
			m_phyTxDropTrace(p);
		}
		return result;
	}

	Ptr<Channel>
		QbbNetDevice::GetChannel(void) const
	{
		return m_channel;
	}

   void QbbNetDevice::NewQp(Ptr<RdmaTxQueuePair> qp){
	   DequeueAndTransmit();
   }
   void QbbNetDevice::ReassignedQp(Ptr<RdmaTxQueuePair> qp){
	   DequeueAndTransmit();
   }
   void QbbNetDevice::TriggerTransmit(void){
	   DequeueAndTransmit();
   }

	void QbbNetDevice::SetQueue(Ptr<BEgressQueue> q){
		NS_LOG_FUNCTION(this << q);
		m_queue = q;
	}

	Ptr<BEgressQueue> QbbNetDevice::GetQueue(){
		return DynamicCast<BEgressQueue>(m_queue);
	}

	Ptr<RdmaEgressQueue> QbbNetDevice::GetRdmaQueue(){
		return m_rdmaEQ;
	}

	void QbbNetDevice::RdmaEnqueueHighPrioQ(Ptr<Packet> p){
		m_traceEnqueue(p, 0);
		m_rdmaEQ->EnqueueHighPrioQ(p);
	}

	void QbbNetDevice::TakeDown(){
		// TODO: delete packets in the queue, set link down
		if (!IsSwitchNode(m_node)){
			// clean the high prio queue
			m_rdmaEQ->CleanHighPrio(m_traceDrop);
			// notify driver/RdmaHw that this link is down
			m_rdmaLinkDownCb(this);
		}else { // switch
			// clean the queue
			for (uint32_t i = 0; i < qCnt; i++)
				m_paused[i] = false;
			while (1){
				Ptr<Packet> p = GetQueue()->DequeueRR(m_paused);
				if (p == 0)
					 break;
				m_traceDrop(p, GetQueue()->GetLastQueue());
			}
			// TODO: Notify switch that this link is down
		}
		m_linkUp = false;
	}

	void QbbNetDevice::UpdateNextAvail(Time t) {
		// This can only make the next event more quick

		if (m_nextSend.IsExpired() || t < Time(m_nextSend.GetTs())) {
			if(!m_nextSend.IsExpired()) {
				Simulator::Cancel(m_nextSend);
			}

			Time delta = t < Simulator::Now() ? Time(0) : t - Simulator::Now();
			m_nextSend = Simulator::Schedule(delta, &QbbNetDevice::DequeueAndTransmit, this);
		}
	}

	void QbbNetDevice::OnPeerJoinGroup(uint32_t group)
	{
		Ptr<SwitchNode> sw = DynamicCast<SwitchNode>(GetNode());
		if(sw) {
			sw->OnPeerJoinGroup(m_ifIndex, group);
		}
	}
	
  Ptr<QbbNetDevice> QbbNetDevice::GetPeerNetDevice() const
	{
		for(int i{0}; i < 2; i++) {
			
			if(m_channel->GetPointToPointDevice(i) == this) {
				
				const auto peer{m_channel->GetPointToPointDevice(1 - i)};
				NS_ASSERT_MSG(peer, "Peer device does not exist");
				
				const auto qbb{DynamicCast<QbbNetDevice>(peer)};
				NS_ASSERT_MSG(qbb, "Peer device is not a QbbNetDevice");

				return qbb;
			}
		}
		
		NS_ABORT_MSG("Peer device not found");
		return {};
	}

	void QbbNetDevice::AddGroup(uint32_t group)
	{
		if(m_groups.count(group) > 0) {
			return; // No need to explore the network graph again.
		}
		m_groups.insert(group);
		
		// Notify peer of the channel
		Ptr<NetDevice> peer;
		{
			Ptr<NetDevice> n[2] = { m_channel->GetDevice(0), m_channel->GetDevice(1) };
			if(n[0] == this) {
				peer = DynamicCast<QbbNetDevice>(n[1]);
			}
			else {
				peer = DynamicCast<QbbNetDevice>(n[0]);
			}
		}
		if(!peer) {
			return;
		}

		Ptr<QbbNetDevice> peer_qbb = DynamicCast<QbbNetDevice>(peer);
		if(!peer_qbb) {
			return;
		}
		peer_qbb->OnPeerJoinGroup(group); // If the uplink is NIC, the early return will stop and avoid infinite loop.
	}
	
	bool IsQbb(Ptr<const NetDevice> self)
	{
		return DynamicCast<const QbbNetDevice>(self) != nullptr;
	}

	Ipv4Address GetServerAddress(Ptr<const Node> node)
	{
		return node->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal();
	}

} // namespace ns3
