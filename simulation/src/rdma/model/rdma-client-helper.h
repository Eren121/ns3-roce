/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2008 INRIA
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
 * Author: Mohamed Amine Ismail <amine.ismail@sophia.inria.fr>
 */
#ifndef RDMA_CLIENT_SERVER_HELPER_H
#define RDMA_CLIENT_SERVER_HELPER_H

#include <stdint.h>
#include "ns3/application-container.h"
#include "ns3/node-container.h"
#include "ns3/object-factory.h"
#include "ns3/ipv4-address.h"
#include "ns3/rdma-client.h"
#include <ns3/rdma-hw.h>

namespace ns3 {

/**
 * \brief Create a client application which does RDMA write
 */
class RdmaClientHelper
{

public:
  /**
   * Create RdmaClientHelper which will make life easier for people trying
   * to set up simulations with udp-client-server.
   *
   */
  RdmaClientHelper ();

  /**
   *  Create RdmaClientHelper which will make life easier for people trying
   * to set up simulations with udp-client-server.
   *
   * \param ip The IP address of the remote udp server
   * \param port The port number of the remote udp server
   */

  RdmaClientHelper (const ScheduledFlow& flow, uint32_t win);

  /**
   * Record an attribute to be set in each Application after it is is created.
   *
   * \param name the name of the attribute to set
   * \param value the value of the attribute to set
   */
  void SetAttribute (std::string name, const AttributeValue &value);

  /**
     * \param c the nodes
     *
     * Create one udp client application on each of the input nodes
     *
     * \returns the applications created, one application per input node.
     */
  ApplicationContainer Install (NodeContainer c);

private:
  static NodeContainer FilterServers(NodeContainer c);

private:
  ScheduledFlow m_flow;
  ObjectFactory m_factory;
};

} // namespace ns3

#endif /* RDMA_CLIENT_SERVER_HELPER_H */
