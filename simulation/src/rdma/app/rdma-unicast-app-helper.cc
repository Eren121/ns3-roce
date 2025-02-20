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

ObjectFactory&
RdmaUnicastAppHelper::GetFactory()
{
  return m_factory;
}

void
RdmaUnicastAppHelper::SetAttribute(const std::string& name, const AttributeValue& value)
{
  m_factory.Set(name, value);
}

ApplicationContainer
RdmaUnicastAppHelper::Install(NodeContainer c)
{
  ApplicationContainer apps;

  for(NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i)
  {
    Ptr<Node> node = *i;
    if(!IsSwitchNode(node)) {
      Ptr<RdmaUnicastApp> app = m_factory.Create<RdmaUnicastApp>();
      app->SetNodes(c);
		  node->AddApplication(app);
      apps.Add(app);
      
      if(app->IsSrc()) {
        app->SetAttribute("OnFlowFinished", CallbackValue(m_on_complete));
      }
    }
  }

  return apps;
}


} // namespace ns3