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

  /**
   * Note: We consider that at the end of the current event, all dependencies of the flow should have 
   * been registered, otherwise it will be not taken into account.
   */
  void AddFlow(Ptr<RdmaFlow> flow);

  const auto& GetAllCompletionTimes() const
  {
    return m_completion_times;
  }

private:
  void RunFlow(Ptr<RdmaFlow> flow);
  void OnFlowFinish(Ptr<RdmaFlow> flow);

private:
  RdmaNetwork& m_network;
  //! Foreground flows count.
  int m_fg_running{};
  //! To call when all flows have completed.
  OnAllFlowsCompleted m_on_all_completed;

  //! All flow dependencies.
  //! m_dependencies[i][j] indicates that `j` depends on `i`.
  std::unordered_map<Ptr<RdmaFlow>, std::unordered_set<Ptr<RdmaFlow>>> m_dependencies;
  //! Store, for each flow, the count of uncompleted dependencies.
  std::unordered_map<Ptr<RdmaFlow>, int> m_rem_dependencies;
  //! Stores the completion time of each flow.
  std::unordered_map<std::string, Time> m_completion_times;
  //! Stores the name of each flow.
  std::unordered_map<Ptr<RdmaFlow>, std::string> m_names;
};

} // namespace ns3