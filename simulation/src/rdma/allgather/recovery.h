#pragma once

#include <unordered_map>
#include <unordered_set>
#include <cstdint>

namespace ns3 {

struct McastConfig
{

};

class AgRecovery
{
private:
  
};

class AgMcastChain
{
public:
  AgMcastChain(uint32_t node_count, uint32_t root_count, uint32_t chunk_per_block);
  void ReceiveMcastChunk(uint32_t chunk);
  void ReceiveRecoveryAck(uint32_t block);

  bool IsComplete() const;
  
private:
  uint32_t m_node_count{};
  uint32_t m_root_count{};
  uint32_t m_chunk_per_block{};
  std::unordered_map<uint32_t, uint32_t> m_recv_chunk_count_per_block; ///< Map a Block ID to the count of succesfully received chunks in this block.
  std::unordered_set<uint32_t> m_recv_chunks; ///< List all received chunks.
  uint32_t m_recv_last_chunk_count{}; ///< Count of last chunks received.
  bool m_complete{};
};

} // namespace ns3