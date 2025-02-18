#include "ns3/rdma-flow.h"
#include "ns3/rdma-unicast-app-helper.h"
#include "ns3/rdma-allgather-helper.h"
#include "ns3/application-container.h"
#include <fstream>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("FlowScheduler");

void FlowScheduler::AddFlowsFromFile(fs::path json_path)
{
  NS_LOG_FUNCTION(this << json_path);

  ScheduledFlowList flows{rfl::json::read<ScheduledFlowList>(raf::read_all_file(json_path)).value()};
  for(ScheduledFlow& flow : flows.flows) {
    AddFlow(std::move(flow));
  }
}

void FlowScheduler::AddFlow(ScheduledFlow flow_)
{
  NS_LOG_FUNCTION(this);
  
  const int flow_id{m_flows.size()};
  m_flows.push_back(std::move(flow_));


  ScheduledFlow& flow{m_flows.back()};
  
  auto& base{*flow.visit([](auto& flow) {
    return &flow.base();
  })};

  if(base.background) {
    m_bg_running++;
  }
  else {
    m_fg_running++;
  }

	if(!base.output_file.empty()) {
		// Make relative to config directory
		base.output_file = m_network->GetConfig().FindFile(base.output_file);
	}

  const Time when = Seconds(base.start_time) - Simulator::Now();
  Simulator::Schedule(when, MakeBoundCallback(&FlowScheduler::RunFlow, this, flow_id));
}

void FlowScheduler::OnFlowFinish(int flow_id)
{
  NS_LOG_FUNCTION(this << flow_id);
  
  const ScheduledFlow& sflow{m_flows.at(flow_id)};
	
  NS_LOG_INFO("Flow " << flow_id << " completed");

  const auto& base{sflow.visit([](const auto& flow) {
    return flow.base();
  })};

  if(base.background) {
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
  
  const ScheduledFlow& sflow{m_flows.at(flow_id)};
  sflow.visit([&](const auto& flow){

    if(flow.base().bandwidth_percent <= 0.0) {
      NS_LOG_INFO("Skipping flow because bandwidth percent is zero");
      return;
    }

    NS_LOG_INFO("Running flow " << rfl::json::write(flow));

    RunFlow(flow_id, flow);
  });
}

void FlowScheduler::RunFlow(int flow_id, const FlowUnicast& flow)
{
  NS_LOG_FUNCTION(this);

  const Ipv4Address sip{m_network->FindNodeIp(flow.src)};
  const Ipv4Address dip{m_network->FindNodeIp(flow.dst)};
  const RdmaConfig& config{m_network->GetConfig()};

	uint32_t win;

	if(config.has_win) {
    win = m_network->FindBdp(flow.src, flow.dst);
	}
	else {
		win = 0;
	}

  NS_ASSERT_MSG(flow.reliable, "Only reliable supported");

  RdmaUnicastAppHelper app_helper;
  app_helper.SetAttribute("SrcNode", UintegerValue(flow.src));
  app_helper.SetAttribute("DstNode", UintegerValue(flow.dst));
  app_helper.SetAttribute("SrcPort", UintegerValue(GetNextUniquePort()));
  app_helper.SetAttribute("DstPort", UintegerValue(GetNextUniquePort()));
  app_helper.SetAttribute("WriteSize", UintegerValue(flow.size));
  app_helper.SetAttribute("PriorityGroup", UintegerValue(flow.base().priority));
  app_helper.SetAttribute("Window", UintegerValue(win));
  app_helper.SetAttribute("Mtu", UintegerValue(m_network->GetMtuBytes()));
  app_helper.SetAttribute("Multicast", BooleanValue(false));
  app_helper.SetAttribute("RateFactor", DoubleValue(flow.base().bandwidth_percent));
  app_helper.SetOnComplete(MakeBoundCallback(&FlowScheduler::OnFlowFinish, this, flow_id));

  NodeContainer nodes;
  nodes.Add(m_network->FindServer(flow.src));
  nodes.Add(m_network->FindServer(flow.dst));

  ApplicationContainer apps{app_helper.Install(nodes)};
  apps.Start(Seconds(flow.base().start_time));
}

void FlowScheduler::RunFlow(int flow_id, const FlowMulticast& flow)
{
  NS_LOG_FUNCTION(this);

  const Ipv4Address sip{m_network->FindNodeIp(flow.src)};
  const Ipv4Address dip{flow.group};
  const RdmaConfig& config{m_network->GetConfig()};

  // Minimum BDP between all nodes of the group would be better,
  // but does not change anything if all nodes belong to the group.
	const uint32_t win{m_network->GetMaxBdp()};

  RdmaUnicastAppHelper app_helper;
  app_helper.SetAttribute("SrcNode", UintegerValue(flow.src));
  app_helper.SetAttribute("DstNode", UintegerValue(flow.group));
  app_helper.SetAttribute("SrcPort", UintegerValue(GetNextUniquePort()));
  app_helper.SetAttribute("DstPort", UintegerValue(GetNextUniquePort()));
  app_helper.SetAttribute("WriteSize", UintegerValue(flow.size));
  app_helper.SetAttribute("PriorityGroup", UintegerValue(flow.base().priority));
  app_helper.SetAttribute("Window", UintegerValue(win));
  app_helper.SetAttribute("Mtu", UintegerValue(m_network->GetMtuBytes()));
  app_helper.SetAttribute("Multicast", BooleanValue(true));
  app_helper.SetAttribute("RateFactor", DoubleValue(flow.base().bandwidth_percent));
  app_helper.SetOnComplete(MakeBoundCallback(&FlowScheduler::OnFlowFinish, this, flow_id));

  NodeContainer nodes{m_network->FindMcastGroup(flow.group)};
  ApplicationContainer apps{app_helper.Install(nodes)};
  apps.Start(Seconds(flow.base().start_time));
}

void FlowScheduler::RunFlow(int flow_id, const FlowAllgather& flow)
{
  NS_LOG_FUNCTION(this);

	const uint32_t win{m_network->GetMaxBdp()};
  NodeContainer nodes{m_network->FindMcastGroup(flow.group)};
  RdmaAllgatherHelper helper(flow, win);
  helper.SetOnComplete([this, flow_id]() {
    OnFlowFinish(flow_id);
  });

  ApplicationContainer apps{helper.Install(nodes)};
  apps.Start(Seconds(flow.base().start_time));
}

} // namespace ns3