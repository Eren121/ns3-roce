#include "ns3/ipv4.h"
#include "ns3/packet.h"
#include "ns3/ipv4-header.h"
#include "ns3/pause-header.h"
#include "ns3/flow-id-tag.h"
#include "ns3/boolean.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "switch-node.h"
#include "qbb-net-device.h"
#include "rdma-bth.h"
#include "ns3/rdma-random.h"
#include "ns3/ppp-header.h"
#include "ns3/int-header.h"
#include "ns3/simulator.h"
#include "ns3/abort.h"
#include "ns3/log.h"
#include "ns3/assert.h"
#include <cmath>
#include <atomic>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("SwitchNode");
NS_OBJECT_ENSURE_REGISTERED (SwitchNode);

TypeId SwitchNode::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::SwitchNode")
    .SetParent<Node> ()
    .AddConstructor<SwitchNode> ()
	.AddAttribute("EcnEnabled",
			"Enable ECN marking",
			BooleanValue(false),
			MakeBooleanAccessor(&SwitchNode::m_ecnEnabled),
			MakeBooleanChecker())
	.AddAttribute("CcMode",
			"CC mode.",
			UintegerValue(0),
			MakeUintegerAccessor(&SwitchNode::m_ccMode),
			MakeUintegerChecker<uint32_t>())
	.AddAttribute("AckHighPrio",
			"Set high priority for ACK/NACK or not",
			UintegerValue(0),
			MakeUintegerAccessor(&SwitchNode::m_ackHighPrio),
			MakeUintegerChecker<uint32_t>())
	.AddAttribute("MaxRtt",
			"Max Rtt of the network",
			UintegerValue(9000),
			MakeUintegerAccessor(&SwitchNode::m_maxRtt),
			MakeUintegerChecker<uint32_t>())
  ;
  return tid;
}

SwitchNode::SwitchNode(){
	static std::atomic<uint32_t> nextEcmpSeed(1);
	m_ecmpSeed = nextEcmpSeed++;
	m_mmu = CreateObject<SwitchMmu>();
	for (uint32_t i = 0; i < pCnt; i++)
		for (uint32_t j = 0; j < pCnt; j++)
			for (uint32_t k = 0; k < qCnt; k++)
				m_bytes[i][j][k] = 0;
	for (uint32_t i = 0; i < pCnt; i++)
		m_txBytes[i] = 0;
	for (uint32_t i = 0; i < pCnt; i++)
		m_lastPktSize[i] = m_lastPktTs[i] = 0;
	for (uint32_t i = 0; i < pCnt; i++)
		m_u[i] = 0;
}

int SwitchNode::GetOutDev(Ptr<const Packet> p, CustomHeader &ch){
	// look up entries
	auto entry = m_rtTable.find(ch.dip);

	// no matching entry
	if (entry == m_rtTable.end())
		return -1;

	// entry found
	auto &nexthops = entry->second;

	// pick one next hop based on hash
	union {
		uint8_t u8[4+4+2+2];
		uint32_t u32[3];
	} buf;
	buf.u32[0] = ch.sip;
	buf.u32[1] = ch.dip;
	if (ch.l3Prot == 0x6)
		buf.u32[2] = ch.tcp.sport | ((uint32_t)ch.tcp.dport << 16);
	else if (ch.l3Prot == 0x11)
		buf.u32[2] = ch.udp.sport | ((uint32_t)ch.udp.dport << 16);
	else if (ch.l3Prot == 0xFC || ch.l3Prot == 0xFD)
		buf.u32[2] = ch.ack.sport | ((uint32_t)ch.ack.dport << 16);

	uint32_t idx = EcmpHash(buf.u8, 12, m_ecmpSeed) % nexthops.size();
	return nexthops[idx];
}

