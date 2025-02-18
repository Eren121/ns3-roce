#include <ns3/rdma-unicast-app.h>
#include <ns3/qbb-net-device.h>
#include <ns3/rdma-hw.h>
#include <ns3/simulator.h>
#include <ns3/log.h>
#include <ns3/double.h>

NS_LOG_COMPONENT_DEFINE("RdmaUnicastApp");

namespace ns3 {

TypeId RdmaUnicastApp::GetTypeId()
{
  static TypeId tid = TypeId ("ns3::RdmaUnicastApp")
    .SetParent<Application>()
		.AddConstructor<RdmaUnicastApp>()
		.AddAttribute("SrcNode",
                  "Source Node ID",
                  UintegerValue(0),
                  MakeUintegerAccessor(&RdmaUnicastApp::m_src),
                  MakeUintegerChecker<uint32_t>())
    .AddAttribute("DstNode",
                  "Destination Node ID",
                  UintegerValue(0),
                  MakeUintegerAccessor(&RdmaUnicastApp::m_dst),
                  MakeUintegerChecker<uint32_t>())
		.AddAttribute("SrcPort",
                  "Source port",
                  UintegerValue(1500),
                  MakeUintegerAccessor(&RdmaUnicastApp::m_sport),
                  MakeUintegerChecker<uint16_t>())
    .AddAttribute("DstPort",
                  "Destination port",
                  UintegerValue(1500),
                  MakeUintegerAccessor(&RdmaUnicastApp::m_dport),
                  MakeUintegerChecker<uint16_t>())
    .AddAttribute("WriteSize",
                  "The number of bytes to write",
                  UintegerValue(10000),
                  MakeUintegerAccessor(&RdmaUnicastApp::m_size),
                  MakeUintegerChecker<uint64_t>())
    .AddAttribute("Mtu",
                  "MTU",
                  UintegerValue(1500),
                  MakeUintegerAccessor(&RdmaUnicastApp::m_mtu),
                  MakeUintegerChecker<uint32_t>())
		.AddAttribute("PriorityGroup",
							 	  "The priority group of this flow",
				   		 	  UintegerValue(0),
				   		 	  MakeUintegerAccessor(&RdmaUnicastApp::m_pg),
				   		 	  MakeUintegerChecker<uint16_t>())
		.AddAttribute("Window",
                  "Bound of on-the-fly packets",
                  UintegerValue(0),
                  MakeUintegerAccessor(&RdmaUnicastApp::m_win),
                  MakeUintegerChecker<uint32_t>())
    .AddAttribute("RateFactor",
                  "Bound percentage of the link bandwidth.",
                  DoubleValue(1.0),
                  MakeDoubleAccessor(&RdmaUnicastApp::m_rate_factor),
                  MakeDoubleChecker<double>(0.0, 1.0))
		.AddAttribute("Multicast",
                  "Whether this is a multicast flow",
                  BooleanValue(false),
                  MakeBooleanAccessor(&RdmaUnicastApp::m_multicast),
                  MakeBooleanChecker())
    .AddAttribute("OnFlowFinished",
                  "Callback when the flow finishes",
                  CallbackValue(),
                  MakeCallbackAccessor(&RdmaUnicastApp::m_on_complete),
                  MakeCallbackChecker());
	return tid;
}

void RdmaUnicastApp::SetNodes(NodeContainer n)
{ 
  m_nodes = n;
}

bool RdmaUnicastApp::IsSrc() const
{
  return GetNode()->GetId() == m_src;
}

void RdmaUnicastApp::InitQP()
{
	NS_ASSERT(!m_ud_qp.sq && !m_rc_qp.sq);
  
  Ipv4Address local_ip{GetServerAddress(GetNode())};
  uint16_t local_port;

  if(IsSrc()) {
    local_port = m_sport;
    m_peer_port = m_dport;
    if(m_multicast) {
      m_peer_ip = Ipv4Address{m_dst};
    }
    else {
      m_peer_ip = GetServerAddress(NodeContainer::GetGlobal().Get(m_dst));
    }
  }
  else {
    local_port = m_dport;
    m_peer_port = m_sport;
    if(m_multicast) {
      m_peer_ip = Ipv4Address{m_dst};
    }
    else {
      m_peer_ip = GetServerAddress(NodeContainer::GetGlobal().Get(m_src));
    }
  }

  const Ptr<RdmaHw> rdma{m_node->GetObject<RdmaHw>()};
  
  Ptr<RdmaTxQueuePair> tx;
  Ptr<RdmaRxQueuePair> rx;

	if(m_multicast) {
		m_ud_qp.sq = CreateObject<RdmaUnreliableSQ>(GetNode(), m_pg, local_ip, local_port);
	  m_ud_qp.rq = CreateObject<RdmaUnreliableRQ>(m_ud_qp.sq);
    tx = m_ud_qp.sq;
    rx = m_ud_qp.rq;
	}
	else {
		m_rc_qp.sq = CreateObject<RdmaReliableSQ>(GetNode(), m_pg, local_ip, local_port, m_peer_ip, m_peer_port);
	  m_rc_qp.rq = CreateObject<RdmaReliableRQ>(m_rc_qp.sq);
    tx = m_rc_qp.sq;
    rx = m_rc_qp.rq;
  }
  
  tx->SetMTU(m_mtu);
  tx->SetRateFactor(m_rate_factor);
  rdma->RegisterQP(tx, rx);
}

void RdmaUnicastApp::StartApplication()
{
  NS_LOG_FUNCTION(this);

  if(!IsSrc() && !m_multicast && GetNode()->GetId() != m_dst) {
    StopApplication();
    return;
  }

  InitQP();

  const int flow_count = 1;

  const uint64_t packet_count{m_size / m_mtu};
  NS_LOG_INFO("Sending " << packet_count << " packets");

  auto on_send{[this, packet_count]() {
    NS_LOG_INFO("Sender finishes. Sent " << packet_count << " packets in total");
    StopApplication();
  }};

  if(IsSrc()) {
    if(m_multicast) {
      for(uint64_t i{0}; i < packet_count; i++) {
        RdmaTxQueuePair::SendRequest sr;
        sr.multicast = true;
        sr.payload_size = m_mtu;
        sr.dip = m_peer_ip;
        sr.dport = m_peer_port;

        if(i == packet_count - 1) {
          sr.imm = 1;
          sr.on_send = on_send;
        }

        m_ud_qp.sq->PostSend(sr);
      }
    }
    else {
      for(int i = 0; i < flow_count; i++) {
        RdmaTxQueuePair::SendRequest sr;
        sr.payload_size = m_size;

        if(i == flow_count - 1) {
          sr.imm = 1;
          sr.on_send = on_send;
        }

        m_rc_qp.sq->PostSend(sr);
      }
    }
  }
	else {
    auto on_recv{[this, recv=0](RdmaRxQueuePair::RecvNotif notif) mutable {
      recv++;
      if(notif.has_imm && notif.imm == 1) {
        NS_LOG_INFO("Receiver finishes. Received " << recv << " packets in total at " << Simulator::Now());
        StopApplication();
      }
		}};

    if(m_multicast) {
      m_ud_qp.rq->SetOnRecv(on_recv);
    }
    else {
		  m_rc_qp.rq->SetOnRecv(on_recv);
    }
	}
}

void RdmaUnicastApp::StopApplication()
{
  NS_LOG_FUNCTION(this);
  
  if(!m_on_complete.IsNull()) {
    m_on_complete();
  }
}

} // namespace ns3
