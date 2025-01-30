#pragma once

#include <ns3/rdma-queue-pair.h>
#include <queue>

namespace ns3 {

class RdmaReliableSQ : public RdmaTxQueuePair
{
public:
    RdmaReliableSQ(Ptr<Node> node, uint16_t pg, Ipv4Address sip, uint16_t sport, Ipv4Address dip, uint16_t dport);
    
    void SetWin(uint32_t win);
	void SetVarWin(bool v);
	void Acknowledge(uint64_t ack);
	uint64_t GetOnTheFly() const;
	bool IsWinBound() const;
	uint64_t GetWin() const; // window size calculated from m_rate

	bool IsReadyToSend() const override;
	void PostSend(SendRequest sr) override;
	Ptr<Packet> GetNextPacket() override;
	
	void RecoverNack(); // TODO the NAK should contain last ACKed ID

private:
	struct AckCallback
	{
		uint64_t seq_no{0};
		OnSendCallback on_send;
	};

	bool ShouldReqAck(uint64_t payload_size) const;

private:
	std::queue<SendRequest> m_to_send; //!< Pending packet to send.
	std::queue<AckCallback> m_ack_cbs; //!< Pending ACK callbacks
	Ipv4Address m_dip;
	uint32_t m_mtu{0};
	uint16_t m_dport{0};
	uint16_t m_ipid{0};
	uint64_t m_snd_nxt{0}; //<! Next PSN to send.
    uint64_t m_snd_una{0}; //<! Highest PSN acknowledged (confusing name).
	uint32_t m_win{0}; 	   //<! Bound of on-the-fly packets.
    bool m_var_win{false}; //<! Variable window size?
	uint64_t m_chunk{0};
	uint64_t m_ack_interval{0};
};

class RdmaReliableRQ : public RdmaRxQueuePair
{
public:
	bool Receive(Ptr<Packet> p, const CustomHeader& ch) override;

protected:
	uint32_t ReceiverNextExpectedSeq{0};
	Time m_nackTimer{Time(0)};
	uint32_t m_lastNACK{0};
	double m_nack_interval{0};
	uint32_t m_chunk{0};
	uint32_t m_ack_interval{0};
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

}