#include "ns3/ag-config.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "ns3/string.h"
#include "ns3/rdma-helper.h"
#include "ns3/rdma-reflection-helper.h"
#include "ns3/rdma-random.h"

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED(AgConfig);

TypeId AgConfig::GetTypeId()
{
  static TypeId tid = TypeId("ns3::AgConfig")
    .SetParent<Object>()
    .AddConstructor<AgConfig>()
    .AddAttribute("PerNodeBytes",
      "Count of bytes to send per node",
      UintegerValue(1),
      MakeUintegerAccessor(&AgConfig::m_pernode),
      MakeUintegerChecker<uint64_t>())
    .AddAttribute("PriorityGroup",
      "PFC priority group for all communication",
      UintegerValue(3),
      MakeUintegerAccessor(&AgConfig::m_priority),
      MakeUintegerChecker<uint32_t>())
    .AddAttribute("ParityChunkPerSegmentCount",
      "Count of parity chunks per segment",
      UintegerValue(0),
      MakeUintegerAccessor(&AgConfig::m_sparity),
      MakeUintegerChecker<uint64_t>())
    .AddAttribute("DataChunkPerSegmentCount",
      "Count of data chunks per segment",
      UintegerValue(1),
      MakeUintegerAccessor(&AgConfig::m_sdata),
      MakeUintegerChecker<uint64_t>())
    .AddAttribute("ChunkSize",
      "Size of a chunk in bytes",
      UintegerValue(30000),
      MakeUintegerAccessor(&AgConfig::m_csize),
      MakeUintegerChecker<uint64_t>())
    .AddAttribute("RootCount",
      "Count of simultaneous multicasts",
      UintegerValue(1),
      MakeUintegerAccessor(&AgConfig::m_roots),
      MakeUintegerChecker<uint64_t>())
    .AddAttribute("McastGroup",
      "Multicast group",
      UintegerValue(1),
      MakeUintegerAccessor(&AgConfig::m_group),
      MakeUintegerChecker<group_id_t>())
    .AddAttribute("PortMcast",
      "Port to use for the multicast",
      UintegerValue(100),
      MakeUintegerAccessor(&AgConfig::m_port_mcast),
      MakeUintegerChecker<uint16_t>())
    .AddAttribute("PortLeft",
      "Peer port to use for communication with left node",
      UintegerValue(101),
      MakeUintegerAccessor(&AgConfig::m_port_lnode),
      MakeUintegerChecker<uint16_t>())
    .AddAttribute("PortRight",
      "Peer port to use for communication with right node",
      UintegerValue(102),
      MakeUintegerAccessor(&AgConfig::m_port_rnode),
      MakeUintegerChecker<uint16_t>())
    .AddAttribute("PortPrev",
      "Peer port to use for communication with previous node (to receive notification to start multicast)",
      UintegerValue(103),
      MakeUintegerAccessor(&AgConfig::m_port_prev),
      MakeUintegerChecker<uint16_t>())
    .AddAttribute("PortNext",
      "Peer port to use for communication with next node (to start next multicast)",
      UintegerValue(104),
      MakeUintegerAccessor(&AgConfig::m_port_next),
      MakeUintegerChecker<uint16_t>())
    .AddAttribute("DumpStats",
      "Path to write statistics",
      StringValue(""),
      MakeStringAccessor(&AgConfig::dump_stats),
      MakeStringChecker())
    .AddAttribute("DumpMissedChunks",
      "Path to write missed chunks records",
      StringValue(""),
      MakeStringAccessor(&AgConfig::dump_missed_chunks),
      MakeStringChecker())
    .AddAttribute("DumpRecvChunks",
      "Path to write received chunks over time records",
      StringValue(""),
      MakeStringAccessor(&AgConfig::dump_recv_chunks),
      MakeStringChecker())
    .AddAttribute("McastStrategy",
      "One of 'markov', 'simulate'",
      StringValue("simulate"),
      MakeStringAccessor(&AgConfig::m_mcastStrategy),
      MakeStringChecker())
    .AddAttribute("MarkovBurstDensity",
      "Fraction of packets lost within a burst",
      DoubleValue(0.5),
      MakeDoubleAccessor(&AgConfig::m_markovBurstDensity),
      MakeDoubleChecker<double>())
    .AddAttribute("MarkovGapDensity",
      "Fraction of packets lost within a gap",
      DoubleValue(0.01),
      MakeDoubleAccessor(&AgConfig::m_markovGapDensity),
      MakeDoubleChecker<double>())
    .AddAttribute("MarkovBurstLength",
      "Average number of packets lost in a burst",
      UintegerValue(5),
      MakeUintegerAccessor(&AgConfig::m_markovBurstLength),
      MakeUintegerChecker<uint32_t>())
    .AddAttribute("MarkovGapLength",
      "Average number of packets received in a gap",
      UintegerValue(5000),
      MakeUintegerAccessor(&AgConfig::m_markovGapLength),
      MakeUintegerChecker<uint32_t>())
    .AddAttribute("BlockingRing",
      "Whether in the recovery phase, neighbor should wait having recovered all blocks",
      UintegerValue(5000),
      MakeUintegerAccessor(&AgConfig::m_markovGapLength),
      MakeUintegerChecker<uint32_t>())
    .AddAttribute("OnAllFinished",
      "Callback called when the allgather is finished for all nodes",
      CallbackValue(),
      MakeCallbackAccessor(&AgConfig::m_on_all_finished),
      MakeCallbackChecker())
  ;
  return tid;
}

