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

  // To store dependencies defined in JSON.
  std::unordered_map<std::string, Ptr<RdmaFlow>> flows;

  // Instanciate flows.
  int next_id = 0;
  for(SerializedFlow& info : info_list.flows) {
    if(!info.enable) {
      continue;
    }

    // Generate random ID if it hasn't.
    if(info.id.empty()) {
      info.id = "unassigned-id-" + std::to_string(next_id);
      next_id++;
    }

    ObjectFactory factory{info.path};
    PopulateAttributes(factory, info.attributes);
    Ptr<RdmaFlow> flow = factory.Create<RdmaFlow>();
    flow->Init(info);

    AddFlow(flow);
    flows[info.id] = flow;
    m_names[flow] = info.id;
  }

  // Add dependencies (after having created the flows, otherwise there is no pointer available).
  for(const SerializedFlow& info : info_list.flows) {
    for(const auto& dep : info.dependencies) {
      AddDependency(flows[info.id], flows[dep]);
    }
  }

  // If there is no foreground flow loaded, stop immediately!
  ScheduleNow([this]() {
    if(m_fg_running == 0) {
      NS_LOG_INFO("No foreground flow scheduled!");
      m_on_all_completed();
    }
  });
}

void FlowScheduler::SetOnAllFlowsCompleted(OnAllFlowsCompleted on_all_completed)
{
  m_on_all_completed = std::move(on_all_completed);
}

void FlowScheduler::AddFlow(Ptr<RdmaFlow> flow)
{
  NS_LOG_FUNCTION(this);

  ScheduleAbs(flow->GetStartTime(), MakeLambdaCallback([this, flow]() {
    // Run the flow only if all dependencies have completed.
    // Otherwise, we will try when each of the dependency completes.
    if(m_rem_dependencies[flow] == 0) {
        RunFlow(flow);
    }
  }));
}

void FlowScheduler::OnFlowFinish(Ptr<RdmaFlow> flow)
{
  NS_LOG_INFO("Flow " << flow->GetId() << " completed at " << Simulator::Now().GetSeconds() << "s");
  
  if(!flow->InBackground()) {
    NS_ABORT_IF(m_fg_running == 0);
    m_fg_running--;
  }

  if(m_names.contains(flow)) {
    m_completion_times[m_names[flow]] = Simulator::Now();
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

  if(m_fg_running == 0) {
    NS_LOG_INFO("All foreground flows completed.");
    m_on_all_completed();
  }
  else {
    NS_LOG_INFO("Remains " << m_fg_running << " flows");
  }
}

void FlowScheduler::RunFlow(Ptr<RdmaFlow> flow)
{
  NS_LOG_INFO("Running flow " << flow->GetId());

  if(!flow->InBackground()) {
    m_fg_running++;
  }

  // Mark the flow finished when the flow completes.
  flow->AddOnCompleteCallback([this, flow]() {
    OnFlowFinish(flow);
  });

  // Finally, start the flow.
  flow->StartFlow(m_network);
}

void FlowScheduler::AddDependency(Ptr<RdmaFlow> target, Ptr<RdmaFlow> dependency)
{
  m_dependencies[dependency].insert(target);

  // Only increase the counter for running dependencies.
  if(!dependency->HasCompleted()) {
    m_rem_dependencies[target]++;
  }
}

} // namespace ns3