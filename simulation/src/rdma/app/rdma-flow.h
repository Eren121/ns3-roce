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

struct FlowBase
{
  using OnFinish = TracedCallback<>;

  double start_time{}; // In seconds
  uint32_t priority{}; // Priority to use for packets of this flow
  bool background{}; /// If true, completing the flow not required to stop the simulator
  std::string output_file; // Path that can be used by the flow to store logging information
  double bandwidth_percent{1}; // Can be used by the flow to limit the flow bandwidth to a part of the available bandwidth.
};

struct FlowUnicast
{
  using Tag = rfl::Literal<"unicast">;

  rfl::Flatten<FlowBase> base; 

  uint32_t src{}; // Source nod ID
  uint32_t dst{}; // Destination node ID
  double size{}; // Count of bytes to write
  bool reliable{};
};

struct FlowMulticast
{
  using Tag = rfl::Literal<"multicast">;

  rfl::Flatten<FlowBase> base; 

  uint32_t src{}; // Source nod ID
  uint32_t group{}; // Multicast group
  double size{}; // Count of bytes to write
};

struct FlowAllgather
{
  using Tag = rfl::Literal<"allgather">;

  rfl::Flatten<FlowBase> base; 

  double size{}; // Count of bytes to write per node
  uint32_t group{}; // Multicast group
};

using ScheduledFlow = rfl::TaggedUnion<"type", FlowUnicast, FlowMulticast, FlowAllgather>;

struct ScheduledFlowList
{
  std::vector<ScheduledFlow> flows;
};

class FlowScheduler
{
public:
  using OnAllFlowsCompleted = std::function<void()>;

  void SetOnAllFlowsCompleted(OnAllFlowsCompleted on_all_completed)
  {
    m_on_all_completed = std::move(on_all_completed);
  }

  /**
   * Has to be called for initialization.
   */
  void SetNetwork(std::shared_ptr<RdmaNetwork> network)
  {
    m_network = network;
  }

  void AddFlowsFromFile(fs::path json_path);

private:
  void AddFlow(ScheduledFlow flow);
  static void RunFlow(FlowScheduler* self, int flow_id) { self->RunFlow(flow_id); }
  void RunFlow(int flow_id);
  void RunFlow(int flow_id, const FlowUnicast& flow);
  void RunFlow(int flow_id, const FlowMulticast& flow);
  void RunFlow(int flow_id, const FlowAllgather& flow);

  static void OnFlowFinish(FlowScheduler* self, int flow_id) { self->OnFlowFinish(flow_id); }
  void OnFlowFinish(int flow_id);
  uint16_t GetNextUniquePort();

  std::shared_ptr<RdmaNetwork> m_network;
  OnAllFlowsCompleted m_on_all_completed;
  std::vector<ScheduledFlow> m_flows;
  int m_fg_running{}; // Foreground flows
  int m_bg_running{}; // Background flows
  uint16_t m_next_unique_port{1000}; // Pool of unique port to use for flows to avoid collisions
};

// Not in ns3 by default
template<typename Func>
auto MakeLambdaCallback(Func&& func)
{
  return Callback<void>(std::forward<Func>(func), true, true);
}

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