// TODO

void RdmaNetwork::CreateAnimation()
{
	if(m_config->anim_output_file.empty()) {
    return;
  }

  for (const RdmaTopology::Node& node_config : m_topology->nodes) {
    const Ptr<Node> node{FindNode(node_config.id)};
    Vector3D p{};
    p.x = node_config.pos.x;
    p.y = node_config.pos.y;
    AnimationInterface::SetConstantPosition(node, p.x, p.y, p.z);
  }

  // Save trace for animation
  auto* const anim = std::make_unique<AnimationInterface>(m_config->FindFile(m_config->anim_output_file));
  
  // High polling interval; the nodes never move. Reduces XML size.
  anim->SetMobilityPollInterval(Seconds(1e9));

  anim->EnablePacketMetadata(true);

  m_modules.push_back(anim);
}