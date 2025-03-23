use bit_set::BitSet;
use rand::Rng;
use crate::sim::event::EventContext;

use super::Config;
use super::Simulator;
use rand_xoshiro::rand_core::SeedableRng;
use rand_xoshiro::Xoshiro256PlusPlus;

pub struct Node<'a> {
    /// Global topology configuration shared between all nodes
    config: &'a Config,
    /// Node index
    i: i64,

    //// Received chunks
    /// 0: chunk not received, 1: chunk received
    chunks: BitSet,

    /// `misses[i]` stores the count of missed chunks for the block `i`.
    misses: Vec<i64>,
    /// Misses of the right neighbor
    right_misses: Vec<i64>
}

impl<'a> Node<'a> {
    fn new(config: &'a Config, i: i64) -> Self {
        let n = config.n() as usize;
        Self {
            config: config,
            i: i,
            chunks: BitSet::with_capacity(n),
            misses: vec![0; n],
            right_misses: vec![0; n]
        }
    }

    fn right(&self) -> i64 {
        (self.i + 1) % self.config.n()
    }

    pub fn new_topology(config: &'a Config) -> Vec<Self> {
        let n = config.n() as usize;
        let mut nodes = Vec::new();
        for i in 0..n {
            nodes.push(Node::new(config, i as i64));
        }
        nodes
    }
    
    /// Fill the chunks with a random loss model.
    pub fn fill_chunks_randomly(nodes: &mut Vec<Self>, seed: u64) {
        let mut rng = Xoshiro256PlusPlus::seed_from_u64(seed);
        let config = nodes[0].config;

        // Fill the missed chunks randomly
        for node in nodes.iter_mut() {
            for i in 0..config.chunk_count() {
                if rng.random_bool(1. - config.l) {
                    node.chunks.insert(i as usize);
                }
            }
        }
        
        // Fill the owned blocks
        for node in nodes.iter_mut() {
            let first_chunk = node.i * config.c();
            for i in 0..config.c() {
                node.chunks.insert((first_chunk + i) as usize);
            }
        }

        Self::fill_misses(nodes);
    }

    /// Fill the count of missed chunks per block,
    /// `misses` and `right_misses`
    fn fill_misses(nodes: &mut Vec<Self>) {
        let config = nodes[0].config;
        for node in nodes.iter_mut() {
            for i in 0..config.chunk_count() {
                if !node.chunks.contains(i as usize) {
                    node.misses[(i / config.c()) as usize] += 1
                }
            }
        }

        // Fill right misses
        let n = nodes.len();
        let mut right_misses = vec![vec![0; n]; n];
        for node in nodes.iter() {
            let i = (node.i - 1 + n as i64) % n as i64;
            right_misses[i as usize] = node.misses.clone();
        }
        for node in nodes.iter_mut().rev() {
            node.right_misses = right_misses.pop().unwrap();
        }
    }
    
    /// Start the recovery phase
    pub fn start_recovery(me: i64, ctxt: &mut Simulator<'a>) {
        let node = ctxt.node(me as i32);
        let n = node.config.n();

        // Try to send all blocks
        let blocks: Vec<i64> = (0..n).collect();
        Self::send_to_right(me, ctxt, blocks.as_slice());
    }

    fn send_to_right(me: i64, ctxt: &mut Simulator<'a>, blocks: &[i64]) {
        let node = ctxt.node(me as i32);
        let right = node.right();
        let config = node.config;

        let mut delay = config.dn();
        let mut cbs = Vec::new();

        for block in blocks {
            let i = *block as usize;

            if node.misses[i] == 0 && node.right_misses[i] > 0 {
                node.right_misses[i] = 0; // Register we send data to right
                delay += config.g.bytes_tx_time(node.right_misses[i] * config.c());
                
                cbs.push((Box::new(
                    move |ctxt: &mut Simulator<'a>| {
                    Self::on_recv_block(right, ctxt, i as i64);
                }), delay));
            }
        }

        for (cb, when) in cbs {
            ctxt.schedule(when, cb);
        }
    }

    /// Called when a block is fully complete,
    /// the node tries to send to its neighbor the still missed chunks.
    fn on_recv_block(me: i64, ctxt: &mut Simulator<'a>, block: i64) {
        ctxt.node(me as i32).misses[block as usize] = 0;
        Self::send_to_right(me, ctxt, vec![block].as_slice());
    }
}