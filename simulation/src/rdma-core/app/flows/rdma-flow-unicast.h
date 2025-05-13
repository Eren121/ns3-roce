#pragma once

#include "ns3/rdma-flow.h"

namespace ns3 {

/**
 * Simple unicast flow.
 * For the unreliable flow, the completion is when the last byte is sent by the source,
 * but not when the receiver receives it.
 */
class RdmaFlowUnicast : public RdmaFlow
{
public:
    static TypeId GetTypeId();
    void StartFlow(RdmaNetwork& network, OnComplete on_complete) override;

private:
    //! Source node ID.
    uint32_t m_snode{};
    //! Destination node ID.
    uint32_t m_dnode{};
    //! Count of bytes to write.
    uint32_t m_bytes_to_write{}; 
    //! Priority group.
    uint16_t m_priority{};
    //! If true, uses RC QP. If false, uses UD QP.
    bool m_reliable{};
};

} // namespace ns3