#include <ns3/simulator.h>
#include <ns3/rdma-seq-header.h>
#include <ns3/udp-header.h>
#include <ns3/ipv4-header.h>
#include "ns3/ppp-header.h"
#include "ns3/boolean.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "ns3/data-rate.h"
#include "ns3/pointer.h"
#include "ns3/data-rate-ops.h"
#include "ns3/assert.h"
#include "rdma-hw.h"
#include "ns3/ppp-header.h"
#include "qbb-header.h"
#include "rdma-bth.h"
#include "cn-header.h"
#include "ns3/rdma-reliable-qp.h"
#include "ns3/rdma-unreliable-qp.h"
#include "ns3/log.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("RdmaHw");
NS_OBJECT_ENSURE_REGISTERED(RdmaHw);

TypeId RdmaHw::GetTypeId (void)
{
	static TypeId tid = TypeId ("ns3::RdmaHw")
		.SetParent<Object> ()
		.AddTraceSource ("QpComplete", "A qp completes.",
				MakeTraceSourceAccessor (&RdmaHw::m_traceQpComplete),
				"ns3::RdmaHw::TraceQpCompleteCallback")
		.AddAttribute("MinRate",
				"Minimum rate of a throttled flow",
				DataRateValue(DataRate("100Mb/s")),
				MakeDataRateAccessor(&RdmaHw::m_minRate),
				MakeDataRateChecker())
		.AddAttribute("Mtu",
				"Mtu.",
				UintegerValue(1000),
				MakeUintegerAccessor(&RdmaHw::m_mtu),
				MakeUintegerChecker<uint32_t>())
		.AddAttribute ("CcMode",
				"which mode of DCQCN is running",
				UintegerValue(0),
				MakeUintegerAccessor(&RdmaHw::m_cc_mode),
				MakeUintegerChecker<uint32_t>())
		.AddAttribute("NackInterval",
				"The NACK Generation interval.",
				TimeValue(MicroSeconds(500.0)),
				MakeTimeAccessor(&RdmaHw::m_nack_interval),
				MakeTimeChecker())
		.AddAttribute("L2ChunkSize",
				"Layer 2 chunk size for Go-Back-0. Disable chunk mode if equals to 0.",
				UintegerValue(0),
				MakeUintegerAccessor(&RdmaHw::m_chunk),
				MakeUintegerChecker<uint32_t>())
		.AddAttribute("L2AckInterval",
				"Layer 2 Ack intervals. Disable ack if equals to 0.",
				UintegerValue(0),
				MakeUintegerAccessor(&RdmaHw::m_ack_interval),
				MakeUintegerChecker<uint32_t>())
		.AddAttribute("L2BackToZero",
				"Layer 2 go back to zero transmission.",
				BooleanValue(false),
				MakeBooleanAccessor(&RdmaHw::m_backto0),
				MakeBooleanChecker())
		.AddAttribute("EwmaGain",
				"Control gain parameter which determines the level of rate decrease",
				DoubleValue(1.0 / 16),
				MakeDoubleAccessor(&RdmaHw::m_g),
				MakeDoubleChecker<double>())
		.AddAttribute ("RateOnFirstCnp",
				"the fraction of rate on first CNP",
				DoubleValue(1.0),
				MakeDoubleAccessor(&RdmaHw::m_rateOnFirstCNP),
				MakeDoubleChecker<double> ())
		.AddAttribute("ClampTargetRate",
				"Clamp target rate.",
				BooleanValue(false),
				MakeBooleanAccessor(&RdmaHw::m_EcnClampTgtRate),
				MakeBooleanChecker())
		.AddAttribute("RPTimer",
				"The rate increase timer at RP in microseconds",
				DoubleValue(1500.0),
				MakeDoubleAccessor(&RdmaHw::m_rpgTimeReset),
				MakeDoubleChecker<double>())
		.AddAttribute("RateDecreaseInterval",
				"The interval of rate decrease check",
				DoubleValue(4.0),
				MakeDoubleAccessor(&RdmaHw::m_rateDecreaseInterval),
				MakeDoubleChecker<double>())
		.AddAttribute("FastRecoveryTimes",
				"The rate increase timer at RP",
				UintegerValue(5),
				MakeUintegerAccessor(&RdmaHw::m_rpgThreshold),
				MakeUintegerChecker<uint32_t>())
		.AddAttribute("AlphaResumInterval",
				"The interval of resuming alpha",
				DoubleValue(55.0),
				MakeDoubleAccessor(&RdmaHw::m_alpha_resume_interval),
				MakeDoubleChecker<double>())
		.AddAttribute("RateAI",
				"Rate increment unit in AI period",
				DataRateValue(DataRate("5Mb/s")),
				MakeDataRateAccessor(&RdmaHw::m_rai),
				MakeDataRateChecker())
		.AddAttribute("RateHAI",
				"Rate increment unit in hyperactive AI period",
				DataRateValue(DataRate("50Mb/s")),
				MakeDataRateAccessor(&RdmaHw::m_rhai),
				MakeDataRateChecker())
		.AddAttribute("VarWin",
				"Use variable window size or not",
				BooleanValue(false),
				MakeBooleanAccessor(&RdmaHw::m_var_win),
				MakeBooleanChecker())
		.AddAttribute("FastReact",
				"Fast React to congestion feedback",
				BooleanValue(true),
				MakeBooleanAccessor(&RdmaHw::m_fast_react),
				MakeBooleanChecker())
		.AddAttribute("RateBound",
				"Bound packet sending by rate, for test only",
				BooleanValue(true),
				MakeBooleanAccessor(&RdmaHw::m_rateBound),
				MakeBooleanChecker())
		;
	return tid;
}

