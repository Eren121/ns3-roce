#include "ns3/rdma-switch-buffer-monitor.h"
#include "ns3/rdma-network.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("SwitchBufferMonitor");
NS_OBJECT_ENSURE_REGISTERED(SwitchBufferMonitor);

TypeId SwitchBufferMonitor::GetTypeId()
{
  static TypeId tid = []() {
    static TypeId tid = TypeId("ns3::SwitchBufferMonitor");

    tid.SetParent<RdmaConfigModule>();
    tid.AddConstructor<SwitchBufferMonitor>();
    
    AddStringAttribute(tid,
      "AvroOutputFile",
      "File path where to write the Avro statistics, relatively to the config directory.",
      &SwitchBufferMonitor::m_avro_out);
   
    AddTimeAttribute(tid,
      "StartTime",
      "Time when to start gathering statistics.",
      &SwitchBufferMonitor::m_start);
    
    AddTimeAttribute(tid,
      "StopTime",
      "Time when to stop gathering statistics. Set to zero for infinite.",
      &SwitchBufferMonitor::m_stop);
    
    AddTimeAttribute(tid,
      "IntervalTime",
      "Interval between two gathering of statistics.",
      &SwitchBufferMonitor::m_interval);

    return tid;
  }();
  
  return tid;
}

SwitchBufferMonitor::~SwitchBufferMonitor()
{
  for(EventId event : m_events) {
    event.Cancel();
  }
}

void SwitchBufferMonitor::OnModuleLoaded(RdmaNetwork& network)
{
  m_record_writer = RdmaSerializer<SwMemRecord>(network.GetConfig().FindFile(m_avro_out));

  // Monitor all switches.
  m_monitored.Add(network.GetAllSwitches());

  // Set the callback.
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

void SwitchBufferMonitor::OnInterval()
{
  for(auto node : m_monitored) {
    auto const sw = DynamicCast<SwitchNode>(node);
    
    if(!sw) {
      continue;
    }

    for(iface_id_t if_i{1}; if_i < sw->GetNDevices(); if_i++) {

      SwMemRecord record;
      record.node = sw->GetId();
      record.time = Simulator::Now().GetSeconds();
      record.iface = if_i;

      for (priority_t prio_i{0}; prio_i < SwitchMmu::qCnt; prio_i++) {
        record.egress_bytes += sw->m_mmu->egress_bytes[if_i][prio_i];
        record.ingress_bytes += sw->m_mmu->ingress_bytes[if_i][prio_i];
      }

      m_record_writer.write(record);
    }
  }
}

} // namespace ns3