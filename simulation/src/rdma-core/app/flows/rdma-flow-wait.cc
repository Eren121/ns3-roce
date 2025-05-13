#include "ns3/rdma-flow-wait.h"
#include "ns3/simulator.h"

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED(RdmaFlowWait);
NS_LOG_COMPONENT_DEFINE("RdmaFlowWait");

TypeId RdmaFlowWait::GetTypeId()
{
  static TypeId tid = []() {
    static TypeId tid = TypeId("ns3::RdmaFlowWait");

    tid.SetParent<RdmaFlow>();
    tid.AddConstructor<RdmaFlowWait>();
    
    AddTimeAttribute(tid,
      "Time",
      "Amount of time to wait.",
      &RdmaFlowWait::m_time);

    return tid;
  }();
  
  return tid;
}

void RdmaFlowWait::OnFlowStarted(RdmaNetwork& network)
{
  Simulator::Schedule(m_time, MakeLambdaCallback([this]() {
    NotifyComplete();
  }));
}

} // namespace ns3