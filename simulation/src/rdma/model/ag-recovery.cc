#include "ag-recovery.h"
#include <ns3/assert.h>
#include <ns3/log.h>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("AgConfig");

AgRecovery::AgRecovery(const AgConfig& config)
  : m_config{config}
{
}

void AgRecovery::ReceiveMcastChunk(AgConfig::chunk_id_t chunk)
{
  const bool newly_inserted{m_recv_chunks.insert(chunk).second};
  if(!newly_inserted) {
    return;
  }

  const AgConfig::block_id_t block{chunk / m_config.GetChunkCountPerBlock()};
  m_recv_chunk_count_per_block[block]++;

  if(AgIsLastChainChunk(m_config, chunk)) {
    m_recv_last_chunk_count++;
    NS_LOG_LOGIC("Multichain chains " << m_recv_last_chunk_count << "/" << m_config.root_count << " completed");

    if(m_recv_last_chunk_count == m_config.root_count) {
      m_mcast_complete = true;
    }
  }
}

uint64_t AgRecovery::GetTotalMissedChunks(const RecoveryRequest& req)
{
  uint64_t chunk_tot{0};
  for(const auto& [block, chunk_count]: req) {
    chunk_tot += chunk_count;
  }
  return chunk_tot;
}

std::vector<std::pair<uint64_t, uint64_t>> AgRecovery::GetPairOfMissedChunks() const
{
    std::vector<std::pair<uint64_t, uint64_t>> missed_ranges;
    
    if (m_recv_chunks.empty()) {
        return missed_ranges;
    }
    
    uint64_t count_chunk = m_config.GetChunkCountPerBlock() * m_config.node_count;
    uint64_t start = 0;
    for (uint64_t i = 0; i < count_chunk; ++i) {
        if (m_recv_chunks.find(i) == m_recv_chunks.end()) { // Chunk is missing
            start = i;
            while (i < count_chunk && m_recv_chunks.find(i) == m_recv_chunks.end()) {
                ++i;
            }
            missed_ranges.emplace_back(start, i - 1);
        }
    }
    
    return missed_ranges;
}
  

bool AgRecovery::IsMcastComplete() const
{
  return m_mcast_complete;
}


AgRecovery::RecoveryRequest AgRecovery::MakeRecoveryRequest() const
{
  RecoveryRequest ret;
  for(auto it : m_recv_chunk_count_per_block) {
    const AgConfig::block_id_t block{it.first};
    const uint32_t recv_chunk_count{it.second};
    const uint32_t missed_chunk_count{m_config.GetChunkCountPerBlock() - recv_chunk_count};
    if(missed_chunk_count > 0) {
      NS_LOG_LOGIC("Missed " << missed_chunk_count << " in block " << block);
      ret[block] = missed_chunk_count;
    }
  }

  return ret;
}

} // namespace ns3