void AgConfig::SetMtu(uint32_t mtu)
{
  m_mtu = mtu;
}

uint64_t AgConfig::GetPerChunkPacketCount() const
{
  return CeilDiv(m_csize, m_mtu);
}

uint64_t AgConfig::GetPerBlockPacketCount() const
{
  return GetPerChunkPacketCount() * GetPerBlockChunkCount();
}

uint32_t AgConfig::GetMcastImmData(block_id_t sender, pkt_id_t pkt_offset) const
{
  const pkt_id_t start{sender * GetPerBlockPacketCount()};
  return static_cast<uint32_t>(start + pkt_offset);
}

void AgConfig::ParseMcastImmData(uint32_t imm, block_id_t& sender, chunk_id_t& chunk) const
{
  chunk = imm / GetPerChunkPacketCount();
  sender = GetOriginalSender(chunk);
}

void AgConfig::OnAllFinished()
{
  m_on_all_finished();
}

void AgConfig::SetBlockCount(uint64_t count)
{
  m_nodes = count;
}

uint64_t AgConfig::GetChunkByteSize() const
{
  return m_csize;
}

uint32_t AgConfig::GetMulticastGroup() const
{
  return m_group;
}

uint64_t AgConfig::GetRootCount() const
{
  return m_roots;
}

uint64_t AgConfig::GetBlockCount() const
{
  return m_nodes;
}


uint32_t AgConfig::GetPriority() const
{
  return m_priority;
}

uint16_t AgConfig::GetPort(AgPort port) const
{
  switch(port) {
    case AgPort::Multicast: return m_port_mcast; 
    case AgPort::RecoveryLeft: return m_port_lnode; 
    case AgPort::RecoveryRight: return m_port_rnode;
    case AgPort::ChainPrev: return m_port_prev;  
    case AgPort::ChainNext: return m_port_next; 
  }

  NS_ABORT_MSG("Trying to get an unknown port type");
  return {};
}

block_id_t AgConfig::GetNearestFirstBlockHigherOrEqu(block_id_t block) const
{
  return CeilDiv(block * m_roots, m_nodes) * m_nodes / m_roots;
}

bool AgConfig::IsFirstInChain(block_id_t block) const
{
  return block == GetNearestFirstBlockHigherOrEqu(block);
}

AgChainOrder AgConfig::GetChainOrder(block_id_t block) const
{
  if(IsFirstInChain(block)) {
    return AgChainOrder::First;
  }
  
  if(IsFirstInChain((block + 1) % m_nodes)) {
    return AgChainOrder::Last;
  }

  return AgChainOrder::Middle;
}

block_id_t AgConfig::GetOriginalSender(chunk_id_t chunk) const
{
  return chunk / GetPerBlockChunkCount();
}

bool AgConfig::IsLastChunkOfBlock(chunk_id_t chunk) const
{
  return (chunk + 1) % GetPerBlockChunkCount() == 0;
}

bool AgConfig::IsLastChunkOfChain(block_id_t chunk) const
{
  const block_id_t block{GetOriginalSender(chunk)};
  return GetChainOrder(block) == AgChainOrder::Last &&  IsLastChunkOfBlock(chunk);
}

uint64_t AgConfig::GetPerBlockChunkCount() const
{
  return GetPerNodeSegmentCount() * (m_sdata + m_sparity);
}

