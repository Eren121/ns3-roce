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
#include <ns3/rdma-client.h>
#include <ns3/rdma-random.h>
#include <ns3/rdma-client-helper.h>
#include <ns3/rdma-unicast-app-helper.h>
#include <ns3/rdma-hw.h>
#include <ns3/switch-node.h>
#include <ns3/sim-setting.h>
#include <ns3/json.h>
#include <ns3/animation-interface.h>
#include <ns3/constant-position-mobility-model.h>
#include <ns3/mobility-helper.h>
#include <filesystem>

using namespace ns3;
namespace fs = std::filesystem;
using json = nlohmann::json;

fs::path config_dir;

NS_LOG_COMPONENT_DEFINE("GENERIC_SIMULATION");

template<typename T>
void WarnInvalidKeys(const json& object)
{
	const json object_default = T();

	// Do not search recursively

	for(const auto& it : object_default.items()) {
		const auto& key = it.key();
		if(!object.contains(key)) {
			std::cerr << "WARNING: Missing json key " << key << std::endl;
		}
	}
	
	for(const auto& it : object.items()) {
		const auto& key = it.key();
		if(!object_default.contains(key)) {
			std::cerr << "WARNING: Unknown json key " << key << std::endl;
		}
	}
}

/**
 * @brief Parameters for the configuration.
 */
struct SimConfig
{
	SimConfig() = default;
	SimConfig(const SimConfig&) = delete;
	SimConfig(SimConfig&&) = default;
	SimConfig& operator=(const SimConfig&) = delete;
	SimConfig& operator=(SimConfig&&) = default;

	uint32_t cc_mode = 1;
	bool enable_pfc = false, enable_qcn = true, use_dynamic_pfc_threshold = true;
	uint32_t packet_payload_size = 1000, l2_chunk_size = 0, l2_ack_interval = 0;
	double nak_interval = 500.0;
	double pause_time = 5, simulator_stop_time = 3.01;
	std::string topology_file, flow_file, groups_file, trace_file, trace_output_file;
	std::string fct_output_file = "fct.txt";
	std::string pfc_output_file = "pfc.txt";

	double alpha_resume_interval = 55, rp_timer, ewma_gain = 1 / 16;
	double rate_decrease_interval = 4;
	uint32_t fast_recovery_times = 5;
	std::string rate_ai, rate_hai, min_rate = "100Mb/s";
	std::string dctcp_rate_ai = "1000Mb/s";

	bool clamp_target_rate = false, l2_back_to_zero = false;
	double error_rate_per_link = 0.0;
	uint32_t has_win = 1;
	uint32_t global_t = 1;
	uint32_t mi_thresh = 5;
	bool var_win = false, fast_react = true;
	bool multi_rate = true;
	bool sample_feedback = false;
	double pint_log_base = 1.05;
	double pint_prob = 1.0;
	double u_target = 0.95;
	uint32_t int_multi = 1;
	bool rate_bound = true;

	uint32_t ack_high_prio = 0;

	struct LinkDown {
		uint32_t src = 0;
		uint32_t dst = 0;
		uint64_t time = 0;

		NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(LinkDown, src, dst, time);
	};

	LinkDown link_down;
	uint32_t enable_trace = 1;
	uint32_t buffer_size = 16;
	uint32_t qlen_dump_interval = 100000000, qlen_mon_interval = 1e9;
	uint64_t qlen_mon_start = 2000000000, qlen_mon_end = 2100000000;
	std::string qlen_mon_file;

	template<typename T>
	struct BandwidthToECNThreshold
	{
		uint64_t bandwidth = 0; // Bps
		T ecn_threshold = T(0);

		NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(BandwidthToECNThreshold, bandwidth, ecn_threshold);
	};

	// `kmin_map` says for which egress minimal buffer size we should mark the "Congestion Experienced" bit.
	// the probability is 100% when > `kmax_map`, pmax% when == `kmax_map`, and 0% when == `kmin_map`, and increases linearly between `kmin_map` and `kmax_map`..
	std::vector<BandwidthToECNThreshold<uint32_t>> kmax_map, kmin_map;
	std::vector<BandwidthToECNThreshold<double>> pmax_map;

	uint32_t rng_seed = 50; // Keep default value before feature was added if not in config

	template<typename T>
	static T find_in_map(const std::vector<BandwidthToECNThreshold<T>>& map, uint64_t bps)
	{
		for(auto& entry : map) {
			if(entry.bandwidth == bps) {
				return entry.ecn_threshold;
			}
		}
		throw std::runtime_error("Cannot find in map");
	}

	NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(SimConfig,
		enable_pfc, enable_qcn, use_dynamic_pfc_threshold, packet_payload_size,
		topology_file, flow_file, groups_file, trace_file, trace_output_file, fct_output_file, pfc_output_file,
		simulator_stop_time, cc_mode, alpha_resume_interval, rate_decrease_interval, clamp_target_rate,
		rp_timer, ewma_gain, fast_recovery_times, rate_ai, rate_hai, min_rate, dctcp_rate_ai, error_rate_per_link,
		l2_chunk_size, l2_ack_interval, l2_back_to_zero, nak_interval, has_win, global_t, var_win, fast_react,
		u_target, mi_thresh, int_multi, multi_rate, sample_feedback, pint_log_base, pint_prob, rate_bound, ack_high_prio, link_down, enable_trace,
		kmax_map, kmin_map, pmax_map, buffer_size, qlen_mon_file, qlen_mon_start, qlen_mon_end, rng_seed);
};

