#pragma once

#include "ns3/ag-config.h"
#include "ns3/node-container.h"
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
  void RegisterStateTransition(AgState state);

  void RegisterNode(Ptr<AgRuntime> node);
  Ptr<AgRuntime> GetNode(block_id_t node) const;
  Ptr<AgConfig> GetConfig() const;
  NodeContainer GetServers() const;

private:
  void DumpStats() const;
  void DumpMissedChunks() const;

private:
  Time m_start;
  Time m_mcast_elapsed;
  Ptr<AgConfig> m_config;
  std::map<block_id_t, Ptr<AgRuntime>> m_nodes;
  uint64_t m_completed_apps{}; //!< Count of finished apps.
  uint64_t m_completed_mcasts{};
  int m_cutoff_triggered{};
  MissedChunkList m_missed;
  NodeContainer m_servers;
};

} // namesppace ns3