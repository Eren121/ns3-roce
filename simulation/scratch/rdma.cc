/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
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
*/

#include <iostream>
#include <iomanip>
#include <fstream>
#include <unordered_map>
#include <time.h> 
#include <charconv>
#include "ns3/core-module.h"
#include "ns3/qbb-helper.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/global-route-manager.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/packet.h"
#include "ns3/error-model.h"
#include <ns3/rdma.h>
#include <ns3/ag-app.h>
#include <ns3/rdma-random.h>
#include <ns3/ag-app-helper.h>
#include <ns3/rdma-hw.h>
#include <ns3/rdma-flow.h>
#include <ns3/switch-node.h>
#include <ns3/json.h>
#include <ns3/animation-interface.h>
#include <ns3/constant-position-mobility-model.h>
#include <ns3/mobility-helper.h>
#include "ns3/rdma-network.h"
#include <filesystem>
#include <cstdlib>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("main");

int main(int argc, char *argv[])
{
	LogComponentEnable("main", LOG_LEVEL_INFO);
	LogComponentEnable("RdmaNetwork", LOG_LEVEL_INFO);
	LogComponentEnable("FlowScheduler", LOG_LEVEL_INFO);
	
	if(argc < 2) {
		std::cout << "Error: require a config file a unique program argument." << std::endl;
		return EXIT_FAILURE;
	}
	
	RdmaNetwork::Initialize(argv[1]);

	return EXIT_SUCCESS;
}
