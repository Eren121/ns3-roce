#pragma once

#include <cstdint>
#include "ns3/ag-config.h"
#include "ns3/ag-shared.h"
#include "ns3/rdma-reliable-qp.h"
#include <set>

namespace ns3 {

/**
 * @brief Runtime data for a multicast.
 */
class AgRuntime : public Object
{
public:
  AgRuntime(Ptr<Node> node, AgShared& shared);

  static TypeId GetTypeId();
  
  bool IsFirstInChain() const;
  AgState GetState() const;
  block_id_t GetBlock() const;
  Ptr<Node> GetNode() const;
  Ptr<Node> GetLeft() const;
  Ptr<Node> GetRight() const;
  
  void NotifyRecvRecoveryRequest();
  void NotifyRecovery(block_id_t block);
  bool NotifyReceiveChunk(chunk_id_t chunk); //!< Returns true when the recovery is ready to start

  /**
   * @return Cutoff timer (in bytes, needs to be converted to time with bandwidth).
   */
  uint64_t GetCutoffByteSize() const;

  /**
   * @brief Transition to recovery phase, even if the multicast is still not completed.
   */
  void TransitionToRecovery();

  bool HasAllChunks() const;
  
  void TryUpdateState();

  Ptr<RdmaReliableSQ> recovery_sq;

private:
  /**
   * @return true If the chunk has not been received before.
   */
  bool MarkChunkAsReceived(chunk_id_t chunk);
  void CompleteChain();
  void RegisterMissedChunks();
  void SetState(AgState state);
  AgRuntime& GetRightRuntime() const;

  block_id_t m_block;
  Ptr<AgConfig> m_config;
  AgShared& m_shared;
  AgState m_state{AgState::Multicast};
  uint64_t m_completed_chains{}; //! Count of completed multicast chains
  std::set<chunk_id_t> m_recv; //!< All received chunk
  std::set<block_id_t> m_rec_sent; //!< All blocks sent for recovery
  std::map<block_id_t, uint64_t> m_torecover; // !< Missed chunk count per block (taking into account FEC)
  
  //
  // Recovery state machine
  //

  bool m_has_recovered_all{false}; //!< True when this node got notified that all blocks have been recovered
  bool m_has_recv_recover_req{false};
  bool m_rec_sent_all{false}; //!< True when all blocks has been sent for recovery
};

} // namesppace ns3