mod sim;
mod ag;

use ag::{Simulator, Node, Config};
use sim::time::{Time,Bw};

fn main() {
    let config = Config {
        k: 2,
        s: 4,
        m: 4,
        c0: 100_000,
        c1: 0_000,
        b: 4096,
        d0: Time::micros(1),
        d1: Time::micros(2),
        l: 0.2,
        e: 0.75,
        g: Bw::from_gbits(100)
    };
    let mut nodes = Node::new_topology(&config);
    Node::fill_chunks_randomly(&mut nodes, 12345);

    let elapsed = Simulator::run(nodes, |ctxt| {
        Node::start_recovery(ctxt);
    });

    println!("Elapsed: {}", elapsed);
}
