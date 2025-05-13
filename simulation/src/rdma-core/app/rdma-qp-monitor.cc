#include "ns3/rdma-qp-monitor.h"
#include "ns3/rdma-network.h"
#include "ns3/rdma-reliable-qp.h"
#include "ns3/rdma-hw.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("QpMonitor");
NS_OBJECT_ENSURE_REGISTERED(QpMonitor);

TypeId QpMonitor::GetTypeId()
{
  static TypeId tid = []() {
    static TypeId tid = TypeId("ns3::QpMonitor");

    tid.SetParent<RdmaConfigModule>();
    tid.AddConstructor<QpMonitor>();
    
    AddStringAttribute(tid,
      "AvroOutputFile",
      "File path where to write the Avro statistics, relatively to the config directory.",
      &QpMonitor::m_avro_out);
   
    AddTimeAttribute(tid,
      "StartTime",
      "Time when to start gathering statistics.",
      &QpMonitor::m_start);
    
    AddTimeAttribute(tid,
      "StopTime",
      "Time when to stop gathering statistics. Set to zero for infinite.",
      &QpMonitor::m_stop);
    
    AddTimeAttribute(tid,
      "IntervalTime",
      "Interval between two gathering of statistics.",
      &QpMonitor::m_interval);

    return tid;
  }();
  
  return tid;
}

QpMonitor::~QpMonitor()
{
  for(EventId event : m_events) {
    event.Cancel();
  }
}

void QpMonitor::OnModuleLoaded(RdmaNetwork& network)
{
  m_record_writer = RdmaSerializer<QpRecord>(network.GetConfig().FindFile(m_avro_out));

  m_monitored.Add(network.GetAllServers());

  m_event.SetTask([this]() {
    OnInterval();
  });

  if(m_interval.IsZero()) {
    const Time fallback_zero_itv{Seconds(1e-6)};
    NS_LOG_WARN("The interval cannot be zero. set it to " << fallback_zero_itv);
    m_interval = fallback_zero_itv;
  }

  m_event.SetInterval(m_interval);

  // Schedule start.
  m_events.push_back(ScheduleAbs(m_start, [this]() {
    m_event.Resume();
  }));

  // Schedule end.
  if(!m_stop.IsZero()) {
    m_events.push_back(ScheduleAbs(m_stop, [this]() {
      m_event.Pause();
    }));
  }
}

void QpMonitor::OnInterval()
{
  for(Ptr<Node> node : m_monitored) {
    const Ptr<RdmaHw> hw = node->GetObject<RdmaHw>();
    
    if(!hw) {
      continue;
    }

    for(const auto& [key, sq_base] : hw->GetAllSQs()) {
      
      // Monitor only RC QPs

      Ptr<RdmaReliableSQ> sq = DynamicCast<RdmaReliableSQ>(sq_base);
      
      if(!sq) {
        continue;
      }

      QpRecord record;
      record.node = sq->GetNode()->GetId();
      record.lkey = key;
      record.time = Simulator::Now().GetSeconds();
      record.lowest_unacked_psn = sq->GetFirstUnaPSN();
      record.lowest_unsent_psn = sq->GetNextToSendPSN();
      record.end_work_psn = sq->GetNextOpFirstPSN();

      // If the QP is completed (but may be resumed later if more data is pushed to it),
      // record only once to show on graphs that the QP is completed when linking scatter points
      
      bool save_record{true};
      const bool complete{record.lowest_unacked_psn == record.end_work_psn};
      if(complete) {
        uint64_t& prev_end{m_paused_sqs[sq]};
        if(prev_end != static_cast<uint64_t>(record.end_work_psn)) {
          prev_end = record.end_work_psn;
        }
        else {
          save_record = false;
        }
      }

      if(save_record) {
        m_record_writer.write(record);
      }
    }
  }
}

} // namespace ns3