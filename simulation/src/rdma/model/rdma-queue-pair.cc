#include <ns3/uinteger.h>
#include <ns3/qbb-net-device.h>
#include <ns3/udp-header.h>
#include <ns3/ipv4-header.h>
#include <ns3/simulator.h>
#include <ns3/ppp-header.h>
#include <ns3/rdma-seq-header.h>
#include <ns3/rdma-queue-pair.h>
#include <ns3/rdma-hw.h>
#include <ns3/rdma-bth.h>
#include <ns3/qbb-header.h>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("RdmaQP");

/**************************
 * RdmaTxQueuePair
 *************************/
TypeId RdmaTxQueuePair::GetTypeId (void)
{
	static TypeId tid = TypeId ("ns3::RdmaTxQueuePair")
		.SetParent<Object> ()
	;
	return tid;
}

RdmaTxQueuePair::RdmaTxQueuePair(Ptr<Node> node, uint16_t pg, Ipv4Address sip, uint16_t sport)
{
	m_node = node;
	m_pg = pg;
	m_sip = sip;
	m_sport = sport;
	m_max_rate = 0;
	m_rate = 0;
	m_nextAvail = Simulator::Now();
	mlx.m_alpha = 1;
	mlx.m_alpha_cnp_arrived = false;
	mlx.m_first_cnp = true;
	mlx.m_decrease_cnp_arrived = false;
	mlx.m_rpTimeStage = 0;
	hp.m_lastUpdateSeq = 0;
	for (uint32_t i = 0; i < sizeof(hp.keep) / sizeof(hp.keep[0]); i++)
		hp.keep[i] = 0;
	hp.m_incStage = 0;
	hp.m_lastGap = 0;
	hp.u = 1;
	for (uint32_t i = 0; i < IntHeader::maxHop; i++){
		hp.hopState[i].u = 1;
		hp.hopState[i].incStage = 0;
	}

	tmly.m_lastUpdateSeq = 0;
	tmly.m_incStage = 0;
	tmly.lastRtt = 0;
	tmly.rttDiff = 0;

	dctcp.m_lastUpdateSeq = 0;
	dctcp.m_caState = 0;
	dctcp.m_highSeq = 0;
	dctcp.m_alpha = 1;
	dctcp.m_ecnCnt = 0;
	dctcp.m_batchSizeOfAlpha = 0;

	hpccPint.m_lastUpdateSeq = 0;
	hpccPint.m_incStage = 0;
}

RdmaTxQueuePair::~RdmaTxQueuePair()
{
	Simulator::Cancel(mlx.m_eventUpdateAlpha);
	Simulator::Cancel(mlx.m_eventDecreaseRate);
	Simulator::Cancel(mlx.m_rpTimer);
}

Ptr<QbbNetDevice> RdmaTxQueuePair::GetDevice()
{
	// Assume each server has only one NIC
	NS_ASSERT(m_node->GetNDevices() == 2);
	return DynamicCast<QbbNetDevice>(m_node->GetDevice(1));	
}

void RdmaTxQueuePair::LazyInitCnp()
{
	// Assume each server has only one NIC
	Ptr<QbbNetDevice> dev = GetDevice();
	Ptr<RdmaHw> rdma = m_node->GetObject<RdmaHw>();
	const uint32_t cc = rdma->GetCC();
	NS_ASSERT(cc == 1);
	
	if (m_rate == 0)			//lazy initialization	
	{
		m_rate = dev->GetDataRate();
		if (cc == 1) {
			mlx.m_targetRate = dev->GetDataRate();
		}
	}
}

/*********************
 * RdmaRxQueuePair
 ********************/

RdmaRxQueuePair::RdmaRxQueuePair(Ptr<RdmaTxQueuePair> tx)
	: m_tx{tx},
	  m_local_ip{m_tx->GetSrcIP().Get()},
	  m_local_port{m_tx->GetSrcPort()}
{
}

TypeId RdmaRxQueuePair::GetTypeId (void)
{
	static TypeId tid = TypeId ("ns3::RdmaRxQueuePair")
		.SetParent<Object> ()
		;
	return tid;
}

bool RdmaRxQueuePair::Receive(Ptr<Packet> p, const CustomHeader &ch)
{
	if (ch.l3Prot == 0x11) { // UDP
		ReceiveUdp(p, ch);
	}
	else if (ch.l3Prot == 0xFF) { // CNP
		ReceiveCnp(p, ch);
	}
	else {
		return false;
	}

	return true;
}

void RdmaRxQueuePair::ReceiveUdp(Ptr<Packet> p, const CustomHeader &ch)
{
	const uint8_t ecnbits = ch.GetIpv4EcnBits();
	const uint32_t payload_size = p->GetSize() - ch.GetSerializedSize();
	
	if (ecnbits != 0) {
		m_ecn_source.ecnbits |= ecnbits;
		m_ecn_source.qfb++;
	}

	m_ecn_source.total++;
}

void RdmaRxQueuePair::ReceiveCnp(Ptr<Packet> p, const CustomHeader &ch)
{
	NS_LOG_FUNCTION(this);
	// QCN on NIC
	// This is a Congestion signal
	// Then, extract data from the congestion packet.
	// We assume, without verify, the packet is destinated to me
	const uint32_t qIndex = ch.cnp.qIndex;
	if (qIndex == 1){		//DCTCP
		std::cout << "TCP--ignore\n";
		return;
	}
	uint16_t udpport = ch.cnp.fid; // corresponds to the sport
	uint8_t ecnbits = ch.cnp.ecnBits;
	uint16_t qfb = ch.cnp.qfb;
	uint16_t total = ch.cnp.total;

	uint32_t i;
	
	m_tx->LazyInitCnp();
}

/*********************
 * RdmaTxQueuePairGroup
 ********************/

TypeId RdmaTxQueuePairGroup::GetTypeId ()
{
	static TypeId tid = TypeId ("ns3::RdmaTxQueuePairGroup")
		.SetParent<Object> ()
		;
	return tid;
}

Ptr<QbbNetDevice> RdmaTxQueuePairGroup::GetDevice()
{
	return m_dev;
}

uint32_t RdmaTxQueuePairGroup::GetN()
{
	return m_qps.size();
}

Ptr<RdmaTxQueuePair> RdmaTxQueuePairGroup::Get(uint32_t idx)
{
	return m_qps[idx];
}

Ptr<RdmaTxQueuePair> RdmaTxQueuePairGroup::operator[](uint32_t idx)
{
	return m_qps[idx];
}

RdmaTxQueuePairGroup::RdmaTxQueuePairGroup(Ptr<QbbNetDevice> dev)
	: m_dev(dev)
{
}

void RdmaTxQueuePairGroup::AddQp(Ptr<RdmaTxQueuePair> qp)
{
	m_qps.push_back(qp);
}

void RdmaTxQueuePairGroup::Clear()
{
	m_qps.clear();
}

void RdmaTxQueuePairGroup::RemoveFinished(int begin, int end, int& res)
{
	int nxt = begin;
	
	for (int i = begin; i < end; i++) {
		if (!m_qps[i]->IsFinished()) {
			if (i == res) {
				res = nxt;
			}
			m_qps[nxt] = m_qps[i];
			nxt++;
		}
	}
	m_qps.resize(nxt);
}

}
