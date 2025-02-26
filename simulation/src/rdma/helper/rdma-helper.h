#pragma once

#include "ns3/simulator.h"
#include "ns3/node-container.h"
#include "ns3/node.h"
#include <map>
#include <set>
#include <functional>
#include <vector>
#include <variant>
#include <sstream>
#include <stdexcept>

#ifndef DISALLOW_COPY
#define DISALLOW_COPY(T) \
  T(const T&) = delete; \
  T& operator=(const T&) = delete
#endif

namespace ns3 {

template<typename T>
inline T CeilDiv(T num, T den)
{
  return (num + den - 1) / den;
}

template<typename T>
T PositiveModulo(T i, T n)
{
  return (i % n + n) % n;
}

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

/**
 * Lambdas does not work with MakeBoundCallback.
 */
template<typename... Args, typename Func>
auto MakeLambdaCallback(Func&& func)
{
  class Lambda : public SimpleRefCount<Lambda>
  {
  public:
    Lambda(Func func) : m_func(std::move(func)) {}
    void Run(Args... args) const { m_func(args...); }

  private:
    Func m_func;
  };

  Ptr<Lambda> lambda = Create<Lambda>(std::forward<Func>(func));
  return MakeCallback(&Lambda::Run, lambda);
}

using group_id_t = uint32_t;
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

template<typename T>
void EnsureSizeAtleast(T&& vec, size_t size)
{
  if(vec.size() < size) {
    vec.resize(size);
  }
}

struct Ranges
{
public:
  using Index = unsigned int;

  struct Range
  {
    Index first;
    Index last;

    static const char delimiter = '-';
  };

  static const char wildcard = '*';
  static const char element_delimiter = ',';

  using Element = std::variant<Index, Range>;

  bool IsWildcard() const
  {
    return m_wildcard;
  }

  auto begin() const { return m_elements.begin(); }
  auto end() const { return m_elements.end(); }

  /**
   * Constructor from a range expression.
   * 
   * \param expr The range expression.
   * Wildcard character '*' means all `i` from 0 to `n`.
   * Otherwise composed of comma-separated subranges. Each subrange is either a number `x` or a range `x-y`, means all `i` from `x` to `y` included.
   * Example: "0,1,2,10-12" means call `f(i)` for i=0, i=1, i=2. i=10, i=11 and i=12.
   */
  Ranges(const std::string& expr)
  {
    if(expr == std::string{wildcard}) {
      m_wildcard = true;
      return;
    }

    std::istringstream is{expr};
    bool end{false};

    while(!end)
    {
      m_elements.push_back(ParseNext(is));
      is.peek(); // Force set EOF flag if not already
      if(is.eof()) {
        end = true;
      }
    }
  }

private:
  static Element ParseNext(std::istringstream& is)
  {
    std::string expr; // Should be eg. "0" or "1-2"
    
    if(!std::getline(is, expr, element_delimiter)) {
      throw std::invalid_argument{"Cannot parse range element"};
    }

    std::istringstream expr_is{expr};

    Index first;
    
    if(!(expr_is >> first)) {
      throw std::invalid_argument{"Invalid range expression: " + expr};
    }
    
    const int next{expr_is.peek()};

    if(next == EOF) {
      return first;
    }
    
    if(next != Range::delimiter) {
      throw std::invalid_argument{"Invalid range expression: " + expr};
    }

    if(!expr_is.get()) {
      throw std::invalid_argument{"Error parsing the expression: " + expr};
    }

    Index last;

    if(!(expr_is >> last)) {
      throw std::invalid_argument{"Invalid range expression: " + expr};
    }

    return Range{first, last};
  }

private:
  bool m_wildcard{};
  std::vector<Element> m_elements;
};

} // namespace ns3