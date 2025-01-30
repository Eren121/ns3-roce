#pragma once

#include <ns3/rdma-queue-pair.h>

namespace ns3 {

class RdmaUnreliableSQ : public RdmaTxQueuePair
{ 
public:
	void PostSend(SendRequest sr) override;
};

}