uint64_t AgConfig::GetTotalChunkCount() const
{
  return GetPerBlockChunkCount() * m_nodes;
}

uint64_t AgConfig::GetTotalDataChunkCount() const
{
  return GetPerNodeSegmentCount() * m_sdata * m_nodes;
}

uint64_t AgConfig::GetPerNodeSegmentCount() const
{
  const uint64_t pernode_data_chunks{CeilDiv(m_pernode, m_csize)};
  return CeilDiv(pernode_data_chunks, m_sdata);
}

uint64_t AgConfig::GetTotalSegmentCount() const
{
  return GetPerNodeSegmentCount() * m_nodes;
}

uint64_t AgConfig::GetPerSegmentParityChunkCount() const
{
  return m_sparity;
}

uint64_t AgConfig::GetPerSegmentDataChunkCount() const
{
  return m_sdata;
}

segment_id_t AgConfig::GetSegmentOfChunk(chunk_id_t chunk) const
{
  const block_id_t block{GetOriginalSender(chunk)};
  const segment_id_t first_segment{block * GetPerNodeSegmentCount()};
  return first_segment + (chunk % GetPerNodeSegmentCount());
}

block_id_t AgConfig::GetBlockOfSegment(segment_id_t segment) const
{
  return segment / GetPerNodeSegmentCount();
}

std::map<block_id_t, uint64_t> AgConfig::BuildToRecover(const std::vector<bool>& recv) const
{
  std::map<block_id_t, uint64_t> missed_per_block;

  for(const auto& [segment, missed_chunks] : BuildPartialSegments(recv)) {
    // Can be zero because subtraction of FEC
    // Avoid creating entry of size zero in `missed_per_block`
    if(missed_chunks > 0) { 
      missed_per_block[GetBlockOfSegment(segment)] += missed_chunks;
    }
  }

  return missed_per_block;
}

std::map<segment_id_t, uint64_t> AgConfig::BuildPartialSegments(const std::vector<bool>& recv) const
{
  std::map<segment_id_t, uint64_t> missed_per_segment;
  for(chunk_id_t chunk{0}; chunk < GetTotalChunkCount(); chunk++) {
    if(!recv[chunk]) {
      const segment_id_t segment{GetSegmentOfChunk(chunk)};
      missed_per_segment[segment]++;
    }
  }

  // Reconstruct when possible, we don't care about missed parity packets
  for(auto& [_, chunks] : missed_per_segment) {
    if(chunks < m_sparity) { chunks = 0; }
    else { chunks -= m_sparity; }
  }

  return missed_per_segment;
}

enum MarkovState {
  B_L, // Burst loss
  B_R, // Burst recv
  G_L, // Gap loss
  G_R // Gap recv
};

MarkovState nextState(MarkovState currentState, double p_b, double p_g, double L_b, double L_g)
{
  double rand_val = GenRandomDouble(0.0, 1.0);

  // B_R and G_L are "fake" states that last zero time
  
  switch (currentState) {
    case B_R:
      currentState = B_L;
      break;
    case G_L:
      currentState = G_R;
      break;
  }

  switch (currentState) {
      case B_L:
        if(rand_val < (1.0 / L_b)) {
          return G_R;
        }
        else {
          rand_val = GenRandomDouble(0.0, 1.0);
          if(rand_val < p_b) {
            return B_L;
          }
          else {
            return B_R;
          }
        }
      case G_R:
        if(rand_val < (1.0 / L_g)) {
          return B_L;
        }
        else {
          rand_val = GenRandomDouble(0.0, 1.0);
          if(rand_val < p_g) {
            return G_L;
          }
          else {
            return G_R;
          }
        }
  }

  throw std::invalid_argument{"Unknown state"};
}

std::vector<bool> AgConfig::SimulateMarkov() const
{
  const uint64_t tot_pkt{GetTotalChunkCount() * GetPerChunkPacketCount()};
  std::vector<bool> recv(tot_pkt);

  MarkovState state{G_R};

  for(pkt_id_t pkt{0}; pkt < tot_pkt; pkt++) {
    state = nextState(
      state,
      m_markovBurstDensity, m_markovGapDensity,
      m_markovBurstLength, m_markovGapLength);

    if (state == B_R || state == G_R) {
      recv[pkt] = true;
    }
  }

  return recv;
}

std::ostream& operator<<(std::ostream& lhs, AgState rhs)
{
  lhs << rfl::enum_to_string(rhs);
  return lhs;
}

} // namespace ns3