#pragma once

#include "ns3/callback.h"
#include "ns3/rdma-reflection-helper.h"
#include "ns3/filesystem.h"
#include "ns3/rdma-network.h"
#include <memory>
#include <cstdint>
#include <variant>
#include <functional>

namespace ns3 {

struct JsonFlow
{
  std::string type;
  std::optional<Time> start_time;
  std::optional<bool> background; //! A background flow is not required to complete to finish the simulation
  rfl::Object<rfl::Generic> parameters;

  void SetMissingValues()
  {
    if(!start_time) { start_time.emplace(Time{}); }
    if(!background) { background.emplace(false); }
  }
};

struct JsonFlowList
{
  std::vector<JsonFlow> flows;
};

class FlowScheduler
{
public:
  using OnAllFlowsCompleted = std::function<void()>;

  FlowScheduler();
  
  void SetOnAllFlowsCompleted(OnAllFlowsCompleted on_all_completed)
  {
    m_on_all_completed = std::move(on_all_completed);
  }

  void AddFlowsFromFile(fs::path json_path);

private:
  void AddFlow(JsonFlow flow);
  static void RunFlow(FlowScheduler* self, int flow_id) { self->RunFlow(flow_id); }
  void RunFlow(int flow_id);
  void RunUnicast(int flow_id, const JsonFlow& flow);
  void RunMulticast(int flow_id, const JsonFlow& flow);
  void RunAllgather(int flow_id, const JsonFlow& flow);

  void OnFlowFinish(int flow_id);
  uint16_t GetNextUniquePort();

  RdmaNetwork& m_network;
  OnAllFlowsCompleted m_on_all_completed;
  std::vector<JsonFlow> m_flows;
  int m_fg_running{}; // Foreground flows
  int m_bg_running{}; // Background flows
  uint16_t m_next_unique_port{1000}; // Pool of unique port to use for flows to avoid collisions
};

template<auto>
class HelperCallback {};

template<typename R,
         typename T,
         typename... Args,
         R(T::*mem)(Args...)>
class HelperCallback<mem>
{
  static auto Call(T* obj, Args... args)
  {
    return obj->*mem(args...);
  }

  static auto Make(T* obj, Args... bound)
  {
    return MakeBoundCallback(
      &HelperCallback::Call,
      obj,
      bound...);
  }
};

template<auto Mem, typename T, typename... Args>
auto MakeMemCallback(T* obj, Args... bound)
{
  return HelperCallback<Mem>::Make(obj, bound...);
}

} // namespace ns3