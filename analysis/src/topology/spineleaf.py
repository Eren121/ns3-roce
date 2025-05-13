from dataclasses import dataclass
from collections import namedtuple
import numpy as np
import argparse
import json
import pyutils as pyu


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
        self.n_leafs = 4
        self.n_spines = 2
    
    def n_servers(self) -> int:
        return self.n_servers_per_leaf * self.n_leafs

    def first_server(self) -> int:
        return self.n_leafs + self.n_spines
    
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
        for leaf in range(self.n_leafs):
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
        for leaf in range(self.n_leafs):
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
        return []
      
    def to_json(self) -> dict:
        sw, links = self._make_switches()
        servers = self._make_servers()
        groups = self._make_groups()
        nodes = sw + servers

        return {
            "nodes": nodes,
            "groups": groups,
            "links": links
        }

if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        prog="spineleaf",
        description="Generate 2-levels fat tree topology JSON.")
    parser.add_argument("-s", "--spines", default=2)
    parser.add_argument("-l", "--leafs", default=4)
    parser.add_argument("-n", "--servers_per_leaf", default=4)
    args = parser.parse_args()

    topology = Topology()
    topology.n_servers_per_leaf = args.servers_per_leaf
    topology.n_leafs = args.leafs
    topology.n_spines = args.spines
    print(pyu.dump_json(topology.to_json()))