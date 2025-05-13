#pragma once

#include "ns3/pfc-record.h"
#include "ns3/filesystem.h"
#include "ns3/rdma-serdes.h"
#include "ns3/net-device-container.h"
#include "ns3/qbb-net-device.h"

namespace ns3 {

/**
 * Priority Flow Control (PFC) statistics can be gathered during the simulation,
 * which indicates whether a given flow of a given node is paused at a given time.
 * 
 * The data is stored in an Avro file which contain an array of `PfcRecord`.
 *
 * Fields are:
 * - time: At what time of the simulation the info applies. 
 * - node: To which node does the info applies.
 * - dev: To which devices of the node does the info applies.
 * - paused: If `true`, the flow is paused by PFC.
 */
class PfcMonitor
{
public:
    PfcMonitor(const fs::path& avro_output, const NetDeviceContainer& qbb_devs);

private:
    enum PfcState {
        Pause, Resume
    };

    void OnPfcStateChanged(Ptr<QbbNetDevice> dev, PfcState state);

private:
    RdmaSerializer<PfcRecord> m_record_writer;
};

} // namespace ns3