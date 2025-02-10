#include "recovery.h"
#include "circle.h"
#include <ns3/assert.h>

namespace ns3 {


AgMcastChain::AgMcastChain(uint32_t node_count, uint32_t root_count, uint32_t chunk_per_block)
  : m_node_count{node_count},
    m_root_count{root_count},
    m_chunk_per_block{chunk_per_block}
{
}

void AgMcastChain::OnRecvMcastChunk(uint32_t chunk)
{
  const bool newly_inserted{m_recv_chunks.insert(chunk)->second};
  if(!newly_inserted) {
    return;
  }

  const uint32_t block{chunk / m_node_count};
  m_recv_chunk_count_per_block[block]++;

  if(AgIsLastChainChunk(m_node_count, m_root_count, m_chunk_per_block, chunk)) {
    m_recv_last_chunk_count++;

    if(m_recv_last_chunk_count == m_root_count) {
      m_complete = true;
    }
  }
}

bool AgMcastChain::IsComplete() const
{
  return m_complete;
}

void AgRecovery::ReceiveRecoveryAck(uint32_t block)
{
  NS_ASSERT(m_missed_blocks > 0);
  m_missed_blocks--;

  m_recv_chunk_count_per_block[block] = m_chunk_per_block;
}

} // namespace ns3