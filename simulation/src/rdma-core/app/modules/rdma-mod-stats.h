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
class RdmaModStats final : public RdmaConfigModule
{
public:
    static TypeId GetTypeId();

public:
    ~RdmaModStats();
    void OnModuleLoaded(RdmaNetwork& network) override;

    void Resume();
    void Pause();

private:
    void OnInterval();

private:
    std::string m_json_out;
};

} // namespace ns3