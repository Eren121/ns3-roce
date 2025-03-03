#pragma once

#include "ns3/object.h"
#include "ns3/rdma-helper.h"
#include "ns3/filesystem.h"
#include <cstdint>
#include <map>
#include <set>

namespace ns3 {

using chunk_id_t = uint64_t;
using block_id_t = uint64_t;
using segment_id_t = uint64_t;

enum class AgState {
  Multicast,
  Recovery,
  Finished
};

std::ostream& operator<<(std::ostream& lhs, AgState rhs);

enum class AgChainOrder {
  First,
  Middle,
  Last
};

enum class AgPort {
  Multicast,
  RecoveryLeft,
  RecoveryRight,
  ChainPrev,
  ChainNext
};

/**
 * @brief Constant data for a given multicast and a given node.
 */
class AgConfig : public Object
{
public:
  static TypeId GetTypeId();

  void OnAllFinished();
  void SetBlockCount(uint64_t count);
  
  uint64_t GetChunkByteSize() const;
  uint32_t GetMulticastGroup() const;
  uint32_t GetPriority() const;
  uint16_t GetPort(AgPort port) const;
  uint64_t GetBlockCount() const;
  uint64_t GetRootCount() const;
  uint64_t GetPerBlockChunkCount() const;
  uint64_t GetTotalChunkCount() const;
  block_id_t GetNearestFirstBlockHigherOrEqu(block_id_t block) const;
  bool IsFirstInChain(block_id_t block) const;
  AgChainOrder GetChainOrder(block_id_t block) const;
  block_id_t GetOriginalSender(chunk_id_t chunk) const;
  bool IsLastChunkOfBlock(chunk_id_t chunk) const;
  bool IsLastChunkOfChain(chunk_id_t chunk) const;
  uint64_t GetPerNodeSegmentCount() const;
  uint64_t GetTotalSegmentCount() const;
  uint64_t GetPerSegmentParityChunkCount() const;
  uint64_t GetPerSegmentDataChunkCount() const;
  segment_id_t GetSegmentOfChunk(chunk_id_t chunk) const;
  block_id_t GetBlockOfSegment(segment_id_t segment) const;

  /**
   * @return The count of missed chunks per block, taking into account FEC.
   */
  std::map<block_id_t, uint64_t> BuildToRecover(const std::set<chunk_id_t>& recv) const;

  fs::path dump_stats;
  fs::path dump_missed_chunks;
  fs::path dump_recv_chunks;

private:
  /**
   * @return Count of unrecoverable chunks per segment.
   */
  std::map<segment_id_t, uint64_t> BuildPartialSegments(const std::set<chunk_id_t>& recv) const;

  //
  // Data that is likely identical between all allgathers
  //

  uint64_t m_sparity; //!< How many parity chunks per segment
  uint64_t m_sdata; //!< How many data chunks per segment
  uint64_t m_csize; //!< Chunk size in bytes
  uint64_t m_roots; //!< Count of multicast roots.
  uint64_t m_pernode; //!< Count of bytes to send in each node. At the end of the allgather each node has `size * node_count` bytes.
  
  uint32_t m_priority; //!< PFC priority group for all communication.
  uint32_t m_group; //!< Multicast group
  uint16_t m_port_mcast; //!< Multicast comm. port
  uint16_t m_port_lnode; //!< Right node RC comm. port
  uint16_t m_port_rnode; //!< Left Node RC comm. port
  uint16_t m_port_prev; //!< Port to comm with prev mcast src
  uint16_t m_port_next; //!< Port to comm with next mcast src

  //
  // Data that can be different between 2 different allgathers
  //

  uint64_t m_nodes; //!< Count of nodes participating in the allgather
  Callback<void> m_on_all_finished;
};

} // namesppace ns3