#include "ns3/ag-flow-mcast-phase.h"
#include "ns3/rdma-flow-multicast.h"
#include "ns3/rdma-flow-wait.h"
#include "ns3/rdma-network.h"
#include "ns3/rdma-hw.h"
#include "ns3/qbb-net-device.h"
#include "ns3/data-rate-ops.h"

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED(AgFlowMcastPhase);
NS_LOG_COMPONENT_DEFINE("AgFlowMcastPhase");

TypeId AgFlowMcastPhase::GetTypeId()
{
  static TypeId tid = []() {
    static TypeId tid = TypeId("ns3::AgFlowMcastPhase");

    tid.SetParent<RdmaFlow>();
    tid.AddConstructor<AgFlowMcastPhase>();
    
    AddStringAttribute(tid,
      "BitmapsAvroOut",
      "File path where to write the Avro bitmaps of all nodes,"
      "for processing by the recovery phase or for later analysis.",
      &AgFlowMcastPhase::m_bitmaps_avro_out);
    
    AddUintegerAttribute(tid,
      "MulticastRootCount",
      "Count of multicast roots. "
      "Defines how many multicasts happen at the same time.",
      &AgFlowMcastPhase::m_num_mcast_roots);

    AddUintegerAttribute(tid,
      "PerNodeChunkCount",
      "Count of chunks sent by a single node in a single multicast.",
      &AgFlowMcastPhase::m_num_chunks_per_node);
    
    AddUintegerAttribute(tid,
      "PerChunkPacketCount",
      "Count of MTU-sized packets per chunk.",
      &AgFlowMcastPhase::m_num_pkts_per_chunk);
    
    AddBooleanAttribute(tid,
      "OptimizeThroughput",
      "Divide the bandwidth of each multicast by the count of multicast root to not overflow receivers. "
      "This should be set to true, but maybe CC can manage this under some conditions?",
      &AgFlowMcastPhase::m_optimize_throughput,
      true);

    return tid;
  }();
  
  return tid;
}

void AgFlowMcastPhase::OnFlowStarted(RdmaNetwork& network)
{
  const DataRate bandwidth = network.GetAnyServerDataRate();
  const McastChains chains = BuildMulticastChains(network);

  const uint32_t num_pkts_per_mcast = m_num_chunks_per_node * m_num_pkts_per_chunk;

  for(const auto& chain : chains) {
    
    // Previous flow of this chain, to build dependencies.
    Ptr<RdmaFlow> previous;

    // Build the multicasts of each chain, each one dependening on the previous
    for(Ptr<Node> mcast_src : chain) {
      const int mcast_src_id = mcast_src->GetId();

      // Called when any packet is received on any receiver for this multicast.
      auto on_recv_pkt = [mcast_src_id](const RdmaFlowMulticast::OnRecvPktInfo& info) {
        NS_LOG_INFO("Allgather multicast: recv packet (tx, rx, pkt) = ("
          << mcast_src_id << ", " << info.receiver << ", " << info.pkt_id);
      };
      
      Ptr<RdmaFlowMulticast> multicast = CreateObject<RdmaFlowMulticast>();
      network.GetFlowScheduler().AddFlow(multicast);

      if(previous) {
        network.GetFlowScheduler().AddDependency(multicast, previous);
      }
      
      multicast->SetAttribute("MulticastSource", UintegerValue(mcast_src_id));
      multicast->SetAttribute("MulticastGroup", UintegerValue(m_mcast_group));
      multicast->SetAttribute("NumPackets", UintegerValue(num_pkts_per_mcast));
      multicast->SetAttribute("PfcPriority", UintegerValue(m_priority));
      multicast->SetOnRecvPktCallback(on_recv_pkt);
    
      if(m_optimize_throughput) {
        multicast->SetThroughput(bandwidth * (1.0 / m_num_mcast_roots));
      }

      // Add a delay to take into account the delay for the last packet to arrive at the furthest servers.
      Ptr<RdmaFlowWait> wait = CreateObject<RdmaFlowWait>();
      wait->SetAttribute("Time", TimeValue(network.GetMaxDelay()));
      network.GetFlowScheduler().AddFlow(wait);
      network.GetFlowScheduler().AddDependency(wait, multicast);

      previous = wait;
    }

    // Aggregate all completed chains to notify the multicast phase is complete.
    auto on_chain_complete = [this, num_chains=chains.size(), completed_chains=0]() mutable {
      completed_chains++;
      NS_ABORT_IF(completed_chains > num_chains);

      if(completed_chains == num_chains) {
        NotifyComplete();
      }
    };

    previous->AddOnCompleteCallback(on_chain_complete);
  }
}

auto AgFlowMcastPhase::BuildMulticastChains(RdmaNetwork& network) const -> McastChains
{
  McastChains res;
  const auto servers = network.FindMcastGroup(m_mcast_group).to_vector();

  NS_ABORT_MSG_IF(servers.size() % m_num_mcast_roots != 0,
    "Count of servers should be a multiple of the count of multicast roots.");
  
  const int chain_length = servers.size() / m_num_mcast_roots;

  // All chains execute in parallel.
  for(int chain = 0; chain < m_num_mcast_roots; chain++) {
    const int first_of_chain = chain_length * chain;
    std::vector<Ptr<Node>> chain_order;

    for(int i = 0; i < chain_length; i++) {
      const uint32_t mcast_src = first_of_chain + i;
      chain_order.push_back(servers[mcast_src]);
    }

    res.push_back(chain_order);
  }

  return res;
}

} // namespace ns3