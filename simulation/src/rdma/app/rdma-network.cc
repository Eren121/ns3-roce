#include "ns3/rdma-network.h"
#include "ns3/qbb-net-device.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/switch-node.h"
#include "ns3/string.h"
#include "ns3/rdma-hw.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/sim-setting.h"
#include "ns3/rdma-flow.h"
#include <array>
#include <type_traits>
#include <cmath>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("RdmaNetwork");

double FindEcnThreshold(const std::vector<BandwidthToEcnThreshold>& map, double bps)
{
	std::map<uint64_t, BandwidthToEcnThreshold> sorted{};
	for(const auto& entry : map) {
		sorted[entry.bandwidth] = entry;
	}

	// Just used the entry lower or equals
	// This permit to avoid crash when the bandwidth is not in the map
	// This just crash if the first item in the map is higher than `bps`
	auto it{sorted.lower_bound(bps)};
	NS_ABORT_MSG_IF(it == sorted.begin(), "Cannot find ECN threshold related to bandwidth " << bps);
	return (--it)->second.ecn_threshold;
}

RdmaNetwork& RdmaNetwork::GetInstance()
{
  static RdmaNetwork instance;
  return instance;
}

RdmaNetwork::~RdmaNetwork()
{
  NS_LOG_FUNCTION(this);
   
  if(m_trace_output) {
	  fclose(m_trace_output);
  }

  if(!m_config->pfc_output_file.empty()) {
    std::ofstream ofs(m_config->FindFile(m_config->pfc_output_file));
    ofs << rfl::json::write(m_pfc_entries) << std::endl;
  }  

}

