#pragma once

#include "ns3/rdma-flow.h"

namespace ns3 {

/**
 * Simple timer.
 * Useful in conjonction with flow dependencies in the flows to pause at some point.
 */
class RdmaFlowWait : public RdmaFlow
{
public:
    using RdmaFlow::OnComplete;
    
    static TypeId GetTypeId();

protected:
    void OnFlowStarted(RdmaNetwork& network) override;

private:
    //! Amount of time to wait.
    Time m_time;
};

} // namespace ns3