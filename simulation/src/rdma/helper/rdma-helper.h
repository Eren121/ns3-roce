#pragma once

#include "ns3/simulator.h"
#include "ns3/node.h"
#include <map>
#include <set>
#include <functional>

namespace ns3 {

/**
 * Schedule an event by an absolute time rather than a relative one like `Simulator::Schedule()`.
 * If the time is in the past, schedule immediately.
 * 
 * @param when Event absolute time.
 */
template<typename... Args>
auto ScheduleAbs(Time when, Args&&... args)
{
  const Time now{Simulator::Now()};
  return Simulator::Schedule(Max(Time{}, when - now), std::forward<Args>(args)...);
}

template<typename... Args>
auto ScheduleNow(Args&&... args)
{
  return Simulator::Schedule(Simulator::Now(), std::forward<Args>(args)...);
}

using node_id_t = uint32_t;
using iface_id_t = uint32_t;
using byte_t = uint64_t;
using priority_t = uint32_t;

/**
 * Same as `NodeContainer` but indexed on node ID and not index in the container.
 */
class NodeMap
{
public:
  using value_type = Ptr<Node>;
  using size_type = size_t;

private:
  struct Comp
  {
    bool operator()(Ptr<Node> a, Ptr<Node> b) const
    {
      return a->GetId() < b->GetId();
    }
  };

public:
  auto begin() const { return m_set.begin(); }
  auto end() const { return m_set.end(); }

  void Add(Ptr<Node> node)
  {
    m_map[node->GetId()] = node;
    m_set.insert(node);
  }

  void Add(const NodeMap& others)
  {
    for(Ptr<Node> node : others) {
      Add(node);
    }
  }

  Ptr<Node> Get(node_id_t id) const
  {
    return m_map.at(id);
  }

private:
  std::map<node_id_t, Ptr<Node>> m_map;
  std::set<Ptr<Node>, Comp> m_set;
};

/**
 * Repetition of an event.
 * When instanciated, is paused.
 */
class PeriodicEvent
{
public:
  using Task = std::function<void()>;

  PeriodicEvent() = default;
  ~PeriodicEvent();
  PeriodicEvent(const PeriodicEvent&) = delete;
  PeriodicEvent& operator=(const PeriodicEvent&) = delete;

  void SetTask(Task task);
  void SetInterval(Time itv);

  void Resume();
  void Pause();

private:
  void RunTask();
  void Reschedule();

private:
  bool m_paused{true};
  Time m_itv;
  Time m_start;
  Time m_stop;
  Time m_last_invoked;
  Task m_task;
  EventId m_event;
};

}