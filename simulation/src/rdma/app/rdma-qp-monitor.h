#pragma once

#include "ns3/rdma-helper.h"
#include "ns3/filesystem.h"
#include <cstdint>
#include <vector>
#include <unordered_map>

namespace ns3 {

class RdmaNetwork;
class RdmaReliableSQ;

struct QpRecord
{
  node_id_t node; //! Source node
  uint64_t key; //! Source QP key
  double time; //! Time data is gather

  byte_t acked; //! Lowest unacked PSN
  byte_t send; //! Lowest unsent PSN
  byte_t end; //! Amount of bytes pushed to the SQ to be transmitted
};

class QpMonitor
{
public:
  struct Config
  {
    Time start;
    Time end;
    Time interval;
    fs::path output; //!< JSON output
    bool enable;
  };

public:
  QpMonitor(const RdmaNetwork& network);
  ~QpMonitor();

  void Start();
  bool Enabled() const;

private:
  void GatherData();
  void Dump() const;

private:
  const RdmaNetwork& m_network;
  PeriodicEvent m_event;
  Config m_config;
  NodeMap m_monitored;
  std::vector<QpRecord> m_records;
  std::vector<EventId> m_events;
  std::unordered_map<Ptr<RdmaReliableSQ>, uint64_t> m_paused_sqs;
};

} // namespace ns3