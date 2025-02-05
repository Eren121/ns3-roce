#pragma once

#include <ns3/application.h>
#include <ns3/node-container.h>
#include <ns3/rdma-reliable-qp.h>

namespace ns3 {

class RdmaUnicastApp : public Application
{
public:
  using OnComplete = Callback<void>;
  static TypeId GetTypeId();

  uint32_t GetSrcId() const { return m_src; }
  uint32_t GetDstId() const { return m_dst; }

  void InitQP(NodeContainer nodes);

private:
  void StartApplication() override;
  void StopApplication() override;
  
  uint32_t m_src{};             //!< Source node ID.
  uint32_t m_dst{};             //!< Destination node ID.
  uint32_t m_sport{};           //!< Source port.
  uint32_t m_dport{};           //!< Destination port.
  uint64_t m_size{};            //!< Count of bytes to write.
  uint32_t m_mtu{1500};         //!< MTU.
  uint16_t m_pg{};              //!< Priority group.
  uint32_t m_win{};             //!< Bound of on-the-fly packets.
  RdmaReliableQP m_qp;
  OnComplete m_on_complete;
};

} // namespace ns3