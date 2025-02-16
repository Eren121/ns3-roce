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
#include <ns3/ag-circle.h>
#include <ns3/abort.h>
#include <ns3/simulator.h>
#include <stdlib.h>
#include <stdio.h>
#include <fstream>

using json = nlohmann::json;

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
    .AddAttribute("OnFlowFinished",
                  "Callback when the flow finishes",
                  CallbackValue(),
                  MakeCallbackAccessor(&RdmaClient::m_onFlowFinished),
                  MakeCallbackChecker());
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
  return m_servers.Get(FindNeighbor(Neighbor::Left))->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal();
}

Ipv4Address RdmaClient::GetRightIp() const
{
  return m_servers.Get(FindNeighbor(Neighbor::Right))->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal();
}

Ipv4Address RdmaClient::GetLocalIp() const
{
  return GetNode()->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal();
}


void RdmaClient::MakeLeftRightReliableConnection(RdmaReliableQP& left_qp, uint16_t left_dst_port, RdmaReliableQP& right_qp, uint16_t right_dst_port)
{
  Ptr<Node> node{GetNode()};
  Ptr<RdmaHw> rdma{node->GetObject<RdmaHw>()};

  left_qp.sq = CreateObject<RdmaReliableSQ>(GetNode(), m_pg, GetLocalIp(), right_dst_port, GetLeftIp(), left_dst_port);
  left_qp.sq->SetMTU(GetMTU());
  left_qp.rq = CreateObject<RdmaReliableRQ>(left_qp.sq);
  rdma->RegisterQP(left_qp.sq, left_qp.rq);

  right_qp.sq = CreateObject<RdmaReliableSQ>(GetNode(), m_pg, GetLocalIp(), left_dst_port, GetRightIp(), right_dst_port);
  right_qp.sq->SetMTU(GetMTU());
  right_qp.rq = CreateObject<RdmaReliableRQ>(right_qp.sq);
  rdma->RegisterQP(right_qp.sq, right_qp.rq);
}

void RdmaClient::InitLeftRightQP()
{
  MakeLeftRightReliableConnection(m_left_qp, PORT_LNODE, m_right_qp, PORT_RNODE);

  const Ptr<Node> node{GetNode()};
  const Ipv4Address my_ip{node->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal()};

	NS_LOG_INFO("Node " << "{" << m_config.current_node << "," << my_ip << "}"
                      << " left={" << FindNeighbor(Neighbor::Left) << "," << GetLeftIp() << "}"
                      << " right={" << FindNeighbor(Neighbor::Right) << "," << GetRightIp() << "}");

  m_left_qp.rq->SetOnRecv(
    [this](RdmaRxQueuePair::RecvNotif notif) {
    
    // The recovery cannot be completed until all notifs have been received
    NS_ASSERT(m_phase == Phase::Recovery);

    // Mark the block as complete
    const AgConfig::block_id_t block{notif.imm};

    NS_LOG_LOGIC("[" << m_config.current_node << "] Received recovery of block " << block);
    
    auto& my_recover_req = m_shared->recovery_request_map[m_config.current_node];
    my_recover_req.erase(block);
    if(my_recover_req.empty()) {
      m_has_recovered_chunks = true;
    }

    TryUpdateState();
  });
  
  m_right_qp.rq->SetOnRecv(
    [this](RdmaRxQueuePair::RecvNotif notif) {

    // It is possible that the phase is still mcast,
    // But we still need to register the request.
    
    const uint32_t missed{m_shared->recovery_request_map.at(FindNeighbor(Neighbor::Right)).size()};
    NS_LOG_LOGIC("Right node has partially missed " << missed << " blocks");
    m_has_recv_recover_req = true;

    if(missed == 0) {
      m_has_send_all_to_right = true;
    }

    TryUpdateState();
  });
}

uint32_t RdmaClient::FindMyIndex() const
{
  for(uint32_t i{0}; i < m_servers.GetN(); i++) {
    if(m_servers.Get(i) == GetNode()) {
      return i;
    }
  }

  NS_ABORT_MSG("Cannot find my node in the server list");
  return 0;
}

uint32_t RdmaClient::FindNeighbor(RdmaClient::Neighbor n) const
{
  int increment;
  switch(n) {
    case Neighbor::Left:
      increment = -1;
      break;
    case Neighbor::Right:
      increment = 1;
      break;
    default:
      NS_ABORT_MSG("Invalid enum");
      break;
  }

  return (m_config.current_node + increment) % m_config.node_count;
}

void RdmaClient::InitPrevNextQP()
{
  MakeLeftRightReliableConnection(m_prev_qp, PORT_PREV, m_next_qp, PORT_NEXT);

  m_prev_qp.rq->SetOnRecv(
    [this](RdmaRxQueuePair::RecvNotif notif) {
    StartMulticast();
  });
}

uint32_t RdmaClient::GetMTU() const
{
  Ptr<Node> node = GetNode();
  Ptr<RdmaHw> rdma = node->GetObject<RdmaHw>();
  return rdma->GetMTU();  
}

