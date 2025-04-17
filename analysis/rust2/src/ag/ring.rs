/// Implementation of the ring-based recovery algorithm
///
/// 

use bit_set::BitSet;
use sim::Simulator;

/// State of the ring simulation
pub struct State {
    /// `bitmaps[r].contains(c)` checks if the rank `r` has successfully received the chunk `c`.
    /// During the recovery algorithn, the bitmap is completed until it is full.
    bitmaps: Vec<BitSet>,
    /// `cache[r]` stores the indices of all currently missing chunks for the rank `r`.
    cache: Vec<Vec<usize>>,

    /// Time to send a chunk between two neighbors, taking into account both delay and transmission time.
    chunk_delay: Time
}

impl State {
    fn build_cache(bitmaps: &Vec<BitSet>) -> Vec<Vec<usize>> {
        let cache = ve![Vec::new(); bitmaps.size()];
        for r in 0..bitmaps.size() {
            let bitmap = &bitmaps[r];
            for c in 0..bitmap.capacity() {
                if !bitmap.contains(c) {
                    cache[r].push(c);
                }
            }
        }
        cache
    }

    pub fn new(bitmaps: Vec<BitSet>, chunk_delay: Time) -> Self {
        Self {
            bitmaps,
            cache: build_cache(bitmaps),
            chunk_delay
        }
    }

    /// Run the recovery algorithm in the simulator.
    pub fn run(&mut self) {
        let sim = Simulator<State>::new();
    }
}

