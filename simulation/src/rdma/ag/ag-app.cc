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
#include "ns3/ag-app.h"
#include "ns3/rdma-seq-header.h"
#include "ns3/rdma-hw.h"
#include "ns3/switch-node.h"
#include "ns3/abort.h"
#include "ns3/simulator.h"
#include <stdlib.h>
#include <stdio.h>
#include <fstream>

using json = nlohmann::json;

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("AgApp");
NS_OBJECT_ENSURE_REGISTERED (AgApp);

TypeId
AgApp::GetTypeId()
{
  static TypeId tid = TypeId ("ns3::AgApp")
    .SetParent<Application>()
  ;
  return tid;
}

AgApp::AgApp(Ptr<AgShared> shared)
  : m_shared{shared}, m_config{shared->GetConfig()}
{
  NS_LOG_FUNCTION(this);
}

AgApp::~AgApp()
{
  NS_LOG_FUNCTION(this);

  if(m_timeout_ev.IsRunning()) {
    m_timeout_ev.Cancel();
  }
}

AgApp::LeftRightConnection AgApp::MakeLRConnection(uint16_t left_dst_port, uint16_t right_dst_port)
{
  const Ptr<RdmaHw> rdma{GetNode()->GetObject<RdmaHw>()};
  const Ipv4Address left_ip{GetServerAddress(m_runtime->GetLeft())};
  const Ipv4Address right_ip{GetServerAddress(m_runtime->GetRight())};

  LeftRightConnection conn;
  conn.left = rdma->CreateReliableQP(m_config->GetPriority(), right_dst_port, left_ip, left_dst_port);
  conn.right = rdma->CreateReliableQP(m_config->GetPriority(), left_dst_port, right_ip, right_dst_port);

  return conn;
}

void AgApp::InitRecoveryQP()
{
  m_recovery = MakeLRConnection(m_config->GetPort(AgPort::RecoveryLeft), m_config->GetPort(AgPort::RecoveryRight));

	NS_LOG_INFO("Allgather node "
    << "{block=" << m_runtime->GetBlock() << ",ip=" << GetServerAddress(GetNode()) << "}"
    << " left={" << GetServerAddress(m_runtime->GetLeft()) << "}"
    << " right={" << GetServerAddress(m_runtime->GetRight()) << "}"
  );

  m_recovery.left.rq->SetOnRecv([this](RdmaRxQueuePair::RecvNotif notif) {
    NS_ASSERT(notif.has_imm);
    m_runtime->NotifyRecovery(notif.imm);
  });

  m_recovery.right.rq->SetOnRecv([this](RdmaRxQueuePair::RecvNotif notif) {
    m_runtime->NotifyRecvRecoveryRequest();
  });

  m_runtime->recovery_sq = m_recovery.right.sq;
}

void AgApp::InitChainQP()
{
  m_chain = MakeLRConnection(m_config->GetPort(AgPort::ChainPrev), m_config->GetPort(AgPort::ChainNext));

  m_chain.left.rq->SetOnRecv(
    [this](RdmaRxQueuePair::RecvNotif notif) {
    StartMulticast();
  });
}

void AgApp::InitMulticastQP()
{
  const Ptr<RdmaHw> rdma{GetNode()->GetObject<RdmaHw>()};
  m_mcast = rdma->CreateUnreliableQP(m_config->GetPriority(), m_config->GetPort(AgPort::Multicast));

  // We reserve bandwidth for each of the multicast that happens in the same time
  m_mcast.sq->SetMaxRate(m_mcast.sq->GetMaxRate() * (1.0 / m_config->GetRootCount()));


  m_cutoff_timer = m_mcast.sq->GetMaxRate().CalculateBytesTxTime(m_runtime->GetCutoffByteSize());
  m_timeout_ev = Simulator::Schedule(m_cutoff_timer, &AgApp::OnCutoffTimer, this);
  
  // Register callback
  m_mcast.rq->SetOnRecv([this](RdmaRxQueuePair::RecvNotif notif) mutable {

    if(m_runtime->GetState() != AgState::Multicast) {
      return;
    }

    NS_ASSERT(notif.has_imm);
    const chunk_id_t chunk{notif.imm};
    const block_id_t block{m_config->GetOriginalSender(chunk)};

    if(m_runtime->NotifyReceiveChunk(chunk)) {
      StartRecoveryPhase();
    }
    else {
      // Because of delays caused by congestion control,
      // the multicast can take longer than `bandwidth * size`, so adjust the timer
      // at each mcast packet received.
    
      const Ptr<RdmaHw> rdma{GetNode()->GetObject<RdmaHw>()};
      const chunk_id_t chain_chunk_last{m_config->GetNearestFirstBlockHigherOrEqu(block + 1) * m_config->GetPerBlockChunkCount() - 1};
      const chunk_id_t chain_chunk_rem{chain_chunk_last - chunk};
      const byte_t chain_rem_bytes{chain_chunk_rem * m_config->GetChunkByteSize()};
      const byte_t additional_delay{rdma->GetMTU() * 100}; // A bit random value, all should it be is > RTT
      const byte_t cutoff_bytes{chain_rem_bytes + additional_delay};
     
      /*
      std::cout << "--\nchunk=" << chunk << std::endl;
      std::cout << "block=" << block
      << "nearest=" << m_config->GetNearestFirstBlockHigherOrEqu(block + 1) << std::endl;
      std::cout << "last_chunk=" << chain_chunk_last << std::endl;
      std::cout << "cutoff_bytes=" << cutoff_bytes << std::endl;
      */

      m_cutoff_timer = m_mcast.sq->GetMaxRate().CalculateBytesTxTime(rdma->GetMTU() * 100);
      m_timeout_ev.Cancel();
      m_timeout_ev = Simulator::Schedule(m_cutoff_timer, &AgApp::OnCutoffTimer, this);      
    }
  });
}