void SwitchNode::OnPeerJoinGroup(uint32_t ifIndex, uint32_t group)
{
	auto& odevs = m_ogroups[group];
	const bool notifyOthers = (odevs.size() == 0);
	odevs.emplace(ifIndex);

	if(!notifyOthers) {
		//return;
		// Not working, all need to be notified
	}
	
	Ptr<QbbNetDevice> iface = DynamicCast<QbbNetDevice>(GetDevice(ifIndex));
	for(uint32_t i = 0; i < GetNDevices(); i++) {
		Ptr<QbbNetDevice> dev = DynamicCast<QbbNetDevice>(GetDevice(i));
		if(dev && dev != iface) { // First port can be null - maybe it is internal switch port 
			dev->AddGroup(group);
		}
	}
}

void SwitchNode::CheckAndSendPfc(uint32_t inDev, uint32_t qIndex){
	Ptr<QbbNetDevice> device = DynamicCast<QbbNetDevice>(GetDevice(inDev));
	if (m_mmu->CheckShouldPause(inDev, qIndex)){
		device->SendPfc(qIndex, 0);
		m_mmu->SetPause(inDev, qIndex);
	}
}
void SwitchNode::CheckAndSendResume(uint32_t inDev, uint32_t qIndex){
	Ptr<QbbNetDevice> device = DynamicCast<QbbNetDevice>(GetDevice(inDev));
	if (m_mmu->CheckShouldResume(inDev, qIndex)){
		device->SendPfc(qIndex, 1);
		m_mmu->SetResume(inDev, qIndex);
	}
}

void SwitchNode::SendMultiToDevs(Ptr<Packet> packet, CustomHeader& ch, int in_iface) {
	
	FlowIdTag t;
	packet->PeekPacketTag(t);
	const uint32_t inDev{t.GetFlowId()};

	// Determine the qIndex
	uint32_t qIndex;
	if (ch.l3Prot == 0xFF || ch.l3Prot == 0xFE || (m_ackHighPrio && (ch.l3Prot == 0xFD || ch.l3Prot == 0xFC))){  //QCN or PFC or NACK, go highest priority
		qIndex = 0;
	}else{
		qIndex = (ch.l3Prot == 0x06 ? 1 : ch.udp.pg); // if TCP, put to queue 1
	}

	// Admission control
	if (qIndex != 0) { //not highest priority
		if(!m_mmu->CheckIngressAdmission(inDev, qIndex, packet->GetSize())) {
			NS_LOG_LOGIC("Drop: Pause multicast " << qIndex);
			return;
		}
	}

	auto iface_it = m_ogroups.find(ch.dip);
	if(iface_it == m_ogroups.end()) {
		NS_LOG_LOGIC("Drop: Cannot find ports for multicast group " << ch.dip);
		return;
	}

	// Keep only one uplink outport port
	// Never send to uplink if the packet comes from uplink

	const bool from_uplink{m_uplink[in_iface]};

	std::vector<iface_id_t> uplink_candidates;

	for(iface_id_t iface : iface_it->second) {
		if(m_uplink[iface]) {
			uplink_candidates.push_back(iface);
		}
	}

	iface_id_t uplink_elected{};
	if(!uplink_candidates.empty()) {
		// pick one next hop based on hash
		union {
			uint8_t u8[4+4+2+2];
			uint32_t u32[3];
		} buf;
		buf.u32[0] = ch.sip;
		buf.u32[1] = ch.dip;
		if (ch.l3Prot == 0x6)
			buf.u32[2] = ch.tcp.sport | ((uint32_t)ch.tcp.dport << 16);
		else if (ch.l3Prot == 0x11)
			buf.u32[2] = ch.udp.sport | ((uint32_t)ch.udp.dport << 16);
		else if (ch.l3Prot == 0xFC || ch.l3Prot == 0xFD)
			buf.u32[2] = ch.ack.sport | ((uint32_t)ch.ack.dport << 16);

		uint32_t hash = EcmpHash(buf.u8, 12, m_ecmpSeed);
		uplink_elected = uplink_candidates[hash % uplink_candidates.size()];
	}

	Ptr<Packet> last;
	
	for(int idx : iface_it->second) {

		if(idx == in_iface) {
			continue;
		}
		
		if(m_uplink[idx] && (from_uplink || uplink_elected != idx)) {
			continue;
		}

		NS_ASSERT_MSG(GetDevice(idx)->IsLinkUp(), "The routing table look up should return link that is up");

		Ptr<Packet> p = packet->Copy();

		// TODO: now we increase the input interface buffer usage for each output device in the routing table of the multicast destination
		// It would be more logical to increase only once, regardless of the count of the output devices.
		// This is easy to do like here, but maybe it changes behaviour when the buffer is almost at full capacity, because the buffer full capacity is triggered earlier than what it should.
		// See `SwitchNotifyDequeue::RemoveFromIngressAdmission()`
		// We need to keep trace of the output packet because increasing only once would do an integer underflow resulting in buffer usage of 4 billion....
		
		last = p;
		
		m_mmu->UpdateIngressAdmission(inDev, qIndex, last->GetSize());
		m_mmu->UpdateEgressAdmission(idx, qIndex, p->GetSize());
		
		if(qIndex != 0) { m_bytes[inDev][idx][qIndex] += p->GetSize(); }
		SwitchSend(GetDevice(idx), qIndex, p);
	}

	if(last) {
		m_egress_lasts.insert(last);
	}

	if(qIndex != 0) {
		CheckAndSendPfc(inDev, qIndex);
	}
}

