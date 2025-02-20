#pragma once

#include "ns3/application.h"
#include "ns3/node-container.h"
#include "ns3/rdma-reliable-qp.h"
#include "ns3/rdma-unreliable-qp.h"
#include "ns3/switch-node.h"

namespace ns3 {

class RdmaUnicastApp : public Application
{
public:
  void SetNodes(NodeContainer c);
  using OnComplete = Callback<void>;
  static TypeId GetTypeId();

  bool IsSrc() const;

private:
  void InitQP();
  void StartApplication() override;
  void StopApplication() override;
  
  NodeContainer m_nodes;
  Ipv4Address m_peer_ip;
  uint16_t m_peer_port{};
  uint32_t m_group;
  uint64_t m_mtu{};
  uint32_t m_src{};             //!< Source node ID.
  uint32_t m_dst{};             //!< Destination node ID.
  uint32_t m_sport{};           //!< Source port.
  uint32_t m_dport{};           //!< Destination port.
  uint64_t m_size{};            //!< Count of bytes to write.
  uint16_t m_pg{};              //!< Priority group.
  bool m_multicast{};           //!< Whether the flow is multicast.
  double m_rate_factor{1.0};
  RdmaReliableQP m_rc_qp;       //!< Used only for RC flow.
  RdmaUnreliableQP m_ud_qp;     //!< Used only for UD flow.
  OnComplete m_on_complete;
};

} // namespace ns3