void AgApp::OnCutoffTimer()
{
  NS_LOG_FUNCTION(this);

  NS_ASSERT(m_runtime->GetState() == AgState::Multicast);
  
  m_shared->NotifyCutoffTimerTriggered(m_runtime->GetBlock());
  m_runtime->TransitionToRecovery();
  StartRecoveryPhase();
}

void AgApp::StartApplication()
{
  NS_LOG_FUNCTION(this);

  NS_ASSERT(!m_runtime);
  m_runtime = CreateObject<AgRuntime>(GetNode(), *m_shared);

  InitMulticastQP();
  InitChainQP();
  InitRecoveryQP();

  if(m_runtime->IsFirstInChain()) {
    StartMulticast();
  }
}

void AgApp::StartMulticast()
{
  NS_LOG_FUNCTION(this);

  if(m_runtime->GetState() != AgState::Multicast) {
    return;
  }

  //
  // Schedule multicast flow
  //

  const chunk_id_t offset{m_runtime->GetBlock() * m_config->GetPerBlockChunkCount()};
  for(chunk_id_t i{0}; i < m_config->GetPerBlockChunkCount(); i++) {
    const chunk_id_t chunk{offset + i};

    RdmaTxQueuePair::SendRequest sr;
    sr.payload_size = m_config->GetChunkByteSize();
    sr.dip = Ipv4Address(m_config->GetMulticastGroup());
    sr.dport = m_config->GetPort(AgPort::Multicast);
    sr.multicast = true;
    sr.imm = chunk;
    sr.on_send = [this, i, chunk]() {
      if(i == m_config->GetPerBlockChunkCount() - 1) {
        OnMulticastTransmissionEnd(chunk);
      }
    };

    // Sometimes the recovery phase starts before the mast transmission ends
    // If the mcast got delayed because of background traffic
    // We want to make sure each node has its own block to not deadlock
    // So before `on_send`, register block
    // TODO should nto be true???

    m_mcast.sq->PostSend(sr);
  }
}

void AgApp::OnMulticastTransmissionEnd(chunk_id_t last_chunk)
{
  NS_LOG_FUNCTION(this);

  NS_LOG_LOGIC(m_runtime->GetBlock() << " hands over next multicast source");

  RdmaTxQueuePair::SendRequest sr;
  m_chain.right.sq->PostSend(sr);
}

void AgApp::StartRecoveryPhase()
{
  NS_LOG_FUNCTION(this);
  
  Simulator::Cancel(m_timeout_ev);

  // Request missed chunks to the left node

  RdmaTxQueuePair::SendRequest recover_request;

  if(m_runtime->HasAllChunks()) {
    // If no block has to be recovered, mark left complete when recv ACK,
    // Because we still need to notify him
    recover_request.on_send = [this]() {
      m_runtime->NotifyRecovery(std::numeric_limits<block_id_t>::max()); // Notify recover of dummy block just to trigger `m_has_recovered_all` to true
      m_runtime->TryUpdateState();
    };
  }

  m_recovery.left.sq->PostSend(recover_request);
}

void AgApp::StopApplication()
{
  NS_LOG_FUNCTION(this);
  // TODO stop the queue pair
  
  if(!m_onAppFinish.IsNull()) {
    m_onAppFinish(*this);
  }
}

} // namespace ns3