void SwitchNode::SendToDev(Ptr<Packet>p, CustomHeader &ch){
	int idx = GetOutDev(p, ch);
	if (idx >= 0){
		NS_ASSERT_MSG(GetDevice(idx)->IsLinkUp(), "The routing table look up should return link that is up");

		// determine the qIndex
		uint32_t qIndex;
		if (ch.l3Prot == 0xFF || ch.l3Prot == 0xFE || (m_ackHighPrio && (ch.l3Prot == 0xFD || ch.l3Prot == 0xFC))){  //QCN or PFC or NACK, go highest priority
			qIndex = 0;
		}else{
			qIndex = (ch.l3Prot == 0x06 ? 1 : ch.udp.pg); // if TCP, put to queue 1
		}

		// admission control
		FlowIdTag t;
		p->PeekPacketTag(t);
		uint32_t inDev = t.GetFlowId();
		if (qIndex != 0){ //not highest priority
			if (m_mmu->CheckIngressAdmission(inDev, qIndex, p->GetSize())){			// Admission control
				m_mmu->UpdateIngressAdmission(inDev, qIndex, p->GetSize());
				m_egress_lasts.insert(p);

				m_mmu->UpdateEgressAdmission(idx, qIndex, p->GetSize());
			}else{
				NS_LOG_LOGIC("Drop: unicast packet not admitted");
				return;
			}
			CheckAndSendPfc(inDev, qIndex);
		}

		m_bytes[inDev][idx][qIndex] += p->GetSize();
		SwitchSend(GetDevice(idx), qIndex, p);
	}else {
		NS_LOG_LOGIC("Drop: cannot find output device for packet");
		return;
	}
}

uint32_t SwitchNode::EcmpHash(const uint8_t* key, size_t len, uint32_t seed) {
  uint32_t h = seed;
  if (len > 3) {
    const uint32_t* key_x4 = (const uint32_t*) key;
    size_t i = len >> 2;
    do {
      uint32_t k = *key_x4++;
      k *= 0xcc9e2d51;
      k = (k << 15) | (k >> 17);
      k *= 0x1b873593;
      h ^= k;
      h = (h << 13) | (h >> 19);
      h += (h << 2) + 0xe6546b64;
    } while (--i);
    key = (const uint8_t*) key_x4;
  }
  if (len & 3) {
    size_t i = len & 3;
    uint32_t k = 0;
    key = &key[i - 1];
    do {
      k <<= 8;
      k |= *key--;
    } while (--i);
    k *= 0xcc9e2d51;
    k = (k << 15) | (k >> 17);
    k *= 0x1b873593;
    h ^= k;
  }
  h ^= len;
  h ^= h >> 16;
  h *= 0x85ebca6b;
  h ^= h >> 13;
  h *= 0xc2b2ae35;
  h ^= h >> 16;
  return h;
}

void SwitchNode::SetEcmpSeed(uint32_t seed){
	m_ecmpSeed = seed;
}