void RdmaClient::InitConfig()
{
  m_config.chunk_size = GetMTU();
  m_config.node_count = m_servers.GetN();
  m_config.current_node = FindMyIndex();
  m_config.root_count = 2;
  m_config.size = m_size;
  m_recovery = AgRecovery{m_config};
}

void RdmaClient::OnRecvMcastChunk(AgConfig::chunk_id_t chunk)
{
  NS_LOG_FUNCTION(this);
  
  if(m_phase != Phase::Mcast) {
    return;
  }

  m_recovery.ReceiveMcastChunk(chunk);
  
  if(m_recovery.IsMcastComplete()) {
    RunRecoveryPhase();
  }
}

void RdmaClient::OnMcastTimeout()
{
  NS_LOG_FUNCTION(this);
  NS_LOG_INFO("[" << m_config.current_node << "] Multicast timeout");
  RunRecoveryPhase();
}

void RdmaClient::OnCutoffTimer()
{
  json& j = m_shared->log["cutoff_timers_triggered"];
  j = j.get<uint64_t>() + 1;

  RunRecoveryPhase();
}

void RdmaClient::InitMcastQP()
{
  Ptr<Node> node{GetNode()};
  Ptr<RdmaHw> rdma{node->GetObject<RdmaHw>()};
  const uint32_t mtu{GetMTU()};

  m_mcast_qp.sq = CreateObject<RdmaUnreliableSQ>(GetNode(), m_pg, GetLocalIp(), PORT_MCAST);
  m_mcast_qp.sq->SetMTU(mtu);
  m_mcast_qp.rq = CreateObject<RdmaUnreliableRQ>(m_mcast_qp.sq);
  m_mcast_qp.sq->SetRateFactor(1.0 / m_config.root_count); // We should reserve bandwidth for each of the multicast that happen in the same time
  rdma->RegisterQP(m_mcast_qp.sq, m_mcast_qp.rq);

  // Paper value
  const uint64_t additional_delay{GetMTU() * 100}; // Should be > RTT
  const uint64_t cutoff_size{m_config.size * m_config.root_count + additional_delay};
  m_cutoff_timer = (m_mcast_qp.sq->GetMaxRate() * (1.0 / m_config.root_count)).CalculateBytesTxTime(cutoff_size);
  m_shared->log["cutoff_timer"] = m_cutoff_timer.GetSeconds();
  m_shared->log["cutoff_timers_triggered"] = 0;

  m_timeout_ev = Simulator::Schedule(m_cutoff_timer, &RdmaClient::OnCutoffTimer, this);
  
  // Register callback
  m_mcast_qp.rq->SetOnRecv([this](RdmaRxQueuePair::RecvNotif notif) mutable {
    NS_ASSERT(notif.has_imm);

    if(GenRandomDouble(0.0, 1.0) < 0.00) {
      return; // Drop randomly
    }
    OnRecvMcastChunk(notif.imm);
  });
}

void RdmaClient::StartApplication()
{
  NS_LOG_FUNCTION(this);
  InitConfig();
  InitPrevNextQP();
  InitLeftRightQP();
  InitMcastQP();

  m_phase = Phase::Mcast;

  if(AgIsFirstMcastSrc(m_config, m_config.current_node)) {
    StartMulticast();
  }
}

void RdmaClient::StartMulticast()
{
  NS_LOG_FUNCTION(this);

  if(m_phase != Phase::Mcast) {
    return;
  }

  NS_LOG_LOGIC("Start multicast {" << LOG_VAR(m_config.current_node) << "}");

  // Schedule multicast flow
  for(uint32_t i{0}; i < m_config.GetChunkCountPerBlock(); i++) {
    RdmaTxQueuePair::SendRequest sr;
    sr.payload_size = m_config.chunk_size;
    sr.dip = Ipv4Address(MCAST_GROUP);
    sr.dport = PORT_MCAST;
    sr.multicast = true;

    const AgConfig::chunk_id_t chunk{m_config.current_node * m_config.GetChunkCountPerBlock() + i};
    sr.imm = chunk;

    // Sometimes the recovery phase starts before the mast transmission ends
    // If the mcast got delayed because of background traffic
    // We want to make sure each node has its own block to not deadlock
    // So before `on_send`, register block
    m_recovery.ReceiveMcastChunk(chunk);
    
    sr.on_send = [this, i, chunk]() {
      // Mark the sent chunk as received immediately, because the sender cannot miss any of them
      OnRecvMcastChunk(chunk);

      if(i == m_config.GetChunkCountPerBlock() - 1) {
        OnMulticastTransmissionEnd(chunk);
      }
    };

    m_mcast_qp.sq->PostSend(sr);
  }
}

void RdmaClient::OnMulticastTransmissionEnd(AgConfig::chunk_id_t last_chunk)
{
  NS_LOG_FUNCTION(this);

  if(AgIsLastChainChunk(m_config, last_chunk)) {
    NS_LOG_LOGIC(m_config.current_node << " Was last of multicast chain {last_chunk=" << last_chunk << "}");
    return;
  }

  NS_LOG_LOGIC(m_config.current_node << " Hand over next multicast source");

  RdmaTxQueuePair::SendRequest sr;
  m_next_qp.sq->PostSend(sr);
}

