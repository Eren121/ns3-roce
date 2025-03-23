mod sim;
mod ag;

use ag::*;

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
    let n = config.n();
    let mut nodes = Node::new_topology(&config);
    Node::fill_chunks_randomly(&mut nodes, 12345);

    let mut sim = Simulator::new(&mut nodes);
    sim.schedule_now(Box::new(move |ctxt| {
        for i in 0..n {
            Node::start_recovery(i, ctxt);
        }
    }));
    
    sim.run();
    println!("Elapsed: {}", sim.now());
}