void SwitchNode::AddTableEntry(Ipv4Address &dstAddr, uint32_t intf_idx){

	NS_LOG_LOGIC("Switch " << GetId() << " add {"
		"port=" << intf_idx << ",ip=" << dstAddr << "}");
	
	uint32_t dip = dstAddr.Get();
	m_rtTable[dip].push_back(intf_idx);
}

void SwitchNode::ClearTable(){
	m_rtTable.clear();
}

// This function can only be called in switch mode
bool SwitchNode::SwitchReceiveFromDevice(Ptr<NetDevice> device, Ptr<Packet> packet, CustomHeader &ch){
	bool multicast = false;
	{
		RdmaBTH bth;
		if(packet->PeekPacketTag(bth)) {
			multicast = bth.GetMulticast();
		}
	}

	if(multicast) {
		SendMultiToDevs(packet, ch, device->GetIfIndex());
	}
	else {
		SendToDev(packet, ch);
	}

	return true;
}

void SwitchNode::SwitchNotifyDequeue(uint32_t ifIndex, uint32_t qIndex, Ptr<Packet> p){
	FlowIdTag t;
	p->PeekPacketTag(t);
	if (qIndex != 0){
		uint32_t inDev = t.GetFlowId();

		{
			// Last packet of the mcast (or unicast), remove from ingress port
			auto it{m_egress_lasts.find(p)};
			if(it != m_egress_lasts.end()) {
				m_egress_lasts.erase(it);
			}
		}
		m_mmu->RemoveFromIngressAdmission(inDev, qIndex, p->GetSize());

		std::cout << m_egress_lasts.size()<< "/" << m_bytes[inDev][ifIndex][qIndex] << std::endl;
		m_mmu->RemoveFromEgressAdmission(ifIndex, qIndex, p->GetSize());
		m_bytes[inDev][ifIndex][qIndex] -= p->GetSize();
		if (m_ecnEnabled){
			bool egressCongested = m_mmu->ShouldSendCN(ifIndex, qIndex);
			if (egressCongested) {
				NS_LOG_DEBUG("Switch marks CE");
				PppHeader ppp;
				Ipv4Header h;
				p->RemoveHeader(ppp);
				p->RemoveHeader(h);
				h.SetEcn((Ipv4Header::EcnType)0x03);
				p->AddHeader(h);
				p->AddHeader(ppp);
			}
		}
		//CheckAndSendPfc(inDev, qIndex);
		CheckAndSendResume(inDev, qIndex);
	}

	NS_ABORT_IF(m_ccMode == 3 || m_ccMode == 10);
	#if 0 // A lot of invalid code in ns3.19
	      // Simpler to remove: it does only concern only HPCC and HPCC-PINT congestion control.
	if (1){
		uint8_t* buf = p->GetBuffer();
		if (buf[PppHeader::GetStaticSize() + 9] == 0x11){ // udp packet
			IntHeader *ih = (IntHeader*)&buf[PppHeader::GetStaticSize() + 20 + 8 + 6]; // ppp, ip, udp, SeqTs, INT
			Ptr<QbbNetDevice> dev = DynamicCast<QbbNetDevice>(GetDevice(ifIndex));
			if (m_ccMode == 3){ // HPCC
				ih->PushHop(Simulator::Now().GetTimeStep(), m_txBytes[ifIndex], dev->GetQueue()->GetNBytesTotal(), dev->GetDataRate().GetBitRate());
			}else if (m_ccMode == 10){ // HPCC-PINT
				uint64_t t = Simulator::Now().GetTimeStep();
				uint64_t dt = t - m_lastPktTs[ifIndex];
				if (dt > m_maxRtt)
					dt = m_maxRtt;
				uint64_t B = dev->GetDataRate().GetBitRate() / 8; //Bps
				uint64_t qlen = dev->GetQueue()->GetNBytesTotal();
				double newU;

				/**************************
				 * approximate calc
				 *************************/
				int b = 20, m = 16, l = 20; // see log2apprx's paremeters
				int sft = logres_shift(b,l);
				double fct = 1<<sft; // (multiplication factor corresponding to sft)
				double log_T = log2(m_maxRtt)*fct; // log2(T)*fct
				double log_B = log2(B)*fct; // log2(B)*fct
				double log_1e9 = log2(1e9)*fct; // log2(1e9)*fct
				double qterm = 0;
				double byteTerm = 0;
				double uTerm = 0;
				if ((qlen >> 8) > 0){
					int log_dt = log2apprx(dt, b, m, l); // ~log2(dt)*fct
					int log_qlen = log2apprx(qlen >> 8, b, m, l); // ~log2(qlen / 256)*fct
					qterm = pow(2, (
								log_dt + log_qlen + log_1e9 - log_B - 2*log_T
								)/fct
							) * 256;
					// 2^((log2(dt)*fct+log2(qlen/256)*fct+log2(1e9)*fct-log2(B)*fct-2*log2(T)*fct)/fct)*256 ~= dt*qlen*1e9/(B*T^2)
				}
				if (m_lastPktSize[ifIndex] > 0){
					int byte = m_lastPktSize[ifIndex];
					int log_byte = log2apprx(byte, b, m, l);
					byteTerm = pow(2, (
								log_byte + log_1e9 - log_B - log_T
								)/fct
							);
					// 2^((log2(byte)*fct+log2(1e9)*fct-log2(B)*fct-log2(T)*fct)/fct) ~= byte*1e9 / (B*T)
				}
				if (m_maxRtt > dt && m_u[ifIndex] > 0){
					int log_T_dt = log2apprx(m_maxRtt - dt, b, m, l); // ~log2(T-dt)*fct
					int log_u = log2apprx(int(round(m_u[ifIndex] * 8192)), b, m, l); // ~log2(u*512)*fct
					uTerm = pow(2, (
								log_T_dt + log_u - log_T
								)/fct
							) / 8192;
					// 2^((log2(T-dt)*fct+log2(u*512)*fct-log2(T)*fct)/fct)/512 = (T-dt)*u/T
				}
				newU = qterm+byteTerm+uTerm;

				#if 0
				/**************************
				 * accurate calc
				 *************************/
				double weight_ewma = double(dt) / m_maxRtt;
				double u;
				if (m_lastPktSize[ifIndex] == 0)
					u = 0;
				else{
					double txRate = m_lastPktSize[ifIndex] / double(dt); // B/ns
					u = (qlen / m_maxRtt + txRate) * 1e9 / B;
				}
				newU = m_u[ifIndex] * (1 - weight_ewma) + u * weight_ewma;
				printf(" %lf\n", newU);
				#endif

				/************************
				 * update PINT header
				 ***********************/
				uint16_t power = Pint::encode_u(newU);
				if (power > ih->GetPower())
					ih->SetPower(power);

				m_u[ifIndex] = newU;
			}
		}
	}
	#endif
	m_txBytes[ifIndex] += p->GetSize();
	m_lastPktSize[ifIndex] = p->GetSize();
	m_lastPktTs[ifIndex] = Simulator::Now().GetTimeStep();
}

