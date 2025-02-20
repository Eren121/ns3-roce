#include "ns3/ag-runtime.h"
#include "ns3/rdma-helper.h"
#include "ns3/rdma-hw.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("AgRuntime");
NS_OBJECT_ENSURE_REGISTERED(AgRuntime);

TypeId AgRuntime::GetTypeId()
{
  static TypeId tid = TypeId("ns3::AgRuntime")
    .SetParent<Object>()
  ;
  return tid;
}

AgRuntime::AgRuntime(Ptr<Node> node, Ptr<AgShared> shared)
  : m_config{shared->GetConfig()},
    m_shared{shared}
{
  bool found{false};
  const NodeContainer& nodes{m_shared->GetServers()};
  for(block_id_t i{0}; i < nodes.GetN(); i++) {
    if(nodes.Get(i) == node) {
      m_block = i;
      found = true;
      break;
    }
  }

  if(!found) {
    NS_ABORT_MSG("Cannot find my node in the server list");
  }

  shared->RegisterNode(this);
}

bool AgRuntime::MarkChunkAsReceived(chunk_id_t chunk)
{
  NS_ASSERT(m_state == AgState::Multicast);
  
  auto it{m_recv.insert(chunk)};
  const bool newly_inserted{it.second};

  return newly_inserted;
}

Ptr<Node> AgRuntime::GetNode() const
{
  return m_shared->GetServers().Get(m_block);
}

Ptr<Node> AgRuntime::GetLeft() const
{
  const NodeContainer& nodes{m_shared->GetServers()};
  return nodes.Get(PositiveModulo<int64_t>(int64_t(m_block) - 1, nodes.GetN()));
}

Ptr<Node> AgRuntime::GetRight() const
{
  const NodeContainer& nodes{m_shared->GetServers()};
  return nodes.Get((m_block + 1) % nodes.GetN());
}

bool AgRuntime::IsFirstInChain() const
{
  return m_config->IsFirstInChain(m_block);
}

AgState AgRuntime::GetState() const
{
  return m_state;
}

block_id_t AgRuntime::GetBlock() const
{
  return m_block;
}

void AgRuntime::CompleteChain()
{
  NS_LOG_FUNCTION(m_block);

  NS_ASSERT(m_state == AgState::Multicast);

  m_completed_chains++;
  if(m_completed_chains == m_config->GetRootCount()) {
    TransitionToRecovery();
  }
}

void AgRuntime::SetState(AgState state)
{
  NS_LOG_FUNCTION(m_block << state);

  m_state = state;
  m_shared->RegisterStateTransition(m_state);
}

void AgRuntime::TransitionToRecovery()
{
  NS_LOG_FUNCTION(m_block);
  
  NS_ASSERT(m_state == AgState::Multicast);
  
  // The sender has always its own chunks

  const chunk_id_t offset{m_block * m_config->GetPerBlockChunkCount()};
  for(chunk_id_t i{0}; i < m_config->GetPerBlockChunkCount(); i++) {
    MarkChunkAsReceived(offset + i);
  }

  SetState(AgState::Recovery);
  m_torecover = m_config->BuildToRecover(m_recv);
  RegisterMissedChunks();
}

AgRuntime& AgRuntime::GetRightRuntime() const
{
  return *m_shared->GetNode((m_block + 1) % m_config->GetBlockCount());
}

uint64_t AgRuntime::GetCutoffByteSize() const
{
  const Ptr<RdmaHw> rdma{GetNode()->GetObject<RdmaHw>()};
  const uint64_t per_node_bytes{m_config->GetChunkByteSize() * m_config->GetPerBlockChunkCount()};
  const uint64_t max_mcast_chain_length{CeilDiv(m_config->GetBlockCount(), m_config->GetRootCount())};

  // Paper value
  const uint64_t additional_delay{rdma->GetMTU() * 100}; // A bit random value, all should it be is > RTT
  const uint64_t cutoff_bytes{per_node_bytes * max_mcast_chain_length + additional_delay};
  
  return cutoff_bytes;
}

