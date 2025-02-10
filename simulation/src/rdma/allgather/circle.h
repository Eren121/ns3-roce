#pragma once

namespace ns3 {

inline int AgCeilDiv(int num, int den)
{
  return (num + den - 1) / den;
}

/**
 * @brief Check if the current node is a multicast root in the first multicast.
 */
inline bool AgIsFirstMcastSrc(int node_count, int root_count, int current_node)
{
  const int spacing{AgCeilDiv(node_count, root_count)};
  
  return (current_node % spacing) == 0;
}

/**
 * @brief Check if the received chunk is the last one of a multicast chain.
 */
inline bool AgIsLastChainChunk(int node_count, int root_count, int chunk_per_block, int recv_chunk)
{
  const int sender{recv_chunk / node_count};
  const bool last_sender{AgIsFirstMcastSrc(node_count, root_count, (sender + 1) % node_count)};
  const bool last_chunk{recv_chunk % size == size - 1};

  return last_chunk && last_sender;
}



} // namespace ns3