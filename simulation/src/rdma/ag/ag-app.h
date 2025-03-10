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
#include "ns3/ag-config.h"
#include "ns3/ag-runtime.h"
#include "ns3/rdma.h"
#include "ns3/node-container.h"
#include "ns3/rdma-reliable-qp.h"
#include "ns3/rdma-unreliable-qp.h"
#include "ns3/rdma-hw.h"
#include "ns3/rdma-random.h"
#include "ns3/json.h"
#include "ns3/rdma-flow.h"
#include <unordered_set>
#include <memory>

#define LOG_VAR(x) #x << "=" << (x)

namespace ns3 {

class Socket;
class Packet;

/**
 * \ingroup AgAppserver
 * \class AgApp
 * \brief An RDMA client.
 * 
 * An RDMA client sends a flow of data to another QP which simulates an RDMA Write operation.
 * For RC QP, the application stops when all data is acknowledged.
 * For UD QP, the application stops when all data is sent.
 * See the format of `flow.txt` which defines the flow of the client.
 */
class AgApp : public Application
{
public:
  struct LeftRightConnection
  {
    RdmaReliableQP left;
    RdmaReliableQP right;
  };

public:
  static TypeId GetTypeId();

  AgApp(Ptr<AgShared> shared);
  ~AgApp() override;

private:
  void StartApplication() override;
  void StopApplication() override;

  void OnLastMcastCompleted();

  LeftRightConnection MakeLRConnection(uint16_t left_dst_port, uint16_t right_dst_port);
  void OnRecvMcastChunk(chunk_id_t chunk);

  void OnMulticastTransmissionEnd();

  void OnCutoffTimer();

  void InitMulticastQP();
  void InitChainQP();
  void InitRecoveryQP();

  void StartMulticast();
  void StartRecoveryPhase();

  void RunMarkovMcast();
  
private:
  Ptr<AgShared> m_shared;
  Ptr<AgConfig> m_config;
  Ptr<AgRuntime> m_runtime;
  Callback<void, AgApp&> m_onAppFinish;

  std::unordered_set<uint64_t> m_recv_chunks; //!< Succesfully received chunks
  std::vector<uint32_t> m_order; //!< Indices of the multicast sources in order.

  RdmaUnreliableQP m_mcast; //!< QP used for multicast.
  LeftRightConnection m_recovery; //! QP used for recovery phase
  LeftRightConnection m_chain; //! QP used to notify the next in the multicast chain
  
  bool m_has_recovered_chunks{false};
  bool m_has_recv_recover_req{false};
  bool m_has_send_all_to_right{false}; //!< Set to `true` when the current node has send all the chunks the right node missed.
  std::unordered_set<block_id_t> m_blocks_sent_to_right;

  enum class Phase {
    Mcast, Recovery, Complete
  };
  Phase m_phase = Phase::Mcast;

  EventId m_timeout_ev; //! Timeout for when no mcast packet is received
  Time m_cutoff_timer;
};

} // namespace ns3

#endif /* RDMA_CLIENT_H */
