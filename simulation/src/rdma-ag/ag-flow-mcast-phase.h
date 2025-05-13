#pragma once

#include "ns3/rdma-flow.h"

namespace ns3 {

/**
 * Runs all the multicasts.
 * 
 * Assumes a fat-tree where all servers IDs are in the same order as 
 * 
 * Stores the bitmaps in an output Avro file.
 * Uses the multicast group zero, which contains all nodes.
 * Uses the PFC priority 3.
 * 
 * Will internally schedule multicast flows and run the dependency graph of all multicasts.
 */
class AgFlowMcastPhase : public RdmaFlow
{
private:
    // Defines the order of the multicast.
    // - First index indicates the multicasts should run in order.
    // - Second index indicates the multicasts should run concurrently.
    using McastChains = std::vector<std::vector<Ptr<Node>>>;

public:
    static TypeId GetTypeId();
    void StartFlow(RdmaNetwork& network, OnComplete on_complete) override;

private:
    McastChains BuildMulticastChains(RdmaNetwork& network) const;

private:
    //! Where to store the bitmaps of the received chunks.
    std::string m_bitmaps_avro_out;
    //! Count of multicast roots.
    uint32_t m_num_mcast_roots{};
    //! Count of MTU-sized packets per chunk.
    uint32_t m_num_pkts_per_chunk{};
    //! Count of local chunks per node.
    uint32_t m_num_chunks_per_node{};
    //! Multicast group to use for all multicasts. Fixed.
    group_id_t m_mcast_group{};
    //! PFC priority to use for all multicasts. Fixed.
    priority_t m_priority{3};
    //! To avoid memory leaks and flows to be destroyed.
    std::vector<Ptr<RdmaFlow>> m_flows;
    //! Divide the bandwidth of each multicast by the count of multicast root to not overflow receivers.
    //! This should be set to true, but maybe CC can manage this under some conditions?
    bool m_optimize_throughput{true};
};

} // namespace ns3