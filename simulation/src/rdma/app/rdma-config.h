#pragma once

#include "ns3/rdma-helper.h"
#include "ns3/rdma-reflection-helper.h"
#include "ns3/json.h"
#include "ns3/filesystem.h"
#include <vector>
#include <memory>

namespace ns3 {

/**
 * During the simulation, we can gather information about each link periodically.
 * 
 * @note This class is serializable: field names matter.
 */
struct LinkStatsConfig
{
    //! Whether statistics should be gathered.
    bool enable;
    //! Output Avro file where to store the count of transmitted bytes from each link at the end of the simulation.
    fs::path output_bytes;
};

/**
 * In the global config, we can define, depending on the bandwidth of a node, what is the threshold of ECN.
 * The threshold indicates that when reached, the node will send a packet asking to pause the flow because the RX is congested.
 * 
 * The ECN configuration is global, it maps for each given bandwidth a given ECN threshold.
 * If a node has a bandwidth which is not mapped in the global config file,
 * the closest configuration bandwidth is chosen (see `FindEcnConfigFromBps()`).
 * 
 * 
 * `min_buf_bytes` says for which egress minimal buffer size we should mark the Congestion Experienced (CE) bit.
 * The probability to mark the CE bit is:
 *  - 100% when the buffer size is greather than `max_buf_bytes`.
 *  - `max_probability * 100` % when == `max_buf_bytes` (`max_probability` stores the probability in a double in [0;1]).
 *  - 0% when less or equal `min_buf_bytes`.
 * 
 * The probability increases linearly between `min_buf_bytes` and `max_buf_bytes`.
 * 
 * @note This class is serializable: field names matter.
 */
struct EcnConfigEntry
{
    //! Bandwidth (in bits per second) associated with this ECN configuration.
    double bandwidth_bps{};

    //! Minimum ECN buffer usage (in bytes) to trigger ECN. 
    double min_buf_bytes{};

    //! Maximum ECN buffer usage (in bytes) to trigger ECN.
    double max_buf_bytes{};

    //! Maximum probability (in [0;1]) to trigger ECN when the max. threshold is reached.
    double max_probability{};
};

/**
 * Finds the ECN threshold from a bandwidth, based on the `map`.
 * If the given bandwidth `bps` is not present in the `map`, just chose the highest entry that is lower than the `bps`.
 * Crashs if no such entry can be found.
 */
EcnConfigEntry FindEcnConfigFromBps(const std::vector<EcnConfigEntry>& map, double bps);

/**
 * @brief Global parameters of the simulation.
 * 
 * All fields corresponds to a JSON field (unless explicitly written) with the same name,
 * that the user has to refer to to write global config file.
 * 
 * Don't construct instances with constructor, call `from_file()` otherwise `config_dir` will not be initialized.
 * 
 * @note This class is serializable: field names matter.
 */
class RdmaConfig final
{
public:
    struct Module
    {
        //! The ns3 full path of the C++ class.
        std::string path;
        //! Whether to enable this module or not.
        bool enable{};
        //! All the ns3 attributes for this module.
        SerializedJsonObject attributes;
    };

public:
    /**
     * Reads a global configuration from a JSON file.
     * The file should contain a JSON object, which is deserialized into an `RdmaConfig`.
     * Each field of the JSON object is mapped to the field of this C++ class that has the exact same name.
     * Crashs if there is even a single mismatch (no corresponding C++ field or missing JSON key).
     */
    static std::shared_ptr<RdmaConfig> from_file(const fs::path& json);

    //! Always stores the path of the directory that contains this configuration file.
    //! This field is not deserialized.
    rfl::Skip<fs::path> config_dir;

    //! Desired global default parameters for NS3.
    //! Should be a JSON object where each key is the attribute name (eg. `ns3::QueueBase::MaxSize`).
    //! Just another way to store ns3 global default attributes.
    //! @see https://www.nsnam.org/docs/manual/html/attributes.html.
	SerializedJsonObject default_attributes;

    //! Acts as a plugin system.
    //! Each module is added as a child to the `RdmaNetwork`.
    //! The module is created after the `RdmaNetwork` is initialized.
    //! The module is destroyed at the end of the simulation.
    std::vector<Module> modules;
	
    //! Simulation time when to stop the simulation.
    //! A `Time` should be stored in a JSON double as seconds.
    //! If zero, never stop until all flows are finished.
	Time simulator_stop_time;

    //! Path of the JSON file that store the network topology to simulate.
	fs::path topology_file;

    //! Path of the JSON file that stores all the flows to simulate.
	fs::path flows_file;

    //! If true, ACKs use PFC flow index zero which is the highest priority.
    //! Otherwise, ACKs use the default flow index 3.
	bool ack_high_prio{false};

    //! Seed for RNG behaviours.
    //! Use the same seed to have the same simulation.
	uint32_t rng_seed{50};

    //! ECN various thresholds configuration.
	std::vector<EcnConfigEntry> ecn;

    //! Gets the path of a file relatively to the containing directory of this global configuration file.
    fs::path FindFile(const fs::path& path) const;

    //! Loads a JSON-serializable class (with RFL) from the path of a file,
    //! relatively to the containing directory of this global configuration file.
	template<typename T>
	T LoadFromfile(fs::path path) const
	{
		return rfl::json::read<T>(read_all_file(FindFile(path))).value();
	}

    //! Modifies ns3 global default attributes based on `defaults`.
    void ApplyDefaultAttributes() const;
};

/**
 * Stores the topology of the network to simulate.
 * 
 * @note This class is serializable: field names matter.
 */
struct RdmaTopology
{
    //! Represents a switch or a server in the topology.
    //! @note This class is serializable: field names matter.
    struct Node
    {
        //! Permits to set the position of the node in netanim.
        //! @note This class is serializable: field names matter.
        struct Pos
        {
            double x{};
            double y{};

            NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Pos, x, y);
        };

        uint32_t id{};
        bool is_switch{};
        Pos pos;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Node, id, is_switch, pos);
    };

    //! Represents a link between two nodes.
    //! @note This class is serializable: field names matter.
	struct Link
    {
		uint32_t src{};           //!< First node ID.
		uint32_t dst{};           //!< Second node ID.
		double bandwidth{100e9};  //!< Link bandwidth. Unit: bits per second.
		double latency{1e-6};     //!< Link latency. Unit: seconds.
		double error_rate{};      //!< Packet drop probability upon reception. Unit: percentage in [0;1].

		NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Link, src, dst, bandwidth, latency, error_rate);
	};

    //! Represents a multicast group.
    //! Any node that is a server can be part of any multicast group.
    //! @note This class is serializable: field names matter.
    struct Group
    {
        group_id_t id{};   //!< Unique ID of the multicast group (replaces the Multicast IP address).
        std::string nodes; //!< Contains node parsable from `Ranges`

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Group, id, nodes);
    };

	std::vector<Node> nodes;
	std::vector<Link> links;
    std::vector<Group> groups;

	NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(RdmaTopology, nodes, links, groups);
};

} // namespace ns3