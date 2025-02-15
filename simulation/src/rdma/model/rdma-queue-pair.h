#ifndef RDMA_QUEUE_PAIR_H
#define RDMA_QUEUE_PAIR_H

#include <ns3/node.h>
#include <ns3/packet.h>
#include <ns3/ipv4-address.h>
#include <ns3/data-rate.h>
#include <ns3/event-id.h>
#include <ns3/custom-header.h>
#include <ns3/int-header.h>
#include <functional>
#include <vector>

namespace ns3 {

class QbbNetDevice;

/**
 * \brief Common base to UD and RC SQs.
 */
class RdmaTxQueuePair : public Object
{ 
public:
	/**
	 * \param node Node associated to this SQ.
	 * \param pg Priority
	 * \param sip Source IP.
	 * \param sport Source port.
	 */
	RdmaTxQueuePair(Ptr<Node> node, uint16_t pg, Ipv4Address sip, uint16_t sport);
	~RdmaTxQueuePair() override;

	/**
	 * \return The device associated to this SQ.
	 */
	Ptr<QbbNetDevice> GetDevice();

	/**
	 * \brief Callback called when the send request is completed.
	 */
	using OnSendCallback = std::function<void()>;
	
	/**
	 * @brief Equivalent of `ibv_send_wr()`.
	 */
	struct SendRequest
	{
		uint32_t payload_size{}; //!< How much bytes to write.
		uint32_t imm{}; //!< Immediate data.
		bool multicast{}; //!< Is `dip` a multicast group?
		Ipv4Address dip{}; //!< Note: Only for UD SQ.
		uint16_t dport{};  //!< Note: Only for UD SQ.
		OnSendCallback on_send{}; //!< For UD QP: called when the packet leaves the NIC; For RC QP: called when ACKed.
	
		// Private
		uint64_t first_psn{}; //!< First PSN of the QP associated to this send request.

		uint64_t GetEndPSN() const { return first_psn + payload_size; }
	};

	virtual void PostSend(SendRequest sr)
	{
		NS_ABORT_MSG("Not implemented");
	}

	/**
	 * \returns true When this SQ is ready to send and a next packet is available.
	 */
	virtual bool IsReadyToSend() const
	{
		return false;
	}

	/**
	 * \returns true When the QP can be deleted.
	 */
	virtual bool IsFinished() const
	{
		return m_finished;
	}

	virtual bool HasDataToSend() const = 0;

	virtual Ptr<Packet> GetNextPacket()
	{
		return nullptr;
	}

	/**
	 * @returns The time when the next packet will be ready to send.
	 */
	Time GetNextAvailTime() const
	{
		return m_nextAvail;
	}

	void TriggerDevTransmit();


	/***********
	 * methods
	 **********/
	static TypeId GetTypeId (void);

	Ptr<Node> GetNode() const { return m_node; }
	Ipv4Address GetSrcIP() const { return m_sip; }
	uint16_t GetSrcPort() const { return m_sport; }
	uint16_t GetPG() const { return m_pg; }
	void SetMTU(uint32_t mtu) { m_mtu = mtu; }
	
	void LazyInitCnp();

	uint64_t GetKey()
	{
		return m_sport;
	}

	void Finish()
	{
		m_finished = true;
	}

	void StopTimers();

	void SetRateFactor(double rate_factor);
	void SetMaxRate(DataRate data_rate);
	DataRate GetMaxRate() const;

protected:
	Ptr<Node> m_node{};
	uint32_t m_mtu{0};
	Ipv4Address m_sip{};
	uint16_t m_sport{0};
	uint16_t m_pg{0};
	DataRate m_max_rate{}; // max rate
	Time m_nextAvail{};	//< Next time the QP is ready to send (regardless of if the queue is empty).
	uint32_t m_lastPktSize{0};
	bool m_finished{false};

	friend class RdmaHw;

