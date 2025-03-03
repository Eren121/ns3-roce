#ifndef SWITCH_NODE_H
#define SWITCH_NODE_H

#include "ns3/node.h"
#include "ns3/node-container.h"
#include "ns3/rdma-helper.h"
#include "ns3/qbb-net-device.h"
#include "ns3/switch-mmu.h"
#include "ns3/pint.h"
#include <unordered_map>
#include <memory>
#include <vector>

namespace ns3 {

class Packet;

class SwitchNode : public Node
{
private:
	/**
	 *  @brief Number of ports on the switch.
	 */
	static const uint32_t pCnt = 257;

	/**
	 * @brief Number of priority queues (per port).
	 */
	static const uint32_t qCnt = 8;

	/**
	 * @brief Random seed to compute the output port for ECMP (multi-path routing).
	 * 
	 * Where written:
	 * - Initialized in the constructor to the node ID.
	 * - Can be changed in `SetEcmpSeed()`.
	 */
	uint32_t m_ecmpSeed;

	/**
	 * @brief Map from IP address (u32) to possible ECMP port (index of dev).
	 * 
	 * When a packet should be forwarded, the destination IP may be reachable via multiple ports (ECMP).
	 * The final port chosen depends on the tuple (sip, sport, dip, dport) (See `GetOutDev()`).
	 * The final port chosen is constant for each tuple to ensure an unique path and in-order reception.
	 */
	std::unordered_map<uint32_t, std::vector<int> > m_rtTable;

	/**
	 * For each interface, stores whether the link points towards an uplink switch in the topology.
	 * Used for ECMP.
	 */
	std::vector<iface_id_t> m_uplink;
	
	/**
	 * Distance from root in the fat tree (root switches have depth zero).
	 * Used for ECMP.
	 */
	int m_depth{};

	/**
	 * @brief Map from multicast group to output ports.
	 */
	std::unordered_map<uint32_t, std::set<int>> m_ogroups;

	// monitor of PFC
	uint32_t m_bytes[pCnt][pCnt][qCnt]; // m_bytes[inDev][outDev][qidx] is the bytes from inDev enqueued for outDev at qidx
	
	uint64_t m_txBytes[pCnt]; // counter of tx bytes

	uint32_t m_lastPktSize[pCnt];
	uint64_t m_lastPktTs[pCnt]; // ns
	double m_u[pCnt];

protected:
	/// When true: when congestion is experienced, the switch marks the ECN bit before forwarding the packets to their destination.
	bool m_ecnEnabled;
	uint32_t m_ccMode;
	uint64_t m_maxRtt;

	uint32_t m_ackHighPrio; // set high priority for ACK/NACK

private:
	//! Packets of a multicast, or every packets of a unicast to dequeue.
	//! Permits to know when to release ingress memory.
	std::unordered_map<Ptr<Packet>, std::shared_ptr<int>> m_egress_lasts;

private:
	int GetOutDev(Ptr<const Packet>, CustomHeader &ch);
	void SendToDev(Ptr<Packet>p, CustomHeader &ch);
	void SendMultiToDevs(Ptr<Packet> p, CustomHeader& ch, int in_inface);
	static uint32_t EcmpHash(const uint8_t* key, size_t len, uint32_t seed);
	void CheckAndSendPfc(uint32_t inDev, uint32_t qIndex);
	void CheckAndSendResume(uint32_t inDev, uint32_t qIndex);

public:
	Ptr<SwitchMmu> m_mmu;

	static TypeId GetTypeId (void);
	SwitchNode();
	void SetEcmpSeed(uint32_t seed);
	void AddTableEntry(Ipv4Address &dstAddr, uint32_t intf_idx);
	void ClearTable();
	bool SwitchReceiveFromDevice(Ptr<NetDevice> device, Ptr<Packet> packet, CustomHeader &ch);
	void SwitchNotifyDequeue(uint32_t ifIndex, uint32_t qIndex, Ptr<Packet> p);

	void OnPeerJoinGroup(uint32_t ifIndex, uint32_t group);

	// for approximate calc in PINT
	int logres_shift(int b, int l);
	int log2apprx(int x, int b, int m, int l); // given x of at most b bits, use most significant m bits of x, calc the result in l bits

	/**
	 * Rebuild some information in the nodes.
	 * Should be called when interfaces have been added.
	 */
	static void Rebuild(NodeContainer nodes);

private:
};

enum NodeType {
	NT_SERVER = 0,
	NT_SWITCH = 1
};

// These free functions are refactored from the original code base.
// The original code added virtual functions to ns-3 classes with noop parent method.
// Free functions make it easier to upgrade to new ns-3 releases without changing the original ns-3 source code.

bool IsSwitchNode(Ptr<Node> self);
NodeType GetNodeType(Ptr<Node> self);
bool SwitchSend(Ptr<NetDevice> self, uint32_t qIndex, Ptr<Packet> packet);
void SwitchNotifyDequeue(Ptr<Node> self, uint32_t ifIndex, uint32_t qIndex, Ptr<Packet> p);
bool SwitchReceiveFromDevice(Ptr<Node> self, Ptr<NetDevice> device, Ptr<Packet> packet, CustomHeader &ch);


/**
 * \param func Should have signature `void(Ptr<SwitchNode>)`
 */
template<typename F>
void ForEachSwitch(NodeContainer nodes, F&& func)
{
  for(int i = 0; i < nodes.GetN(); i++) {
    const auto sw{DynamicCast<SwitchNode>(nodes.Get(i))};
    if(sw) { func(sw); }
  }
}

/**
 * \param func Should have signature `void(Ptr<QbbNetDevice>)`
 */
template<typename F>
void ForEachDevice(Ptr<Node> node, F&& func)
{
  for(int i = 0; i < node->GetNDevices(); i++) {
    auto dev{DynamicCast<QbbNetDevice>(node->GetDevice(i))};
    if(dev) { func(dev); }
  }
}

} /* namespace ns3 */

#endif /* SWITCH_NODE_H */
