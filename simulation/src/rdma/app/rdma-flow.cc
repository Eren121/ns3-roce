#include "ns3/rdma-flow.h"
#include "ns3/ag-app-helper.h"
#include "ns3/application-container.h"
#include <fstream>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("RdmaFlow");
NS_OBJECT_ENSURE_REGISTERED(RdmaFlow);

TypeId RdmaFlow::GetTypeId()
{
    static TypeId tid = []() {
        static TypeId tid = TypeId("ns3::RdmaFlow");
        tid.SetParent<Object>();
        return tid;
    }();
  
    return tid;
}

} // namespace ns3