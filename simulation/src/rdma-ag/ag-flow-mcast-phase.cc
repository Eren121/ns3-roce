#include "ns3/ag-flow-mcast-phase.h"
#include "ns3/rdma-flow-multicast.h"
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

void AgFlowMcastPhase::StartFlow(RdmaNetwork& network, OnComplete on_complete)
{
  const DataRate bandwidth = network.GetAnyServerDataRate();
  const McastChains chains = BuildMulticastChains(network);

  const uint32_t num_pkts_per_mcast = m_num_chunks_per_node * m_num_pkts_per_chunk;

  const uint64_t reduction_bytes = static_cast<uint64_t>(num_pkts_per_mcast) * network.GetMtuBytes();

  // Next multicast happens at this time.
  const uint64_t num_bytes_mcast = static_cast<uint64_t>(num_pkts_per_mcast) * network.GetMtuBytes();
  const Time mcast_duration = network.GetAnyServerDataRate().CalculateBytesTxTime(num_bytes_mcast);
  Time next_start{};

  for(const auto& simultaneous_mcast : chains) {
    for(Ptr<Node> mcast_src : simultaneous_mcast) {
      const int mcast_src_id = mcast_src->GetId();

      // Called when any packet is received on any receiver for this multicast.
      auto on_recv_pkt = [mcast_src_id](const RdmaFlowMulticast::OnRecvPktInfo& info) {
        NS_LOG_INFO("Allgather multicast: recv packet (tx, rx, pkt) = ("
          << mcast_src_id << ", " << info.receiver << ", " << info.pkt_id);
      };
      
      auto multicast = CreateObject<RdmaFlowMulticast>();
      multicast->SetAttribute("MulticastSource", UintegerValue(mcast_src_id));
      multicast->SetAttribute("MulticastGroup", UintegerValue(m_mcast_group));
      multicast->SetAttribute("NumPackets", UintegerValue(num_pkts_per_mcast));
      multicast->SetAttribute("PfcPriority", UintegerValue(m_priority));
      multicast->SetOnRecvPktCallback(on_recv_pkt);
    
      if(m_optimize_throughput) {
        multicast->SetThroughput(bandwidth * (1.0 / m_num_mcast_roots));
      }

      const Time completion_time = bandwidth.CalculateBytesTxTime(reduction_bytes);
      Simulator::Schedule(next_start, MakeLambdaCallback([multicast, &network]() {
        multicast->StartFlow(network, {});
      }));

      m_flows.push_back(multicast);
    }

    next_start += mcast_duration;

    // Let time for the packets to arrive.
    next_start += network.GetMaxDelay();
  }

  // We can estimate the completion time.
  // This will not take into account the few packets at the end that will be maybe be missed, but how cares.
  Simulator::Schedule(next_start, MakeLambdaCallback(std::move(on_complete)));
}

auto AgFlowMcastPhase::BuildMulticastChains(RdmaNetwork& network) const -> McastChains
{
  McastChains res;
  const auto servers = network.FindMcastGroup(m_mcast_group).to_vector();

  NS_ABORT_MSG_IF(servers.size() % m_num_mcast_roots != 0,
    "Count of servers should be a multiple of the count of multicast roots.");
  
  const int num_mcast_chains = servers.size() / m_num_mcast_roots;
  
  for(int chain = 0; chain < num_mcast_chains; chain++) {
    // All multicast that happens at the same time in this timestep.
    std::vector<Ptr<Node>> mcast_at_same_time;
    for(int i = 0; i < m_num_mcast_roots; i++) {
      const uint32_t mcast_src = i * num_mcast_chains + chain;
      mcast_at_same_time.push_back(servers[mcast_src]);
    }

    res.push_back(mcast_at_same_time);
  }

  return res;
}

} // namespace ns3