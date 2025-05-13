#include "ns3/rdma-network.h"
#include "ns3/qbb-net-device.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/switch-node.h"
#include "ns3/string.h"
#include "ns3/rdma-hw.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/rdma-flow-scheduler.h"
#include <array>
#include <type_traits>
#include <cmath>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("RdmaNetwork");

bool RdmaNetwork::m_initialized = false;

void RdmaNetwork::Initialize(const fs::path& config_path)
{
  NS_ABORT_MSG_IF(m_initialized, "You cannot initialize RdmaNetwork twice");
  m_initialized = true;

  // Load configuration.
	const auto config = RdmaConfig::from_file(config_path);
	NS_LOG_INFO("Config: " << rfl::json::write(*config));
	config->ApplyDefaultAttributes();
  
  if(!config->simulator_stop_time.IsZero()) {
	  Simulator::Stop(config->simulator_stop_time);
  }

  // Load topology.
	const fs::path topology_path = config->FindFile(config->topology_file);
	auto topology = std::make_shared<RdmaTopology>(json::parse(std::ifstream{topology_path}));
  
  // Initialize the `RdmaNetwork`.
  auto& instance = GetInstance();
  instance.InitConfig(config);
  instance.InitTopology(topology);
  instance.InitModules();

  // Load flows.
	FlowScheduler flow_scheduler{instance, config->FindFile(config->flows_file)};
	flow_scheduler.SetOnAllFlowsCompleted([]() {
		NS_LOG_INFO("Simulation stopped at " << Simulator::Now().GetSeconds() << "s.");
		Simulator::Stop();
	});

  // Run the simulation.
	NS_LOG_INFO("Running Simulation.");
	Simulator::Run();
  NS_LOG_INFO("Exit stopped at " << Simulator::Now().GetSeconds() << "s.");

  // Permits to modules to get the time of the simulator, before it is destroyed.
  instance.m_modules.clear();

	Simulator::Destroy();
}

RdmaNetwork& RdmaNetwork::GetInstance()
{
  NS_ABORT_MSG_IF(!m_initialized, "RdmaNetwork is not initialized");

  static RdmaNetwork instance;
  return instance;
}

void RdmaNetwork::CreateNodes()
{
  NS_ABORT_MSG_IF(!m_nodes.empty(), "Nodes already created");

  NodeContainer n;

	for (const RdmaTopology::Node& node_config : m_topology->nodes) {
		if (node_config.is_switch) {
			Ptr<SwitchNode> sw = CreateObject<SwitchNode>();
			n.Add(sw);
		}
		else {
			n.Add(CreateObject<Node>());
		}
	}

  for (NodeContainer::Iterator i = n.Begin(); i != n.End(); ++i) {
    AddNode(*i);
	}
}

void RdmaNetwork::AddNode(Ptr<Node> node)
{
  const node_id_t id{node->GetId()};
  m_nodes[id] = node;
  
  if(IsSwitchNode(node)) {
    m_switches[id] = DynamicCast<SwitchNode>(node);
  }
  else {
    m_servers[id] = node;
  }
}

Ptr<Node> RdmaNetwork::FindNode(node_id_t id) const
{
  return m_nodes.at(id);
}

Ptr<Node> RdmaNetwork::FindServer(node_id_t id) const
{
  return m_servers.at(id);
}

Ipv4Address RdmaNetwork::FindNodeIp(node_id_t id) const
{
  return GetNodeIp(id);
}

uint64_t RdmaNetwork::GetMaxBdp() const
{
  return m_maxBdp;
}

Time RdmaNetwork::GetMaxDelay() const
{
  return NanoSeconds(m_maxRtt) / 2;
}

NodeMap RdmaNetwork::FindMcastGroup(uint32_t id) const
{
  NodeMap nodes;
  for(node_id_t node_id : m_mcast_groups.at(id)) {
    nodes.Add(FindServer(node_id));
  }
  return nodes;
}

NodeMap RdmaNetwork::GetAllSwitches() const
{
  NodeMap nodes;
  for(const auto& [_, sw] : m_switches) {
    nodes.Add(sw);
  }
  return nodes;
}

NodeMap RdmaNetwork::GetAllServers() const
{
  NodeMap nodes;
  for(const auto& [_, server] : m_servers) {
    nodes.Add(server);
  }
  return nodes;
}

