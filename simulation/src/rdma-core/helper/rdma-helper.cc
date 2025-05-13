#include "ns3/rdma-helper.h"
#include <stdexcept>

namespace ns3 {

PeriodicEvent::~PeriodicEvent()
{
  Pause();
}

void PeriodicEvent::SetTask(Task task)
{
  m_task = std::move(task);
}

void PeriodicEvent::SetInterval(Time itv)
{
  m_itv = itv;
  Reschedule();
}

void PeriodicEvent::Resume()
{
  m_paused = false;
  Reschedule();
}

void PeriodicEvent::Pause()
{
  m_paused = true;
  Reschedule();
}

void PeriodicEvent::RunTask()
{
  m_last_invoked = Simulator::Now();
  
  // Invoke user callback if any.
  if(m_task) {
    m_task();
  }

  Reschedule();
}

void PeriodicEvent::Reschedule()
{
  // Cancel any scheduled task.
  if(m_event.IsRunning()) {
    m_event.Cancel();
  }

  // If not paused, schedule the next task at the appropriate time.
  if(!m_paused) {
    m_event = ScheduleAbs(m_last_invoked + m_itv, &PeriodicEvent::RunTask, this);
  }
}

Ranges::Ranges(const std::string& expr)
{
  // Check if is the wildcard.
  if(expr == std::string{wildcard}) {
    m_wildcard = true;
    return;
  }

  std::istringstream is{expr};
  bool end{false};

  // Parse every element, if not a wildcard.
  while(!end) {
    m_elements.push_back(ParseNext(is));
    
    // Does nothing, except sets the EOF flag if there are no more characters,
    // which permits to know when to stop the loop.
    is.peek();

    // Stop if no more character.
    if(is.eof()) {
      end = true;
    }
  }
}

Ranges::Element Ranges::ParseNext(std::istringstream& is)
{
  // Will contain eg. "0" or "1-2".
  std::string expr;
  
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

namespace
{

class UniquePortTag : public Object
{
public:
  static TypeId GetTypeId()
  {
    static TypeId tid = TypeId("ns3::UniquePortTag")
    .SetParent<Object>()
    .AddConstructor<UniquePortTag>();

    return tid;
  }

public:
  uint16_t GetNextPort()
  {
    return m_next_port++;
  }

private:
  uint16_t m_next_port{2000};
};

} // namespace

uint16_t GetNextUniquePort(Ptr<Node> node)
{
  Ptr<UniquePortTag> tag;
  tag = node->GetObject<UniquePortTag>();
  if(!tag) {
    tag = CreateObject<UniquePortTag>();
    node->AggregateObject(tag);
  }

  return tag->GetNextPort();
}

uint16_t GetNextMulticastUniquePort()
{
  // Assumes it doesn't conflict with `GetNextUniquePort()`.
  // So no more than 3000 unique ports per node.
  
  static uint16_t next_port{5000};
  return next_port++;
}

} // namespace ns3