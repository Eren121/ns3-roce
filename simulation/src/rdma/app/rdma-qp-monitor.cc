#include "ns3/rdma-qp-monitor.h"
#include "ns3/rdma-network.h"
#include "ns3/rdma-reliable-qp.h"
#include "ns3/rdma-hw.h"

namespace ns3 {

QpMonitor::QpMonitor(const RdmaNetwork& network)
  : m_network{network},
    m_config{network.GetConfig().qp_monitor}
{
  for(EventId event : m_events) {
    event.Cancel();
  }
}

QpMonitor::~QpMonitor()
{
  Dump();
}

void QpMonitor::Start()
{
  if(!Enabled()) {
    return;
  }

  m_monitored.Add(m_network.GetAllServers());
  m_event.SetTask([this]() { GatherData(); });
  m_event.SetInterval(m_config.interval);
  m_events.push_back(ScheduleAbs(m_config.start, [this]() { m_event.Resume(); }));
  m_events.push_back(ScheduleAbs(m_config.end,  [this]() { m_event.Pause(); }));
}

bool QpMonitor::Enabled() const
{
  return m_config.enable
    && !m_config.output.empty()
    && m_config.end >= m_config.start;
}
  
void QpMonitor::Dump() const
{
  if(!Enabled()) {
    return;
  }

  std::ofstream out{m_network.GetConfig().FindFile(m_config.output)};
  out << rfl::json::write(m_records, YYJSON_WRITE_PRETTY);
}

void QpMonitor::GatherData()
{
  for(Ptr<Node> node : m_monitored) {
    Ptr<RdmaHw> hw = node->GetObject<RdmaHw>();
    
    if(!hw) {
      continue;
    }

    for(const auto& [key, sq_base] : hw->GetAllSQs()) {
      
      // Monitor only RC QPs

      Ptr<RdmaReliableSQ> sq = DynamicCast<RdmaReliableSQ>(sq_base);
      
      if(!sq) {
        continue;
      }

      QpRecord entry;
      entry.node = sq->GetNode()->GetId();
      entry.key = key;
      entry.time = Simulator::Now().GetSeconds();
      entry.acked = sq->GetFirstUnaPSN();
      entry.send = sq->GetNextToSendPSN();
      entry.end = sq->GetNextOpFirstPSN();

      // If the QP is completed (but may be resumed later if more data is pushed to it),
      // record only once to show on graphs that the QP is completed when linking scatter points
      
      bool record{true};
      const bool complete{entry.acked == entry.end};
      if(complete) {
        uint64_t& prev_end{m_paused_sqs[sq]};
        if(prev_end != entry.end) {
          prev_end = entry.end;
        }
        else {
          record = false;
        }
      }

      if(record) {
        m_records.push_back(entry);
      }
    }
  }
}

} // namespace ns3