void RdmaNetwork::InitConfig(std::shared_ptr<RdmaConfig> config)
{
  NS_ASSERT_MSG(!m_config, "RdmaNetwork has already a config");
  
  m_config = config;
  
	// set ACK priority on hosts
	if (m_config->ack_high_prio){
		RdmaEgressQueue::ack_q_idx = 0; // TODO refactor
  }
	else {
		RdmaEgressQueue::ack_q_idx = 3;
  }

  // Todo set AckHighPrio
}

const RdmaConfig& RdmaNetwork::GetConfig() const
{
  return *m_config;
}

void RdmaNetwork::InitTopology(std::shared_ptr<RdmaTopology> topology)
{
  NS_ABORT_MSG_IF(m_topology, "Topology already initialized");
  
  m_topology = topology;

  CreateNodes();
  InstallInternet();
  CreateLinks();
  ConfigureSwitches();
  InstallRdma();
  BuildRoutes();
  BuildGroups();
}

void RdmaNetwork::InitModules()
{
  NS_ABORT_MSG_IF(!m_modules.empty(), "Modules already initialized");

  for(const auto& module_info : m_config->modules) {
    if(!module_info.enable) {
      continue;
    }

    ObjectFactory factory{module_info.path};
    PopulateAttributes(factory, module_info.attributes);

    Ptr<RdmaConfigModule> module_instance = factory.Create<RdmaConfigModule>();
    module_instance->OnModuleLoaded(*this);
    m_modules.push_back(module_instance);
  }
}

void RdmaNetwork::BuildGroups()
{
  //
  // Populate `m_mcast_groups`
  //

  // Add reserved multicast group zero which contains all nodes.
  {
    RdmaTopology::Group group_zero;
    group_zero.id = 0;
    group_zero.nodes = "*";
    m_topology->groups.push_back(group_zero);
  }

  for(const RdmaTopology::Group& group : m_topology->groups) {

    Ranges node_ranges{group.nodes};

    if(node_ranges.IsWildcard()) {

      for(const auto& [_, node] : m_servers) {
        m_mcast_groups[group.id].insert(node->GetId());
      }
    }

    for(const Ranges::Element& elem : node_ranges) {
      
      if(const auto* idx = std::get_if<Ranges::Index>(&elem)) {

        const unsigned int node_id{*idx};
        NS_ABORT_IF(IsSwitchNode(FindNode(node_id)));
        m_mcast_groups[group.id].insert(node_id);
      }
      else if(const auto* range = std::get_if<Ranges::Range>(&elem)) {

        for(int node_id = range->first; node_id <= range->last; node_id++) {

          NS_ABORT_IF(IsSwitchNode(FindNode(node_id)));
          m_mcast_groups[group.id].insert(node_id);
        }
      }
      else {

        NS_ABORT_MSG("Unknown range type");
      }
    }
  }

  //
  // Initialize routing table for multicast groups
  //

  for(const auto& [group_id, nodes] : m_mcast_groups) {

    for(node_id_t node_id : nodes) {

      const Ptr<Node> node{FindServer(node_id)};
      
      NS_LOG_LOGIC("Group " << group_id << " has node " << node_id);

      //
      // Assume the node has only one NIC and one link
      //

      const int nic_iface{1};
      Ptr<QbbNetDevice> qbb{DynamicCast<QbbNetDevice>(node->GetDevice(nic_iface))};
      qbb->AddGroup(group_id);
      
      //
      // Add a table entry for the mcast address
      // To know which interface to send mcast packets
      //

      const Ipv4Address mcast_addr{group_id};
      node->GetObject<RdmaHw>()->AddTableEntry(mcast_addr, nic_iface);
    }
  }
}

NetDeviceContainer RdmaNetwork::GetAllQbbNetDevices() const
{
  NetDeviceContainer res;
	for(const auto& [_, node] : m_nodes) {
    for(uint32_t dev_i = 0; dev_i < node->GetNDevices(); dev_i++) {
      auto const dev = DynamicCast<QbbNetDevice>(node->GetDevice(dev_i));
      if(dev) {
        res.Add(dev);
      }
    }
  }

  // Each node should have exactly one net device!
  NS_ABORT_IF(res.GetN() != m_nodes.size());

  return res;
}

