#include "ag-recovery.h"
#include <ns3/assert.h>

namespace ns3 {

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

    if(m_recv_last_chunk_count == m_config.root_count) {
      m_mcast_complete = true;
    }
  }
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
      ret[block] = missed_chunk_count;
    }
  }

  return ret;
}

} // namespace ns3