#include "ns3/rdma-mod-stats.h"
#include "ns3/rdma-network.h"
#include "ns3/rdma-reliable-qp.h"
#include "ns3/rdma-hw.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("RdmaModStats");
NS_OBJECT_ENSURE_REGISTERED(RdmaModStats);

TypeId RdmaModStats::GetTypeId()
{
  static TypeId tid = []() {
    static TypeId tid = TypeId("ns3::RdmaModStats");

    tid.SetParent<RdmaConfigModule>();
    tid.AddConstructor<RdmaModStats>();
    
    AddStringAttribute(tid,
      "JsonOutputFile",
      "File path to write various JSON statistics.",
      &RdmaModStats::m_json_out);

    return tid;
  }();
  
  return tid;
}

RdmaModStats::~RdmaModStats()
{
    struct Stats
    {
        Time stop_time;
    };

    Stats stats;
    stats.stop_time = Simulator::Now();

    const fs::path out_json_path{RdmaNetwork::GetInstance().GetConfig().FindFile(m_json_out)};
    std::ofstream ofs{out_json_path};
    ofs << rfl::json::write(stats);
}

void RdmaModStats::OnModuleLoaded(RdmaNetwork& network)
{
}

} // namespace ns3