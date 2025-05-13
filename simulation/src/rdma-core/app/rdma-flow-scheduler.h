#pragma once

#include "ns3/filesystem.h"
#include "ns3/rdma-flow.h"
#include <functional>
#include <vector>
#include <unordered_map>
#include <unordered_set>

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

  /**
   * Makes the flow `target` can only run when `dependency` has completed.
   */
  void AddDependency(Ptr<RdmaFlow> target, Ptr<RdmaFlow> dependency);

  void AddSerializedFlow(SerializedFlow flow);
  void AddFlow(Ptr<RdmaFlow> flow);

private:
  void RunFlow(Ptr<RdmaFlow> flow);
  void OnFlowFinish(Ptr<RdmaFlow> flow);

private:
  RdmaNetwork& m_network;
  std::unordered_map<int, Ptr<RdmaFlow>> m_flows;
  //! Keep running flows for debugging.
  std::unordered_set<Ptr<RdmaFlow>> m_running_flows;
  //! Foreground flows count.
  int m_fg_running{};
  //! Background flows count.
  int m_bg_running{};
  //! To call when all flows have completed.
  OnAllFlowsCompleted m_on_all_completed;

  //! All flow dependencies.
  //! m_dependencies[i][j] indicates that `j` depends on `i`.
  std::unordered_map<Ptr<RdmaFlow>, std::vector<Ptr<RdmaFlow>>> m_dependencies;
  //! Store, for each flow, the count of uncompleted dependencies.
  std::unordered_map<Ptr<RdmaFlow>, int> m_rem_dependencies;
};

} // namespace ns3