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
  SerializedFlowList flow_list{rfl::json::read<SerializedFlowList>(read_all_file(json_flows)).value()};
  for(SerializedFlow& flow : flow_list.flows) {
    AddFlow(std::move(flow));
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

void FlowScheduler::AddFlow(SerializedFlow serialized_flow_arg)
{
  NS_LOG_FUNCTION(this);

  if(!serialized_flow_arg.enable) {
    return;
  }
  
  const int flow_id{m_flows.size()};
  m_flows.push_back(std::move(serialized_flow_arg));
  SerializedFlow& serialized_flow{m_flows.back()};

  if(serialized_flow.in_background) {
    m_bg_running++;
  }
  else {
    m_fg_running++;
  }

  ScheduleAbs(serialized_flow.start_time, MakeBoundCallback(&FlowScheduler::RunFlow, this, flow_id));
}

void FlowScheduler::OnFlowFinish(int flow_id)
{
  NS_LOG_FUNCTION(this << flow_id);
  
  const SerializedFlow& flow{m_flows.at(flow_id)};
	
  NS_LOG_INFO("Flow " << flow_id << " completed");

  if(flow.in_background) {
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
}

void FlowScheduler::RunFlow(int flow_id)
{
  NS_LOG_FUNCTION(this << flow_id);
  
  NS_LOG_INFO("Running flow " << flow_id);
  const SerializedFlow& flow{m_flows.at(flow_id)};


  ObjectFactory factory{flow.path};
  PopulateAttributes(factory, flow.attributes);

  Ptr<RdmaFlow> flow_instance = factory.Create<RdmaFlow>();
  flow_instance->StartFlow(m_network, [this, flow_id]() {
    OnFlowFinish(flow_id);
  });
  
  m_running_flows.push_back(flow_instance);
}

} // namespace ns3