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
    struct FlowStats
    {
        std::string id;
        Time completion_time;
    };

    struct Stats
    {
        Time stop_time;
        std::vector<FlowStats> flows;
    };

    Stats stats;
    stats.stop_time = Simulator::Now();

    for(const auto& [flow_name, comp_time] : RdmaNetwork::GetInstance().GetFlowScheduler().GetAllCompletionTimes()) {
      FlowStats flow_stats;
      flow_stats.id = flow_name;
      flow_stats.completion_time = comp_time;
      stats.flows.push_back(flow_stats);
    }

    const fs::path out_json_path{RdmaNetwork::GetInstance().GetConfig().FindFile(m_json_out)};
    std::ofstream ofs{out_json_path};
    ofs << rfl::json::write(stats);
}

void RdmaModStats::OnModuleLoaded(RdmaNetwork& network)
{
}

} // namespace ns3