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

RdmaFlow::RdmaFlow()
{
    static int next_id = 0;
    m_id = (next_id++);
}

int RdmaFlow::GetId() const
{
    return m_id;
}

void RdmaFlow::StartFlow(RdmaNetwork& network)
{
    OnFlowStarted(network);  
}

bool RdmaFlow::HasCompleted() const
{
    return m_completed;
}

void RdmaFlow::AddOnCompleteCallback(OnComplete on_complete)
{
    m_on_complete.push_back(on_complete);
}

void RdmaFlow::NotifyComplete()
{
    NS_ABORT_IF(m_completed);
    m_completed = true;

    for(auto& callback : m_on_complete) {
        callback();
    }
}

} // namespace ns3