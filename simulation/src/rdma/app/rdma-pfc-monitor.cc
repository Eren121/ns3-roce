#include "ns3/rdma-pfc-monitor.h"
#include "ns3/rdma-network.h"
#include "ns3/pfc-record.h"

namespace ns3 {

PfcMonitor::PfcMonitor(const fs::path& avro_output, const NetDeviceContainer& qbb_devs)
  : m_record_writer{avro_output}
{
    for(auto it = qbb_devs.Begin(); it != qbb_devs.End(); it++) {
        const auto dev = DynamicCast<QbbNetDevice>(*it);
        
        auto on_pfc_state_changed = MakeLambdaCallback<uint32_t>(
            [this, dev](uint32_t state) {
            OnPfcStateChanged(dev, state == 0 ? PfcState::Resume : PfcState::Pause);
        });

        dev->TraceConnectWithoutContext("QbbPfc", on_pfc_state_changed);
    }
}

void PfcMonitor::OnPfcStateChanged(Ptr<QbbNetDevice> dev, PfcState state)
{
    PfcRecord record;
    record.time = Simulator::Now().GetSeconds();
    record.node = dev->GetNode()->GetId();
    record.dev = dev->GetIfIndex();
    record.paused = (state == PfcState::Pause);
    m_record_writer.write(record);
}

} // namespace ns3