void RdmaNetwork::CreateLinks()
{
  // Create the links; and initialize `p2p.iface`
  
  uint64_t next_rng_seed{m_config->rng_seed};

  size_t link_id{0};

  // Note: since `m_qbb` is member, we need to pay attention to reset the fields for each link.

  // Iterate all links in the configuration topology.
	for (const RdmaTopology::Link& link : m_topology->links) {
    
		const node_id_t src{link.src};
		const node_id_t dst{link.dst};
		const Ptr<Node> snode{FindNode(link.src)};
		const Ptr<Node> dnode{FindNode(link.dst)};

    // Set link delay and bandwidth based on the configuration.
		m_qbb.SetDeviceAttribute("DataRate", DataRateValue(DataRate(link.bandwidth)));
		m_qbb.SetChannelAttribute("Delay", TimeValue(Seconds(link.latency)));

    // Set the packet drop rate if necessary, otherwise never drop
		if(link.error_rate > 0)
		{
			Ptr<RateErrorModel> rem{CreateObject<RateErrorModel>()};
			Ptr<UniformRandomVariable> uv{CreateObject<UniformRandomVariable>()};
			rem->SetRandomVariable(uv);
			uv->SetStream(next_rng_seed++);
			rem->SetAttribute("ErrorRate", DoubleValue(link.error_rate));
			rem->SetAttribute("ErrorUnit", StringValue("ERROR_UNIT_PACKET"));
			m_qbb.SetDeviceAttribute("ReceiveErrorModel", PointerValue(rem));
		}
    else {
      // Set the error model as NULL.
			m_qbb.SetDeviceAttribute("ReceiveErrorModel", PointerValue());
    }

		// Create the net devices.
    // A net device is just a queue to receive and send packets.
    // This does not assign any IP address for now, just a MAC address.
		NetDeviceContainer pair_devs{m_qbb.Install(snode, dnode)};
    
		// Create a graph of the topology.
    const std::array<Ptr<Node>, 2> pair_nodes{snode, dnode};
    
    // Iterate for `snode -> dnode` and `dnode -> snode`.
    // `i` can only be zero or one.
    for(size_t i{0}; i < pair_nodes.size(); i++) {
      const Ptr<Node> src{pair_nodes[i]};
      const Ptr<Node> dst{pair_nodes[1 - i]};
      const Ptr<QbbNetDevice> src_dev{DynamicCast<QbbNetDevice>(pair_devs.Get(i))};
      const Ptr<QbbChannel> src_channel{DynamicCast<QbbChannel>(src_dev->GetChannel())};

      // Assign the IP address on `src`.
      if (!IsSwitchNode(src)) {
        Ptr<Ipv4> ipv4{src->GetObject<Ipv4>()};
        
        // Associate the net device as the output interface during packet forwarding.
        const uint32_t iface{ipv4->AddInterface(src_dev)};
        NS_ABORT_MSG_IF(iface != 1, "The node should have no interface");

        // Assign the IP address to the just added interface.
        const bool success{ipv4->AddAddress(iface, Ipv4InterfaceAddress(GetNodeIp(src->GetId()), Ipv4Mask(0xff000000)))};
        NS_ABORT_MSG_IF(!success, "Cannot assign IP address");
      }
  
      // Initialize the P2P info.
      Interface iface;
      iface.up = true;
      iface.idx = src_dev->GetIfIndex();
      iface.delay = src_channel->GetDelay();
      iface.bw = src_dev->GetDataRate();
      m_p2p[src][dst].iface = iface;

      NS_LOG_LOGIC("Link (src,dst)=("
        << src->GetId() << ", "
        << dst->GetId() << ")."
        << " Source uses interface "
        << src_dev->GetIfIndex());
    }

		// This is just to set up the connectivity between nodes.
    // The IP addresses are useless.
    {
      char ipstring[32];
      snprintf(ipstring, sizeof(ipstring), "10.%d.%d.0", link_id / 254 + 1, link_id % 254 + 1);
      Ipv4AddressHelper ipv4;
      ipv4.SetBase(ipstring, "255.255.255.0");
      ipv4.Assign(pair_devs);
    }

    link_id++;
	}

  NodeContainer all_nodes;
  for(const auto& [_, node] : m_nodes) {
    all_nodes.Add(node);
  }
  SwitchNode::Rebuild(all_nodes);
}

