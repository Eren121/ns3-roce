#include <ns3/rdma-unicast-app-helper.h>
#include <ns3/rdma-unicast-app.h>
#include <ns3/log.h>

namespace ns3 {

RdmaUnicastAppHelper::RdmaUnicastAppHelper()
{
  	m_factory.SetTypeId(RdmaUnicastApp::GetTypeId());
}

void RdmaUnicastAppHelper::SetOnComplete(RdmaUnicastAppHelper::OnComplete on_complete)
{
  m_on_complete = on_complete;
}

void
RdmaUnicastAppHelper::SetAttribute(const std::string& name, const AttributeValue& value)
{
  m_factory.Set(name, value);
}

ApplicationContainer
RdmaUnicastAppHelper::Install(NodeContainer c)
{
  Ptr<RdmaUnicastApp> src_app = m_factory.Create<RdmaUnicastApp>();
  Ptr<RdmaUnicastApp> dst_app = m_factory.Create<RdmaUnicastApp>();
  uint32_t src_node = src_app->GetSrcId();
  uint32_t dst_node = src_app->GetDstId();

  // Finished = when last ACK is received
  src_app->SetAttribute("OnFlowFinished", CallbackValue(m_on_complete));

  for(NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i)
  {
    Ptr<Node> node = *i;
    if(node->GetId() == src_node) {
		  node->AddApplication(src_app);
      src_app->InitQP(c);
    }
    else if(node->GetId() == dst_node) {
		  node->AddApplication(dst_app);
      dst_app->InitQP(c);
    }
  }

  ApplicationContainer apps;
  apps.Add(src_app);
  apps.Add(dst_app);
  return apps;
}


} // namespace ns3