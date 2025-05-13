#include "ns3/rdma-flow-scheduler.h"
#include "ns3/rdma-flow.h"
#include "ns3/rdma-network.h"
#include "ns3/ag-app-helper.h"
#include "ns3/application-container.h"
#include <fstream>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("FlowScheduler");

FlowScheduler::FlowScheduler(RdmaNetwork& network, const fs::path& json_flows)
  : m_network{network}
{
  SerializedFlowList info_list{rfl::json::read<SerializedFlowList>(read_all_file(json_flows)).value()};
  for(SerializedFlow& info : info_list.flows) {
    AddSerializedFlow(info);
  }

  // If there is no foreground flow loaded, stop immediately!
  if(m_fg_running == 0) {
		NS_LOG_INFO("No foreground flow scheduled!");
    ScheduleNow([this]() {
      m_on_all_completed();
    });
  }
}

void FlowScheduler::SetOnAllFlowsCompleted(OnAllFlowsCompleted on_all_completed)
{
  m_on_all_completed = std::move(on_all_completed);
}

void FlowScheduler::AddSerializedFlow(SerializedFlow info)
{
  if(!info.enable) {
    return;
  }

  ObjectFactory factory{info.path};
  PopulateAttributes(factory, info.attributes);
  Ptr<RdmaFlow> flow = factory.Create<RdmaFlow>();
  flow->Init(info);

  AddFlow(flow);
}

void FlowScheduler::AddFlow(Ptr<RdmaFlow> flow)
{
  NS_LOG_FUNCTION(this);
  
  m_flows[flow->GetId()] = flow;

  if(flow->InBackground()) {
    m_bg_running++;
  }
  else {
    m_fg_running++;
  }

  // Run the flow only if all dependencies have completed.
  // Otherwise, we will try when each of the dependency completes.
  if(m_rem_dependencies[flow] == 0) {
    ScheduleAbs(flow->GetStartTime(), MakeLambdaCallback([this, flow]() {
      // Check again because in the current event, dependencies may have been added.
      // If the user adds dependencies after adding the flow.
      if(m_rem_dependencies[flow] == 0) {
          RunFlow(flow);
      }
    }));
  }
}

void FlowScheduler::OnFlowFinish(Ptr<RdmaFlow> flow)
{
  NS_LOG_INFO("Flow " << flow->GetId() << " completed");

  if(flow->InBackground()) {
    m_bg_running--;
  }
  else {
    m_fg_running--;
  }
  
  if(m_fg_running == 0) {
    NS_LOG_INFO("All foreground flows completed.");
    m_on_all_completed();
  }
  else {
    NS_LOG_INFO("Remains " << m_fg_running << " flows");
  }

  // Check if any dependency is resolved.
  for(Ptr<RdmaFlow> target : m_dependencies[flow]) {
    NS_ABORT_IF(m_rem_dependencies[target] == 0);
    m_rem_dependencies[target]--;
    
    if(m_rem_dependencies[target] == 0 && Simulator::Now() >= target->GetStartTime()) {
      // All dependencies have completed, run the flow.
      // If the start time is not yet reached, the flow will be scheduled normally later.
      RunFlow(target);
    }
  }
}

void FlowScheduler::RunFlow(Ptr<RdmaFlow> flow)
{
  NS_LOG_INFO("Running flow " << flow->GetId());

  // Mark the flow finished when the flow completes.
  flow->AddOnCompleteCallback([this, flow]() {
    OnFlowFinish(flow);
  });

  // Finally, start the flow.
  flow->StartFlow(m_network);
}

void FlowScheduler::AddDependency(Ptr<RdmaFlow> target, Ptr<RdmaFlow> dependency)
{
  m_dependencies[dependency].push_back(target);

  // Only increase the counter for running dependencies.
  if(!dependency->HasCompleted()) {
    m_rem_dependencies[target]++;
  }
}

} // namespace ns3