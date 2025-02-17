from dataclasses import dataclass
from collections import namedtuple
import numpy as np


FatTreeConfig = namedtuple("FatTreeConfig", "depth link_bw link_latency")

class FatTree:
  def __init__(self, config: FatTreeConfig):
    self.config = config
  
  def get_depth(self, idx) -> int:
      return int(np.log2(idx + 1))
  
  def node_count(self) -> int:
     return 2 ** (self.config.depth + 1) - 1
  
  def is_switch(self, node_idx: int) -> bool:
     return node_idx < 2 ** self.config.depth - 1
  
  def get_server_node_indices(self) -> list:
     begin = 2 ** self.config.depth - 1
     end = 2 ** (self.config.depth + 1) - 1
     return [i for i in range(begin, end)]   
  
  def get_groups(self) -> dict:
    """
    We just need a single multicast group that all servers belong to.
    """
    group = {
       "id": 1,
       "nodes": self.get_server_node_indices()
    }
    return {
       "groups": [group]
    }
  
  def build_link(self, src: int, dst: int) -> dict:
     depth = min(self.get_depth(src), self.get_depth(dst))
     return {
      "src": src,
      "dst": dst,
      "bandwidth": self.config.link_bw * (self.config.depth - depth),
      "latency": self.config.link_latency,
      "error_rate": 0.0
     }

  def get_topology(self) -> dict:
    nodes = []
    for idx in range(self.node_count()):
      depth = self.get_depth(idx)
      cols = 2 ** depth
      col = idx - (2 ** int(np.log2(idx + 1)) - 1)
      half_col = 0.5 / cols
      zoom = 3
      node = {
        "id": idx,
        "is_switch": self.is_switch(idx),
        "pos": {
          "x": ((col / cols + half_col) - 0.5) * (2 ** self.config.depth) * zoom,
          "y": depth * zoom
        }
      }
      nodes.append(node)
    links = []
    for idx in range(2 ** self.config.depth - 1):
        left_child_idx = 2 * idx + 1
        right_child_idx = 2 * idx + 2
        links.append(self.build_link(idx, left_child_idx))
        links.append(self.build_link(idx, right_child_idx))
    return {
      "nodes": nodes,
      "links": links
    }


def make_flow_allgather(**kwargs) -> dict:
  ret = {
    "type": "allgather",
    "start_time": 0,
    "priority": 3
  }
  ret.update(kwargs)
  return ret


def make_flow_multicast(**kwargs) -> dict:
  ret = {
    "type": "flow",
    "start_time": 0,
    "priority": 3,
    "multicast": True,
    "reliable": False
  }
  ret.update(kwargs)
  return ret