int SwitchNode::logres_shift(int b, int l){
	static int data[] = {0,0,1,2,2,3,3,3,3,4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5};
	return l - data[b];
}

int SwitchNode::log2apprx(int x, int b, int m, int l)
{
	int x0 = x;
	int msb = int(log2(x)) + 1;
	if (msb > m){
		x = (x >> (msb - m) << (msb - m));
		#if 0
		x += + (1 << (msb - m - 1));
		#else
		int mask = (1 << (msb-m)) - 1;
		if ((x0 & mask) > (rand() & mask))
			x += 1<<(msb-m);
		#endif
	}
	return int(log2(x) * (1<<logres_shift(b, l)));
}


void SwitchNode::Rebuild(NodeContainer nodes)
{
	std::unordered_map<Ptr<SwitchNode>, int> dist_from_leaf;
	std::unordered_set<Ptr<Node>> visited;
	std::unordered_set<Ptr<Node>> visiting;
	std::unordered_set<Ptr<Node>> tovisit;

	ForEachSwitch(nodes, [](Ptr<SwitchNode> sw) {
		sw->m_uplink.clear();
	});

	// First compute the inverse of the depth of each node
	// So visit servers first
	
	for(int i{}; i < nodes.GetN(); i++) {

		const Ptr<Node> node{nodes.Get(i)};

		if(!IsSwitchNode(node)) {
			tovisit.insert(node);
		}
	}

	int leaf_dist{};

	while(!tovisit.empty()) {

		visiting = std::move(tovisit);
		tovisit.clear();

		for(Ptr<Node> node : visiting) {

			const bool already_visited{!visited.insert(node).second};
			NS_ABORT_IF(already_visited);
			
			const auto sw{DynamicCast<SwitchNode>(node)};
			
			if(sw) {
				dist_from_leaf[sw] = leaf_dist;
			}
			
			// Visit all connections in the next iterations
			ForEachDevice(node, [&](Ptr<QbbNetDevice> dev) {

				const auto peer_dev{dev->GetPeerNetDevice()};
				const Ptr<Node> peer{peer_dev->GetNode()};

				if(!visited.contains(peer)) {
					tovisit.insert(peer);
				}
			});
		}

		leaf_dist++;
	}
	
	NS_ABORT_IF(leaf_dist <= 0);
	const int maxdepth{leaf_dist - 1};

	ForEachSwitch(nodes, [&](Ptr<SwitchNode> sw) {

		sw->m_depth = maxdepth - dist_from_leaf.at(sw);
	});
	
	ForEachSwitch(nodes, [&](Ptr<SwitchNode> sw) {

		ForEachDevice(sw, [&](Ptr<QbbNetDevice> dev) {

			const Ptr<QbbNetDevice> peer_dev{dev->GetPeerNetDevice()};
			const auto peer{peer_dev->GetNode()};
			const iface_id_t iface{dev->GetIfIndex()};
			EnsureSizeAtleast(sw->m_uplink, iface + 1);
			
			int peer_depth{maxdepth};

			if(DynamicCast<SwitchNode>(peer)) {
				peer_depth = DynamicCast<SwitchNode>(peer)->m_depth;
			}

			if(sw->m_depth == peer_depth + 1) {
				sw->m_uplink[iface] = true;
			}
			else if(sw->m_depth == peer_depth - 1) {
				sw->m_uplink[iface] = false;
			}
			else {
				NS_ABORT_MSG("Invalid link between depths ("
					<< sw->m_depth << ", " << peer_depth
					<< ")"
				);
			}

			NS_LOG_INFO("node "
				<< sw->GetId() << "->" << peer->GetId()
				<< " uplink=" << sw->m_uplink[iface]
			);
		});
	});
}

