#pragma once

#include "ns3/rdma-qlen-monitor.h"
#include "ns3/rdma-qp-monitor.h"
#include "ns3/qbb-helper.h"
#include "ns3/switch-node.h"
#include "ns3/node-container.h"
#include "ns3/json.h"
#include "ns3/filesystem.h"
#include "ns3/animation-interface.h"
#include "ns3/rdma-reflection-helper.h"
#include "ns3/filesystem.h"
#include "ns3/rdma-config.h"
#include <map>
#include <vector>
#include <cstdint>
#include <memory>
#include <vector>

namespace ns3 {

/**
 * Singleton that represents the entire simulation.
 * Stores all: topology, configuration, flows, loggers...
 * 
 * Initialize the singleton with `Initialize()`, before any call to `GetInstance()` otherwise it crashes.
 */
class RdmaNetwork
{
private:
	RdmaNetwork() = default;
  ~RdmaNetwork();
	
	DISALLOW_COPY(RdmaNetwork);

public:
  /**
   * Runs the simulation from a global configuration file.
   */
  static void Initialize(const fs::path& config_path);

  /**
   * Gets the singleton `RdmaNetwork` instance.
   */
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
  NodeMap GetAllServers() const;
  Ptr<Node> FindNode(node_id_t id) const; //!< Crash if not found
  Ptr<Node> FindServer(node_id_t id) const; //!< Crash if not found
  Ipv4Address FindNodeIp(node_id_t id) const;
  uint64_t GetMaxBdp() const;
  NodeContainer FindMcastGroup(uint32_t id) const;

  const RdmaConfig& GetConfig() const;
  
private:
  void InitConfig(std::shared_ptr<RdmaConfig> config);
  void InitTopology(std::shared_ptr<RdmaTopology> topology);

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
  
  /// See AnimationInterface.
  void DevTxTrace (
    std::string context,
    Ptr<const Packet> p,
    Ptr<NetDevice> tx,
    Ptr<NetDevice> rx,
    Time txTime,
    Time rxTime);

private:
  static bool m_initialized;

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
  std::unique_ptr<QpMonitor> m_qp_monitor;
  FILE *m_trace_output{};
  std::vector<PfcEntry> m_pfc_entries;
	std::map<uint32_t, std::set<node_id_t>> m_mcast_groups;
  std::unique_ptr<AnimationInterface> m_anim;

  /// `m_txrx_bytes[i][j]` stores transmitted bytes from node `i` to node `j`. 
  std::vector<std::vector<uint64_t>> m_txrx_bytes;

  using device_id_t = uint32_t;
};

} // namespace ns3