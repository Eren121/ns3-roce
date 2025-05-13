#include "ns3/rdma-flow-unicast.h"
#include "ns3/rdma-network.h"
#include "ns3/rdma-hw.h"
#include "ns3/qbb-net-device.h"

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED(RdmaFlowUnicast);
NS_LOG_COMPONENT_DEFINE("RdmaFlowUnicast");

TypeId RdmaFlowUnicast::GetTypeId()
{
  static TypeId tid = []() {
    static TypeId tid = TypeId("ns3::RdmaFlowUnicast");

    tid.SetParent<RdmaFlow>();
    tid.AddConstructor<RdmaFlowUnicast>();
    
    AddUintegerAttribute(tid,
      "SourceNode",
      "Source node ID.",
      &RdmaFlowUnicast::m_snode);
   
    AddUintegerAttribute(tid,
      "DestinationNode",
      "Destination node ID.",
      &RdmaFlowUnicast::m_dnode);
    
    AddUintegerAttribute(tid,
      "WriteByteAmount",
      "Amount of bytes to write with an RDMA Write.",
      &RdmaFlowUnicast::m_bytes_to_write);
    
    AddUintegerAttribute(tid,
      "PfcPriority",
      "PFC flow priority.",
      &RdmaFlowUnicast::m_priority);
    
    AddBooleanAttribute(tid,
      "IsReliable",
      "If true, uses RC QP. If false, uses UD QP",
      &RdmaFlowUnicast::m_reliable);

    return tid;
  }();
  
  return tid;
}

void RdmaFlowUnicast::StartFlow(RdmaNetwork& network, OnComplete on_complete)
{
    const Ptr<Node> snode = network.FindServer(m_snode);
    const Ptr<Node> dnode = network.FindServer(m_dnode);
    const Ipv4Address src_ip = GetServerAddress(snode);
    const Ipv4Address dst_ip = GetServerAddress(dnode);
    const uint16_t src_port = GetNextUniquePort(snode);
    const uint16_t dst_port = GetNextUniquePort(dnode);
    const Ptr<RdmaHw> src_rdma{snode->GetObject<RdmaHw>()};
    const Ptr<RdmaHw> dst_rdma{dnode->GetObject<RdmaHw>()};
    

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
            src_tx_queue = CreateObject<RdmaReliableSQ>(snode, m_priority, src_ip, src_port, dst_ip, dst_port);
            src_rx_queue = CreateObject<RdmaReliableRQ>(DynamicCast<RdmaReliableSQ>(src_tx_queue));
        }
        else {
            src_tx_queue = CreateObject<RdmaUnreliableSQ>(snode, m_priority, src_ip, src_port);
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
            dst_tx_queue = CreateObject<RdmaReliableSQ>(dnode, m_priority, dst_ip, dst_port, src_ip, src_port);
            dst_rx_queue = CreateObject<RdmaReliableRQ>(DynamicCast<RdmaReliableSQ>(dst_tx_queue));
        }
        else {
            dst_tx_queue = CreateObject<RdmaUnreliableSQ>(dnode, m_priority, dst_ip, dst_port);
            dst_rx_queue = CreateObject<RdmaUnreliableRQ>(DynamicCast<RdmaUnreliableSQ>(dst_tx_queue));
        }

        dst_rdma->RegisterQP(dst_tx_queue, dst_rx_queue);
    }
}

} // namespace ns3