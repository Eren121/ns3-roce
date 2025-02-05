#ifndef RDMA_HW_H
#define RDMA_HW_H

#include <ns3/rdma.h>
#include <ns3/rdma-queue-pair.h>
#include <ns3/node.h>
#include <ns3/custom-header.h>
#include "qbb-net-device.h"
#include <unordered_map>
#include <functional>
#include "pint.h"

namespace ns3 {

class RdmaHw : public Object {
public:

	static TypeId GetTypeId (void);

	void Setup(); // setup shared data and callbacks with the QbbNetDevice
	void RegisterQP(Ptr<RdmaTxQueuePair> sq, Ptr<RdmaRxQueuePair> rq);
	void DeleteRxQp(uint32_t dip, uint16_t pg, uint16_t dport);

	// call this function after the NIC is setup
	void AddTableEntry(const Ipv4Address &dstAddr, uint32_t intf_idx);
	void ClearTable();
	void RedistributeQp();

	uint32_t GetCC() const { return m_cc_mode; }

	uint32_t GetMTU() const { return m_mtu; }
	
private:
	static uint64_t GetRxQpKey(uint16_t dport); // get the lookup key for m_rxQpMap
	uint32_t ResolveIface(Ipv4Address ip); //!< Get the interface connected to this IP.
	void DeleteQueuePair(Ptr<RdmaTxQueuePair> qp);

	Ptr<RdmaRxQueuePair> GetRxQp(uint32_t sip, uint32_t dip, uint16_t sport, uint16_t dport, uint16_t pg, bool create); // get a rxQp
	
	int Receive(Ptr<Packet> p, CustomHeader &ch); // callback function that the QbbNetDevice should use when receive packets. Only NIC can call this function. And do not call this upon PFC

	void CheckandSendQCN(Ptr<RdmaRxQueuePair> q);
	bool SenderShouldReqAck(Ptr<RdmaTxQueuePair> q, uint64_t payload_size);
	int ReceiverCheckSeq(uint32_t seq, Ptr<RdmaRxQueuePair> q, uint32_t size);
	void AddHeader (Ptr<Packet> p, uint16_t protocolNumber);
	static uint16_t EtherToPpp (uint16_t protocol);

	void QpComplete(Ptr<RdmaTxQueuePair> qp);
	void SetLinkDown(Ptr<QbbNetDevice> dev);

	Ptr<Packet> GetNxtPacket(Ptr<RdmaTxQueuePair> qp); // get next packet to send, inc snd_nxt
	void PktSent(Ptr<RdmaTxQueuePair> qp, Ptr<Packet> pkt, Time interframeGap);
	void UpdateNextAvail(Ptr<RdmaTxQueuePair> qp, Time interframeGap, uint32_t pkt_size);
	void ChangeRate(Ptr<RdmaTxQueuePair> qp, DataRate new_rate);
	
	using WhenReadyGen = std::function<Ptr<Packet>()>;
	/**
	 * @brief Enqueue a packet.
	 * 
	 * The packet is not pushed directly, but the callback is called when the 
	 * NIC is ready to send the packet.
	 */
	void WhenReady(WhenReadyGen whenReady);

	using OnRecv = std::function<void(Ptr<RdmaTxQueuePair>, Ptr<Packet>)>;
	/**
	 * @brief Set a callback when a packet is received from any QP.
	 */
	void SetOnRecv(OnRecv onRecv);
	
private:
	Ptr<Node> m_node;
	DataRate m_minRate;		//< Min sending rate
	uint32_t m_mtu;
	uint32_t m_cc_mode;
	double m_nack_interval;
	uint32_t m_chunk;
	uint32_t m_ack_interval;
	bool m_backto0;
	bool m_var_win, m_fast_react;
	bool m_rateBound;

	std::vector<RdmaTxQueuePairGroup> m_nic; // list of running nic controlled by this RdmaHw
	
	/// @brief Each QP has a key. Mapping from the key to the QP.
	std::unordered_map<uint64_t, Ptr<RdmaTxQueuePair> > m_qpMap;

	/// @brief The key is the same as in `m_qpMap`.
	std::unordered_map<uint64_t, Ptr<RdmaRxQueuePair> > m_rxQpMap;

	/// @brief Routing table from IP to output port index.
	std::unordered_map<uint32_t, int> m_rtTable;

	TracedCallback<Ptr<RdmaTxQueuePair>> m_traceQpComplete;

	/******************************
	 * Mellanox's version of DCQCN
	 *****************************/
	double m_g; //feedback weight
	double m_rateOnFirstCNP; // the fraction of line rate to set on first CNP
	bool m_EcnClampTgtRate;
	double m_rpgTimeReset;
	double m_rateDecreaseInterval;
	uint32_t m_rpgThreshold;
	double m_alpha_resume_interval;
	DataRate m_rai;		//< Rate of additive increase
	DataRate m_rhai;		//< Rate of hyper-additive increase

	// the Mellanox's version of alpha update:
	// every fixed time slot, update alpha.
	void UpdateAlphaMlx(Ptr<RdmaTxQueuePair> q);
	void ScheduleUpdateAlphaMlx(Ptr<RdmaTxQueuePair> q);

	// Mellanox's version of CNP receive
	void cnp_received_mlx(Ptr<RdmaTxQueuePair> q);

	// Mellanox's version of rate decrease
	// It checks every m_rateDecreaseInterval if CNP arrived (m_decrease_cnp_arrived).
	// If so, decrease rate, and reset all rate increase related things
	void CheckRateDecreaseMlx(Ptr<RdmaTxQueuePair> q);
	void ScheduleDecreaseRateMlx(Ptr<RdmaTxQueuePair> q, uint32_t delta);

	// Mellanox's version of rate increase
	void RateIncEventTimerMlx(Ptr<RdmaTxQueuePair> q);
	void RateIncEventMlx(Ptr<RdmaTxQueuePair> q);
	void FastRecoveryMlx(Ptr<RdmaTxQueuePair> q);
	void ActiveIncreaseMlx(Ptr<RdmaTxQueuePair> q);
	void HyperIncreaseMlx(Ptr<RdmaTxQueuePair> q);
};

} /* namespace ns3 */

#endif /* RDMA_HW_H */
