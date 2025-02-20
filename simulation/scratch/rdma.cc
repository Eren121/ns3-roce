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
#include <ns3/rdma-unicast-app-helper.h>
#include <ns3/rdma-hw.h>
#include <ns3/rdma-flow.h>
#include <ns3/switch-node.h>
#include <ns3/sim-setting.h>
#include <ns3/json.h>
#include <ns3/animation-interface.h>
#include <ns3/constant-position-mobility-model.h>
#include <ns3/mobility-helper.h>
#include "ns3/rdma-network.h"
#include <filesystem>
#include <cstdlib>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("main");

void SetupCC(const RdmaConfig& config)
{
#if RAF_WAITS_REFACTORING
	// set int_multi
	IntHop::multi = config.int_multi;
	// IntHeader::mode
	if (config.cc_mode == 7) // timely, use ts
		IntHeader::mode = IntHeader::TS;
	else if (config.cc_mode == 3) // hpcc, use int
		IntHeader::mode = IntHeader::NORMAL;
	else if (config.cc_mode == 10) // hpcc-pint
		IntHeader::mode = IntHeader::PINT;
	else // others, no extra header
		IntHeader::mode = IntHeader::NONE;

	// Set Pint
	if (config.cc_mode == 10) {
		Pint::set_log_base(config.pint_log_base);
		IntHeader::pint_bytes = Pint::get_n_bytes();
		printf("PINT bits: %d bytes: %d\n", Pint::get_n_bits(), Pint::get_n_bytes());
	}
#endif
}

int main(int argc, char *argv[])
{
	LogComponentEnable("main", LOG_LEVEL_INFO);
	LogComponentEnable("RdmaNetwork", LOG_LEVEL_INFO);
	LogComponentEnable("FlowScheduler", LOG_LEVEL_INFO);
	
	if(argc < 2) {
		std::cout << "Error: require a config file" << std::endl;
		return EXIT_FAILURE;
	}

	auto config = std::make_shared<RdmaConfig>(rfl::json::read<RdmaConfig>(raf::read_all_file(argv[1])).value());
	config->config_dir = fs::path{argv[1]}.parent_path();

	NS_LOG_INFO("Config: " << rfl::json::write(*config));

	for(const auto& [key, val] : config->defaults) {
		Ptr<AttributeValue> new_val{ConvertJsonToAttribute(val, FindConfigAttribute(key))};
		Config::SetDefault(key, *new_val);
		NS_LOG_INFO("Set " << key << " to " << new_val->SerializeToString(MakeEmptyAttributeChecker()));
	}
	
	SetupCC(*config);

	// SeedManager::SetSeed(time(NULL));

	auto topology = std::make_shared<RdmaTopology>(json::parse(std::ifstream{config->FindFile(config->topology_file)}));
	auto& network = RdmaNetwork::GetInstance();
	network.SetConfig(config);
	network.SetTopology(topology);

	FlowScheduler flow_scheduler;
	flow_scheduler.SetOnAllFlowsCompleted([]() {
		NS_LOG_INFO("Simulation stopped at " << Simulator::Now().GetSeconds());
		Simulator::Stop();
	});
	flow_scheduler.AddFlowsFromFile(config->FindFile(config->flow_file));

	NS_LOG_INFO("Running Simulation");

	Simulator::Stop(config->simulator_stop_time);
	Simulator::Run();
	
	NS_LOG_INFO("Exit.");
	
	Simulator::Destroy();
	return EXIT_SUCCESS;
}
