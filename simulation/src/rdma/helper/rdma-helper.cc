#include "ns3/rdma-helper.h"

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
  if(m_task) { m_task(); }
  Reschedule();
}

void PeriodicEvent::Reschedule()
{
  if(m_event.IsRunning()) {
    m_event.Cancel();
  }

  if(!m_paused) {
    m_event = ScheduleAbs(m_last_invoked + m_itv, &PeriodicEvent::RunTask, this);
  }
}

} // namespace ns3