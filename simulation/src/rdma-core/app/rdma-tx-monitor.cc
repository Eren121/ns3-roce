#include "ns3/rdma-tx-monitor.h"
#include "ns3/tx-record.h"
#include "ns3/rdma-reflection-helper.h"
#include "ns3/rdma-network.h"
#include "ns3/config.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("TxMonitor");
NS_OBJECT_ENSURE_REGISTERED(TxMonitor);

TypeId TxMonitor::GetTypeId()
{
  static TypeId tid = []() {
    static TypeId tid = TypeId("ns3::TxMonitor");
    
    tid.SetParent<RdmaConfigModule>();
    tid.AddConstructor<TxMonitor>();
    
    AddStringAttribute(tid,
      "AvroOutputFile",
      "File path where to write the Avro statistics, relatively to the config directory.",
      &TxMonitor::m_avro_out);

    return tid;
  }();
  
  return tid;
}

void TxMonitor::OnModuleLoaded(RdmaNetwork& network)
{
    m_avro_out_fullpath = network.GetConfig().FindFile(m_avro_out);
    
    NetDeviceContainer devs = network.GetAllQbbNetDevices();
    const size_t node_count{devs.GetN()};

    m_txrx_bytes.resize(node_count);
    for(auto& v : m_txrx_bytes) {
        v.resize(node_count);
    }

    const char* const callback_path{"/ChannelList/*/TxRxPointToPoint"};
    auto callback{MakeCallback(&TxMonitor::OnTxSend, this)};
    Config::ConnectFailSafe(callback_path, callback);
}

void TxMonitor::OnTxSend(
    std::string context,
    Ptr<const Packet> p,
    Ptr<NetDevice> tx, Ptr<NetDevice> rx,
    Time tx_time, Time rx_time)
{
    // Update the TX bytes.
    m_txrx_bytes[tx->GetNode()->GetId()][rx->GetNode()->GetId()] += p->GetSize();
}

TxMonitor::~TxMonitor()
{
    // Save all links statistics.
    RdmaSerializer<TxRecord> writer{m_avro_out};

    // Iterate all links.
    for(size_t tx_i = 0; tx_i < m_txrx_bytes.size(); tx_i++) {
        for(size_t rx_i = 0; rx_i < m_txrx_bytes.size(); rx_i++) {
            // Build the record.
            TxRecord record;
            record.src = tx_i;
            record.dst = rx_i;
            record.bytes = m_txrx_bytes[tx_i][rx_i];
            
            // Save only where a NIC sends data, not a switch.
            const bool is_tx_nic = (GetNodeType(RdmaNetwork::GetInstance().FindNode(tx_i)) == NT_SERVER);

            // Save only if the count of transmitted bytes is higher than zero.
            if(record.bytes > 0 && is_tx_nic) {
                writer.write(record);
            }
        }
    }
}

} // namespace ns3