void RdmaNetwork::CreateNodes()
{
const size_t node_count{m_topology->nodes.size()};
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

  for (NodeContainer::Iterator i{n.Begin()}; i != n.End (); ++i) {
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
  return m_nodes.at(id);
}

Ipv4Address RdmaNetwork::FindNodeIp(node_id_t id) const
{
  return GetNodeIp(id);
}

uint64_t RdmaNetwork::FindBdp(node_id_t src, node_id_t dst) const
{
  if(m_config->global_t == 1) {
    return m_maxBdp;
  }
  
  return m_p2p.at(FindNode(src)).at(FindNode(dst)).bdp;
}

uint64_t RdmaNetwork::GetMaxBdp() const
{
  return m_maxBdp;
}

NodeContainer RdmaNetwork::FindMcastGroup(uint32_t id) const
{
  NodeContainer nodes;
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

void RdmaNetwork::SetConfig(std::shared_ptr<RdmaConfig> config)
{
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

void RdmaNetwork::SetTopology(std::shared_ptr<RdmaTopology> topology)
{
  NS_ASSERT_MSG(!m_topology, "RdmaNetwork has already been created");
  
  m_topology = topology;

  CreateNodes();
  InstallInternet();
  CreateLinks();
  ConfigureSwitches();
  InstallRdma();
  BuildRoutes();
  BuildGroups();
  CreateAnimation();
  StartQLenMonitor();
}

void RdmaNetwork::StartQLenMonitor()
{
  m_qlen_monitor = std::make_unique<QLenMonitor>(*this);
  m_qlen_monitor->Start();

  m_qp_monitor = std::make_unique<QpMonitor>(*this);
  m_qp_monitor->Start();
}

void RdmaNetwork::CreateAnimation()
{
	if(m_config->anim_output_file.empty()) {
    return;
  }

  for (const RdmaTopology::Node& node_config : m_topology->nodes) {
    const Ptr<Node> node{FindNode(node_config.id)};
    Vector3D p{};
    p.x = node_config.pos.x;
    p.y = node_config.pos.y;
    AnimationInterface::SetConstantPosition(node, p.x, p.y, p.z);
  }

  // Save trace for animation
  m_anim = std::make_unique<AnimationInterface>(m_config->FindFile(m_config->anim_output_file));
  
  // High polling interval; the nodes never move. Reduces XML size.
  m_anim->SetMobilityPollInterval(Seconds(1e9));

  m_anim->EnablePacketMetadata(true);
}

void RdmaNetwork::BuildGroups()
{
  //
  // Populate `m_mcast_groups`
  //

  for(const RdmaTopology::Group& group : m_topology->groups) {

    Ranges node_ranges{group.nodes};

    if(node_ranges.IsWildcard()) {

      for(const auto& [_, node] : m_servers) {
        m_mcast_groups[group.id].insert(node->GetId());
      }
    }

    for(const Ranges::Element& elem : node_ranges) {
      
      if(const auto* idx = std::get_if<Ranges::Index>(&elem)) {

        const int node_id{*idx};
        NS_ASSERT(!IsSwitchNode(FindNode(node_id)));
        m_mcast_groups[group.id].insert(node_id);
      }
      else if(const auto* range = std::get_if<Ranges::Range>(&elem)) {

        for(int node_id = range->first; node_id <= range->last; node_id++) {

          NS_ASSERT(!IsSwitchNode(FindNode(node_id)));
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

void RdmaNetwork::CreateLinks()
{
  // Create the links; and initialize `p2p.iface`
  
  uint64_t next_rng_seed{m_config->rng_seed};

  size_t link_id{0};
	for (const RdmaTopology::Link& link : m_topology->links) {
    
		const node_id_t src{link.src};
		const node_id_t dst{link.dst};
		const Ptr<Node> snode{FindNode(link.src)};
		const Ptr<Node> dnode{FindNode(link.dst)};

		m_qbb.SetDeviceAttribute("DataRate", DataRateValue(DataRate(link.bandwidth)));
		m_qbb.SetChannelAttribute("Delay", TimeValue(Seconds(link.latency)));

		if (link.error_rate > 0) // Set the packet drop rate if necessary, otherwise never drop
		{
			Ptr<RateErrorModel> rem{CreateObject<RateErrorModel>()};
			Ptr<UniformRandomVariable> uv{CreateObject<UniformRandomVariable>()};
			rem->SetRandomVariable(uv);
			uv->SetStream(next_rng_seed++);
			rem->SetAttribute("ErrorRate", DoubleValue(link.error_rate));
			rem->SetAttribute("ErrorUnit", StringValue("ERROR_UNIT_PACKET"));
			m_qbb.SetDeviceAttribute("ReceiveErrorModel", PointerValue(rem));
		}

		// Assigne server IP
		// Note: this should be before the automatic assignment below (ipv4.Assign(d)),
		// because we want our IP to be the primary IP (first in the IP address list),
		// so that the global routing is based on our IP

		NetDeviceContainer d{m_qbb.Install(snode, dnode)};
    
		// Used to create a graph of the topology
    // Initialize both src -> dst and dst -> src
    
    const std::array<Ptr<Node>, 2> pair{snode, dnode};
    for(size_t i{0}; i < pair.size(); i++) {
      const Ptr<Node> src{pair[i]};
      const Ptr<Node> dst{pair[1 - i]};

      if (!IsSwitchNode(src)) {
        Ptr<Ipv4> ipv4 = src->GetObject<Ipv4>();
        ipv4->AddInterface(d.Get(i));
        ipv4->AddAddress(1, Ipv4InterfaceAddress(GetNodeIp(src->GetId()), Ipv4Mask(0xff000000)));
      }
  
      m_p2p[src][dst].iface.idx = DynamicCast<QbbNetDevice>(d.Get(i))->GetIfIndex();
      m_p2p[src][dst].iface.up = true;
      m_p2p[src][dst].iface.delay = DynamicCast<QbbChannel>(DynamicCast<QbbNetDevice>(d.Get(i))->GetChannel())->GetDelay();
      m_p2p[src][dst].iface.bw = DynamicCast<QbbNetDevice>(d.Get(i))->GetDataRate();

      // Setup PFC trace
      if(!m_config->pfc_output_file.empty()) {
        Ptr<QbbNetDevice> qbb{DynamicCast<QbbNetDevice>(d.Get(i))};
        qbb->TraceConnectWithoutContext("QbbPfc",
          MakeLambdaCallback<uint32_t>([this, qbb](uint32_t type) {
            MonitorPfc(qbb, type);
          }
        ));
      }

      NS_LOG_LOGIC("Link n=("
        << src->GetId() << ", "
        << dst->GetId() << "): node "
        << src->GetId() << " use iface "
        << d.Get(i)->GetIfIndex());
    }

		// This is just to set up the connectivity between nodes. The IP addresses are useless
    
		char ipstring[32];
		snprintf(ipstring, sizeof(ipstring), "10.%d.%d.0", link_id / 254 + 1, link_id % 254 + 1);
    Ipv4AddressHelper ipv4;
		ipv4.SetBase(ipstring, "255.255.255.0");
		ipv4.Assign(d);

    link_id++;
	}

  NodeContainer n;
  for(const auto& [_, node] : m_nodes) { n.Add(node); }
  SwitchNode::Rebuild(n);
}

void RdmaNetwork::ConfigureSwitches()
{
  for (const auto& [_, sw] : m_switches) {
    
    // Multiplies the switch memory by `1 << shift`
    const uint32_t shift{3}; // By default 1/8 (?)

    for (uint32_t j = 1; j < sw->GetNDevices(); j++) {
      Ptr<QbbNetDevice> dev = DynamicCast<QbbNetDevice>(sw->GetDevice(j));

      // Init ECN
      uint64_t rate{dev->GetDataRate().GetBitRate()};
      const uint32_t kmin{FindEcnThreshold(m_config->kmin_map, rate)};
      const uint32_t kmax{FindEcnThreshold(m_config->kmax_map, rate)};
      const double pmax{FindEcnThreshold(m_config->pmax_map, rate)};
      sw->m_mmu->ConfigEcn(j, kmin, kmax, pmax);

      // Init PFC
      const uint64_t delay{DynamicCast<QbbChannel>(dev->GetChannel())->GetDelay().GetTimeStep()};
      const uint32_t headroom{rate * delay / 8 / 1e9 * 3};
      sw->m_mmu->ConfigHdrm(j, headroom);

      // Get the rate of the servers
      // In a fat tree all servers have same link bandwidth (the lowest of the topology)
      const uint64_t nic_rate{DynamicCast<QbbNetDevice>(begin(m_servers)->second->GetDevice(1))->GetDataRate().GetBitRate()};
      
      // Init PFC alpha, proportional to link bandwidth
      sw->m_mmu->pfc_a_shift[j] = shift;
      while (rate > nic_rate && sw->m_mmu->pfc_a_shift[j] > 0){
        sw->m_mmu->pfc_a_shift[j]--;
        rate /= 2;
      }
    }

    sw->m_mmu->ConfigNPort(sw->GetNDevices() - 1);
    sw->m_mmu->node_id = sw->GetId();
  }
}

void RdmaNetwork::InstallRdma()
{
	for(const auto& [_, node] : m_servers) {

    // create RdmaHw
    
    Ptr<RdmaHw> rdmaHw = CreateObject<RdmaHw>();
    
    #if RAF_WAITS_REFACTORING
    rdmaHw->SetAttribute("MiThresh", UintegerValue(m_config->mi_thresh));
    #endif

    #if RAF_WAITS_REFACTORING
    rdmaHw->SetAttribute("MultiRate", BooleanValue(m_config->multi_rate));
    rdmaHw->SetAttribute("SampleFeedback", BooleanValue(m_config->sample_feedback));
    rdmaHw->SetAttribute("TargetUtil", DoubleValue(m_config->u_target));
    #endif
    
    #if RAF_WAITS_REFACTORING
    rdmaHw->SetAttribute("DctcpRateAI", DataRateValue(DataRate(m_config->dctcp_rate_ai)));
    rdmaHw->SetAttribute("PintSmplThresh", UintegerValue(m_config->pint_prob * 65536));
    #endif

    node->AggregateObject(rdmaHw);
    rdmaHw->Setup();
    
    // TODO
    // rdmaHw->TraceConnectWithoutContext("QpComplete", MakeBoundCallback(qp_finish, fct_output, m_config.get()));
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
  TraceNodes();
}

void RdmaNetwork::BuildRoute(Ptr<Node> host)
{
	std::vector<Ptr<Node> > q; // Queue for the BFS
	std::map<Ptr<Node>, int> dis; // Distance from the host to each node (each hop is +1)
	std::map<Ptr<Node>, uint64_t> delay;
	std::map<Ptr<Node>, uint64_t> tx_delay;
	std::map<Ptr<Node>, uint64_t> bw;

	// Init BFS

	q.push_back(host); 
	dis[host] = 0;
	delay[host] = 0;
	tx_delay[host] = 0;
	bw[host] = 0xfffffffffffffffflu;

	// Run BFS

	for (int i = 0; i < (int)q.size(); i++) {
		Ptr<Node> now = q[i];
		int d = dis[now];
		for (auto it = m_p2p[now].begin(); it != m_p2p[now].end(); it++) {
      const P2pInfo& p2p{it->second};

			// skip down link
			if (!p2p.iface.up) {
        continue;
      }
			
			Ptr<Node> next = it->first;

			// If 'next' have not been visited
			if (dis.find(next) == dis.end()) {
				dis[next] = d + 1;
				delay[next] = delay[now] + p2p.delay;
				tx_delay[next] = tx_delay[now] + GetMtuBytes() * 1e9 * 8 / p2p.bw;
				bw[next] = std::min(bw[now], p2p.bw);
				
        // We only enqueue switch, because we do not want packets to go through host as middle point
				if (IsSwitchNode(next)) { q.push_back(next); }
			}

			// If 'now' is on the shortest path from 'next' to 'host'
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
	// Build p2p
	m_maxRtt = 0;
  m_maxBdp = 0;

  // Iterate all pairs of node from server to server
	for (const auto& [_, src] : m_servers) {
    for (const auto& [_, dst] : m_servers) {
      NS_ASSERT(!IsSwitchNode(src));
      NS_ASSERT(!IsSwitchNode(dst));

      P2pInfo& p2p{m_p2p.at(src).at(dst)};
			p2p.rtt = p2p.delay * 2 + p2p.tx_delay;
			p2p.bdp = p2p.rtt * p2p.bw / 1e9 / 8; 

			if (p2p.bdp > m_maxBdp) { m_maxBdp = p2p.bdp; }
			if (p2p.rtt > m_maxRtt) { m_maxRtt = p2p.rtt; }
		}
	}
	printf("maxRtt=%lu maxBdp=%lu\n", m_maxRtt, m_maxBdp);
  
	for (const auto& [_, sw] : m_switches) {
    sw->SetAttribute("MaxRtt", UintegerValue(m_maxRtt));
	}
}

void RdmaNetwork::TraceNodes()
{
	if(!m_config->enable_trace) {
    return;
  }

  if(m_config->trace_file.empty()) {
    return;
  }

  m_to_trace = m_config->LoadFromfile<RdmaTraceList>(m_config->trace_file);

	NodeContainer trace_nodes;
  for(node_id_t node_id : m_to_trace.nodes) {
    trace_nodes.Add(FindNode(node_id));
  }

  m_trace_output = fopen(m_config->FindFile(m_config->trace_output_file).c_str(), "w");
  NS_ABORT_MSG_IF(!m_trace_output, "Cannot open output trace file"); 
  m_qbb.EnableTracing(m_trace_output, trace_nodes); 

	// Dump link speed to trace file

	{
		SimSetting sim_setting;

    for(auto i : m_p2p) {
      for(auto j : i.second) {
        if(!j.second.iface.up) {
          continue;
        }

				uint16_t node = i.first->GetId();
				uint8_t intf = j.second.iface.idx;
				uint64_t bps = DynamicCast<QbbNetDevice>(i.first->GetDevice(j.second.iface.idx))->GetDataRate().GetBitRate();
				sim_setting.port_speed[node][intf] = bps;
      }
    }

		sim_setting.win = m_maxBdp;
		sim_setting.Serialize(m_trace_output);
	}
}

void RdmaNetwork::MonitorPfc(Ptr<QbbNetDevice> qbb, uint32_t type)
{
  NS_LOG_FUNCTION(this);

  m_pfc_entries.emplace_back(
    Simulator::Now().GetSeconds(),
    qbb->GetNode()->GetId(),
    qbb->GetIfIndex(),
    type
  );
}

} // namespace ns3