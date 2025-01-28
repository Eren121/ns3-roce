#include "rdma-driver.h"

namespace ns3 {

/***********************
 * RdmaDriver
 **********************/
TypeId RdmaDriver::GetTypeId (void)
{
	static TypeId tid = TypeId ("ns3::RdmaDriver")
		.SetParent<Object> ()
		.AddTraceSource ("QpComplete", "A qp completes.",
				MakeTraceSourceAccessor (&RdmaDriver::m_traceQpComplete))
		;
	return tid;
}

RdmaDriver::RdmaDriver(){
}

void RdmaDriver::Init() {

	for (uint32_t i = 0; i < m_node->GetNDevices(); i++){
		Ptr<QbbNetDevice> dev = NULL;
		if (IsQbb(m_node->GetDevice(i)))
			dev = DynamicCast<QbbNetDevice>(m_node->GetDevice(i));
		m_rdma->m_nic.push_back(RdmaInterfaceMgr(dev));
	}	

	// RdmaHw do setup
	m_rdma->SetNode(m_node);
	m_rdma->Setup(MakeCallback(&RdmaDriver::QpComplete, this));
}

void RdmaDriver::SetNode(Ptr<Node> node){
	m_node = node;
}

void RdmaDriver::SetRdmaHw(Ptr<RdmaHw> rdma){
	m_rdma = rdma;
}

void RdmaDriver::AddQueuePair(uint64_t size, bool reliable, uint16_t pg, Ipv4Address sip, Ipv4Address dip, uint16_t sport, uint16_t dport, uint32_t win, uint64_t baseRtt, bool multicast, Callback<void> notifyAppFinish){
	m_rdma->AddQueuePair(size, reliable, pg, sip, dip, sport, dport, win, baseRtt, multicast, notifyAppFinish);
}

void RdmaDriver::QpComplete(Ptr<RdmaQueuePair> q){
	m_traceQpComplete(q);
}

} // namespace ns3
