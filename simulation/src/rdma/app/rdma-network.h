#pragma once

#include "ns3/rdma-config-module.h"
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
 * @note Initialize the singleton with `Initialize()`, before any call to `GetInstance()` otherwise it crashes.
 */
class RdmaNetwork
{
private:
	RdmaNetwork() = default;
	
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

  //! Represents a network interface on a node port.
  struct Interface
  {
    //! False if the interface is down (there is no link connected to it).
    bool up{};
    //! The QBB interface index corresponding to this link (on the source node).
    uint32_t idx{};
    //! The delay to receive one 1 byte of data on the remote side of this link.
    Time delay;
    //! The bandwidth of this link.
    DataRate bw;
  };

  //! Stores information between a pair of nodes `(source, destination)`.
  struct P2pInfo
  {
    //! Stores information of the interface of the souce node.
    Interface iface;
    //! Stores all possible next hops after the source node to go to the destination node
    //! There are multiple possibilities (vector), because we support multiple routes with ECMP.
    std::vector<Ptr<Node>> next_hops;
    //! Sum of delay in nanoseconds of all hops, without transmission delay ("time to transfer a single byte").
    uint64_t delay{};
    //! Sum of transmission delays of all hops in nanoseconds.
    //! This is the time to fully push one MTU in the network.
    uint64_t tx_delay{};
    //! Bandwidth in bits per second of the slowest link in-between the two hosts.
    uint64_t bw{};
    //! Bandwidth-delay product in bytes between the two hosts.
    uint64_t bdp{};
    //! Round-trip time in nanoseconds.
    uint64_t rtt{};
  };

  /**
   * Gets the MTU in bytes.
   * The MTU is defined as the default attribute value of the global attribute `RdmaHw::Mtu`.
   */
	uint64_t GetMtuBytes() const;

public:
  //! Get the IP address of a node from its global node ID.
  static Ipv4Address GetNodeIp(node_id_t id);

	NodeMap GetAllSwitches() const;
  NodeMap GetAllServers() const;
  NetDeviceContainer GetAllQbbNetDevices() const;
  Ptr<Node> FindNode(node_id_t id) const; //!< Crash if not found
  Ptr<Node> FindServer(node_id_t id) const; //!< Crash if not found
  Ipv4Address FindNodeIp(node_id_t id) const;
  uint64_t GetMaxBdp() const;

  //! Get all nodes that belongs to the given multicast group.
  NodeContainer FindMcastGroup(uint32_t id) const;

  const RdmaConfig& GetConfig() const;
  
private:
  bool HaveAllServersSameBandwidth() const;

  void InitConfig(std::shared_ptr<RdmaConfig> config);
  void InitTopology(std::shared_ptr<RdmaTopology> topology);
  void InitModules();

  void AddNode(Ptr<Node> node);
	
  /**
   * Loads the nodes from the config topology.
   */
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

private:
  //! If the singleton is initialized.
  static bool m_initialized;

  //! Stores all nodes (servers + switches).
  std::map<node_id_t, Ptr<Node>> m_nodes;
  //! Stores all servers only (no switch).
  std::map<node_id_t, Ptr<Node>> m_servers;
  //! Stores all switches only (no server).
  std::map<node_id_t, Ptr<SwitchNode>> m_switches;

  //! Stores the global configuration information.
  std::shared_ptr<RdmaConfig> m_config;
  //! Stores the global topology information.
  std::shared_ptr<RdmaTopology> m_topology;

  //! Store each p2p info between server `i` and each node `j` in `m_p2p[i][j]`
  std::map<Ptr<Node>, std::map<Ptr<Node>, P2pInfo>> m_p2p;
  //! Stores the highest RTT between all pairs of nodes.
  uint64_t m_maxRtt{};
  //! Stores the highest bandwidth-delay product between all pairs of nodes.
  uint64_t m_maxBdp{};

  //! Note: Not sure it is usdeful as member variable.
  QbbHelper m_qbb;

  //! Stores which servers belong to which multicast groups.
	std::map<uint32_t, std::set<node_id_t>> m_mcast_groups;

  //! Aggregate any object.
  //! Useful for extensibility.
  std::vector<Ptr<RdmaConfigModule>> m_modules;
};

} // namespace ns3