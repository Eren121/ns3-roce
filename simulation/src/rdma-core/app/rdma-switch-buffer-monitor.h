#pragma once

#include "ns3/rdma-serdes.h"
#include "ns3/sw-mem-record.h"
#include "ns3/rdma-config-module.h"
#include "ns3/rdma-helper.h"
#include "ns3/filesystem.h"
#include <cstdint>
#include <vector>

namespace ns3 {

class SwitchBufferMonitor final : public RdmaConfigModule
{
public:
    static TypeId GetTypeId();

public:
    ~SwitchBufferMonitor();
    void OnModuleLoaded(RdmaNetwork& network) override;

    void Resume();
    void Pause();

private:
    void OnInterval();

private:
    std::string m_avro_out;
    Time m_start;
    Time m_stop;
    Time m_interval;

    PeriodicEvent m_event;
    NodeMap m_monitored;
    std::vector<EventId> m_events;
    RdmaSerializer<SwMemRecord> m_record_writer;
};

} // namespace ns3