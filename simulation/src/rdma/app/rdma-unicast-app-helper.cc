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

  ApplicationContainer apps;

  for(NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i)
  {
    Ptr<Node> node = *i;
    if(!IsSwitchNode(node)) {
      Ptr<RdmaUnicastApp> app = m_factory.Create<RdmaUnicastApp>();
      app->SetNodes(c);
		  node->AddApplication(app);
      apps.Add(app);
    }
  }

  return apps;
}


} // namespace ns3