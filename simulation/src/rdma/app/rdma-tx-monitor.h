#pragma once

#include "ns3/rdma-serdes.h"
#include "ns3/rdma-config-module.h"
#include "ns3/filesystem.h"
#include "ns3/net-device-container.h"

namespace ns3 {

/**
 * Monitors the count of transmitted bytes on each link.
 */
class TxMonitor final : public RdmaConfigModule
{
public:
    static TypeId GetTypeId();

public:
    ~TxMonitor();
    void OnModuleLoaded(RdmaNetwork& network) override;

private:
    void OnTxSend(
        std::string context,
        Ptr<const Packet> p,
        Ptr<NetDevice> tx, Ptr<NetDevice> rx,
        Time tx_time, Time rx_time);

private:
    std::string m_avro_out;
    fs::path m_avro_out_fullpath;
    
    /// `m_txrx_bytes[i][j]` stores transmitted bytes from node `i` to node `j`. 
    std::vector<std::vector<uint64_t>> m_txrx_bytes;
};

} // namespace ns3