#pragma once

#include "ns3/ag-config.h"
#include "ns3/node-container.h"
#include "ns3/rdma-serdes.h"
#include "ns3/ag-recv-chunk-record.h"
#include <avro/Encoder.hh>
#include <cstdint>
#include <set>

namespace ns3 {

class AgRuntime;

/**
 * @brief Shared state between all nodes for the allgather to simplify.
 */
class AgShared : public Object
{
public:
  AgShared(Ptr<AgConfig> config, NodeContainer servers);

  struct MissedChunkRecord
  {
    block_id_t block;
    chunk_id_t chunk;
  };

  using MissedChunkList = std::vector<MissedChunkRecord>;

public:
  static TypeId GetTypeId();

  void NotifyCutoffTimerTriggered(block_id_t block);
  void AddMissedChunk(block_id_t block, chunk_id_t chunk);
  void RegisterRecvChunk(block_id_t block, chunk_id_t chunk);
  void RegisterMissedDataChunkCount(block_id_t block, uint64_t count);
  void RegisterStateTransition(AgState state);

  void RegisterNode(Ptr<AgRuntime> node);
  Ptr<AgRuntime> GetNode(block_id_t node) const;
  Ptr<AgConfig> GetConfig() const;
  NodeContainer GetServers() const;

  void NotifyChunkRecovered() {
    m_retr_chunks_tot++;
  }
  
private:
  void Finish() const;
  void DumpStats() const;

  void DumpMissedChunks() const;
  fs::path FindFile(fs::path in) const;

private:
  Time m_start;
  Time m_mcast_elapsed;
  Ptr<AgConfig> m_config;
  std::map<block_id_t, Ptr<AgRuntime>> m_nodes;
  uint64_t m_completed_apps{}; //!< Count of finished apps.
  uint64_t m_completed_mcasts{};
  uint64_t m_missed_data_chunks_tot{};
  uint64_t m_missed_chunks_tot{};
  uint64_t m_retr_chunks_tot{}; // Total count of retransmitted chunks in the recovery phase.
  int m_cutoff_triggered{};
  NodeContainer m_servers;
  std::unique_ptr<RdmaSerializer<AgRecvChunkRecord>> m_recv_chunks_writer;
};

} // namesppace ns3