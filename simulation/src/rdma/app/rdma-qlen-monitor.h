#pragma once

#include "ns3/rdma-helper.h"
#include "ns3/filesystem.h"
#include <cstdint>
#include <vector>

namespace ns3 {

class RdmaNetwork;

struct QLenRecord
{
  node_id_t node;
  iface_id_t iface;
  double time;
  byte_t bytes;
};

class QLenMonitor
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
  QLenMonitor(const RdmaNetwork& network);
  ~QLenMonitor();

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
  std::vector<QLenRecord> m_records;
  std::vector<EventId> m_events;
};

} // namespace ns3