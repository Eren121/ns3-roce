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
#include "rdma-client-helper.h"
#include "ns3/rdma-client.h"
#include "ns3/uinteger.h"
#include "ns3/boolean.h"
#include "ns3/string.h"
#include <ns3/switch-node.h>

namespace ns3 {

RdmaClientHelper::RdmaClientHelper()
{
}

RdmaClientHelper::RdmaClientHelper (NodeContainer nodes, uint16_t pg, Ipv4Address sip, Ipv4Address dip, uint16_t sport, uint16_t dport, uint64_t size, uint32_t win, uint64_t baseRtt)
	: m_nodes(nodes)
{
	m_factory.SetTypeId (RdmaClient::GetTypeId ());
	SetAttribute ("PriorityGroup", UintegerValue (pg));
	SetAttribute ("WriteSize", UintegerValue (size));
	SetAttribute ("Window", UintegerValue (win));
	SetAttribute ("BaseRtt", UintegerValue (baseRtt));
}

void
RdmaClientHelper::SetAttribute (std::string name, const AttributeValue &value)
{
  m_factory.Set (name, value);
}

ApplicationContainer
RdmaClientHelper::Install (NodeContainer c)
{
  ApplicationContainer apps;
  for (NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i)
	{
		Ptr<Node> node = *i;
		if(!IsSwitchNode(node)) {
			Ptr<RdmaClient> client = m_factory.Create<RdmaClient> ();
			client->SetNodeContainer(m_nodes);
			node->AddApplication (client);
			apps.Add (client);
		}
	}
  return apps;
}

} // namespace ns3
