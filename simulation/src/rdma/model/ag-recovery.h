#pragma once

#include <unordered_map>
#include <unordered_set>
#include <cstdint>
#include <ns3/ag-circle.h>

namespace ns3 {

class AgRecovery
{
public:
  using RecoveryRequest = std::unordered_map<AgConfig::block_id_t, uint32_t>;
  
  static uint64_t GetTotalMissedChunks(const RecoveryRequest& req);

  AgRecovery() = default;
  AgRecovery(const AgConfig& config);
  void ReceiveMcastChunk(AgConfig::chunk_id_t chunk);
  void ReceiveRecoveryAck(AgConfig::block_id_t block);

  /**
   * A recovery request is a list of pair (block_id, count) that requests `count` chunks from the block.
   */
  RecoveryRequest MakeRecoveryRequest() const;

  bool IsMcastComplete() const;
  
private:
  AgConfig m_config;
  std::unordered_map<AgConfig::block_id_t, uint32_t> m_recv_chunk_count_per_block;
  std::unordered_set<AgConfig::chunk_id_t> m_recv_chunks;
  uint32_t m_recv_last_chunk_count{};
  bool m_mcast_complete{};
};

} // namespace ns3