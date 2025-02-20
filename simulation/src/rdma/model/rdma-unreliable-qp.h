#pragma once

#include <ns3/rdma-queue-pair.h>
#include <queue>

namespace ns3 {

class RdmaUnreliableSQ : public RdmaTxQueuePair
{ 
public:
	using RdmaTxQueuePair::RdmaTxQueuePair;

	void PostSend(SendRequest sr) override;
	bool IsReadyToSend() const override;
	bool HasDataToSend() const override;
	Ptr<Packet> GetNextPacket() override;
	
private:
	uint16_t m_ipid{0}; //!< IP packet header number, incremented by one on each packet.
	uint64_t m_snd_nxt{0}; //!< RDMA packet header byte offset, incremented by the size of the payload on each packet.
	std::queue<SendRequest> m_to_send; //!< Pending packet to send.
	
	struct AckCallback
	{
		uint64_t seq_no{0};
		OnSendCallback on_send;
	};
};

class RdmaUnreliableRQ : public RdmaRxQueuePair
{
public:
	RdmaUnreliableRQ(Ptr<RdmaUnreliableSQ> sq)
		: RdmaRxQueuePair{sq}
	{	
	}
	
	void ReceiveUdp(Ptr<Packet> p, const CustomHeader &ch) override;

private:
	void SendEcn(const CustomHeader& recv);

private:
	Time m_ecn_next_avail{Time(0)};
	Time m_ecn_delay{MicroSeconds(100)};
};

struct RdmaUnreliableQP {
	Ptr<class RdmaUnreliableSQ> sq;
	Ptr<class RdmaUnreliableRQ> rq;
};

}