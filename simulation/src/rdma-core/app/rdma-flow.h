#pragma once

#include "ns3/callback.h"
#include "ns3/rdma-reflection-helper.h"
#include "ns3/rdma-helper.h"
#include "ns3/filesystem.h"
#include <memory>
#include <cstdint>
#include <variant>
#include <functional>

namespace ns3 {

class RdmaNetwork;

struct SerializedFlow
{
    //! Unique ID, useful for graph dependencies.
    std::string id;
    //! Stores the ID of dependencies, that is other flows that should complete before this one.
    std::vector<std::string> dependencies;
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
    RdmaFlow();
    
public:
    static TypeId GetTypeId();

    int GetId() const;
    void SetInfo(SerializedFlow info);
    bool HasCompleted() const;
    
    void StartFlow(RdmaNetwork& network);

    void Init(const SerializedFlow& info)
    {
        m_info = info;
    }

    Time GetStartTime() const
    {
        return m_info.start_time;
    }

    bool InBackground() const
    {
        return m_info.in_background;
    }

    void AddOnCompleteCallback(OnComplete);

protected:
    virtual void OnFlowStarted(RdmaNetwork&) = 0;
    void NotifyComplete();

private:
    int m_id{};
    SerializedFlow m_info;
    bool m_completed{};
    std::vector<OnComplete> m_on_complete;
};

} // namespace ns3