#include "ns3/rdma-config.h"
#include "ns3/config.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("RdmaConfig");

EcnConfigEntry FindEcnConfigFromBps(const std::vector<EcnConfigEntry>& map, double bps)
{
    // Sort entries first for easy search (we don't care about performance here).

    std::map<double, EcnConfigEntry> sorted{};
    for(const auto& entry : map) {
        sorted[entry.bandwidth_bps] = entry;
    }

    // Just use the higher entry which is lower or equals.

    auto it{sorted.lower_bound(bps)};
    
    NS_ABORT_MSG_IF(it == sorted.begin(), "No ECN config entry with a bandwidth lower or equal a node bandwidth" << bps);

    return (--it)->second;
}

std::shared_ptr<RdmaConfig> RdmaConfig::from_file(const fs::path& json)
{
    RdmaConfig config = rfl::json::read<RdmaConfig>(read_all_file(json)).value();

    // `config_dir` is just the containing directory.
	config.config_dir = json.parent_path();

    // Wraps in a `std::shared_ptr`.
    return std::make_shared<RdmaConfig>(config);
}

fs::path RdmaConfig::FindFile(const fs::path& path) const
{
    return config_dir.get() / path;
}

void RdmaConfig::ApplyDefaultAttributes() const
{
	for(const auto& [key, val] : default_attributes) {
		Ptr<AttributeValue> new_val{ConvertJsonToAttribute(val, FindConfigAttribute(key))};
		Config::SetDefault(key, *new_val);
		NS_LOG_INFO("Set " << key << " to " << new_val->SerializeToString(MakeEmptyAttributeChecker()));
	}
}

} // namespace ns3