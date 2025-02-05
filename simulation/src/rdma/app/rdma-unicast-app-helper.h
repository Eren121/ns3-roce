#pragma once

#include <ns3/application-container.h>
#include <ns3/object-factory.h>
#include <ns3/node-container.h>

namespace ns3 {

class RdmaUnicastAppHelper
{
public:
  using OnComplete = Callback<void>;

  RdmaUnicastAppHelper();
  
  void SetOnComplete(OnComplete on_complete);
  void SetAttribute(const std::string& name, const AttributeValue& value);
  ApplicationContainer Install(NodeContainer c);

private:
  ObjectFactory m_factory;
  OnComplete m_on_complete;
};

} // namespace ns3