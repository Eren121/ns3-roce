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
};

struct RdmaUnreliableQP {
	Ptr<RdmaUnreliableSQ> sq;
	Ptr<RdmaUnreliableRQ> rq;
};

}