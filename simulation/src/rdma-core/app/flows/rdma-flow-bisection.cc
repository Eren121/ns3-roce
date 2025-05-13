#include "ns3/rdma-flow-bisection.h"
#include "ns3/rdma-network.h"
#include "ns3/rdma-hw.h"
#include "ns3/qbb-net-device.h"

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED(RdmaFlowBisection);
NS_LOG_COMPONENT_DEFINE("RdmaFlowBisection");

TypeId RdmaFlowBisection::GetTypeId()
{
  static TypeId tid = []() {
    static TypeId tid = TypeId("ns3::RdmaFlowBisection");

    tid.SetParent<RdmaFlow>();
    tid.AddConstructor<RdmaFlowBisection>();
    
    AddUintegerAttribute(tid,
      "WriteByteAmount",
      "Amount of bytes to write with an RDMA Write.",
      &RdmaFlowBisection::m_bytes_to_write);
    
    AddUintegerAttribute(tid,
      "PfcPriority",
      "PFC flow priority.",
      &RdmaFlowBisection::m_priority);
    
    AddBooleanAttribute(tid,
      "IsReliable",
      "If true, uses RC QP. If false, uses UD QP",
      &RdmaFlowBisection::m_reliable);

    return tid;
  }();
  
  return tid;
}

void RdmaFlowBisection::StartFlow(RdmaNetwork& network, OnComplete on_complete)
{
    using std::swap;

    // Create all flows.
    const std::vector<Ptr<Node>> servers = network.GetAllServers().to_vector();
    
    NS_ABORT_MSG_IF(servers.size() % 2 != 0,
        "The topology should be a fat tree that contains an even count of servers");

    // Total count of servers.
    const size_t n_servers = servers.size();

    // Count of server in each half of the fat tree.
    const size_t n_servers_per_half = n_servers / 2;

    // Stops when all bisection RDMA Write have completed.
    auto on_single_write_complete = [on_complete, n_servers_per_half, n_write_complete=0]() mutable {
        n_write_complete++;
        if(n_write_complete == n_servers_per_half) {
            on_complete();
        }
    };

    for(size_t i = 0; i < n_servers_per_half; i++) {
        size_t initiator = i;
        size_t target = n_servers_per_half + i;
        if(i % 2 == 1) {
            swap(initiator, target);
        }

        StartWrite(servers.at(initiator), servers.at(target), on_single_write_complete);
    }
}

void RdmaFlowBisection::StartWrite(Ptr<Node> initiator, Ptr<Node> target, OnComplete on_complete) const
{
    NS_LOG_LOGIC("Bisection: Initiator " << initiator->GetId() << " talks to " << target->GetId());

    const Ipv4Address src_ip = GetServerAddress(initiator);
    const Ipv4Address dst_ip = GetServerAddress(target);
    const uint16_t src_port = GetNextUniquePort(initiator);
    const uint16_t dst_port = GetNextUniquePort(target);
    const Ptr<RdmaHw> src_rdma{initiator->GetObject<RdmaHw>()};
    const Ptr<RdmaHw> dst_rdma{target->GetObject<RdmaHw>()};

    // Create the RDMA Write request.
    RdmaTxQueuePair::SendRequest sr;
    sr.payload_size = m_bytes_to_write;
    sr.multicast = false;
    sr.dip = dst_ip;          // Only useful for UD QP.
    sr.dport = dst_port;      // Only useful for UD QP.
    sr.on_send = on_complete; // Notify completion.

    // Create the queues on the source.
    {
        Ptr<RdmaTxQueuePair> src_tx_queue;
        Ptr<RdmaRxQueuePair> src_rx_queue;

        if(m_reliable) {
            src_tx_queue = CreateObject<RdmaReliableSQ>(initiator, m_priority, src_ip, src_port, dst_ip, dst_port);
            src_rx_queue = CreateObject<RdmaReliableRQ>(DynamicCast<RdmaReliableSQ>(src_tx_queue));
        }
        else {
            src_tx_queue = CreateObject<RdmaUnreliableSQ>(initiator, m_priority, src_ip, src_port);
            src_rx_queue = CreateObject<RdmaUnreliableRQ>(DynamicCast<RdmaUnreliableSQ>(src_tx_queue));
        }
        
        src_rdma->RegisterQP(src_tx_queue, src_rx_queue);

        // Post the send request on the source.
        src_tx_queue->PostSend(sr);
    }

    // Create the queues on the destination.
    {
        Ptr<RdmaTxQueuePair> dst_tx_queue;
        Ptr<RdmaRxQueuePair> dst_rx_queue;
        
        if(m_reliable) {
            dst_tx_queue = CreateObject<RdmaReliableSQ>(target, m_priority, dst_ip, dst_port, src_ip, src_port);
            dst_rx_queue = CreateObject<RdmaReliableRQ>(DynamicCast<RdmaReliableSQ>(dst_tx_queue));
        }
        else {
            dst_tx_queue = CreateObject<RdmaUnreliableSQ>(target, m_priority, dst_ip, dst_port);
            dst_rx_queue = CreateObject<RdmaUnreliableRQ>(DynamicCast<RdmaUnreliableSQ>(dst_tx_queue));
        }

        dst_rdma->RegisterQP(dst_tx_queue, dst_rx_queue);
    }
}

} // namespace ns3