class FlowsConfig
{
private:
	using FlowsMap = std::multimap<double, ScheduledFlow>;

public:
	struct Json
	{
		std::vector<ScheduledFlow> flows;
		NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Json, flows);
	};

	FlowsConfig(const FlowsConfig&) = delete;
	FlowsConfig& operator=(const FlowsConfig&) = delete;
	
	FlowsConfig(FlowsConfig&& other)
	{
		*this = std::move(other);
	}
	
	FlowsConfig& operator=(FlowsConfig&& other)
	{
		// Swapping a map, iterator is invalidated only if iterator is end
		if(other.Finished())
		{
			std::swap(m_flows, other.m_flows);
			m_it = m_flows.end();
		}
		else
		{
			std::swap(m_flows, other.m_flows);
			std::swap(m_it, other.m_it);
		}

		return *this;
	}
	
	FlowsConfig() = default;

	FlowsConfig(const Json& j) {
		for(const ScheduledFlow& flow : j.flows) {
			AddFlow(flow);
		}
		m_it = m_flows.begin();
	}
	
	/**
	 * @brief Get the next flow in ascending time.
	 */
	bool GetNextFlow(ScheduledFlow& flow)
	{
		if(Finished()) {
			return false;
		}

		flow = m_it->second;
		m_it++;
		return true;
	}

	bool Finished() const
	{
		return m_it == m_flows.end();
	}

private:
	void AddFlow(const ScheduledFlow& flow) {
		m_flows.emplace(flow.start_time, flow);
	}
	
private:
	FlowsMap m_flows;
	FlowsMap::const_iterator m_it = m_flows.end();
};

struct TopoConfig {
	struct Node {
		struct Pos {
			double x = 0.0;
			double y = 0.0;
			NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Pos, x, y);
		};

		uint32_t id = 0;
		bool is_switch = false;
		Pos pos;
		NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Node, id, is_switch, pos);
	};
	struct Link {
		uint32_t src = 0;      // First node ID
		uint32_t dst = 0;      // Second node ID
		double bandwidth = 100e9;  // Unit: bps
		double latency = 1e-6;    // Unit: seconds
		double error_rate = 0.0; // Unit: percentage in [0;1]
		NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Link, src, dst, bandwidth, latency, error_rate);
	};

	std::vector<Node> nodes;
	std::vector<Link> links;
	NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(TopoConfig, nodes, links);
};

SimConfig simConfig;

/************************************************
 * Runtime varibles
 ***********************************************/

FlowsConfig flowsConfig;
NodeContainer n;

uint64_t maxRtt, maxBdp;

struct Interface{
	uint32_t idx;
	bool up;
	uint64_t delay;
	uint64_t bw;

	Interface() : idx(0), up(false){}
};
std::map<Ptr<Node>, std::map<Ptr<Node>, Interface> > nbr2if;
// Mapping destination to next hop for each node: <node, <dest, <nexthop0, ...> > >
std::map<Ptr<Node>, std::map<Ptr<Node>, std::vector<Ptr<Node> > > > nextHop;
std::map<Ptr<Node>, std::map<Ptr<Node>, uint64_t> > pairDelay;
std::map<Ptr<Node>, std::map<Ptr<Node>, uint64_t> > pairTxDelay;
std::map<uint32_t, std::map<uint32_t, uint64_t> > pairBw;
std::map<Ptr<Node>, std::map<Ptr<Node>, uint64_t> > pairBdp;
std::map<uint32_t, std::map<uint32_t, uint64_t> > pairRtt;

std::vector<Ipv4Address> serverAddress;

// maintain port number for each host pair
std::unordered_map<uint32_t, std::unordered_map<uint32_t, uint16_t> > portNumder;

struct MCastConfig
{
	MCastConfig() = default;
	MCastConfig(const MCastConfig&) = delete;
	MCastConfig(MCastConfig&&) = default;
	MCastConfig& operator=(const MCastConfig&) = delete;
	MCastConfig& operator=(MCastConfig&&) = default;
	
	struct Group
	{
		uint32_t id;
		std::set<uint32_t> nodes;
		NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Group, id, nodes);
	};
	std::vector<Group> groups;
	NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(MCastConfig, groups);

	void ParseFromFile(const std::string& path)
	{
		std::ifstream file(path);
		json j = json::parse(file);
		WarnInvalidKeys<MCastConfig>(j);
		*this = j;
	}
};

class MCastNetwork {
private:
	using Group = std::set<uint32_t>;

public:
	MCastNetwork() = default;
	MCastNetwork(const MCastNetwork&) = delete;
	MCastNetwork& operator=(const MCastNetwork&) = delete;

	explicit MCastNetwork(const MCastConfig& config)
	{
		for(const MCastConfig::Group& group : config.groups) {
			for(uint32_t node_id : group.nodes) {
				AddToGroup(group.id, node_id);
			}
		}
	}

	void AddToGroup(uint32_t group_id, uint32_t node_id)
	{
		m_groups[group_id].insert(node_id);
	}