DataRate RdmaNetwork::GetAnyServerDataRate() const
{
	for (const RdmaTopology::Link& link : m_topology->links) {
		const node_id_t src{link.src};
		const node_id_t dst{link.dst};
		const Ptr<Node> snode{FindNode(link.src)};
		const Ptr<Node> dnode{FindNode(link.dst)};

    const bool is_server_link{!IsSwitchNode(snode) || !IsSwitchNode(dnode)};
    if(is_server_link) {
      return link.bandwidth;
    }
  }
}

bool RdmaNetwork::HaveAllServersSameBandwidth() const
{
  const RdmaTopology::Link* first_server_link{};

	for (const RdmaTopology::Link& link : m_topology->links) {
		const node_id_t src{link.src};
		const node_id_t dst{link.dst};
		const Ptr<Node> snode{FindNode(link.src)};
		const Ptr<Node> dnode{FindNode(link.dst)};

    const bool is_server_link{!IsSwitchNode(snode) || !IsSwitchNode(dnode)};
    if(is_server_link) {
      if(!first_server_link) {
        first_server_link = &link;
      }
      else if(first_server_link->bandwidth != link.bandwidth) {
        return false;
      }
    }
  }

  return true;
}

void RdmaNetwork::ConfigureSwitches()
{
  NS_ABORT_IF(!HaveAllServersSameBandwidth());

  // Get the rate of the servers.
  // Assumes all servers have the same bandwidth.
  // In a fat tree all servers have same link bandwidth (the lowest of the topology).
  const uint64_t nic_rate{DynamicCast<QbbNetDevice>(begin(m_servers)->second->GetDevice(1))->GetDataRate().GetBitRate()};

  for (const auto& [_, sw] : m_switches) {
    
    auto& mmu = *sw->m_mmu;

    // Multiply the switch memory by `1 << shift`.
    // By default 1/8 (original project: why?).
    const uint32_t shift{3};

    for (uint32_t j = 1; j < sw->GetNDevices(); j++) {
      Ptr<QbbNetDevice> dev = DynamicCast<QbbNetDevice>(sw->GetDevice(j));
      
      // Init ECN.
      uint64_t rate = dev->GetDataRate().GetBitRate();
      const auto ecn = FindEcnConfigFromBps(m_config->ecn, rate);
      mmu.ConfigEcn(j, ecn.min_buf_bytes, ecn.max_buf_bytes, ecn.max_probability);

      // Init PFC.
      const uint64_t delay{DynamicCast<QbbChannel>(dev->GetChannel())->GetDelay().GetTimeStep()};
      const uint32_t headroom{rate * delay / 8 / 1e9 * 3};
      mmu.ConfigHdrm(j, headroom);
      
      // Init PFC alpha, proportional to link bandwidth.
      mmu.pfc_a_shift[j] = shift;
      while(rate > nic_rate && mmu.pfc_a_shift[j] > 0) {
        mmu.pfc_a_shift[j]--;
        rate /= 2;
      }
    }

    mmu.ConfigNPort(sw->GetNDevices() - 1);
    mmu.node_id = sw->GetId();
  }
}

void RdmaNetwork::InstallRdma()
{
	for(const auto& [_, node] : m_servers) {

    // Create RDMA hardware.
    Ptr<RdmaHw> rdmaHw = CreateObject<RdmaHw>();

    node->AggregateObject(rdmaHw);
    rdmaHw->Setup();
  }
}

void RdmaNetwork::InstallInternet()
{
  NodeContainer n;
  for(const auto& [_, node] : m_nodes) { n.Add(node); }
	InternetStackHelper internet;
	internet.Install(n);
}

Ipv4Address RdmaNetwork::GetNodeIp(node_id_t id)
{
	return Ipv4Address(0x0b000001 + ((id / 256) * 0x00010000) + ((id % 256) * 0x00000100));
}

void RdmaNetwork::BuildRoutes()
{
	for(const auto& [_, server] : m_servers) {
    BuildRoute(server);
	}

	BuildRoutingTables();
  BuildP2pInfo();
	Ipv4GlobalRoutingHelper::PopulateRoutingTables();
}

