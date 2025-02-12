#include <ns3/rdma-unicast-app.h>
#include <ns3/qbb-net-device.h>
#include <ns3/rdma-hw.h>
#include <ns3/log.h>

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
  m_peer_ip = (m_multicast ? Ipv4Address{m_dst} : GetServerAddress(m_nodes.Get(m_dst)));
  uint16_t local_port;

  if(IsSrc()) {
    local_port = m_sport;
    m_peer_port = m_dport;
  }
  else {
    local_port = m_dport;
    m_peer_port = m_sport;
  }

  const Ptr<RdmaHw> rdma{m_node->GetObject<RdmaHw>()};
  
	if(m_multicast) {
		m_ud_qp.sq = CreateObject<RdmaUnreliableSQ>(GetNode(), m_pg, local_ip, local_port);
	  m_ud_qp.sq->SetMTU(m_mtu);
	  m_ud_qp.rq = CreateObject<RdmaUnreliableRQ>(m_ud_qp.sq);
    rdma->RegisterQP(m_ud_qp.sq, m_ud_qp.rq);
	}
	else {
		m_rc_qp.sq = CreateObject<RdmaReliableSQ>(GetNode(), m_pg, local_ip, local_port, m_peer_ip, m_peer_port);
	  m_rc_qp.sq->SetMTU(m_mtu);
	  m_rc_qp.rq = CreateObject<RdmaReliableRQ>(m_rc_qp.sq);
    rdma->RegisterQP(m_rc_qp.sq, m_rc_qp.rq);
	}
}

void RdmaUnicastApp::StartApplication()
{
  NS_LOG_FUNCTION(this);

  if(!IsSrc() && !m_multicast) {
    StopApplication();
    return;
  }

  InitQP();

  const int flow_count = 1;

  auto on_send{[this]() {
    NS_LOG_LOGIC("Sender finishes");
    StopApplication();
  }};

  if(IsSrc()) {
    if(m_multicast) {
      const uint64_t packet_count{m_size / m_mtu};
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
    auto on_recv{[this](RdmaRxQueuePair::RecvNotif notif) mutable {
      if(notif.has_imm && notif.imm == 1) {
        NS_LOG_LOGIC("Receiver finishes");
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
