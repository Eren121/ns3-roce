#include "ns3/rdma-config-module.h"

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED(RdmaConfigModule);
NS_LOG_COMPONENT_DEFINE("RdmaConfigModule");

TypeId RdmaConfigModule::GetTypeId()
{
    static TypeId tid = TypeId("ns3::RdmaConfigModule")
    .SetParent<Object>();
    
    return tid;
}

} // namespace ns3