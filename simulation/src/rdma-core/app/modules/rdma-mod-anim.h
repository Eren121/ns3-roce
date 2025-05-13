#pragma once

#include "ns3/rdma-config-module.h"
#include <memory>

namespace ns3 {

class AnimationInterface;

/**
 * Enable generating the XML output animation file for netanim.
 */
class RdmaModAnim final : public RdmaConfigModule
{
public:
    static TypeId GetTypeId();

public:
    void OnModuleLoaded(RdmaNetwork& network) override;

private:
    std::string m_xml_anim_out;
    std::unique_ptr<AnimationInterface> m_anim;
};

} // namespace ns3