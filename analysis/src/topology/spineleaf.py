from dataclasses import dataclass
from collections import namedtuple
import numpy as np


class Style:
    def __init__(self):
        self.server_xpadding = 2 # X padding between servers
        self.server_xmargin = 2 # Additional margin between servers of different leaf
        self.server_ymargin = 4 # Margin between leaf switch and server
        self.leaf_ymargin = 5 # Margin between leaf switch and spine switch


class Config:
    def __init__(self):
        self.link_bw = 100e9 # In bps
        self.link_latency = 1e-6 # In seconds


class Topology():
    def __init__(self):
        self.config = Config()
        self.style = Style()
        self.n_servers_per_leaf = 4
        self.n_leaf = 4
        self.n_spines = 2
    
    def n_servers(self) -> int:
        return self.n_servers_per_leaf * self.n_leaf

    def first_server(self) -> int:
        return self.n_leaf + self.n_spines
    
    def first_leaf(self) -> int:
        return self.n_spines
    
    def leaf_of_server(self, server) -> int:
        off = server - self.first_server()
        return self.first_leaf() + off // self.n_servers_per_leaf

    def servers(self) -> np.ndarray:
        i = self.first_server()
        return np.arange(i, i + self.n_servers()) 
      
    def _make_link(self, src: int, dst: int) -> dict:
        return {
         "src": src,
         "dst": dst,
         "bandwidth": self.config.link_bw,
         "latency": self.config.link_latency,
         "error_rate": 0.0
        }
    
    def _make_servers(self) -> dict:
        servers = []
        x = 0
        y = 0
        id = self.first_server()
        for leaf in range(self.n_leaf):
            for s in range(self.n_servers_per_leaf):
                servers.append({
                    "id": id,
                    "is_switch": False,
                    "pos": {
                        "x": x,
                        "y": y
                    }
                })
                id += 1
                x += self.style.server_xpadding
            x += self.style.server_xmargin
        return servers
    
    def _make_switches(self) -> dict:
        sw = []
        links = []

        first_x = (self.n_servers_per_leaf - 1) * self.style.server_xpadding / 2
        x = first_x
        for leaf in range(self.n_leaf):
            leaf_id = self.n_spines + leaf
            sw.append({
                "id": leaf_id,
                "is_switch": True,
                "pos": {
                    "x": x,
                    "y": self.style.server_ymargin
                }
            })
            last_x = x
            x += (self.n_servers_per_leaf * self.style.server_xpadding) + self.style.server_xmargin

            # Make leaf to each spine connection
            for spine in range(self.n_spines):
                links.append(self._make_link(leaf_id, spine))

        xs = np.linspace(first_x, last_x, num=self.n_spines + 2)[1:-1]
        for spine in range(self.n_spines):
            sw.append({
                "id": spine,
                "is_switch": True,
                "pos": {
                    "x": xs[spine],
                    "y": self.style.server_ymargin + self.style.leaf_ymargin
                }
            })
        
        # Make each server to each leaf connection
        for server in self.servers():
            links.append(self._make_link(self.leaf_of_server(server), server))
        
        return sw, links

    def _make_groups(self) -> dict:
        """
        We just need a single multicast group that all servers belong to.
        """
        group = {
           "id": 1,
           "nodes": "*"
        }
        return [group]
      
    def to_json(self):
        sw, links = self._make_switches()
        servers = self._make_servers()
        groups = self._make_groups()
        nodes = sw + servers

        return {
            "nodes": nodes,
            "groups": groups,
            "links": links
        }