#include "ns3/rdma-mod-anim.h"
#include "ns3/rdma-network.h"
#include "ns3/animation-interface.h"
#include "ns3/rdma-reflection-helper.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("RdmaModAnim");
NS_OBJECT_ENSURE_REGISTERED(RdmaModAnim);

TypeId RdmaModAnim::GetTypeId()
{
  static TypeId tid = []() {
    static TypeId tid = TypeId("ns3::RdmaModAnim");

    tid.SetParent<RdmaConfigModule>();
    tid.AddConstructor<RdmaModAnim>();
    
    AddStringAttribute(tid,
      "XmlOutputFile",
      "File path to write the XML animation of the simulation for netanim.",
      &RdmaModAnim::m_xml_anim_out);

    return tid;
  }();
  
  return tid;
}

void RdmaModAnim::OnModuleLoaded(RdmaNetwork& network)
{
	  if(m_xml_anim_out.empty()) {
        return;
    }

    for (const RdmaTopology::Node& node_config : network.GetTopology().nodes) {
        const Ptr<Node> node{network.FindNode(node_config.id)};
        Vector3D p{};
        p.x = node_config.pos.x;
        p.y = node_config.pos.y;
        AnimationInterface::SetConstantPosition(node, p.x, p.y, p.z);
    }

    // Save trace for animation
    m_anim = std::make_unique<AnimationInterface>(network.GetConfig().FindFile(m_xml_anim_out));

    // Very low (basically never) polling interval because the nodes never move.
    // Reduces XML size and makes simulation faster.
    m_anim->SetMobilityPollInterval(Seconds(1e9));

    m_anim->EnablePacketMetadata(true);
}

} // namespace ns3