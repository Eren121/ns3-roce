#ifndef SWITCH_NODE_H
#define SWITCH_NODE_H

#include <unordered_map>
#include <ns3/node.h>
#include "qbb-net-device.h"
#include "switch-mmu.h"
#include "pint.h"

namespace ns3 {

class Packet;

class SwitchNode : public Node{
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
	 * @brief Map from multicast group to output ports.
	 * 
	 * Does **NOT** support ECMP for now.
	 */
	std::unordered_map<uint32_t, std::set<int>> m_ogroups;

	// monitor of PFC
	uint32_t m_bytes[pCnt][pCnt][qCnt]; // m_bytes[inDev][outDev][qidx] is the bytes from inDev enqueued for outDev at qidx
	
	uint64_t m_txBytes[pCnt]; // counter of tx bytes

	uint32_t m_lastPktSize[pCnt];
	uint64_t m_lastPktTs[pCnt]; // ns
	double m_u[pCnt];

protected:
	bool m_ecnEnabled;
	uint32_t m_ccMode;
	uint64_t m_maxRtt;

	uint32_t m_ackHighPrio; // set high priority for ACK/NACK

private:
	int GetOutDev(Ptr<const Packet>, CustomHeader &ch);
	void SendToDev(Ptr<Packet>p, CustomHeader &ch);
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
};

enum NodeType {
	NT_SERVER = 0,
	NT_SWITCH = 1
};

bool IsSwitchNode(Ptr<Node> node);
NodeType GetNodeType(Ptr<Node> node);

} /* namespace ns3 */

#endif /* SWITCH_NODE_H */
