pub mod node;

pub use node::Node;

use crate::sim;
pub use crate::sim::time::{Time, Bw, Bytes};
pub use crate::sim::event::EventContext;

pub type Simulator<'a> = sim::Simulator<'a, Node<'a>>;

use bytesize::ByteSize;

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

    /// Size of a block in bytes
    pub fn block_bytes(&self) -> i64 {
        self.c() * self.b()
    }

    /// Estimation of lost chunks per block in the multicast after applying FEC
    pub fn cm(&self) -> i64 {
        let c0 = self.c0 as f64;
        let c1 = self.c1 as f64;
        ((self.l * c0 - self.e * (1. - self.l) * c1).ceil() as i64).max(0)
    }

    /// Estimation recovery time
    pub fn est_rec_t(&self) -> Time {
        let bp = (self.b() as f64 / (1. - self.l)) as i64;
        (self.g.bytes_tx_time(bp * self.cm()) + self.dn()) * (self.n() - 1) as i128
        
    }

    // Compute how many bytes to send in average with retransmission taking into account packet loss
    pub fn with_loss(&self, bytes: Bytes) -> Bytes {
        ((bytes as f64) * (1. / (1. - self.l))).ceil() as Bytes
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

    pub fn print(&self) {
        println!("Block size: {}", ByteSize::b(self.block_bytes() as u64));
        println!("Buffer size: {}", ByteSize::b((self.n() * self.block_bytes()) as u64));

        let est = self.chunk_count() * self.n() / 8;
        println!("Estimated simulation memory: {}", ByteSize::b(est as u64))
    }
}