void AgRuntime::NotifyRecvRecoveryRequest()
{
  NS_LOG_FUNCTION(m_block);

  // It is possible that the phase is still mcast,
  // But we still need to register the request.

  NS_ASSERT(GetRightRuntime().m_state == AgState::Recovery);

  // We magically fetch the blocks that the right nodes has missed, simpler
  const uint32_t missed{GetRightRuntime().m_torecover.size()};
  NS_LOG_LOGIC("Right node has partially missed " << missed << " blocks");
  m_has_recv_recover_req = true;

  if(missed == 0) {
    m_rec_sent_all = true;
  }

  TryUpdateState();
}

void AgRuntime::NotifyRecovery(block_id_t block)
{
  NS_LOG_FUNCTION(m_block << block);

  NS_ASSERT(m_state == AgState::Recovery); // The recovery cannot be completed until all notifs have been received

  NS_LOG_LOGIC("Node " << m_block << " received recovery of block " << block);
  m_torecover.erase(block);
  
  if(m_torecover.empty()) {
    m_has_recovered_all = true;
  }

  TryUpdateState();
}

bool AgRuntime::HasAllChunks() const
{
  return m_state != AgState::Multicast
    && m_torecover.size() == 0;
}

bool AgRuntime::NotifyReceiveChunk(chunk_id_t chunk)
{
  if(m_state != AgState::Multicast) { return false; }
  if(!MarkChunkAsReceived(chunk)) { return false; }

  if(m_config->IsLastChunkOfChain(chunk)) {
    CompleteChain();
  }
  
  return m_state == AgState::Recovery;
}

void AgRuntime::RegisterMissedChunks()
{
  NS_LOG_FUNCTION(m_block);

  uint64_t total{};

  for(chunk_id_t chunk{0}; chunk < m_config->GetTotalChunkCount(); chunk++) {
    if(!m_recv.contains(chunk)) {
      m_shared->AddMissedChunk(m_block, chunk);
      total++;
    }
  }

  NS_LOG_INFO("Missed " << total << " chunks in " << m_torecover.size() << " blocks");
}

void AgRuntime::TryUpdateState()
{
  NS_LOG_FUNCTION(m_block);

  if(m_state != AgState::Recovery) {
    return;
  }
  
  const auto& right_torecover{GetRightRuntime().m_torecover};
  const uint32_t right_missed_blocks{right_torecover.size()};

  NS_LOG_LOGIC("{"
    << "has_recovered_all=" << m_has_recovered_all << ","
    << "has_recv_recover_req=" << m_has_recv_recover_req << "}");

  if(m_has_recv_recover_req && !m_rec_sent_all) {

    for(const auto& [block, chunk_count] : right_torecover) {
      
      if(m_rec_sent.count(block) > 0) {
        NS_LOG_LOGIC("Skip block ale=ready sent " << block);
        continue; // Don't send the same block multiple time!
      }

      const bool can_send{m_torecover.count(block) == 0};
      if(can_send) {

        m_rec_sent.insert(block);
        
        NS_LOG_LOGIC("["
          << m_block << "]"
          << " Sending {block=" << block << ",chunk_count=" << chunk_count
          << "} to right node because right node missed them.");

        RdmaTxQueuePair::SendRequest sr;
        sr.imm = block;
        sr.payload_size = chunk_count * m_config->GetChunkByteSize();
        sr.on_send = [this, &right_torecover]() {
          // The received has ACKed, so right node has removed the block from `right_torecover`
          if(right_torecover.empty()) {
            m_rec_sent_all = true;
          }
          TryUpdateState();
        };

        recovery_sq->PostSend(sr);
      }
      else {
        NS_LOG_LOGIC(m_block << " cannot send block " << block);
      }
    }
  }

  if(m_has_recovered_all && m_rec_sent_all && m_state != AgState::Finished) {

    SetState(AgState::Finished);
    NS_LOG_INFO("Completed " << m_block);
  }
}

} // namespace ns3