void RdmaClient::RunRecoveryPhase()
{
  NS_LOG_FUNCTION(this);

  if(m_phase == Phase::Recovery) {
    // We are already running the recovery phase
    return;
  }
  
  Simulator::Cancel(m_timeout_ev);

  m_phase = Phase::Recovery;

  AgRecovery::RecoveryRequest req = m_recovery.MakeRecoveryRequest();

  NS_LOG_INFO("[" << m_config.current_node << "] Missed " << AgRecovery::GetTotalMissedChunks(req)
                  << "/" << (m_config.GetChunkCountPerBlock() * m_config.node_count) << " chunks in " << req.size() << " blocks");

  const auto missed_pairs = m_recovery.GetPairOfMissedChunks();
  for(const auto& [start, end] : missed_pairs)
  {
    NS_LOG_INFO("Missed " << start << "-" << end);
  }

  m_shared->completed_mcasts++;
  m_shared->total_chunk_lost += AgRecovery::GetTotalMissedChunks(req);

  if(m_shared->completed_mcasts == m_config.node_count) {
    std::cout << "Elapsed mcast time: " << Simulator::Now().GetSeconds() << std::endl;
    const float total_chunk_lost_percent{1.0f * m_shared->total_chunk_lost / m_config.GetTotalChunkCount() / m_config.node_count};
    std::cout << "Chunks lost across all nodes: " << m_shared->total_chunk_lost << " (" << (total_chunk_lost_percent * 100.0f) << "%)" << std::endl;

    m_shared->log["elapsed_mcast_time"] = Simulator::Now().GetSeconds();
  }

  // Request missed chunks to the left node

  RdmaTxQueuePair::SendRequest recover_request;
  if(req.size() == 0) {
    // If no block has to be recovered, mark left complete when recv ACK
    recover_request.on_send = [this]() {
      m_has_recovered_chunks = true;
      NS_LOG_LOGIC("All blocks have been received");
      TryUpdateState();
    };
  }

  m_shared->recovery_request_map[m_config.current_node] = std::move(req);
  m_left_qp.sq->PostSend(recover_request);
}

void RdmaClient::TryUpdateState()
{
  NS_LOG_FUNCTION(this);

  if(m_phase != Phase::Recovery) {
    return;
  }

  const auto& right_recover_req = m_shared->recovery_request_map[FindNeighbor(Neighbor::Right)];
  auto& my_recover_req = m_shared->recovery_request_map[m_config.current_node];
  const uint32_t peer_req_block_count{right_recover_req.size()};

  NS_LOG_LOGIC("{"
    << "has_recovered_chunks=" << m_has_recovered_chunks << ","
    << "peer_req_block_count=" << peer_req_block_count << ","
    << "has_send_all_to_right=" << m_has_send_all_to_right << ","
    << "has_recv_recover_req=" << m_has_recv_recover_req << "}");

  if(m_has_recv_recover_req && !m_has_send_all_to_right) {

    for(const auto& [block, chunk_count] : right_recover_req) {
      if(m_blocks_sent_to_right.count(block) > 0) {
        continue; // Don't send the same block multiple time!
      }

      const bool can_send{my_recover_req.count(block) == 0};
      if(can_send) {
        m_blocks_sent_to_right.insert(block);
        NS_LOG_LOGIC("[" << m_config.current_node << "] Sending {block=" << block << ",chunk_count=" << chunk_count << "} to right node because right node missed them.");
        RdmaTxQueuePair::SendRequest sr;
        sr.imm = block;
        sr.payload_size = chunk_count * GetMTU();
        sr.on_send = [this, &right_recover_req]() {
          if(right_recover_req.empty()) {
            m_has_send_all_to_right = true;
          }
          TryUpdateState();
        };
        m_right_qp.sq->PostSend(sr);
      }
    }
  }

  if(m_has_recovered_chunks && m_has_send_all_to_right && m_phase != Phase::Complete) {
    m_phase = Phase::Complete;
    NS_LOG_INFO("Completed " << m_config.current_node);

    m_shared->completed_apps++;
    if(m_shared->completed_apps == m_servers.GetN()) {
      std::cout << "Elapsed simulation time: " << Simulator::Now().GetSeconds() << std::endl;
      Simulator::Stop();

      const std::string& out_path{m_shared->flow.output_file};
      if(!out_path.empty()) {
        std::ofstream ofs{out_path.c_str()};

        m_shared->log["elapsed_total_time"] = Simulator::Now().GetSeconds();
        m_shared->log["total_chunk_lost_percent"] = 1.0f * m_shared->total_chunk_lost / m_config.GetTotalChunkCount() / m_config.node_count;
        ofs << m_shared->log.dump(4) << std::endl;
      }
    }
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
