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

RdmaClientHelper::RdmaClientHelper (const ScheduledFlow& flow, uint32_t win)
	: m_flow{flow}
{
	m_factory.SetTypeId (RdmaClient::GetTypeId ());
	SetAttribute ("PriorityGroup", UintegerValue (flow.priority));
	SetAttribute ("WriteSize", UintegerValue (flow.size));
	SetAttribute ("Window", UintegerValue (win));
}

void
RdmaClientHelper::SetAttribute (std::string name, const AttributeValue &value)
{
  m_factory.Set (name, value);
}


NodeContainer RdmaClientHelper::FilterServers(NodeContainer c)
{
	NodeContainer ret;
  for (NodeContainer::Iterator i{c.Begin()}; i != c.End (); ++i)
	{
		Ptr<Node> node{*i};
		if(!IsSwitchNode(node)) { ret.Add(node); }
	}
	return ret;
}

ApplicationContainer
RdmaClientHelper::Install (NodeContainer c)
{
	const NodeContainer servers{FilterServers(c)};
  ApplicationContainer apps;

	std::shared_ptr<RdmaClient::SharedState> shared_state(new RdmaClient::SharedState());
	shared_state->flow = m_flow;

  for (NodeContainer::Iterator i{servers.Begin()}; i != servers.End (); ++i)
	{
		Ptr<Node> server{*i};
		Ptr<RdmaClient> client{m_factory.Create<RdmaClient>()};
		client->SetServers(servers);
		client->SetSharedState(shared_state);
		const uint32_t app_id{server->AddApplication(client)};
		NS_ASSERT(app_id == 0);
		apps.Add(client);
	}
  return apps;
}

} // namespace ns3
