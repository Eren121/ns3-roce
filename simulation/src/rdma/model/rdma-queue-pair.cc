#include <ns3/hash.h>
#include <ns3/uinteger.h>
#include <ns3/rdma-seq-header.h>
#include <ns3/udp-header.h>
#include <ns3/ipv4-header.h>
#include <ns3/simulator.h>
#include "ns3/ppp-header.h"
#include "rdma-queue-pair.h"

namespace ns3 {

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

RdmaTxQueuePair::RdmaTxQueuePair(uint16_t pg, Ipv4Address _sip, Ipv4Address _dip, uint16_t _sport, uint16_t _dport){
	startTime = Simulator::Now();
	sip = _sip;
	dip = _dip;
	sport = _sport;
	dport = _dport;
	m_size = 0;
	snd_nxt = snd_una = 0;
	m_pg = pg;
	m_ipid = 0;
	m_reliable = true;
	m_multicast = false;
	m_win = 0;
	m_baseRtt = 0;
	m_max_rate = 0;
	m_var_win = false;
	m_rate = 0;
	m_nextAvail = Time(0);
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

void RdmaTxQueuePair::SetSize(uint64_t size){
	m_size = size;
}

void RdmaTxQueuePair::SetWin(uint32_t win){
	m_win = win;
}

void RdmaTxQueuePair::SetBaseRtt(uint64_t baseRtt){
	m_baseRtt = baseRtt;
}

void RdmaTxQueuePair::SetVarWin(bool v){
	m_var_win = v;
}

void RdmaTxQueuePair::SetAppNotifyCallback(Callback<void> notifyAppFinish){
	m_notifyAppFinish = notifyAppFinish;
}

uint64_t RdmaTxQueuePair::GetBytesLeft(){
	return m_size >= snd_nxt ? m_size - snd_nxt : 0;
}

void RdmaTxQueuePair::Acknowledge(uint64_t ack){
	if (ack > snd_una){
		snd_una = ack;
	}
}

uint64_t RdmaTxQueuePair::GetOnTheFly(){
	return snd_nxt - snd_una;
}

bool RdmaTxQueuePair::IsWinBound(){
	uint64_t w = GetWin();
	return w != 0 && GetOnTheFly() >= w;
}

uint64_t RdmaTxQueuePair::GetWin(){
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

uint64_t RdmaTxQueuePair::HpGetCurWin(){
	if (m_win == 0)
		return 0;
	uint64_t w;
	if (m_var_win){
		w = m_win * hp.m_curRate.GetBitRate() / m_max_rate.GetBitRate();
		if (w == 0)
			w = 1; // must > 0
	}else{
		w = m_win;
	}
	return w;
}

bool RdmaTxQueuePair::IsFinished(){
	if(!m_reliable) {
		return snd_nxt >= m_size;
	}
	return snd_una >= m_size;
}

/*********************
 * RdmaRxQueuePair
 ********************/
TypeId RdmaRxQueuePair::GetTypeId (void)
{
	static TypeId tid = TypeId ("ns3::RdmaRxQueuePair")
		.SetParent<Object> ()
		;
	return tid;
}

RdmaRxQueuePair::RdmaRxQueuePair(){
	m_local_ip = 0;
	m_local_port = 0;
	m_ipid = 0;
	ReceiverNextExpectedSeq = 0;
	m_nackTimer = Time(0);
	m_lastNACK = 0;
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