void RdmaHw::Setup()
{
	NS_LOG_FUNCTION(this);
	m_node = GetObject<Node>();
	NS_ASSERT(m_node);
		
	for (uint32_t i = 0; i < m_node->GetNDevices(); i++){
		Ptr<QbbNetDevice> dev = NULL;
		if (!IsQbb(m_node->GetDevice(i)))
			continue;
		
		dev = DynamicCast<QbbNetDevice>(m_node->GetDevice(i));
		m_nic.push_back(RdmaTxQueuePairGroup(dev));
	}

	for (uint32_t i = 0; i < m_nic.size(); i++){
		Ptr<QbbNetDevice> dev = m_nic[i].GetDevice();
		if (dev == NULL)
			continue;
		// share data with NIC
		dev->m_rdmaEQ->m_qpGrp = &m_nic[i];
		// setup callback
		dev->m_rdmaReceiveCb = MakeCallback(&RdmaHw::Receive, this);
		dev->m_rdmaLinkDownCb = MakeCallback(&RdmaHw::SetLinkDown, this);
		dev->m_rdmaPktSent = MakeCallback(&RdmaHw::PktSent, this);
		// config NIC
		dev->m_rdmaEQ->m_rdmaGetNxtPkt = MakeCallback(&RdmaHw::GetNxtPacket, this);
	}
}

RdmaReliableQP RdmaHw::CreateReliableQP(uint16_t pg, uint16_t sport, Ipv4Address dip, uint16_t dport)
{
	RdmaReliableQP qp;
  qp.sq = CreateObject<RdmaReliableSQ>(m_node, pg, GetServerAddress(m_node), sport, dip, dport);
  qp.rq = CreateObject<RdmaReliableRQ>(qp.sq);
  RegisterQP(qp.sq, qp.rq);

	return qp;
}

RdmaUnreliableQP RdmaHw::CreateUnreliableQP(uint16_t pg, uint16_t sport)
{
	RdmaUnreliableQP qp;
  qp.sq = CreateObject<RdmaUnreliableSQ>(m_node, pg, GetServerAddress(m_node), sport);
  qp.rq = CreateObject<RdmaUnreliableRQ>(qp.sq);
  RegisterQP(qp.sq, qp.rq);
	
	return qp;
}

void RdmaHw::RegisterQP(Ptr<RdmaTxQueuePair> sq, Ptr<RdmaRxQueuePair> rq)
{
	Ptr<RdmaReliableRQ> rel_rq = DynamicCast<RdmaReliableRQ>(rq);
	if(rel_rq) {
		rel_rq->SetNackInterval(m_nack_interval);
		// DynamicCast<RdmaReliableSQ>(sq)->SetWin(0.95);
		DynamicCast<RdmaReliableSQ>(sq)->SetAckInterval(m_chunk, m_ack_interval);
	}
	const uint64_t key = sq->GetKey();

	// Store in map
	m_rxQpMap[key] = rq;

	// add qp
	m_qpMap[key] = sq;

	// set init variables
	NS_ASSERT(m_cc_mode == 1);
	DataRate m_bps = sq->GetDevice()->GetDataRate();
	sq->SetMaxRate(m_bps);
	sq->SetMTU(m_mtu);

	// Notify Nic
	NS_ASSERT(m_nic.size() == 1);
	m_nic[0].AddQp(sq);
	sq->GetDevice()->NewQp(sq);
}