	bool BelongsToGroup(uint32_t group_id, uint32_t node_id) const
	{
		auto it = m_groups.find(group_id);
		if(it == m_groups.end()) {
			return false;
		}
		const Group& group = it->second;
		return group.count(node_id) > 0;
	}

private:
	std::unordered_map<uint32_t, Group> m_groups;
};

void RunFlow(ScheduledFlow flow)
{
	constexpr uint32_t first_src_port = 10000; // each host pair use port number from 10000
	const uint32_t src_port = first_src_port + (portNumder[flow.src][flow.dst]++); // get a new port number
	Ipv4Address sip = serverAddress[flow.src];
	Ipv4Address dip = serverAddress[flow.dst];
	uint32_t win;
	uint64_t baseRtt;

	if(simConfig.global_t == 1) {
		baseRtt = maxRtt;
	}
	else {
		baseRtt = pairRtt[flow.src][flow.dst];
	}

	if(simConfig.has_win) {
		if(simConfig.global_t == 1) {
			win = maxBdp;
		}
		else {
			win = pairBdp[n.Get(flow.src)][n.Get(flow.dst)];
		}
	}
	else {
		win = 0;
	}

	if(flow.multicast) {
		// We use IP as an index for the multicsat group (simpler than using IGMP multicast address)
		dip = Ipv4Address(flow.dst);
	}

	
	std::cout << "Running flow " << Simulator::Now() << json(flow) << std::endl;

	if(!flow.output_file.empty()) {
		// Make relative to config directory
		flow.output_file = (config_dir / flow.output_file);
	}

	if(flow.type == FlowType::Flow)
	{
		static uint16_t unique_port = 10;

		RdmaUnicastAppHelper app_helper;
		app_helper.SetAttribute("SrcNode", UintegerValue(flow.src));
		app_helper.SetAttribute("DstNode", UintegerValue(flow.dst));
		app_helper.SetAttribute("SrcPort", UintegerValue(unique_port++));
		app_helper.SetAttribute("DstPort", UintegerValue(unique_port++));
		app_helper.SetAttribute("WriteSize", UintegerValue(flow.size));
		app_helper.SetAttribute("PriorityGroup", UintegerValue(flow.priority));
		app_helper.SetAttribute("Window", UintegerValue(win));
		app_helper.SetAttribute("Mtu", UintegerValue(1500));
		app_helper.SetAttribute("Multicast", BooleanValue(flow.multicast));
		app_helper.SetAttribute("RateFactor", DoubleValue(flow.bandwidth_percent));
	
		ApplicationContainer app_con = app_helper.Install(n);
		app_con.Start(Time(0));
	
		return;
	}
	else if(flow.type == FlowType::Allgather) {
		RdmaClientHelper clientHelper(flow, win);
		ApplicationContainer appCon = clientHelper.Install(n);
		appCon.Start(Time(0));
	}
	else {
		NS_ABORT_MSG("Unknown flow type");
	}
}

void ScheduleFlowInputs()
{
	ScheduledFlow flow;
	while(flowsConfig.GetNextFlow(flow)) {
		
		const Time when = Seconds(flow.start_time) - Simulator::Now();

		if(flow.type == FlowType::Flow) {
			NS_ABORT_IF(IsSwitchNode(n.Get(flow.src)));
			
			if(!flow.multicast) {
				NS_ABORT_IF(IsSwitchNode(n.Get(flow.dst)));
			}
		}

		Simulator::Schedule(when, RunFlow, flow);
	}
}

Ipv4Address node_id_to_ip(uint32_t id){
	return Ipv4Address(0x0b000001 + ((id / 256) * 0x00010000) + ((id % 256) * 0x00000100));
}

uint32_t ip_to_node_id(Ipv4Address ip){
	return (ip.Get() >> 8) & 0xffff;
}

void qp_finish(FILE* fout, const SimConfig* simConfig, Ptr<RdmaTxQueuePair> q)
{
#if RAF_WAITS_REFACTORING
	if(q->dip.Get() < 1000) { // Quick dirty test to check if it is multicast address
		return; // Stop since the `ip_to_node_id()` doesn't work with multicast
	}

	const uint32_t sid = ip_to_node_id(q->sip);
	const uint32_t did = ip_to_node_id(q->dip);
	
	uint64_t base_rtt = pairRtt[sid][did], b = pairBw[sid][did];
	uint32_t total_bytes = q->m_size + ((q->m_size-1) / simConfig->packet_payload_size + 1) * (CustomHeader::GetStaticWholeHeaderSize() - IntHeader::GetStaticSize()); // translate to the minimum bytes required (with header but no INT)
	uint64_t standalone_fct = base_rtt + total_bytes * 8000000000lu / b;
	// sip, dip, sport, dport, size (B), start_time, fct (ns), standalone_fct (ns)
	fprintf(fout, "%08x %08x %u %u %lu %lu %lu %lu\n", q->sip.Get(), q->dip.Get(), q->sport, q->dport, q->m_size, q->startTime.GetTimeStep(), (Simulator::Now() - q->startTime).GetTimeStep(), standalone_fct);
	fflush(fout);

	// remove rxQp from the receiver
	Ptr<Node> dstNode = n.Get(did);
	Ptr<RdmaHw> rdma = dstNode->GetObject<RdmaHw> ();
	rdma->DeleteRxQp(q->sip.Get(), q->m_pg, q->sport);
#endif
}

