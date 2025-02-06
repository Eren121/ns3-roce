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
 */
#include "ns3/log.h"
#include "ns3/ipv4-address.h"
#include "ns3/nstime.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/boolean.h"
#include "ns3/callback.h"
#include "ns3/rdma-random.h"
#include "ns3/qbb-net-device.h"
#include "rdma-client.h"
#include "ns3/rdma-seq-header.h"
#include <ns3/rdma-hw.h>
#include <ns3/switch-node.h>
#include <stdlib.h>
#include <stdio.h>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("RdmaClient");
NS_OBJECT_ENSURE_REGISTERED (RdmaClient);

TypeId
RdmaClient::GetTypeId()
{
  static TypeId tid = TypeId ("ns3::RdmaClient")
    .SetParent<Application> ()
    .AddConstructor<RdmaClient> ()
    .AddAttribute ("WriteSize",
                   "The number of bytes to write",
                   UintegerValue (10000),
                   MakeUintegerAccessor (&RdmaClient::m_size),
                   MakeUintegerChecker<uint64_t> ())
	.AddAttribute ("PriorityGroup", "The priority group of this flow",
				   UintegerValue (0),
				   MakeUintegerAccessor (&RdmaClient::m_pg),
				   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("Window",
                   "Bound of on-the-fly packets",
                   UintegerValue (0),
                   MakeUintegerAccessor (&RdmaClient::m_win),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("BaseRtt",
                   "Base Rtt",
                   UintegerValue (0),
                   MakeUintegerAccessor (&RdmaClient::m_baseRtt),
                   MakeUintegerChecker<uint64_t> ())
    .AddAttribute("OnFlowFinished",
                  "Callback when the flow finishes",
                  CallbackValue(),
                  MakeCallbackAccessor(&RdmaClient::m_onFlowFinished),
                  MakeCallbackChecker())
    .AddAttribute("MulticastSource",
                  "Mulitcast source",
                  CallbackValue(),
                  MakeUintegerAccessor(&RdmaClient::m_mcastSrcNodeId),
                  MakeUintegerChecker<uint64_t>());
    ;
  return tid;
}

RdmaClient::RdmaClient()
{
  NS_LOG_FUNCTION(this);
}

RdmaClient::~RdmaClient()
{
  NS_LOG_FUNCTION(this);
}

Ipv4Address RdmaClient::GetLeftIp() const
{
  return m_nodes.Get(m_left)->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal();
}

Ipv4Address RdmaClient::GetRightIp() const
{
  return m_nodes.Get(m_right)->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal();
}

Ipv4Address RdmaClient::GetLocalIp() const
{
  return GetNode()->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal();
}

void RdmaClient::InitLeftRight()
{
  const Ptr<Node> node{GetNode()};
  const Ipv4Address my_ip{node->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal()};
  const Ptr<RdmaHw> rdma{node->GetObject<RdmaHw>()};
  const uint32_t mtu{rdma->GetMTU()};
  const uint32_t node_count = m_nodes.GetN();
  const uint32_t my_id = [&]() -> uint32_t {
    for(uint32_t i = 0; i < node_count; i++) {
      if(m_nodes.Get(i) == node) {
        return i;
      }
    }
    NS_ASSERT(false);
    return 0;
  }();

  const auto find_neighbor_server = [&](int increment){
    NS_ASSERT(increment == 1 || increment == -1);
    int cur = my_id + increment;
    while(true) {
      if(cur < 0) {
        cur += node_count;
      }
      else if(cur >= node_count) {
        cur -= node_count;
      }

      if(!IsSwitchNode(m_nodes.Get(cur))) {
        NS_ASSERT(cur != my_id);
        return cur;
      }

      cur += increment;
    }
  };

  m_left = find_neighbor_server(-1);
  m_left_qp.sq = CreateObject<RdmaReliableSQ>(GetNode(), m_pg, GetLocalIp(), PORT_RNODE, GetLeftIp(), PORT_LNODE);
  m_left_qp.sq->SetMTU(mtu);
  m_left_qp.rq = CreateObject<RdmaReliableRQ>(m_left_qp.sq);
  rdma->RegisterQP(m_left_qp.sq, m_left_qp.rq);

  m_right = find_neighbor_server(1);
  m_right_qp.sq = CreateObject<RdmaReliableSQ>(GetNode(), m_pg, GetLocalIp(), PORT_LNODE, GetRightIp(), PORT_RNODE);
  m_right_qp.sq->SetMTU(mtu);
  m_right_qp.rq = CreateObject<RdmaReliableRQ>(m_right_qp.sq);
  rdma->RegisterQP(m_right_qp.sq, m_right_qp.rq);

	NS_LOG_LOGIC("Node " << "{" << my_id << "," << my_ip << "}"
               << " left={" << m_left << "," << GetLeftIp() << "}"
               << " right={" << m_right << "," << GetRightIp() << "}");

  m_left_qp.rq->SetOnRecv(
    [this](RdmaRxQueuePair::RecvNotif notif) {
    NS_LOG_LOGIC("Missed chunks have been received");
    m_has_recovered_chunks = true;
    TryUpdateState();
  });
  
  m_right_qp.rq->SetOnRecv(
    [this, mtu](RdmaRxQueuePair::RecvNotif notif) {
    m_peer_requested_chunks = notif.imm;
    NS_LOG_LOGIC("Right node has requested " << m_peer_requested_chunks << " chunks");
    TryUpdateState();
  });

  for(uint32_t i{0}; i < node_count; i++) {
    if(!IsSwitchNode(m_nodes.Get(i))) {
      m_mcastSrcNodeId = m_nodes.Get(i)->GetId();
    }
  }
}

void RdmaClient::StartApplication()
{
  NS_LOG_FUNCTION(this);
  InitLeftRight();

  Ptr<Node> node = GetNode();
  Ptr<RdmaHw> rdma = node->GetObject<RdmaHw>();
  const uint32_t mtu = rdma->GetMTU();
  const uint32_t chunk_count = (m_size + mtu - 1) / mtu;

  if(m_mcastSrcNodeId == GetNode()->GetId()) {
    // This node is the multicast source, launch the multicast

    m_mcast_qp.sq = CreateObject<RdmaUnreliableSQ>(GetNode(), m_pg, GetLocalIp(), PORT_MCAST);
    m_mcast_qp.sq->SetMTU(mtu);

    // Create RQ (unused)
    m_mcast_qp.rq = CreateObject<RdmaUnreliableRQ>(m_mcast_qp.sq);

    // Register queue
    rdma->RegisterQP(m_mcast_qp.sq, m_mcast_qp.rq);

    // Schedule flow
    for(uint32_t i = 0; i < chunk_count; i++) {
      RdmaTxQueuePair::SendRequest sr;
      sr.payload_size = mtu;
      sr.dip = Ipv4Address(MCAST_GROUP);
      sr.dport = PORT_MCAST;
      sr.multicast = true;
      sr.imm = i;
      
      if(i == chunk_count - 1) {
        sr.on_send = [this]() {
          RunRecoveryPhase();
        };
      };

      m_mcast_qp.sq->PostSend(sr);
    }
  }
  else {
    // Create SQ (unused)
    m_mcast_qp.sq = CreateObject<RdmaUnreliableSQ>(GetNode(), m_pg, GetLocalIp(), PORT_MCAST);

    // Create RQ
    m_mcast_qp.rq = CreateObject<RdmaUnreliableRQ>(m_mcast_qp.sq);

    // Timeout for when no packet is received
    Time timeout = MicroSeconds(50);
    EventId timeout_ev = Simulator::Schedule(timeout, &RdmaClient::RunRecoveryPhase, this);


    // Register callback
    m_mcast_qp.rq->SetOnRecv(
      [this, chunk_count, timeout, timeout_ev](RdmaRxQueuePair::RecvNotif notif) mutable {
      NS_ASSERT(notif.has_imm);
      const uint64_t chunk_id = notif.imm;
      m_recv_chunks.insert(chunk_id);
    
      // Reset the timeout
      Simulator::Cancel(timeout_ev);
      timeout_ev = Simulator::Schedule(timeout, &RdmaClient::RunRecoveryPhase, this);

      const bool is_last_chunk = (chunk_id == chunk_count - 1);
      if(is_last_chunk) {
        Simulator::Cancel(timeout_ev);
        RunRecoveryPhase();
      }
    });
  }

  rdma->RegisterQP(m_mcast_qp.sq, m_mcast_qp.rq);
}

void RdmaClient::RunRecoveryPhase()
{
  NS_LOG_FUNCTION(this);

  if(m_phase == Phase::Recovery) {
    // We are already running the recovery phase
    return;
  }

  m_phase = Phase::Recovery;
  
  Ptr<RdmaHw> rdma{GetNode()->GetObject<RdmaHw>()};
  const uint32_t mtu{rdma->GetMTU()};
  const auto chunk_count{uint32_t{(m_size + mtu - 1) / mtu}};
  const uint32_t missed_chunk_count{[&]() -> uint32_t {
    uint32_t ret{0};
    if(m_mcastSrcNodeId != GetNode()->GetId()) {
      for(uint32_t i{0}; i < chunk_count; i++) {
        if(m_recv_chunks.count(i) < 1) {
          ret++;
        }
      }
    }
    return ret;
  }()};

  NS_LOG_LOGIC("Missed " << missed_chunk_count << " chunks");

  // Request missed chunks to the left node
  // The request imm. data only contains the count of missed nodes to simplify
  // The left node will RDMA Write the missed chunks, be notified when it is completed
  RdmaTxQueuePair::SendRequest recover_request;
  recover_request.imm = missed_chunk_count;
  m_left_qp.sq->PostSend(recover_request);
}

void RdmaClient::TryUpdateState()
{
  NS_LOG_FUNCTION(this);
  NS_LOG_LOGIC("{"
    << "has_recovered_chunks=" << m_has_recovered_chunks << ","
    << "peer_requested_chunks=" << m_peer_requested_chunks << ","
    << "has_send_to_right=" << m_has_send_to_right << "}");
  
  if(m_phase != Phase::Recovery) {
    return;
  }

  const Ptr<RdmaHw> rdma{GetNode()->GetObject<RdmaHw>()};
  const uint32_t mtu{rdma->GetMTU()};

  if((m_has_recovered_chunks || m_peer_requested_chunks == 0)
    && m_peer_requested_chunks >= 0) {
    NS_LOG_LOGIC("Sending " << m_peer_requested_chunks << " to right node because right node missed them.");
    
    RdmaTxQueuePair::SendRequest sr;
    sr.payload_size = m_peer_requested_chunks * mtu,
    sr.on_send = [this]() {
      m_has_send_to_right = true;
      NS_LOG_LOGIC("Has just send missed chunks to right");
      TryUpdateState();
    };
    m_peer_requested_chunks = -2; // Avoid sending twice
    m_right_qp.sq->PostSend(sr);
  }
  if(m_has_recovered_chunks && m_has_send_to_right && !m_completed) {
    m_completed = true;
    NS_LOG_LOGIC("Completed " << GetNode()->GetId() << "/" << m_mcastSrcNodeId);
  }
}

void RdmaClient::StopApplication()
{
  NS_LOG_FUNCTION(this);
  // TODO stop the queue pair
  
  if(!m_onFlowFinished.IsNull()) {
    m_onFlowFinished(*this);
  }
}

} // Namespace ns3
