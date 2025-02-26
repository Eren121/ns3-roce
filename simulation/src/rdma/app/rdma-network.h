#pragma once

#include "ns3/rdma-qlen-monitor.h"
#include "ns3/qbb-helper.h"
#include "ns3/switch-node.h"
#include "ns3/node-container.h"
#include "ns3/json.h"
#include "ns3/filesystem.h"
#include "ns3/animation-interface.h"
#include "ns3/rdma-reflection-helper.h"
#include "ns3/filesystem.h"
#include <map>
#include <vector>
#include <cstdint>
#include <memory>
#include <vector>

namespace ns3 {

struct PfcEntry
{
  double time;
  int node;
  int dev;
  bool paused;
};

struct BandwidthToEcnThreshold
{
	double bandwidth{0}; // Bps
	double ecn_threshold{0}; // Buffer size in KB
};

double FindEcnThreshold(const std::vector<BandwidthToEcnThreshold>& map, double bps);

/**
 * @brief Parameters for the configuration.
 */
struct RdmaConfig
{
  rfl::Skip<fs::path> config_dir;
	rfl::Generic::Object defaults; //!< Stores ns3 default attributes
	
	Time simulator_stop_time;

	QLenMonitor::Config qlen_monitor;

	fs::path topology_file;
	fs::path flow_file;
	fs::path trace_file;
	fs::path trace_output_file;
	fs::path fct_output_file;
	fs::path pfc_output_file;
	fs::path anim_output_file;

	bool has_win;
	bool global_t;
	bool ack_high_prio;

#if RAF_WAITS_REFACTORING
	uint32_t mi_thresh{5};
	bool sample_feedback{false};
	double pint_log_base{1.05};
	double pint_prob{1.0};
	double u_target{0.95};
	uint32_t int_multi{1};
#endif

	uint32_t rng_seed{50};

	uint32_t enable_trace{1};

	// `kmin_map` says for which egress minimal buffer size we should mark the "Congestion Experienced" bit.
	// the probability is 100% when > `kmax_map`, pmax% when == `kmax_map`, and 0% when == `kmin_map`, and increases linearly between `kmin_map` and `kmax_map`..
	std::vector<BandwidthToEcnThreshold> kmax_map;
	std::vector<BandwidthToEcnThreshold> kmin_map;
	std::vector<BandwidthToEcnThreshold> pmax_map;

  fs::path FindFile(fs::path path) const
  {
    return config_dir.get() / path;
  }

	template<typename T>
	T LoadFromfile(fs::path path) const
	{
		return rfl::json::read<T>(raf::read_all_file(FindFile(path))).value();
	}
};

struct RdmaTraceList
{
  std::vector<uint32_t> nodes;
};

struct RdmaTopology
{
	struct Node
  {
		struct Pos
    {
			double x = 0.0;
			double y = 0.0;
			NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Pos, x, y);
		};

		uint32_t id = 0;
		bool is_switch = false;
		Pos pos;
		NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Node, id, is_switch, pos);
	};

	struct Link
  {
		uint32_t src = 0;      // First node ID
		uint32_t dst = 0;      // Second node ID
		double bandwidth = 100e9;  // Unit: bps
		double latency = 1e-6;    // Unit: seconds
		double error_rate = 0.0; // Unit: percentage in [0;1]
		NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Link, src, dst, bandwidth, latency, error_rate);
	};

  struct Group
  {
    group_id_t id;
    std::string nodes; // Contains node parsable from `Ranges`
		NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Group, id, nodes);
  };

	std::vector<Node> nodes;
	std::vector<Link> links;
  std::vector<Group> groups;
	NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(RdmaTopology, nodes, links, groups);
};

/**
 * @note First call `SetConfig()`, and then `SetTopology()`.
 */
class RdmaNetwork
{
private:
	RdmaNetwork() = default;
  ~RdmaNetwork();
	
	DISALLOW_COPY(RdmaNetwork);

public:
	static RdmaNetwork& GetInstance();

  struct Interface
  {
    bool up{}; //!< False if there is no direct link between the nodes
    uint32_t idx{}; //!< Which Qbb interface on the src node
    Time delay;
    DataRate bw;
  };

  struct P2pInfo {
    Interface iface;
    std::vector<Ptr<Node>> next_hops; //!< Mapping next hop after src to go to dst (vector because ECMP)
    uint64_t delay{}; //!< (ns) Sum of delay of all hops, without transmission delay ("time to transfer a single byte")
    uint64_t tx_delay{}; //!< (ns) Sum of transmission delays of all hops (time to fully push one MTU in the network)
    uint64_t bw{}; //!< (bps) Bandwidth of the slowest link in-between the two hosts
    uint64_t bdp{}; //!< (Bytes) Bandwidth-delay product between the two hosts
    uint64_t rtt{}; //!< (ns)
  };

	uint64_t GetMtuBytes() const;

public:
  static Ipv4Address GetNodeIp(node_id_t id);

	NodeMap GetAllSwitches() const;
  Ptr<Node> FindNode(node_id_t id) const; //!< Crash if not found
  Ptr<Node> FindServer(node_id_t id) const; //!< Crash if not found
  Ipv4Address FindNodeIp(node_id_t id) const;
  uint64_t FindBdp(node_id_t src, node_id_t dst) const;
  uint64_t GetMaxBdp() const;
  NodeContainer FindMcastGroup(uint32_t id) const;

  void SetConfig(std::shared_ptr<RdmaConfig> config);
  const RdmaConfig& GetConfig() const;

  void SetTopology(std::shared_ptr<RdmaTopology> topology);

private:
  void AddNode(Ptr<Node> node);
	
  void CreateNodes();
  void InstallInternet();
	void InstallRdma();
  void CreateLinks();
  void ConfigureSwitches();
  void BuildRoutes();
  void BuildRoutingTables();
  void BuildRoute(Ptr<Node> node);
  void BuildP2pInfo();
  void BuildGroups();
  void TraceNodes(); //!< Setup tracing the nodes we want to trace
  void CreateAnimation();

  void StartQLenMonitor();
  void MonitorPfc(Ptr<QbbNetDevice> qbb, uint32_t type);

private:
  std::map<node_id_t, Ptr<Node>> m_nodes; //!< All nodes
  std::map<node_id_t, Ptr<Node>> m_servers; //!< Servers only
  std::map<node_id_t, Ptr<SwitchNode>> m_switches; //!< Switches only
  std::map<Ptr<Node>, std::map<Ptr<Node>, P2pInfo>> m_p2p; //!< Store each p2p info between server `i` and each node `j` in `m_p2p[i][j]`
  std::shared_ptr<RdmaConfig> m_config;
  std::shared_ptr<RdmaTopology> m_topology;
  uint64_t m_maxRtt{};
  uint64_t m_maxBdp{};
  QbbHelper m_qbb;
  RdmaTraceList m_to_trace;
	std::unique_ptr<QLenMonitor> m_qlen_monitor;
  FILE *m_trace_output{};
  std::vector<PfcEntry> m_pfc_entries;
	std::map<uint32_t, std::set<node_id_t>> m_mcast_groups;
  std::unique_ptr<AnimationInterface> m_anim;

  //! Monitoring of queue buffer usage result
  using device_id_t = uint32_t;
};

} // namespace ns3