void get_pfc(FILE* fout, Ptr<QbbNetDevice> dev, uint32_t type){
	fprintf(fout, "%lu %u %u %u %u\n", Simulator::Now().GetTimeStep(), dev->GetNode()->GetId(), GetNodeType(dev->GetNode()), dev->GetIfIndex(), type);
}

struct QlenDistribution{
	std::vector<uint32_t> cnt; // cnt[i] is the number of times that the queue len is i KB

	void add(uint32_t qlen){
		uint32_t kb = qlen / 1000;
		if (cnt.size() < kb+1)
			cnt.resize(kb+1);
		cnt[kb]++;
	}
};
std::map<uint32_t, std::map<uint32_t, QlenDistribution> > queue_result;
void monitor_buffer(FILE* qlen_output, const SimConfig* simConfig, NodeContainer *n){
	for (uint32_t i = 0; i < n->GetN(); i++){
		if (IsSwitchNode(n->Get(i))){ // is switch
			Ptr<SwitchNode> sw = DynamicCast<SwitchNode>(n->Get(i));
			if (queue_result.find(i) == queue_result.end())
				queue_result[i];
			for (uint32_t j = 1; j < sw->GetNDevices(); j++){
				uint32_t size = 0;
				for (uint32_t k = 0; k < SwitchMmu::qCnt; k++)
					size += sw->m_mmu->egress_bytes[j][k];
				queue_result[i][j].add(size);
			}
		}
	}
	if (Simulator::Now().GetTimeStep() % simConfig->qlen_dump_interval == 0){
		fprintf(qlen_output, "time: %lu\n", Simulator::Now().GetTimeStep());
		for (auto &it0 : queue_result)
			for (auto &it1 : it0.second){
				fprintf(qlen_output, "%u %u", it0.first, it1.first);
				auto &dist = it1.second.cnt;
				for (uint32_t i = 0; i < dist.size(); i++)
					fprintf(qlen_output, " %u", dist[i]);
				fprintf(qlen_output, "\n");
			}
		fflush(qlen_output);
	}
	if (Simulator::Now().GetTimeStep() < simConfig->qlen_mon_end)
		Simulator::Schedule(NanoSeconds(simConfig->qlen_mon_interval), &monitor_buffer, qlen_output, simConfig, n);
}

void CalculateRoute(Ptr<Node> host){
	// queue for the BFS.
	std::vector<Ptr<Node> > q;
	// Distance from the host to each node.
	std::map<Ptr<Node>, int> dis;
	std::map<Ptr<Node>, uint64_t> delay;
	std::map<Ptr<Node>, uint64_t> txDelay;
	std::map<Ptr<Node>, uint64_t> bw;
	// init BFS.
	q.push_back(host);
	dis[host] = 0;
	delay[host] = 0;
	txDelay[host] = 0;
	bw[host] = 0xfffffffffffffffflu;
	// BFS.
	for (int i = 0; i < (int)q.size(); i++){
		Ptr<Node> now = q[i];
		int d = dis[now];
		for (auto it = nbr2if[now].begin(); it != nbr2if[now].end(); it++){
			// skip down link
			if (!it->second.up)
				continue;
			Ptr<Node> next = it->first;
			// If 'next' have not been visited.
			if (dis.find(next) == dis.end()){
				dis[next] = d + 1;
				delay[next] = delay[now] + it->second.delay;
				txDelay[next] = txDelay[now] + simConfig.packet_payload_size * 1000000000lu * 8 / it->second.bw;
				bw[next] = std::min(bw[now], it->second.bw);
				// we only enqueue switch, because we do not want packets to go through host as middle point
				if (IsSwitchNode(next))
					q.push_back(next);
			}
			// if 'now' is on the shortest path from 'next' to 'host'.
			if (d + 1 == dis[next]){
				nextHop[next][host].push_back(now);
			}
		}
	}
	for (auto it : delay)
		pairDelay[it.first][host] = it.second;
	for (auto it : txDelay)
		pairTxDelay[it.first][host] = it.second;
	for (auto it : bw)
		pairBw[it.first->GetId()][host->GetId()] = it.second;
}

void CalculateRoutes(NodeContainer &n){
	for (int i = 0; i < (int)n.GetN(); i++){
		Ptr<Node> node = n.Get(i);
		if (!IsSwitchNode(node))
			CalculateRoute(node);
	}
}

void SetRoutingEntries(){
	// For each node.
	for (auto i = nextHop.begin(); i != nextHop.end(); i++){
		Ptr<Node> node = i->first;
		auto &table = i->second;
		for (auto j = table.begin(); j != table.end(); j++){
			// The destination node.
			Ptr<Node> dst = j->first;
			// The IP address of the dst.
			Ipv4Address dstAddr = dst->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal();
			// The next hops towards the dst.
			std::vector<Ptr<Node> > nexts = j->second;
			for (int k = 0; k < (int)nexts.size(); k++){
				Ptr<Node> next = nexts[k];
				uint32_t interface = nbr2if[node][next].idx;
				if (IsSwitchNode(node))
					DynamicCast<SwitchNode>(node)->AddTableEntry(dstAddr, interface);
				else{
					node->GetObject<RdmaHw>()->AddTableEntry(dstAddr, interface);
				}
			}
		}
	}
}

