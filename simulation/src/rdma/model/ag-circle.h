#pragma once

#include <cstdint>

namespace ns3 {

template<typename T>
inline T AgCeilDiv(T num, T den)
{
  return (num + den - 1) / den;
}

// Definitions:
//   - Block: All chunks that belongs to the same node form a contiguous block.
//   - Chunk: Part of data sent, contained in a single packet.

struct AgConfig
{
  using block_id_t = uint32_t;
  using chunk_id_t = uint32_t;

  uint32_t chunk_size;
  uint32_t node_count; //!< Count of nodes in the allgather.
  uint32_t root_count; //!< Count of multicast roots.
  uint32_t current_node; //!< Current node ID. In `[0;node_count)`.
  uint64_t size; //!< Count of bytes to send in each node. At the end of the allgather each node has `size * node_count` bytes.

  uint32_t GetChunkCountPerBlock() const
  {
    return AgCeilDiv(size, uint64_t{chunk_size});
  }
};

/**
 * @brief Check if the node is a multicast root in the first multicast.
 */
inline bool AgIsFirstMcastSrc(const AgConfig& config, uint32_t node)
{
  const int spacing{AgCeilDiv<uint64_t>(config.node_count, config.root_count)};
  
  return (node % spacing) == 0;
}

inline uint32_t GetSenderFromChunk(const AgConfig& config, uint32_t chunk)
{
  return chunk / config.GetChunkCountPerBlock();
}

/**
 * @brief Check if the received chunk is the last one of a multicast chain.
 */
inline bool AgIsLastChainChunk(const AgConfig& config, uint32_t recv_chunk)
{
  const uint32_t sender_id{GetSenderFromChunk(config, recv_chunk)};
  const bool is_last_sender{AgIsFirstMcastSrc(config, (sender_id + 1) % config.node_count)};
  const bool is_last_chunk{recv_chunk % config.GetChunkCountPerBlock() == config.GetChunkCountPerBlock() - 1};

  return is_last_sender && is_last_chunk;
}



} // namespace ns3