#pragma once

#include <ns3/rdma-queue-pair.h>
#include <queue>

namespace ns3 {

enum SendFlags {
	None,

	/**
	 * When a RDMA Write is too big to fit a single packet, it is split in multiple packet.
	 * With this flag, The immediate value is set to the index of the fragment (0, 1, 2... up to the count of fragments - 1).
	 * And the receiver is notified for fragments (which is not the case normally).
	 * And the immediate value set by the user is ignored.
	 */
	FragmentAsImmediate	
};

class RdmaUnreliableSQ : public RdmaTxQueuePair
{
private:
	struct WorkElement
	{
		// The original RDMA Write request.
		SendRequest sr;

		//! @see `SendFlags::FragmentAsImmediate`.
		bool fragment_as_immediate{};

		//! Count of fragments already sent
		uint32_t sent_fragments{};

		//! Count of bytes already sent.
		uint32_t bytes_sent{};
	};

public:
	using RdmaTxQueuePair::RdmaTxQueuePair;

	void PostSend(SendRequest sr, SendFlags flags);
	void PostSend(SendRequest sr) override;
	bool IsReadyToSend() const override;
	bool HasDataToSend() const override;
	Ptr<Packet> GetNextPacket() override;
	
private:
 	//!< IP packet header number, incremented by one on each packet.
	//! Wraps-around to zero after max is reached.
	uint16_t m_ipid{0};
	//!< RDMA packet header byte offset, incremented by the size of the payload on each packet.
	uint64_t m_snd_nxt{0};
	//!< Pending send requests.
	std::queue<WorkElement> m_to_send;
	
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