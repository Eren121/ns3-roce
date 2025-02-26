#include "ns3/ag-config.h"
#include "ns3/uinteger.h"
#include "ns3/string.h"
#include "ns3/rdma-helper.h"
#include "ns3/rdma-reflection-helper.h"

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
                  UintegerValue(1),
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
    .AddAttribute("OnAllFinished",
                  "Callback called when the allgather is finished for all nodes",
                  CallbackValue(),
                  MakeCallbackAccessor(&AgConfig::m_on_all_finished),
                  MakeCallbackChecker())
  ;
  return tid;
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

bool AgConfig::IsFirstInChain(block_id_t block) const
{
  const uint32_t nearest_first_block_higher_or_equ{
    CeilDiv(block * m_roots, m_nodes) * m_nodes / m_roots
  };

  return block == nearest_first_block_higher_or_equ;
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

uint64_t AgConfig::GetPerNodeSegmentCount() const
{
  const uint64_t pernode_data_packets{CeilDiv(m_pernode, m_csize)};
  return CeilDiv(pernode_data_packets, m_sdata);
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
  return chunk / (m_sdata + m_sparity);
}

block_id_t AgConfig::GetBlockOfSegment(segment_id_t segment) const
{
  return segment / GetPerNodeSegmentCount();
}

std::map<block_id_t, uint64_t> AgConfig::BuildToRecover(const std::set<chunk_id_t>& recv) const
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

std::map<segment_id_t, uint64_t> AgConfig::BuildPartialSegments(const std::set<chunk_id_t>& recv) const
{
  std::map<segment_id_t, uint64_t> missed_per_segment;
  for(chunk_id_t chunk{0}; chunk < GetTotalChunkCount(); chunk++) {
    if(!recv.contains(chunk)) {
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

std::ostream& operator<<(std::ostream& lhs, AgState rhs)
{
  lhs << rfl::enum_to_string(rhs);
  return lhs;
}

} // namespace ns3