uint64_t RdmaHw::GetRxQpKey(uint16_t dport)
{
	return dport;
}

void RdmaHw::DeleteQueuePair(Ptr<RdmaTxQueuePair> qp){
	// remove qp from the m_qpMap
	uint64_t key = qp->GetKey();
	m_qpMap.erase(key);
}

Ptr<RdmaRxQueuePair> RdmaHw::GetRxQp(uint32_t sip, uint32_t dip, uint16_t sport, uint16_t dport, uint16_t pg, bool create){
	uint64_t key = GetRxQpKey(dport);
	auto it = m_rxQpMap.find(key);
	NS_ASSERT_MSG(it != m_rxQpMap.end(), "Cannot find the destination RQ");
	if (it != m_rxQpMap.end())
		return it->second;
	if (create){
		// create new rx qp
		Ptr<RdmaRxQueuePair> q = CreateObject<RdmaRxQueuePair>(m_qpMap.at(sport));
		// init the qp
		q->m_local_ip = sip;
		q->m_local_port = sport;
		q->m_ecn_source.qIndex = pg;
		// store in map
		m_rxQpMap[key] = q;
		return q;
	}
	return NULL;
}

uint32_t RdmaHw::ResolveIface(Ipv4Address ip)
{
	auto it = m_rtTable.find(ip.Get());
	if(it == m_rtTable.end()) {
		NS_ASSERT_MSG(false, "We assume at least one NIC is alive");	
	}
	return it->second;
}
void RdmaHw::DeleteRxQp(uint32_t dip, uint16_t pg, uint16_t dport){
	uint64_t key = ((uint64_t)dip << 32) | ((uint64_t)pg << 16) | (uint64_t)dport;
	m_rxQpMap.erase(key);
}

int RdmaHw::Receive(Ptr<Packet> p, CustomHeader &ch)
{
	RdmaBTH bth;
	NS_ABORT_UNLESS(p->PeekPacketTag(bth));
	m_rxQpMap.at(bth.GetDestQpKey())->Receive(p, ch);
	return 0;
}

void RdmaHw::QpComplete(Ptr<RdmaTxQueuePair> qp){
	m_traceQpComplete(qp);

	// delete the qp
	DeleteQueuePair(qp);
}

void RdmaHw::SetLinkDown(Ptr<QbbNetDevice> dev){
	printf("RdmaHw: node:%u a link down\n", m_node->GetId());
}

void RdmaHw::AddTableEntry(const Ipv4Address &dstAddr, uint32_t intf_idx)
{
	const uint32_t dip = dstAddr.Get();
	NS_ASSERT_MSG(m_rtTable.find(dip) == m_rtTable.end(), "ECMP not implemented");
	m_rtTable[dip] = intf_idx;
}

void RdmaHw::ClearTable()
{
	m_rtTable.clear();
}

void RdmaHw::RedistributeQp()
{
	// clear old qpGrp
	for (uint32_t i = 0; i < m_nic.size(); i++){
		if (m_nic[i].GetDevice() == NULL)
			continue;
		m_nic[i].Clear();
	}

	// redistribute qp
	for (auto &it : m_qpMap){
		Ptr<RdmaTxQueuePair> qp = it.second;

		NS_ASSERT(m_nic.size() == 1); // Refactored, allow only one NIC
		uint32_t nic_idx = 0;
		m_nic[nic_idx].AddQp(qp);
		// Notify Nic
		m_nic[nic_idx].GetDevice()->ReassignedQp(qp);
	}
}

Ptr<Packet> RdmaHw::GetNxtPacket(Ptr<RdmaTxQueuePair> qp){
	return qp->GetNextPacket();
}

void RdmaHw::PktSent(Ptr<RdmaTxQueuePair> qp, Ptr<Packet> pkt, Time interframeGap){
	qp->m_lastPktSize = pkt->GetSize();
	UpdateNextAvail(qp, interframeGap, pkt->GetSize());
	
	// Since UD has no ACKed, it cannot stop upon ACK reception.
	// Then we stop the app when last packet is sent.
	if (qp->IsFinished()){
		QpComplete(qp);
	}
}

void RdmaHw::UpdateNextAvail(Ptr<RdmaTxQueuePair> qp, Time interframeGap, uint32_t pkt_size){
	Time sendingTime;
	if (m_rateBound)
		sendingTime = interframeGap + (qp->m_rate.CalculateBytesTxTime(pkt_size));
	else
		sendingTime = interframeGap + (qp->GetMaxRate().CalculateBytesTxTime(pkt_size));
	qp->m_nextAvail = Simulator::Now() + sendingTime;
}

