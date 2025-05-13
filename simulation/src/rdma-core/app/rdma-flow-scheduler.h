#pragma once

#include "ns3/filesystem.h"
#include "ns3/rdma-flow.h"
#include <functional>
#include <vector>

namespace ns3 {

class RdmaNetwork;

/**
 * Manage all the flows.
 */
class FlowScheduler
{
public:
  //! Type of the callback called when all foreground flows have completed.
  using OnAllFlowsCompleted = std::function<void()>;

  //! Load all flows from the JSON file.
  FlowScheduler(RdmaNetwork& network, const fs::path& json_flows);
  
  //! Set the callback to call when all flows have completed.
  void SetOnAllFlowsCompleted(OnAllFlowsCompleted on_all_completed);

private:
  void AddFlow(SerializedFlow flow);

  static void RunFlow(FlowScheduler* self, int flow_id) { self->RunFlow(flow_id); }
  void RunFlow(int flow_id);

  void OnFlowFinish(int flow_id);

private:
  RdmaNetwork& m_network;

  OnAllFlowsCompleted m_on_all_completed;
  std::vector<SerializedFlow> m_flows;
  std::vector<Ptr<RdmaFlow>> m_running_flows;
  int m_fg_running{}; // Foreground flows
  int m_bg_running{}; // Background flows
};

} // namespace ns3