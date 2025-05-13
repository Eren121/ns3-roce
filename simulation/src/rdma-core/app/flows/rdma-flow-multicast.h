#pragma once

#include "ns3/rdma-flow.h"
#include "ns3/data-rate.h"

namespace ns3 {

/**
 * The completion is when the last byte is sent by the source,
 * but the receivers may not have receive them yet.
 */
class RdmaFlowMulticast : public RdmaFlow
{
public:
    using RdmaFlow::OnComplete;

    struct OnRecvPktInfo
    {
        node_id_t mcast_src;
        node_id_t receiver;
        //! Varies between zero an the count of multicast packets to send.
        uint64_t pkt_id;
    };

    //! `pkt_id` varies between zero and the count of packets to send.
    using OnRecvPktCallback = std::function<void(const OnRecvPktInfo&)>;

    void SetThroughput(DataRate throughput)
    {
        m_throughput = throughput;
    }

    static TypeId GetTypeId();

public:
    //! This is only accessible in C++, not in JSON, for more configurability if needed.
    void SetOnRecvPktCallback(OnRecvPktCallback on_recv_pkt);

protected:
    void OnFlowStarted(RdmaNetwork& network) override;

private:
    //! Node of the multicast source.
    node_id_t m_mcast_src{};
    //! Count of packets to write.
    uint32_t m_num_pkts_to_write{}; 
    //! Priority group.
    uint16_t m_priority{};
    //! Multicast group.
    group_id_t m_group{};
    //! Optional callback to call when any receiver receives a packet of the multicast.
    OnRecvPktCallback m_on_recv_pkt;
    //! Throughput for the multicast. If not set, uses the link bandwidth.
    std::optional<DataRate> m_throughput;
};

} // namespace ns3