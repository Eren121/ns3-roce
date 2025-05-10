#include "ns3/rdma-flow.h"
#include "ns3/rdma-unicast-app-helper.h"
#include "ns3/ag-app-helper.h"
#include "ns3/application-container.h"
#include <fstream>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("FlowScheduler");

FlowScheduler::FlowScheduler()
  : m_network{RdmaNetwork::GetInstance()}
{
}

void FlowScheduler::AddFlowsFromFile(fs::path json_path)
{
  NS_LOG_FUNCTION(this << json_path);

  JsonFlowList list{rfl::json::read<JsonFlowList>(read_all_file(json_path)).value()};
  for(JsonFlow& flow : list.flows) {
    AddFlow(std::move(flow));
  }
}

void FlowScheduler::AddFlow(JsonFlow flow_)
{
  NS_LOG_FUNCTION(this);
  
  const int flow_id{m_flows.size()};
  m_flows.push_back(std::move(flow_));

  JsonFlow& flow{m_flows.back()};
  flow.SetMissingValues();

  if(flow.background.value()) {
    m_bg_running++;
  }
  else {
    m_fg_running++;
  }

  ScheduleAbs(flow.start_time.value(), MakeBoundCallback(&FlowScheduler::RunFlow, this, flow_id));
}

void FlowScheduler::OnFlowFinish(int flow_id)
{
  NS_LOG_FUNCTION(this << flow_id);
  
  const JsonFlow& flow{m_flows.at(flow_id)};
	
  NS_LOG_INFO("Flow " << flow_id << " completed");

  if(flow.background.value()) {
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

uint16_t FlowScheduler::GetNextUniquePort()
{
  NS_LOG_FUNCTION(this);
  return m_next_unique_port++;
}

void FlowScheduler::RunFlow(int flow_id)
{
  NS_LOG_FUNCTION(this << flow_id);
  
  NS_LOG_INFO("Running flow " << flow_id);
  const JsonFlow& flow{m_flows.at(flow_id)};

  if(flow.type == "unicast") {
    RunUnicast(flow_id, flow);
  }
  else if(flow.type == "multicast") {
    RunMulticast(flow_id, flow);
  }
  else if(flow.type == "allgather") {
    RunAllgather(flow_id, flow);
  }
}

void FlowScheduler::RunUnicast(int flow_id, const JsonFlow& flow)
{
  NS_LOG_FUNCTION(this);

  RdmaUnicastAppHelper app_helper;
  PopulateAttributes(app_helper.GetFactory(), flow.parameters);
  app_helper.SetOnComplete(MakeLambdaCallback([this, flow_id]() {
    OnFlowFinish(flow_id);
  }));

  const uint32_t src{flow.parameters.at("SrcNode").to_int().value()};
  const uint32_t dst{flow.parameters.at("DstNode").to_int().value()};

  NodeContainer nodes;
  nodes.Add(m_network.FindServer(src));
  nodes.Add(m_network.FindServer(dst));

  ApplicationContainer apps{app_helper.Install(nodes)};
  apps.Start(flow.start_time.value());
}

void FlowScheduler::RunMulticast(int flow_id, const JsonFlow& flow)
{
  NS_LOG_FUNCTION(this);

  RdmaUnicastAppHelper app_helper;
  PopulateAttributes(app_helper.GetFactory(), flow.parameters);
  app_helper.SetAttribute("Multicast", BooleanValue(true));
  app_helper.SetOnComplete(MakeLambdaCallback([this, flow_id]() {
    OnFlowFinish(flow_id);
  }));

  const uint32_t group{flow.parameters.at("McastGroup").to_int().value()};
  NodeContainer nodes{m_network.FindMcastGroup(group)};
  ApplicationContainer apps{app_helper.Install(nodes)};
  apps.Start(flow.start_time.value());
}

void FlowScheduler::RunAllgather(int flow_id, const JsonFlow& flow)
{
  NS_LOG_FUNCTION(this);

  AgAppHelper app_helper;
  PopulateAttributes(app_helper.GetFactory(), flow.parameters);
  app_helper.SetAttribute("OnAllFinished", CallbackValue(MakeLambdaCallback([this, flow_id]() {
    OnFlowFinish(flow_id);
  })));

  const uint32_t group{flow.parameters.at("McastGroup").to_int().value()};
  NodeContainer nodes{m_network.FindMcastGroup(group)};
  ApplicationContainer apps{app_helper.Install(nodes)};
  apps.Start(flow.start_time.value());
}

} // namespace ns3