void RdmaHw::ChangeRate(Ptr<RdmaTxQueuePair> qp, DataRate new_rate){
	#if 1
	Time sendingTime = (qp->m_rate.CalculateBytesTxTime(qp->m_lastPktSize));
	Time new_sendintTime = (new_rate.CalculateBytesTxTime(qp->m_lastPktSize));
	qp->m_nextAvail = qp->m_nextAvail + new_sendintTime - sendingTime;
	
	// update nic's next avail event
	NS_ASSERT(m_nic.size() == 1); // refactored
	uint32_t nic_idx = 0;
	m_nic[nic_idx].GetDevice()->UpdateNextAvail(qp->m_nextAvail);
	#endif

	// change to new rate
	qp->m_rate = new_rate;
}

#define PRINT_LOG 0
/******************************
 * Mellanox's version of DCQCN
 *****************************/
void RdmaHw::UpdateAlphaMlx(Ptr<RdmaTxQueuePair> q){
	#if PRINT_LOG
	//std::cout << Simulator::Now() << " alpha update:" << m_node->GetId() << ' ' << q->mlx.m_alpha << ' ' << (int)q->mlx.m_alpha_cnp_arrived << '\n';
	//printf("%lu alpha update: %08x %08x %u %u %.6lf->", Simulator::Now().GetTimeStep(), q->sip.Get(), q->dip.Get(), q->sport, q->dport, q->mlx.m_alpha);
	#endif
	if (q->mlx.m_alpha_cnp_arrived){
		q->mlx.m_alpha = (1 - m_g)*q->mlx.m_alpha + m_g; 	//binary feedback
	}else {
		q->mlx.m_alpha = (1 - m_g)*q->mlx.m_alpha; 	//binary feedback
	}
	#if PRINT_LOG
	//printf("%.6lf\n", q->mlx.m_alpha);
	#endif
	q->mlx.m_alpha_cnp_arrived = false; // clear the CNP_arrived bit
	ScheduleUpdateAlphaMlx(q);
}
void RdmaHw::ScheduleUpdateAlphaMlx(Ptr<RdmaTxQueuePair> q){
	q->mlx.m_eventUpdateAlpha = Simulator::Schedule(MicroSeconds(m_alpha_resume_interval), &RdmaHw::UpdateAlphaMlx, this, q);
}

void RdmaHw::cnp_received_mlx(Ptr<RdmaTxQueuePair> q){
	q->mlx.m_alpha_cnp_arrived = true; // set CNP_arrived bit for alpha update
	q->mlx.m_decrease_cnp_arrived = true; // set CNP_arrived bit for rate decrease
	if (q->mlx.m_first_cnp){
		// init alpha
		q->mlx.m_alpha = 1;
		q->mlx.m_alpha_cnp_arrived = false;
		// schedule alpha update
		ScheduleUpdateAlphaMlx(q);
		// schedule rate decrease
		ScheduleDecreaseRateMlx(q, 1); // add 1 ns to make sure rate decrease is after alpha update
		// set rate on first CNP
		q->mlx.m_targetRate = q->m_rate = m_rateOnFirstCNP * q->m_rate;
		q->mlx.m_first_cnp = false;
	}
}

void RdmaHw::CheckRateDecreaseMlx(Ptr<RdmaTxQueuePair> q){
	ScheduleDecreaseRateMlx(q, 0);
	if (q->mlx.m_decrease_cnp_arrived){
		#if PRINT_LOG
		printf("%lu rate dec: %08x %08x %u %u (%0.3lf %.3lf)->", Simulator::Now().GetTimeStep(), q->sip.Get(), q->dip.Get(), q->sport, q->dport, q->mlx.m_targetRate.GetBitRate() * 1e-9, q->m_rate.GetBitRate() * 1e-9);
		#endif
		bool clamp = true;
		if (!m_EcnClampTgtRate){
			if (q->mlx.m_rpTimeStage == 0)
				clamp = false;
		}
		if (clamp)
			q->mlx.m_targetRate = q->m_rate;
		q->m_rate = std::max(m_minRate, q->m_rate * (1 - q->mlx.m_alpha / 2));
		// reset rate increase related things
		q->mlx.m_rpTimeStage = 0;
		q->mlx.m_decrease_cnp_arrived = false;
		Simulator::Cancel(q->mlx.m_rpTimer);
		q->mlx.m_rpTimer = Simulator::Schedule(MicroSeconds(m_rpgTimeReset), &RdmaHw::RateIncEventTimerMlx, this, q);
		#if PRINT_LOG
		printf("(%.3lf %.3lf)\n", q->mlx.m_targetRate.GetBitRate() * 1e-9, q->m_rate.GetBitRate() * 1e-9);
		#endif
	}
}
void RdmaHw::ScheduleDecreaseRateMlx(Ptr<RdmaTxQueuePair> q, uint32_t delta){
	q->mlx.m_eventDecreaseRate = Simulator::Schedule(MicroSeconds(m_rateDecreaseInterval) + NanoSeconds(delta), &RdmaHw::CheckRateDecreaseMlx, this, q);
}