// take down the link between a and b, and redo the routing
void TakeDownLink(NodeContainer n, Ptr<Node> a, Ptr<Node> b){
	if (!nbr2if[a][b].up)
		return;
	// take down link between a and b
	nbr2if[a][b].up = nbr2if[b][a].up = false;
	nextHop.clear();
	CalculateRoutes(n);
	// clear routing tables
	for (uint32_t i = 0; i < n.GetN(); i++){
		if (IsSwitchNode(n.Get(i)))
			DynamicCast<SwitchNode>(n.Get(i))->ClearTable();
		else
			n.Get(i)->GetObject<RdmaHw>()->ClearTable();
	}
	DynamicCast<QbbNetDevice>(a->GetDevice(nbr2if[a][b].idx))->TakeDown();
	DynamicCast<QbbNetDevice>(b->GetDevice(nbr2if[b][a].idx))->TakeDown();
	// reset routing table
	SetRoutingEntries();

	// redistribute qp on each host
	for (uint32_t i = 0; i < n.GetN(); i++){
		if (!IsSwitchNode(n.Get(i)))
			n.Get(i)->GetObject<RdmaHw>()->RedistributeQp();
	}
}

uint64_t get_nic_rate(NodeContainer &n){
	for (uint32_t i = 0; i < n.GetN(); i++)
		if (!IsSwitchNode(n.Get(i)))
			return DynamicCast<QbbNetDevice>(n.Get(i)->GetDevice(1))->GetDataRate().GetBitRate();
	NS_ABORT_MSG("Cannot find NIC");
}

