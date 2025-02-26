#include "ns3/rdma-qlen-monitor.h"
#include "ns3/rdma-network.h"

namespace ns3 {

QLenMonitor::QLenMonitor(const RdmaNetwork& network)
  : m_network{network},
    m_config{network.GetConfig().qlen_monitor}
{
  for(EventId event : m_events) {
    event.Cancel();
  }
}

QLenMonitor::~QLenMonitor()
{
  Dump();
}

void QLenMonitor::Start()
{
  if(!Enabled()) {
    return;
  }

  m_monitored.Add(m_network.GetAllSwitches());
  m_event.SetTask([this]() { GatherData(); });
  m_event.SetInterval(m_config.interval);
  m_events.push_back(ScheduleAbs(m_config.start, [this]() { m_event.Resume(); }));
  m_events.push_back(ScheduleAbs(m_config.end,  [this]() { m_event.Pause(); }));
}

bool QLenMonitor::Enabled() const
{
  return m_config.enable
    && !m_config.output.empty()
    && m_config.end >= m_config.start;
}
  
void QLenMonitor::Dump() const
{
  if(!Enabled()) {
    return;
  }

  std::ofstream out{m_network.GetConfig().FindFile(m_config.output)};
  out << rfl::json::write(m_records, YYJSON_WRITE_PRETTY);
}

void QLenMonitor::GatherData()
{
  for(Ptr<Node> node : m_monitored) {
    Ptr<SwitchNode> sw = DynamicCast<SwitchNode>(node);
    if(!sw) { continue; }

    for(iface_id_t j{1}; j < sw->GetNDevices(); j++) {

      QLenRecord entry;
      entry.node = sw->GetId();
      entry.time = Simulator::Now().GetSeconds();
      entry.iface = j;

      for (priority_t k{0}; k < SwitchMmu::qCnt; k++) {
        entry.egress += sw->m_mmu->egress_bytes[j][k];
        entry.ingress += sw->m_mmu->ingress_bytes[j][k];
      }

      m_records.push_back(entry);
    }
  }
}

} // namespace ns3