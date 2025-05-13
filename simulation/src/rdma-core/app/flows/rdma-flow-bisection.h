#pragma once

#include "ns3/rdma-flow.h"

namespace ns3 {

/**
 * Assumes a fat tree topology.
 * Assumes all servers are ordered by their ID in the fat tree topology if running a breath-first search.
 * 
 * With all `n` servers, sorted by their ID:
 * - Server 0 talks with server `n/2`.
 *   Initiator is server 0.
 * - Server 1 talks with server `n/2 + 1`.
 *   Initiator is server `n/2 + 1`.
 * - Server `k` talks with server `n/2 + k`.
 *   Initiator is server `k` if `k` is even, otherwise initiator is `n/2 + k`.
 * - etc... 
 * 
 * With alternating RDMA Write.
 */
class RdmaFlowBisection : public RdmaFlow
{
public:
    static TypeId GetTypeId();
    void StartFlow(RdmaNetwork& network, OnComplete on_complete) override;

private:
    void StartWrite(Ptr<Node> initiator, Ptr<Node> target, OnComplete on_complete) const;

private:
    //! Count of bytes to write.
    uint32_t m_bytes_to_write{}; 
    //! Priority group.
    uint16_t m_priority{};
    //! If true, uses RC QP. If false, uses UD QP.
    bool m_reliable{};
};

} // namespace ns3