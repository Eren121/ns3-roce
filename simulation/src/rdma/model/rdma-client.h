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

  /**
   * \brief Set the destination where to send the data.
   * \param ip   Destination IP address.
   * \param port Destination port.
   * 
   * Works only if called before the application starts.
   */
  void SetRemote(Ipv4Address ip, uint16_t port);
  
  /**
   * \brief Set the source from where to send the data.
   * \param ip   Local IP address. It should belong to the node that runs the application.
   * \param port Local port.
   * 
   * Works only if called before the application starts.
   */
  void SetLocal(Ipv4Address ip, uint16_t port);

  /**
   * \brief Set the priority group of the QP.
   * \param pg New priority group.
   * 
   * Works only if called before the application starts.
   */
  void SetPG(uint16_t pg);
  
  /**
   * \brief Set the count of bytes to RDMA Write.
   * \param size New count of bytes to write.
   * 
   * Works only if called before the application starts.
   */
  void SetSize(uint64_t size);

protected:
  void DoDispose() override;

private:
  void StartApplication() override;
  void StopApplication() override;

  uint64_t m_size;            //!< Count of bytes to write.
  bool m_reliable;            //!< `true` for RC, `false` for UD.
  bool m_multicast;           //<! `true` if `m_dip` is a multicast group, and `false` for unicast.
  uint16_t m_pg;              //!< Priority group.
  Ipv4Address m_sip, m_dip;
  uint16_t m_sport, m_dport;
  uint32_t m_win;             //<! Bound of on-the-fly packets.
  uint64_t m_baseRtt;         //<! Base Rtt.
  Callback<void, RdmaClient&> m_onFlowFinished; //< Callback when flow finished.
};

} // namespace ns3

#endif /* RDMA_CLIENT_H */
