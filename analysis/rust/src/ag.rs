pub mod node;

pub use node::*;
pub type Simulator = crate::sim::Simulator;
pub type Context<'a> = crate::sim::Context<Node<'a>>;

use crate::sim::time::{Time, Bw, Bytes};

#[derive(Clone)]
pub struct Config {
    pub k: i64, // Count of multicast roots
    pub s: i64, // Count of servers per leaf switch
    pub m: i64, // Count of leaf switches
    pub c0: i64, // Count of data chunks per block
    pub c1: i64, // Count of parity chunks per block
    pub b: Bytes, // Chunk size (B)
    pub d0: Time, // Delay of servers in same leaf
    pub d1: Time, // Delay of servers in different leaf
    pub l: f64, // Loss proportion (in [0;1])
    pub e: f64, // FEC efficiency (in [0;1])
    pub g: Bw, // Bandwidth (Bytes per second)
}

impl Config {
    /// Total chunk count counting all blocks
    pub fn chunk_count(&self) -> i64 {
        self.c() * self.n()
    }

    pub fn n(&self) -> i64 { self.m * self.s }
    pub fn c(&self) -> i64 { self.c0 + self.c1 }
    pub fn k(&self) -> i64 { self.k }
    pub fn s(&self) -> i64 { self.s }
    pub fn m(&self) -> i64 { self.m }
    pub fn c0(&self) -> i64 { self.c0 }
    pub fn c1(&self) -> i64 { self.c1 }
    pub fn b(&self) -> Bytes { self.b }
    pub fn d0(&self) -> Time { self.d0 }
    pub fn d1(&self) -> Time { self.d1 }
    pub fn l(&self) -> f64 { self.l }
    pub fn e(&self) -> f64 { self.e }
    pub fn g(&self) -> Bw { self.g }
    
    pub fn dn(&self) -> Time {
        (self.d0 * (self.s - 1) as i128 + self.d1) / self.s as i128
    }
}