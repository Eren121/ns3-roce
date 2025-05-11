#pragma once

#include "ns3/rdma-serdes.h"
#include "ns3/qp-record.h"
#include "ns3/rdma-config-module.h"
#include "ns3/rdma-helper.h"
#include "ns3/filesystem.h"
#include <cstdint>
#include <vector>
#include <unordered_map>

namespace ns3 {

class RdmaNetwork;
class RdmaReliableSQ;

/**
 * Module to monitors the progress of all RDMA queue pairs.
 * 
 * The available columns in the records are:
 * - node: Source node.
 * - lkey: Source QP key.
 * - time: Time data is gather.
 * - lowest_unacked_psn: Lowest unacked PSN.
 * - lowest_unsent_psn: Lowest unsent PSN.
 * - end_work_psn: Amount of bytes pushed to the SQ to be transmitted.
 */
class QpMonitor final : public RdmaConfigModule
{
public:
    static TypeId GetTypeId();

public:
    ~QpMonitor();
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
    std::unordered_map<Ptr<RdmaReliableSQ>, uint64_t> m_paused_sqs;
    RdmaSerializer<QpRecord> m_record_writer;
};

} // namespace ns3