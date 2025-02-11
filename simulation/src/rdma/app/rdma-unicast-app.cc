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
    .AddAttribute("OnFlowFinished",
                  "Callback when the flow finishes",
                  CallbackValue(),
                  MakeCallbackAccessor(&RdmaUnicastApp::m_on_complete),
                  MakeCallbackChecker());
	return tid;
}

void RdmaUnicastApp::InitQP(NodeContainer nodes)
{
	NS_ASSERT(!m_qp.sq);
  
	const Ipv4Address sip{GetServerAddress(nodes.Get(m_src))};
	const Ipv4Address dip{GetServerAddress(nodes.Get(m_dst))};
	
	Ptr<Node> node = GetNode();
	if(node->GetId() == m_src) {
		m_qp.sq = CreateObject<RdmaReliableSQ>(GetNode(), m_pg, sip, m_sport, dip, m_dport);
	}
	else if(node->GetId() == m_dst) {
		m_qp.sq = CreateObject<RdmaReliableSQ>(GetNode(), m_pg, dip, m_dport, sip, m_sport);
	}
	else {
		NS_ABORT_MSG("Node is neither source or destination");
	}
		
	m_qp.sq->SetMTU(m_mtu);
	m_qp.rq = CreateObject<RdmaReliableRQ>(m_qp.sq);
 
  const Ptr<RdmaHw> rdma{m_node->GetObject<RdmaHw>()};
  rdma->RegisterQP(m_qp.sq, m_qp.rq);
}

void RdmaUnicastApp::StartApplication()
{
  NS_LOG_FUNCTION(this);

  const int flow_count = 1;

  {
    for(int i = 0; i < flow_count; i++) {
      RdmaTxQueuePair::SendRequest sr;
      sr.payload_size = m_size;

      if(i == flow_count - 1) {
        sr.on_send = [this]() {
          NS_LOG_LOGIC("Sender finishes");
          StopApplication();
        };
      }

      m_qp.sq->PostSend(sr);
    }
  }
	{
		m_qp.rq->SetOnRecv([this, flow_count, recv=0](RdmaRxQueuePair::RecvNotif notif) mutable {
      recv++;
      if(recv == flow_count) {
        NS_LOG_LOGIC("Receiver finishes");
        StopApplication();
      }
		});
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
