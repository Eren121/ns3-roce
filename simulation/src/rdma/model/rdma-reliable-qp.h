#pragma once

#include <ns3/rdma-queue-pair.h>
#include <queue>
#include <map>

namespace ns3 {

class RdmaReliableSQ : public RdmaTxQueuePair
{
public:
	using psn_t = uint64_t;
	
	RdmaReliableSQ(Ptr<Node> node, uint16_t pg, Ipv4Address sip, uint16_t sport, Ipv4Address dip, uint16_t dport);
  ~RdmaReliableSQ() override;
	
	void SetAckInterval(uint64_t chunk, uint64_t ack_interval);
	void SetWin(uint32_t win);
	void SetVarWin(bool v);
	void Acknowledge(uint64_t next_psn_expected);
	uint64_t GetOnTheFly() const;
	bool IsWinBound() const;
	uint64_t GetWin() const; // window size calculated from m_rate
	uint64_t GetChunk() const { return m_chunk; }
	bool IsReadyToSend() const override;
	bool HasDataToSend() const override;
	void PostSend(SendRequest sr) override;
	Ptr<Packet> GetNextPacket() override;
	
	void RecoverNack(uint64_t next_psn_expected);

	uint16_t GetDestPort() const { return m_dport; }
	uint32_t GetDestIP() const { return m_dip.Get(); }

private:

	bool ShouldReqAck(uint64_t payload_size) const;
	void NotifyPendingCompEvents();
	void Rollback();
	void TriggerDevTransmit();
	void ScheduleRetrTimeout();
	void OnRetrTimeout();

private:
	EventId m_retr_to;
	uint64_t highest_ack_psn{0}; //!< PSN following the highest PSN sent with an ACK request.

	std::map<psn_t, SendRequest> m_to_send; //!< Pending packet to send. Removed when ACKed.
	Ipv4Address m_dip;
	uint16_t m_dport{0};
	uint16_t m_ipid{0};
	uint64_t m_snd_nxt{0}; 	  		//<! Next PSN to send.
  uint64_t m_snd_una{0}; 	  		//<! Lowest PSN unacknowledged.
	uint32_t m_win{0}; 	   	  		//<! Bound of on-the-fly packets.
	bool m_var_win{false}; 	  		//<! Variable window size?
	uint64_t m_next_op_first_psn{};
	uint64_t m_chunk{0};
	uint64_t m_ack_interval{0};
};

class RdmaReliableRQ : public RdmaRxQueuePair
{
public:
	RdmaReliableRQ(Ptr<RdmaReliableSQ> sq);
	bool Receive(Ptr<Packet> p, const CustomHeader& ch) override;

	uint32_t GetChunk() const { return DynamicCast<RdmaReliableSQ>(m_tx)->GetChunk(); }
	void SetNackInterval(double nack_itv) { m_nack_interval = nack_itv; }
	void SetBackTo0(bool backto0) { m_backto0 = backto0; }

protected:
	//! Next expected PSN to receive.
	uint32_t ReceiverNextExpectedSeq{0};

	Time m_nackTimer{Time(0)};
	uint32_t m_lastNACK{0};
	double m_nack_interval{0};
	bool m_backto0{false};
	
	int ReceiverCheckSeq(uint32_t seq, uint32_t size);
	
	/**
	 * @brief Send ACK or NACK when appropriate.
	 */
	void ReceiveUdp(Ptr<Packet> p, const CustomHeader &ch) override;

	/**
	 * @brief The sender receives back an ACK or NACK.
	 */
	virtual void ReceiveAck(Ptr<Packet> p, const CustomHeader &ch);

};

struct RdmaReliableQP {
	Ptr<RdmaReliableSQ> sq;
	Ptr<RdmaReliableRQ> rq;
};

}