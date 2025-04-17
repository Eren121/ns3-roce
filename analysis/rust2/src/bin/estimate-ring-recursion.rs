use rust::gilbert_elliott::{Model, Chain};
use bit_set::BitSet;

fn main() {
    let model = Model::from_bernouilli(10., 1000., 0.01, 0.01);

    // Total count of ranks.
    let rank_count = 1024;
    // Total count of chunks. Each rank store the bitmap of chunks.
    let chunk_count = 1_000_000;
    // bitmap[i][j] stores if the chunk `j` of the rank `i` is received by rank `j`.
    let mut bitmaps = vec![BitSet::with_capacity(chunk_count); rank_count];

    // Fill the bitmaps
    for r in 0..rank_count {
        let mut chain = Chain::new(model, rand::rng());
        for c in 0..chunk_count {
            if chain.next() {
                bitmaps[r].insert(c);
            }
        }
    }

    // Compute the count of miss chains.
    // That means `miss_chains[i]` stores the count of chunks that are missed by `i` neighbors.
    // `miss_chains[0]` is left unused.
    let mut miss_chains = vec![0; rank_count];
    for c in 0..chunk_count {
        let mut len = 0; // Current miss chain length
        for r in 0..rank_count {
            if !bitmaps[r].contains(c) {
                len += 1;
            }
            else {
                if len > 0 {
                    miss_chains[len] += 1;
                    len = 0;
                }
            }
        }
        if len > 0 { // Don't forget last one
            miss_chains[len] += 1;
        }
    }

    // Print `miss_chain`
    let mut tot_loss = 0; // Total chunk loss
    for i in 0..rank_count {
        if miss_chains[i] > 0 {
            tot_loss += miss_chains[i] * i;
            println!("miss_chain[{}] = {}", i, miss_chains[i]);
        }
    }

    println!("Chunk loss probability = {:3}", tot_loss as f64 / (rank_count * chunk_count) as f64);
}