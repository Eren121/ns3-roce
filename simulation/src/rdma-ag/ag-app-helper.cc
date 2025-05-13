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
#include "ns3/ag-app-helper.h"
#include "ns3/ag-app.h"
#include "ns3/uinteger.h"
#include "ns3/boolean.h"
#include "ns3/string.h"
#include <ns3/switch-node.h>

namespace ns3 {

AgAppHelper::AgAppHelper()
{
	m_factory.SetTypeId (AgConfig::GetTypeId());
}

void
AgAppHelper::SetAttribute (std::string name, const AttributeValue &value)
{
  m_factory.Set (name, value);
}

ObjectFactory&
AgAppHelper::GetFactory()
{
	return m_factory;
}

NodeContainer AgAppHelper::FilterServers(NodeContainer c)
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
AgAppHelper::Install (NodeContainer c)
{
	const NodeContainer servers{FilterServers(c)};

	Ptr<AgConfig> config{m_factory.Create<AgConfig>()};
	config->SetBlockCount(servers.GetN());
	config->SetMtu(servers.Get(0)->GetObject<RdmaHw>()->GetMTU());

	Ptr<AgShared> shared{CreateObject<AgShared>(config, servers)};
	
  ApplicationContainer apps;
  for (NodeContainer::Iterator i{servers.Begin()}; i != servers.End (); ++i)
	{
		Ptr<Node> server{*i};
		Ptr<AgApp> client{CreateObject<AgApp>(shared)};
		server->AddApplication(client);
		apps.Add(client);
	}
  return apps;
}

} // namespace ns3