bool IsSwitchNode(Ptr<Node> node)
{
	return DynamicCast<SwitchNode>(node) != nullptr;
}

NodeType GetNodeType(Ptr<Node> node)
{
	return IsSwitchNode(node) ? NT_SWITCH : NT_SERVER;
}

bool SwitchSend(Ptr<NetDevice> self, uint32_t qIndex, Ptr<Packet> packet)
{
	Ptr<QbbNetDevice> qbb = DynamicCast<QbbNetDevice>(self);
	NS_ASSERT(qbb);
	return qbb->SwitchSend(qIndex, packet);
}

void SwitchNotifyDequeue(Ptr<Node> self, uint32_t ifIndex, uint32_t qIndex, Ptr<Packet> p)
{
	Ptr<SwitchNode> sw = DynamicCast<SwitchNode>(self);
	NS_ASSERT(sw);
	sw->SwitchNotifyDequeue(ifIndex, qIndex, p);
}

bool SwitchReceiveFromDevice(Ptr<Node> self, Ptr<NetDevice> device, Ptr<Packet> packet, CustomHeader &ch)
{
	Ptr<SwitchNode> sw = DynamicCast<SwitchNode>(self);
	NS_ASSERT(sw);
	return sw->SwitchReceiveFromDevice(device, packet, ch);
}

} /* namespace ns3 */