void RdmaNetwork::BuildRoute(Ptr<Node> host)
{
	std::vector<Ptr<Node> > q; // Queue for the BFS
	std::map<Ptr<Node>, int> dis; // Distance from the host to each node (each hop is +1)
	std::map<Ptr<Node>, uint64_t> delay;
	std::map<Ptr<Node>, uint64_t> tx_delay;
	std::map<Ptr<Node>, uint64_t> bw;

	// Init BFS.

	q.push_back(host); 
	dis[host] = 0;
	delay[host] = 0;
	tx_delay[host] = 0;
	bw[host] = 0xfffffffffffffffflu;

	// Run BFS.

	for (int i = 0; i < (int)q.size(); i++) {
		Ptr<Node> now = q[i];
		int d = dis[now];
		for (auto it = m_p2p[now].begin(); it != m_p2p[now].end(); it++) {
      const P2pInfo& p2p{it->second};

			// Skip down links.
			if (!p2p.iface.up) {
        continue;
      }
			
			Ptr<Node> next = it->first;

			// If `next` has not been visited.
			if (dis.find(next) == dis.end()) {
				dis[next] = d + 1;
				delay[next] = delay[now] + p2p.delay;
				tx_delay[next] = tx_delay[now] + GetMtuBytes() * 1e9 * 8 / p2p.bw;
				bw[next] = std::min(bw[now], p2p.bw);
				
        // We only enqueue switch, because we do not want packets to go through host as middle point.
				if (IsSwitchNode(next)) { q.push_back(next); }
			}

			// If `now` is on the shortest path from `next` to `host`.
			if (d + 1 == dis[next]){
				m_p2p[next][host].next_hops.push_back(now);
			}
		}
	}

	for (auto p : delay)    { m_p2p[p.first][host].delay = p.second; }
	for (auto p : tx_delay) { m_p2p[p.first][host].tx_delay = p.second; }
	for (auto p : bw)       { m_p2p[p.first][host].bw = p.second; }
}

void RdmaNetwork::BuildRoutingTables()
{
	// For each node.
  
	for (auto src_it{m_p2p.begin()}; src_it != m_p2p.end(); src_it++) {
    
    Ptr<Node> src_server{src_it->first};
    const auto& all_other_nodes{src_it->second};

		for (auto dst_it{all_other_nodes.begin()}; dst_it != all_other_nodes.end(); dst_it++) {

			Ptr<Node> dst_node{dst_it->first};
			Ipv4Address dst_addr{dst_node->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal()};
      const P2pInfo& p2p{dst_it->second};
      
			// The next hops towards the dst.
			for (Ptr<Node> next : p2p.next_hops) {

				const uint32_t interface{m_p2p[src_server][next].iface.idx};

				if (IsSwitchNode(src_server)) {
          DynamicCast<SwitchNode>(src_server)->AddTableEntry(dst_addr, interface);
        }
				else {
          src_server->GetObject<RdmaHw>()->AddTableEntry(dst_addr, interface);
        }
			}
		}
	}
}

uint64_t RdmaNetwork::GetMtuBytes() const
{
  TypeId::AttributeInformation mtu;
  NS_ABORT_IF(!RdmaHw::GetTypeId().LookupAttributeByName("Mtu", &mtu));
  return DynamicCast<const UintegerValue>(mtu.initialValue)->Get();
}

void RdmaNetwork::BuildP2pInfo()
{
  // Requires: All `delay`, `tx_delay` of `P2pInfo` are set.
	// Builds `m_maxRtt`, `m_maxBdp`;
  // and `rtt` and `bdp` of all `P2pInfo`.

	m_maxRtt = 0;
  m_maxBdp = 0;

  // Iterate all pairs of node from server to server
	for(const auto& [_, src] : m_servers) {
    for(const auto& [_, dst] : m_servers) {
      NS_ASSERT(!IsSwitchNode(src));
      NS_ASSERT(!IsSwitchNode(dst));

      P2pInfo& p2p{m_p2p.at(src).at(dst)};
			p2p.rtt = p2p.delay * 2 + p2p.tx_delay;
			p2p.bdp = p2p.rtt * p2p.bw / 1e9 / 8; 

			if(p2p.bdp > m_maxBdp) {
        m_maxBdp = p2p.bdp;
      }

			if(p2p.rtt > m_maxRtt) {
        m_maxRtt = p2p.rtt;
      }
		}
	}

	NS_LOG_INFO("Highest RTT: " << NanoSeconds(m_maxRtt));
  NS_LOG_INFO("Highest BDP: " << m_maxBdp << "B");
  
	for (const auto& [_, sw] : m_switches) {
    sw->SetAttribute("MaxRtt", UintegerValue(m_maxRtt));
	}
}

} // namespace ns3