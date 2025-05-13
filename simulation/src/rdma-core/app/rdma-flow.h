#pragma once

#include "ns3/callback.h"
#include "ns3/rdma-reflection-helper.h"
#include "ns3/filesystem.h"
#include "ns3/rdma-network.h"
#include <memory>
#include <cstdint>
#include <variant>
#include <functional>

namespace ns3 {

class RdmaNetwork;

struct SerializedFlow
{
    //! Whether to enable or disable this flow.
    bool enable{};
    //! Full path to the ns3 C++ class of the flow.
    std::string path;
    //! When to start the flow.
    Time start_time;
    //! Whether the flow is a background flow.
    //! Background flows does not need to complete to finish the simulation.
    bool in_background{};

    //! All attributes to initialize the ns3 object that represent the flow.
    SerializedJsonObject attributes;
};

struct SerializedFlowList
{
    std::vector<SerializedFlow> flows;
};

class RdmaFlow : public Object
{
public:
    using OnComplete = std::function<void()>;
    
public:
    static TypeId GetTypeId();

public:
    virtual void StartFlow(RdmaNetwork&, OnComplete on_complete) = 0;
};

} // namespace ns3