void RdmaHw::RateIncEventTimerMlx(Ptr<RdmaTxQueuePair> q){
	q->mlx.m_rpTimer = Simulator::Schedule(MicroSeconds(m_rpgTimeReset), &RdmaHw::RateIncEventTimerMlx, this, q);
	RateIncEventMlx(q);
	q->mlx.m_rpTimeStage++;
}
void RdmaHw::RateIncEventMlx(Ptr<RdmaTxQueuePair> q){
	// check which increase phase: fast recovery, active increase, hyper increase
	if (q->mlx.m_rpTimeStage < m_rpgThreshold){ // fast recovery
		FastRecoveryMlx(q);
	}else if (q->mlx.m_rpTimeStage == m_rpgThreshold){ // active increase
		ActiveIncreaseMlx(q);
	}else { // hyper increase
		HyperIncreaseMlx(q);
	}
}

void RdmaHw::FastRecoveryMlx(Ptr<RdmaTxQueuePair> q){
	#if PRINT_LOG
	printf("%lu fast recovery: %08x %08x %u %u (%0.3lf %.3lf)->", Simulator::Now().GetTimeStep(), q->sip.Get(), q->dip.Get(), q->sport, q->dport, q->mlx.m_targetRate.GetBitRate() * 1e-9, q->m_rate.GetBitRate() * 1e-9);
	#endif
	q->m_rate = (q->m_rate / 2) + (q->mlx.m_targetRate / 2);
	#if PRINT_LOG
	printf("(%.3lf %.3lf)\n", q->mlx.m_targetRate.GetBitRate() * 1e-9, q->m_rate.GetBitRate() * 1e-9);
	#endif
}
void RdmaHw::ActiveIncreaseMlx(Ptr<RdmaTxQueuePair> q){
	#if PRINT_LOG
	printf("%lu active inc: %08x %08x %u %u (%0.3lf %.3lf)->", Simulator::Now().GetTimeStep(), q->sip.Get(), q->dip.Get(), q->sport, q->dport, q->mlx.m_targetRate.GetBitRate() * 1e-9, q->m_rate.GetBitRate() * 1e-9);
	#endif
	// get NIC
	Ptr<QbbNetDevice> dev = q->GetDevice();
	// increate rate
	q->mlx.m_targetRate += m_rai;
	if (q->mlx.m_targetRate > dev->GetDataRate())
		q->mlx.m_targetRate = dev->GetDataRate();
	q->m_rate = (q->m_rate / 2) + (q->mlx.m_targetRate / 2);
	#if PRINT_LOG
	printf("(%.3lf %.3lf)\n", q->mlx.m_targetRate.GetBitRate() * 1e-9, q->m_rate.GetBitRate() * 1e-9);
	#endif
}
void RdmaHw::HyperIncreaseMlx(Ptr<RdmaTxQueuePair> q){
	#if PRINT_LOG
	printf("%lu hyper inc: %08x %08x %u %u (%0.3lf %.3lf)->", Simulator::Now().GetTimeStep(), q->sip.Get(), q->dip.Get(), q->sport, q->dport, q->mlx.m_targetRate.GetBitRate() * 1e-9, q->m_rate.GetBitRate() * 1e-9);
	#endif
	// get NIC
	Ptr<QbbNetDevice> dev = q->GetDevice();
	// increate rate
	q->mlx.m_targetRate += m_rhai;
	if (q->mlx.m_targetRate > dev->GetDataRate())
		q->mlx.m_targetRate = dev->GetDataRate();
	q->m_rate = (q->m_rate / 2) + (q->mlx.m_targetRate / 2);
	#if PRINT_LOG
	printf("(%.3lf %.3lf)\n", q->mlx.m_targetRate.GetBitRate() * 1e-9, q->m_rate.GetBitRate() * 1e-9);
	#endif
}

}
