#include "ns3/rdma-flow-multicast.h"
#include "ns3/rdma-network.h"
#include "ns3/rdma-hw.h"
#include "ns3/qbb-net-device.h"

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED(RdmaFlowMulticast);
NS_LOG_COMPONENT_DEFINE("RdmaFlowMulticast");

TypeId RdmaFlowMulticast::GetTypeId()
{
    static TypeId tid = []() {
        static TypeId tid = TypeId("ns3::RdmaFlowMulticast");

        tid.SetParent<RdmaFlow>();
        tid.AddConstructor<RdmaFlowMulticast>();

        AddUintegerAttribute(tid,
            "MulticastSource",
            "Multicast source node ID.",
            &RdmaFlowMulticast::m_mcast_src);
   
        AddUintegerAttribute(tid,
            "MulticastGroup",
            "Destination multicast group.",
            &RdmaFlowMulticast::m_group);
    
        AddUintegerAttribute(tid,
            "NumPackets",
            "Count of packets to send in the multicast. "
            "Uses a full MTU of the multicast source as packet size.",
            &RdmaFlowMulticast::m_num_pkts_to_write);
    
        AddUintegerAttribute(tid,
            "PfcPriority",
            "PFC flow priority.",
            &RdmaFlowMulticast::m_priority);

        return tid;
    }();
  
    return tid;
}

void RdmaFlowMulticast::SetOnRecvPktCallback(OnRecvPktCallback on_recv_pkt)
{
    m_on_recv_pkt = std::move(on_recv_pkt);
}

void RdmaFlowMulticast::StartFlow(RdmaNetwork& network, OnComplete on_complete)
{
    const Ptr<Node> snode = network.FindServer(m_mcast_src);
    const Ipv4Address src_ip = GetServerAddress(snode);
    const Ptr<RdmaHw> src_rdma{snode->GetObject<RdmaHw>()};
    const uint32_t mtu_bytes{src_rdma->GetMTU()};

    // Use same port for initiator and targets.
    const uint16_t port = GetNextMulticastUniquePort();

    // Create the RDMA Write Unreliable Multicast request.
    RdmaTxQueuePair::SendRequest sr;
    sr.payload_size = mtu_bytes * m_num_pkts_to_write;
    sr.multicast = true;
    sr.dip = Ipv4Address{m_group};
    sr.dport = port;
    sr.on_send = on_complete; // Notify completion.

    // Create the queues on the source.
    {
        const auto src_tx_queue = CreateObject<RdmaUnreliableSQ>(snode, m_priority, src_ip, port);
        const auto src_rx_queue = CreateObject<RdmaUnreliableRQ>(DynamicCast<RdmaUnreliableSQ>(src_tx_queue));
        
        if(m_throughput) {
            src_tx_queue->SetMaxRate(*m_throughput);
        }

        src_rdma->RegisterQP(src_tx_queue, src_rx_queue);

        // Post the send request on the source.
        src_tx_queue->PostSend(sr);
    }

    // For each node in the destination multicast group.
    for(Ptr<Node> dnode : network.FindMcastGroup(m_group)) {
        // Get destination IP & `RdmaHw`.
        const Ipv4Address dst_ip = GetServerAddress(dnode);
        const Ptr<RdmaHw> dst_rdma{dnode->GetObject<RdmaHw>()};

        // Create the queues to receive multicast data.
        const auto dst_tx_queue = CreateObject<RdmaUnreliableSQ>(dnode, m_priority, dst_ip, port);
        const auto dst_rx_queue = CreateObject<RdmaUnreliableRQ>(DynamicCast<RdmaUnreliableSQ>(dst_tx_queue));   

        // The RX should never send data but for consistency we also set the throughput.
        if(m_throughput) {
            dst_tx_queue->SetMaxRate(*m_throughput);
        }

        dst_rdma->RegisterQP(dst_tx_queue, dst_rx_queue);
    }
}

} // namespace ns3