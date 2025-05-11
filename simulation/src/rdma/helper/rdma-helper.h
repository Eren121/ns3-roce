#pragma once

#include "ns3/simulator.h"
#include "ns3/node-container.h"
#include "ns3/node.h"
#include <map>
#include <set>
#include <functional>
#include <vector>
#include <variant>

/**
 * @file This file contains a bunch of unclassifiable utility functions.
 */

 /**
  * @def DISALLOW_COPY(T)
  * Defines deleted copy constructor and assignment operator.
  * Insert it into class definition to disallow copy of the enclosing class.
  */

#ifndef DISALLOW_COPY
#define DISALLOW_COPY(T) \
  T(const T&) = delete; \
  T& operator=(const T&) = delete
#endif

namespace ns3 {

/**
 * Performs a ceil division.
 * This is the same as the division rounded up, but without performing floating point arithmetic.
 */
template<typename T>
inline T CeilDiv(T num, T den)
{
  return (num + den - 1) / den;
}

/**
 * Performs a positive modulo.
 * In C, the result of the modulo operation  `%` can be negative.
 * This function ensures the result is always positive, in `[0; n)`.
 */
template<typename T>
T PositiveModulo(T i, T n)
{
  return (i % n + n) % n;
}

/**
 * Schedules an event by an absolute time rather than a relative time like `Simulator::Schedule()`.
 * If the time is in the past, schedules immediately.
 * 
 * @param when When to schedule the event, in absolute time.
 */
template<typename... Args>
auto ScheduleAbs(Time when, Args&&... args)
{
  const Time now{Simulator::Now()};
  return Simulator::Schedule(Max(Time{}, when - now), std::forward<Args>(args)...);
}

/**
 * Schedules an event to be run immediately.
 */
template<typename... Args>
auto ScheduleNow(Args&&... args)
{
  return Simulator::Schedule(Simulator::Now(), std::forward<Args>(args)...);
}

/**
 * Permits to make ns3 callbacks with C++11 lambdas syntax.
 * Otherwise, lambdas does not work with `MakeBoundCallback()`.
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

//! Global node ID type returned by `Node::GetId()`.
using node_id_t = uint32_t;

//! Multicast group index type.
using group_id_t = uint32_t;

//! Network interface index type.
using iface_id_t = uint32_t;

//! Byte type to store quantity of bytes.
using byte_t = uint64_t;

//! PFC priority type.
using priority_t = uint32_t;

/**
 * Stores nodes mapped by their global node ID.
 * Same as `NodeContainer`, but nodes are indexed by their global node ID and not by their index in the container.
 */
class NodeMap
{
public:
  using value_type = Ptr<Node>;
  using size_type = size_t;

private:
  /**
   * Comparator for the `std::set<Ptr<Node>>`.
   * Sort `Ptr<Node>` by comparing their node IDs.
   */
  struct Comp
  {
    bool operator()(Ptr<Node> a, Ptr<Node> b) const
    {
      return a->GetId() < b->GetId();
    }
  };

public:
  /**
   * @{
   * Iterates nodes in the container.
   */
  auto begin() const { return m_set.begin(); }
  auto end() const { return m_set.end(); }
  //! @}
  
  /**
   * Adds one node to the container.
   */
  void Add(Ptr<Node> node)
  {
    m_map[node->GetId()] = node;
    m_set.insert(node);
  }

  /**
   * Adds all the nodes to the container.
   */
  void Add(const NodeMap& others)
  {
    for(Ptr<Node> node : others) {
      Add(node);
    }
  }

  //! Gets a node based on its global ID.
  Ptr<Node> Get(node_id_t id) const
  {
    return m_map.at(id);
  }

private:
  std::map<node_id_t, Ptr<Node>> m_map;
  //! For simplicity, we use a set which permits to provide iterators on only nodes and not their IDs which would be redundant.
  std::set<Ptr<Node>, Comp> m_set;
};

/**
 * Permits to repeat a function called a "task" every X seconds in the simulator.
 * To start firing events, call `Resume()`, because it does not do it automatically when instanciated.
 */
class PeriodicEvent
{
public:
  //! User-defined callback type.
  using Task = std::function<void()>;

  PeriodicEvent() = default;
  ~PeriodicEvent();
  PeriodicEvent(const PeriodicEvent&) = delete;
  PeriodicEvent& operator=(const PeriodicEvent&) = delete;

  /**
   * Sets the task to execute every X seconds.
   */
  void SetTask(Task task);

  /**
   * Sets the interval between two tasks.
   */
  void SetInterval(Time itv);

  /**
   * Prevents to run any new task.
   */
  void Resume();

  /**
   * Resumes the generation of tasks.
   * Still satisfies the interval constraint:
   *   - If the last event was a long time ago, a new task is scheduled immediately.
   *   - If the last event was recent, the new task will be run after only after the specified interval.
   */
  void Pause();

private:
  /**
   * Callback that is called every interval, should call the user callback.
   */
  void RunTask();

  /**
   * Schedules `RunTask()` after the specified interval, if not paused.
   * The previous scheduled task (if any) is always cancelled.
   * This can be called after the interval or the task is modified.
   */
  void Reschedule();

private:
  bool m_paused{true}; //!< Whether the task is paused or not.
  Time m_itv; //!< Interval between two tasks.
  Time m_last_invoked; //!< Time the callback was invoked for the last time (or zero if never called).
  Task m_task; //!< User-defined callback.
  EventId m_event; //!< Callback event ID.
};

/**
 * Ensures the size of a container can contain at least `size` elements.
 * Do nothing if the container is big enough, otherwise resize to a size of `size`.
 */
template<typename T>
void EnsureSizeAtleast(T&& vec, size_t size)
{
  if(vec.size() < size) {
    vec.resize(size);
  }
}

/**
 * Utility to parse a string that selects multiple ranges of numbers, which can be useful to parse user input in various contexts.
 *
 * Wildcard character `*` means every possible number. Cannot contain any other range.
 * If not a wildcard, contains a comma-separated ranges expression.
 * Each range expression is either a number `x` or a range `x-y`, means all numbers from `x` to `y` included.
   `x` is a shortcut for `x-x`.
 * Example: selector "0,1,2,10-12" means numbers 0, 1, 2, 10, 11 and 12.
 */
struct Ranges
{
public:
  //! We can only store unsigned integers.
  using Index = unsigned int;

  //! Defines a range between a lower and an upper bound.
  struct Range
  {
    Index first;
    Index last;

    //! Delimiter inside a range expression between the lower and upper bound.
    static const char delimiter = '-';
  };

  //! An element in a range expression can either be a single number or a set of number.
  using Element = std::variant<Index, Range>;

  //! Character for wildcard to denote all possible numbers.
  static const char wildcard = '*';

  //! Delimiter between each range expression.
  static const char element_delimiter = ',';

  //! Checks if the selector is a wildcard.
  bool IsWildcard() const
  {
    return m_wildcard;
  }

  //! @{
  //! Iterates all ranges. Does not contain anything if the selector is a wildcard.
  auto begin() const {
    return m_elements.begin();
  }

  auto end() const {
    return m_elements.end();
  }
  //! @}

  //! Parses from a selector expression.
  Ranges(const std::string& expr);

private:
  static Element ParseNext(std::istringstream& is);

private:
  bool m_wildcard{};
  std::vector<Element> m_elements;
};

} // namespace ns3