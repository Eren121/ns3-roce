/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2007,2008,2009 INRIA, UDCAST
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Amine Ismail <amine.ismail@sophia.inria.fr>
 *                      <amine.ismail@udcast.com>
 *
 */

#ifndef RDMA_CLIENT_H
#define RDMA_CLIENT_H

#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/ipv4-address.h"
#include <ns3/rdma.h>
#include <ns3/node-container.h>
#include <ns3/rdma-reliable-qp.h>
#include <ns3/rdma-unreliable-qp.h>
#include <unordered_set>

namespace ns3 {

class Socket;
class Packet;

/**
 * \ingroup rdmaclientserver
 * \class RdmaClient
 * \brief An RDMA client.
 * 
 * An RDMA client sends a flow of data to another QP which simulates an RDMA Write operation.
 * For RC QP, the application stops when all data is acknowledged.
 * For UD QP, the application stops when all data is sent.
 * See the format of `flow.txt` which defines the flow of the client.
 */
class RdmaClient : public Application
{
public:
  static TypeId GetTypeId();

  RdmaClient();
  ~RdmaClient() override;
  
  void SetNodeContainer(NodeContainer nodes) { m_nodes = nodes; }

private:
  void StartApplication() override;
  void StopApplication() override;

  /**
   * @brief Initialize `m_left` and `m_right` and left and right QPs.
   */
  void InitLeftRight();

  void TryUpdateState();

  void RunRecoveryPhase();
  Ipv4Address GetLeftIp() const;
  Ipv4Address GetRightIp() const;
  Ipv4Address GetLocalIp() const;
  
  const uint32_t MCAST_GROUP = 1;
  const uint16_t PORT_MCAST = 100; //!< Multicast comm. port
  const uint16_t PORT_RNODE = 101; //!< Right node RC comm. port
  const uint16_t PORT_LNODE = 102; //!< Left Node RC comm. port

  NodeContainer m_nodes;
  uint64_t m_mcastSrcNodeId;
  uint64_t m_size;            //<! Count of bytes to write.
  uint16_t m_pg;              //<! Priority group.
  uint32_t m_win;             //<! Bound of on-the-fly packets.
  uint64_t m_baseRtt;         //<! Base Rtt.
  Callback<void, RdmaClient&> m_onFlowFinished; //< Callback when flow finished.

  std::unordered_set<uint64_t> m_recv_chunks; //!< Succesfully received chunks
  uint32_t m_left; //<! Index of the left node in `m_nodes`.
  uint32_t m_right; //!< Index of the right node in `m_nodes`.

  RdmaUnreliableQP m_mcast_qp; //!< QP used for multicast.
  RdmaReliableQP m_left_qp; //!< QP used for left node comm.
  RdmaReliableQP m_right_qp; //!< Qp used for right node comm.

  bool m_has_recovered_chunks{false}; //!< Set to `true` when the missed chunks are recovered.
  bool m_has_send_to_right{false}; //!< Set to `true` when the current node has send all the chunks the right node missed.
  int m_peer_requested_chunks{-1}; //!< Count of nodes requested by right node. Set to -1 until the request arrives.
  bool m_completed{false}; //!< Set to `true` when the allgather finished.

  enum class Phase {
    Mcast, Recovery
  };
  Phase m_phase = Phase::Mcast;
};

} // namespace ns3

#endif /* RDMA_CLIENT_H */