int main(int argc, char *argv[])
{
	if(argc < 2) {
		std::cout << "Error: require a config file" << std::endl;
		return 1;
	}

	{
		const json j = json::parse(std::ifstream(argv[1]));
		from_json(j, simConfig);
		std::cout << "Config: " << json(simConfig).dump(4) << std::endl;
		WarnInvalidKeys<SimConfig>(j);
	}

	config_dir = fs::path{argv[1]}.parent_path();

	Config::SetDefault("ns3::QbbNetDevice::PauseTime", UintegerValue(simConfig.pause_time));
	Config::SetDefault("ns3::QbbNetDevice::QbbEnabled", BooleanValue(simConfig.enable_pfc));
	Config::SetDefault("ns3::QbbNetDevice::DynamicThreshold", BooleanValue(simConfig.use_dynamic_pfc_threshold));
	
	// set int_multi
	IntHop::multi = simConfig.int_multi;
	// IntHeader::mode
	if (simConfig.cc_mode == 7) // timely, use ts
		IntHeader::mode = IntHeader::TS;
	else if (simConfig.cc_mode == 3) // hpcc, use int
		IntHeader::mode = IntHeader::NORMAL;
	else if (simConfig.cc_mode == 10) // hpcc-pint
		IntHeader::mode = IntHeader::PINT;
	else // others, no extra header
		IntHeader::mode = IntHeader::NONE;

	// Set Pint
	if (simConfig.cc_mode == 10){
		Pint::set_log_base(simConfig.pint_log_base);
		IntHeader::pint_bytes = Pint::get_n_bytes();
		printf("PINT bits: %d bytes: %d\n", Pint::get_n_bits(), Pint::get_n_bytes());
	}

	//SeedManager::SetSeed(time(NULL));

	const TopoConfig topo = json::parse(std::ifstream((config_dir / simConfig.topology_file).c_str()));
	std::ifstream tracef;

	{
		const json j = json::parse(std::ifstream((config_dir / simConfig.flow_file).c_str()));
		flowsConfig = FlowsConfig::Json(j);
		flowsConfig.Finished();
	}
	
	tracef.open((config_dir / simConfig.trace_file).c_str());
	NS_ABORT_MSG_IF(!tracef, "Cannot open input trace file");

	uint32_t trace_num;
	tracef >> trace_num;

	std::vector<uint32_t> node_type(topo.nodes.size(), 0);
	const uint32_t node_num = topo.nodes.size();
  	
	for (uint32_t i = 0; i < node_num; i++){
		const TopoConfig::Node& node_config = topo.nodes[i];

		if (!node_config.is_switch) {
			n.Add(CreateObject<Node>());
		}
		else {
			Ptr<SwitchNode> sw = CreateObject<SwitchNode>();
			n.Add(sw);
			sw->SetAttribute("EcnEnabled", BooleanValue(simConfig.enable_qcn));
		}
	}

	NS_LOG_INFO("Create nodes.");

	InternetStackHelper internet;
	internet.Install(n);

	//
	// Assign IP to each server
	//
	for (uint32_t i = 0; i < node_num; i++){
		if (!IsSwitchNode(n.Get(i))){ // is server
			serverAddress.resize(i + 1);
			serverAddress[i] = node_id_to_ip(i);
		}
	}

	NS_LOG_INFO("Create channels.");

	//
	// Explicitly create the channels required by the topology.
	//

	if(simConfig.rng_seed == 0) {
		FILE* urandom = fopen("/dev/urandom", "r");
		NS_ABORT_MSG_IF(!urandom, "Cannot open /dev/urandom");
		NS_ABORT_IF(fread(&simConfig.rng_seed, sizeof(simConfig.rng_seed), 1, urandom) != sizeof(simConfig.rng_seed));
		if(!fclose(urandom)) {
			std::cerr << "ERROR: Closing /dev/urandom failed" << std::endl;
		}
	}
	std::cout << "Effective RNG seed: " << simConfig.rng_seed << std::endl;
	Ptr<RateErrorModel> rem = CreateObject<RateErrorModel>();
	Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable>();
	rem->SetRandomVariable(uv);
	uv->SetStream(simConfig.rng_seed);
	rem->SetAttribute("ErrorRate", DoubleValue(simConfig.error_rate_per_link));
	rem->SetAttribute("ErrorUnit", StringValue("ERROR_UNIT_PACKET"));

	FILE *pfc_file = fopen((config_dir / simConfig.pfc_output_file).c_str(), "w");

	QbbHelper qbb;
	Ipv4AddressHelper ipv4;
	for (uint32_t i = 0; i < topo.links.size(); i++)
	{
		const TopoConfig::Link& link = topo.links[i];
		const uint32_t src = link.src;
		const uint32_t dst = link.dst;
		Ptr<Node> snode = n.Get(link.src);
		Ptr<Node> dnode = n.Get(link.dst);

		qbb.SetDeviceAttribute("DataRate", DataRateValue(DataRate(link.bandwidth)));
		qbb.SetChannelAttribute("Delay", TimeValue(Seconds(link.latency)));

		if (link.error_rate > 0)
		{
			Ptr<RateErrorModel> rem = CreateObject<RateErrorModel>();
			Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable>();
			rem->SetRandomVariable(uv);
			uv->SetStream(50);
			rem->SetAttribute("ErrorRate", DoubleValue(link.error_rate));
			rem->SetAttribute("ErrorUnit", StringValue("ERROR_UNIT_PACKET"));
			qbb.SetDeviceAttribute("ReceiveErrorModel", PointerValue(rem));
		}
		else
		{
			qbb.SetDeviceAttribute("ReceiveErrorModel", PointerValue(rem));
		}

		// Assigne server IP
		// Note: this should be before the automatic assignment below (ipv4.Assign(d)),
		// because we want our IP to be the primary IP (first in the IP address list),
		// so that the global routing is based on our IP
		NetDeviceContainer d = qbb.Install(snode, dnode);
		if (!IsSwitchNode(snode)){
			Ptr<Ipv4> ipv4 = snode->GetObject<Ipv4>();
			ipv4->AddInterface(d.Get(0));
			ipv4->AddAddress(1, Ipv4InterfaceAddress(serverAddress[src], Ipv4Mask(0xff000000)));
		}
		if (!IsSwitchNode(dnode)){
			Ptr<Ipv4> ipv4 = dnode->GetObject<Ipv4>();
			ipv4->AddInterface(d.Get(1));
			ipv4->AddAddress(1, Ipv4InterfaceAddress(serverAddress[dst], Ipv4Mask(0xff000000)));
		}

		// used to create a graph of the topology
		nbr2if[snode][dnode].idx = DynamicCast<QbbNetDevice>(d.Get(0))->GetIfIndex();
		nbr2if[snode][dnode].up = true;
		nbr2if[snode][dnode].delay = DynamicCast<QbbChannel>(DynamicCast<QbbNetDevice>(d.Get(0))->GetChannel())->GetDelay().GetTimeStep();
		nbr2if[snode][dnode].bw = DynamicCast<QbbNetDevice>(d.Get(0))->GetDataRate().GetBitRate();
		nbr2if[dnode][snode].idx = DynamicCast<QbbNetDevice>(d.Get(1))->GetIfIndex();
		nbr2if[dnode][snode].up = true;
		nbr2if[dnode][snode].delay = DynamicCast<QbbChannel>(DynamicCast<QbbNetDevice>(d.Get(1))->GetChannel())->GetDelay().GetTimeStep();
		nbr2if[dnode][snode].bw = DynamicCast<QbbNetDevice>(d.Get(1))->GetDataRate().GetBitRate();

		// This is just to set up the connectivity between nodes. The IP addresses are useless
		char ipstring[32];
		snprintf(ipstring, sizeof(ipstring), "10.%d.%d.0", i / 254 + 1, i % 254 + 1);
		ipv4.SetBase(ipstring, "255.255.255.0");
		ipv4.Assign(d);

		// setup PFC trace
		DynamicCast<QbbNetDevice>(d.Get(0))->TraceConnectWithoutContext("QbbPfc", MakeBoundCallback (&get_pfc, pfc_file, DynamicCast<QbbNetDevice>(d.Get(0))));
		DynamicCast<QbbNetDevice>(d.Get(1))->TraceConnectWithoutContext("QbbPfc", MakeBoundCallback (&get_pfc, pfc_file, DynamicCast<QbbNetDevice>(d.Get(1))));
	}

	const uint64_t nic_rate = get_nic_rate(n);

	// config switch
	for (uint32_t i = 0; i < node_num; i++){
		if (IsSwitchNode(n.Get(i))){ // is switch
			Ptr<SwitchNode> sw = DynamicCast<SwitchNode>(n.Get(i));
			uint32_t shift = 3; // by default 1/8
			for (uint32_t j = 1; j < sw->GetNDevices(); j++){
				Ptr<QbbNetDevice> dev = DynamicCast<QbbNetDevice>(sw->GetDevice(j));
				// set ecn
				uint64_t rate = dev->GetDataRate().GetBitRate();
				sw->m_mmu->ConfigEcn(j, SimConfig::find_in_map(simConfig.kmin_map, rate),
										SimConfig::find_in_map(simConfig.kmax_map, rate),
										SimConfig::find_in_map(simConfig.pmax_map, rate));
				// set pfc
				uint64_t delay = DynamicCast<QbbChannel>(dev->GetChannel())->GetDelay().GetTimeStep();
				uint32_t headroom = rate * delay / 8 / 1000000000 * 3;
				sw->m_mmu->ConfigHdrm(j, headroom);

				// set pfc alpha, proportional to link bw
				sw->m_mmu->pfc_a_shift[j] = shift;
				while (rate > nic_rate && sw->m_mmu->pfc_a_shift[j] > 0){
					sw->m_mmu->pfc_a_shift[j]--;
					rate /= 2;
				}
			}
			sw->m_mmu->ConfigNPort(sw->GetNDevices()-1);
			sw->m_mmu->ConfigBufferSize(simConfig.buffer_size* 1024 * 1024);
			sw->m_mmu->node_id = sw->GetId();
		}
	}

	#if ENABLE_QP
	FILE *fct_output = fopen((config_dir / simConfig.fct_output_file).c_str(), "w");
	//
	// install RDMA driver
	//
	for (uint32_t i = 0; i < node_num; i++){
		if (!IsSwitchNode(n.Get(i))){ // is server
			// create RdmaHw
			Ptr<RdmaHw> rdmaHw = CreateObject<RdmaHw>();
			rdmaHw->SetAttribute("ClampTargetRate", BooleanValue(simConfig.clamp_target_rate));
			rdmaHw->SetAttribute("AlphaResumInterval", DoubleValue(simConfig.alpha_resume_interval));
			rdmaHw->SetAttribute("RPTimer", DoubleValue(simConfig.rp_timer));
			rdmaHw->SetAttribute("FastRecoveryTimes", UintegerValue(simConfig.fast_recovery_times));
			rdmaHw->SetAttribute("EwmaGain", DoubleValue(simConfig.ewma_gain));
			rdmaHw->SetAttribute("RateAI", DataRateValue(DataRate(simConfig.rate_ai)));
			rdmaHw->SetAttribute("RateHAI", DataRateValue(DataRate(simConfig.rate_hai)));
			rdmaHw->SetAttribute("L2BackToZero", BooleanValue(simConfig.l2_back_to_zero));
			rdmaHw->SetAttribute("L2ChunkSize", UintegerValue(simConfig.l2_chunk_size));
			rdmaHw->SetAttribute("L2AckInterval", UintegerValue(simConfig.l2_ack_interval));
			rdmaHw->SetAttribute("NackInterval", DoubleValue(simConfig.nak_interval));
			rdmaHw->SetAttribute("CcMode", UintegerValue(simConfig.cc_mode));
			rdmaHw->SetAttribute("RateDecreaseInterval", DoubleValue(simConfig.rate_decrease_interval));
			rdmaHw->SetAttribute("MinRate", DataRateValue(DataRate(simConfig.min_rate)));
			rdmaHw->SetAttribute("Mtu", UintegerValue(simConfig.packet_payload_size));
			
			#if RAF_WAITS_REFACTORING
			rdmaHw->SetAttribute("MiThresh", UintegerValue(simConfig.mi_thresh));
			#endif

			rdmaHw->SetAttribute("VarWin", BooleanValue(simConfig.var_win));
			rdmaHw->SetAttribute("FastReact", BooleanValue(simConfig.fast_react));
			
			#if RAF_WAITS_REFACTORING
			rdmaHw->SetAttribute("MultiRate", BooleanValue(simConfig.multi_rate));
			rdmaHw->SetAttribute("SampleFeedback", BooleanValue(simConfig.sample_feedback));
			rdmaHw->SetAttribute("TargetUtil", DoubleValue(simConfig.u_target));
			#endif

			rdmaHw->SetAttribute("RateBound", BooleanValue(simConfig.rate_bound));
			
			
			#if RAF_WAITS_REFACTORING
			rdmaHw->SetAttribute("DctcpRateAI", DataRateValue(DataRate(simConfig.dctcp_rate_ai)));
			rdmaHw->SetAttribute("PintSmplThresh", UintegerValue(simConfig.pint_prob * 65536));
			#endif
			
			// create and install RdmaDriver
			// Ptr<RdmaDriver> rdma = CreateObject<RdmaDriver>();
			// Ptr<Node> node = n.Get(i);
			// rdma->SetNode(node);
			// rdma->SetRdmaHw(rdmaHw);

			Ptr<Node> node = n.Get(i);
			node->AggregateObject (rdmaHw);
			rdmaHw->Setup();
			rdmaHw->TraceConnectWithoutContext("QpComplete", MakeBoundCallback (qp_finish, fct_output, &simConfig));
		}
	}
	#endif

	// set ACK priority on hosts
	if (simConfig.ack_high_prio)
		RdmaEgressQueue::ack_q_idx = 0;
	else
		RdmaEgressQueue::ack_q_idx = 3;

	// setup routing
	CalculateRoutes(n);
	SetRoutingEntries();

	//
	// get BDP and delay
	//
	maxRtt = maxBdp = 0;
	for (uint32_t i = 0; i < node_num; i++){
		if (IsSwitchNode(n.Get(i)))
			continue;
		for (uint32_t j = 0; j < node_num; j++){
			if (IsSwitchNode(n.Get(j)))
				continue;
			uint64_t delay = pairDelay[n.Get(i)][n.Get(j)];
			uint64_t txDelay = pairTxDelay[n.Get(i)][n.Get(j)];
			uint64_t rtt = delay * 2 + txDelay;
			uint64_t bw = pairBw[i][j];
			uint64_t bdp = rtt * bw / 1000000000/8; 
			pairBdp[n.Get(i)][n.Get(j)] = bdp;
			pairRtt[i][j] = rtt;
			if (bdp > maxBdp)
				maxBdp = bdp;
			if (rtt > maxRtt)
				maxRtt = rtt;
		}
	}
	printf("maxRtt=%lu maxBdp=%lu\n", maxRtt, maxBdp);

	//
	// setup switch CC
	//
	for (uint32_t i = 0; i < node_num; i++){
		if (IsSwitchNode(n.Get(i))){ // switch
			Ptr<SwitchNode> sw = DynamicCast<SwitchNode>(n.Get(i));
			sw->SetAttribute("CcMode", UintegerValue(simConfig.cc_mode));
			sw->SetAttribute("MaxRtt", UintegerValue(maxRtt));
		}
	}

	//
	// add trace
	//

	NodeContainer trace_nodes;
	for (uint32_t i = 0; i < trace_num; i++)
	{
		uint32_t nid;
		tracef >> nid;
		if (nid >= n.GetN()){
			continue;
		}
		trace_nodes = NodeContainer(trace_nodes, n.Get(nid));
	}

	FILE *trace_output = fopen((config_dir / simConfig.trace_output_file).c_str(), "w");
	NS_ABORT_MSG_IF(!trace_output, "Cannot open output trace file");
	if (simConfig.enable_trace)
		qbb.EnableTracing(trace_output, trace_nodes);

	// dump link speed to trace file
	{
		SimSetting sim_setting;
		for (auto i: nbr2if){
			for (auto j : i.second){
				uint16_t node = i.first->GetId();
				uint8_t intf = j.second.idx;
				uint64_t bps = DynamicCast<QbbNetDevice>(i.first->GetDevice(j.second.idx))->GetDataRate().GetBitRate();
				sim_setting.port_speed[node][intf] = bps;
			}
		}
		sim_setting.win = maxBdp;
		sim_setting.Serialize(trace_output);
	}

	Ipv4GlobalRoutingHelper::PopulateRoutingTables();

	{
		MCastConfig mcastConfig;
		mcastConfig.ParseFromFile((config_dir / simConfig.groups_file).string());
		MCastNetwork mcastNet(mcastConfig);

		for(const MCastConfig::Group& group : mcastConfig.groups) {
			for(uint32_t node_id : group.nodes) {
				// To make simple, don't support ECMP
				// Assume the node has only one NIC and one link
				Ptr<Node> node = n.Get(node_id);
				NS_ASSERT(!IsSwitchNode(node));
				
				const int nic_iface = 1;
				Ptr<QbbNetDevice> qbb = DynamicCast<QbbNetDevice>(node->GetDevice(nic_iface));
				qbb->AddGroup(group.id);

				const Ipv4Address mcast_addr(group.id);
				node->GetObject<RdmaHw>()->AddTableEntry(mcast_addr, nic_iface);
			}
		}
	}

	NS_LOG_INFO("Create Applications.");

	Time interPacketInterval = Seconds(0.0000005 / 2);

	ScheduleFlowInputs();

	tracef.close();

	// schedule link down
	if (simConfig.link_down.time > 0){
		Simulator::Schedule(Seconds(2) + MicroSeconds(simConfig.link_down.time), &TakeDownLink, n, n.Get(simConfig.link_down.src), n.Get(simConfig.link_down.dst));
	}

	// schedule buffer monitor
	FILE* qlen_output = fopen((config_dir / simConfig.qlen_mon_file).c_str(), "w");
	Simulator::Schedule(NanoSeconds(simConfig.qlen_mon_start), &monitor_buffer, qlen_output, &simConfig, &n);

	std::unique_ptr<AnimationInterface> anim;
	if(simConfig.enable_trace)
	{
		for (uint32_t i = 0; i < node_num; i++){
			const TopoConfig::Node& node_config = topo.nodes[i];
			Ptr<Node> node = n.Get(i);
			Vector3D p = {};
			p.x = node_config.pos.x;
			p.y = node_config.pos.y;
			AnimationInterface::SetConstantPosition(node, p.x, p.y, p.z);
		}

		// Save trace for animation
		anim.reset(new AnimationInterface{"anim.xml"});
		// High polling interval; the nodes never move. Reduces XML size.
		anim->SetMobilityPollInterval(Seconds(1e9));
		anim->EnablePacketMetadata(true);
	}

	// Now, do the actual simulation.
	std::cout << "Running Simulation.\n";
	fflush(stdout);
	NS_LOG_INFO("Run Simulation.");
	Simulator::Stop(Seconds(simConfig.simulator_stop_time));
	Simulator::Run();
	Simulator::Destroy();
	fclose(trace_output);
}
