#pragma once

#include "ns3/rdma-flow.h"
#include "ns3/ag-recv-chunk-record.h"
#include "ns3/rdma-serdes.h"

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
    // If chains is of type `McastChains`.
    // - `chains[0]` is the first chain, `chains[1]` the second, etc...
    // - `chains[0][1]` should be executed when `chains[0][0]` has completed, etc...
    using McastChains = std::vector<std::vector<Ptr<Node>>>;

public:
    static TypeId GetTypeId();

protected:
    void OnFlowStarted(RdmaNetwork& network) override;

private:
    McastChains BuildMulticastChains(RdmaNetwork& network) const;
    void OnChainComplete();
    void SaveStats() const;

private:
    //! Where to store the bitmaps of the received chunks.
    std::string m_bitmaps_avro_out;
    //! Where to store the configuration and some statistics.
    std::string m_stats_json_out;
    //! The Avro record writer.
    RdmaSerializer<AgRecvChunkRecord> m_trace_writer;
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
    //! Divide the bandwidth of each multicast by the count of multicast root to not overflow receivers.
    //! This should be set to true, but maybe CC can manage this under some conditions?
    bool m_optimize_throughput{true};
    //! Keep track of chain progress
    int m_completed_chains{};
    int m_num_chains{};
    //! Just maps Node ID to rank in the allgather.
    std::unordered_map<int, int> m_id_to_rank;
    //! Keep trace.
    uint64_t m_num_success_pkts{};
};

} // namespace ns3