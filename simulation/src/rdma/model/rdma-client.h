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
#include <ns3/ag-recovery.h>
#include <unordered_set>

#define LOG_VAR(x) #x << "=" << (x)

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
  static std::unordered_map<uint32_t, AgRecovery::RecoveryRequest> m_recovery_request_map; // Easier than serializing / deserializing... No problem as long as we dont use MPI.
  static int m_completed_apps;
  
  static TypeId GetTypeId();

  enum class Neighbor {
    Left, Right
  };

  RdmaClient();
  ~RdmaClient() override;
  void SetServers(NodeContainer servers) { m_servers = std::move(servers); }

private:
  uint32_t GetMTU() const;
  void StartApplication() override;
  void StopApplication() override;

  uint32_t FindMyIndex() const;
  uint32_t FindNeighbor(Neighbor n) const;
  void MakeLeftRightReliableConnection(RdmaReliableQP& left_qp, uint16_t left_dst_port, RdmaReliableQP& right_qp, uint16_t right_dst_port);
  void OnRecvMcastChunk(AgConfig::chunk_id_t chunk);

  void OnMulticastTransmissionEnd(AgConfig::chunk_id_t last_chunk);

  void InitConfig();
  void InitPrevNextQP();
  void InitLeftRightQP();
  void InitMcastQP();
  /**
   * @brief Initialize `m_left` and `m_right` and left and right QPs.
   */
  void InitLeftRight();

  void StartMulticast();

  void TryUpdateState();

  void OnMcastTimeout();
  void RunRecoveryPhase();
  Ipv4Address GetLeftIp() const;
  Ipv4Address GetRightIp() const;
  Ipv4Address GetLocalIp() const;
  
  const uint32_t MCAST_GROUP = 1;
  const uint16_t PORT_MCAST = 100; //!< Multicast comm. port
  const uint16_t PORT_RNODE = 101; //!< Right node RC comm. port
  const uint16_t PORT_LNODE = 102; //!< Left Node RC comm. port
  const uint16_t PORT_NEXT = 103; //!< Port to comm with next mcast src
  const uint16_t PORT_PREV = 104; //!< Port to comm with prev mcast src

  NodeContainer m_servers; //!< Servers participating the allgather.
  AgConfig m_config; //!< Allgather config.
  AgRecovery m_recovery; //!< Allgather state.
  uint64_t m_size; //!< Buffer size per node.

  uint16_t m_pg;              //<! Priority group.
  uint32_t m_win;             //<! Bound of on-the-fly packets.
  uint64_t m_baseRtt;         //<! Base Rtt.
  Callback<void, RdmaClient&> m_onFlowFinished; //< Callback when flow finished.

  std::unordered_set<uint64_t> m_recv_chunks; //!< Succesfully received chunks
  std::vector<uint32_t> m_order; //!< Indices of the multicast sources in order.

  RdmaUnreliableQP m_mcast_qp; //!< QP used for multicast.
  RdmaReliableQP m_left_qp; //!< QP used for left node comm.
  RdmaReliableQP m_right_qp; //!< QP used for right node comm.
  RdmaReliableQP m_next_qp; //!< QP used to ask the next multicast source to run the multicast.
  RdmaReliableQP m_prev_qp; //!< QP used to be notified to start the mcast.
  bool m_has_recovered_chunks{false};
  bool m_has_recv_recover_req{false};
  bool m_has_send_all_to_right{false}; //!< Set to `true` when the current node has send all the chunks the right node missed.
  std::unordered_set<AgConfig::block_id_t> m_blocks_sent_to_right;

  enum class Phase {
    Mcast, Recovery, Complete
  };
  Phase m_phase = Phase::Mcast;

  EventId m_timeout_ev; //! Timeout for when no mcast packet is received
  Time timeout{MicroSeconds(50)};
};

} // namespace ns3

#endif /* RDMA_CLIENT_H */