	/******************************
	 * runtime states
	 *****************************/
	DataRate m_rate;	//< Current rate
	struct {
		DataRate m_targetRate;	//< Target rate
		EventId m_eventUpdateAlpha;
		double m_alpha;
		bool m_alpha_cnp_arrived; // indicate if CNP arrived in the last slot
		bool m_first_cnp; // indicate if the current CNP is the first CNP
		EventId m_eventDecreaseRate;
		bool m_decrease_cnp_arrived; // indicate if CNP arrived in the last slot
		uint32_t m_rpTimeStage;
		EventId m_rpTimer;
	} mlx{};
	struct {
		uint32_t m_lastUpdateSeq;
		DataRate m_curRate;
		IntHop hop[IntHeader::maxHop];
		uint32_t keep[IntHeader::maxHop];
		uint32_t m_incStage;
		double m_lastGap;
		double u;
		struct {
			double u;
			DataRate Rc;
			uint32_t incStage;
		}hopState[IntHeader::maxHop];
	} hp{};
	struct{
		uint32_t m_lastUpdateSeq;
		DataRate m_curRate;
		uint32_t m_incStage;
		uint64_t lastRtt;
		double rttDiff;
	} tmly{};
	struct{
		uint32_t m_lastUpdateSeq;
		uint32_t m_caState;
		uint32_t m_highSeq; // when to exit cwr
		double m_alpha;
		uint32_t m_ecnCnt;
		uint32_t m_batchSizeOfAlpha;
	} dctcp{};
	struct{
		uint32_t m_lastUpdateSeq;
		DataRate m_curRate;
		uint32_t m_incStage;
	} hpccPint{};

private:
	double m_rate_multiplier{1.0}; //!< Limit the rate from the max. rate by this factor.
};

using RdmaImm = uint32_t;

class RdmaRxQueuePair : public Object { // Rx side queue pair
public:
	struct RecvNotif
	{
		Ptr<Packet> packet;
		const CustomHeader* ch;
		bool has_imm{false};
		RdmaImm imm{0};
	};

	using OnRecvCallback = std::function<void(RecvNotif)>;

	struct ECNAccount{
		uint16_t qIndex{0};
		uint8_t ecnbits{0};
		uint16_t qfb{0};
		uint16_t total{0};
	};

	static TypeId GetTypeId (void);
	
	RdmaRxQueuePair(Ptr<RdmaTxQueuePair> tx);

	/**
	 * @return true If the packet was succesfully processed, or false if the protocol is not known.
	 */
	virtual bool Receive(Ptr<Packet> p, const CustomHeader& ch);

	/**
	 * @brief Set a callback called when an RDMA Write operation has completed.
	 * 
	 * Works for both unreliable and reliable RDMA Writes.
	 * For reliable RDMA Write, the callback is called once the entire message is written
	 * (as opposed for each packet).
	 * For unreliable RDMA Write, as any message cannot exceed MTU, the callback is called
	 * for each received packet.
	 */
	void SetOnRecv(OnRecvCallback cb)
	{
		m_onRecv = std::move(cb);
	}

protected:
	/**
	 * @brief Receive application data.
	 * 
	 * No reliability at this layer.
	 */
	virtual void ReceiveUdp(Ptr<Packet> p, const CustomHeader &ch);
	virtual void ReceiveCnp(Ptr<Packet> p, const CustomHeader &ch);

	Ptr<RdmaTxQueuePair> m_tx;
	OnRecvCallback m_onRecv;

public:
	ECNAccount m_ecn_source{};
	uint32_t m_local_ip{0};
	uint16_t m_local_port{0};
	uint16_t m_ipid{0};
	EventId QcnTimerEvent{}; // if destroy this rxQp, remember to cancel this timer
};

/**
 * @brief Simple wrapper around `std::vector<Ptr<RdmaTxQueuePair>>`.
 */
class RdmaTxQueuePairGroup : public Object
{
public:
	RdmaTxQueuePairGroup(Ptr<QbbNetDevice> dev);

	static TypeId GetTypeId();
	Ptr<QbbNetDevice> GetDevice();
	uint32_t GetN();
	Ptr<RdmaTxQueuePair> Get(uint32_t idx);
	Ptr<RdmaTxQueuePair> operator[](uint32_t idx);
	void AddQp(Ptr<RdmaTxQueuePair> qp);
	void Clear();

	/**
	 * @brief Remove all finished QPs in the index range `[begin;end)`.
	 * @param res If stores a valid QP index,
	 *            this variable is updated to reflect the new QP index
	 *            after removal of all finished QPs.
	 */
	void RemoveFinished(int begin, int last, int& res);

private:
	std::vector<Ptr<RdmaTxQueuePair>> m_qps;
	Ptr<QbbNetDevice> m_dev;
};

}

#endif /* RDMA_QUEUE_PAIR_H */
