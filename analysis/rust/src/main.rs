mod sim;
mod ag;

use ag::*;

struct Result {

}
fn main() {
    let config = Config {
        k: 2,
        s: 16,
        m: 16,
        c0: 100_000,
        c1: 0_000,
        b: 4096,
        d0: Time::micros(1),
        d1: Time::micros(2),
        l: 0.1,
        e: 0.75,
        g: Bw::from_gbits(100)
    };
    config.print();

    let n = config.n();
    let mut nodes = Node::new_topology(&config);
    
    println!("Filling chunks randomly...");

    Node::fill_chunks_randomly(&mut nodes, 12345);

    for node in nodes.iter() {
        //node.print_chunks();
    }
    for node in nodes.iter() {
        //node.print_misses();
    }

    println!("Running simulation...");

    let mut sim = Simulator::new(&mut nodes);
    sim.schedule_now(Box::new(move |ctxt| {
        for i in 0..n {
            Node::start_recovery(i, ctxt);
        }
    }));
    
    sim.run();
    println!("Time elapsed: {}", sim.now());
    println!("Time estimation: {}", config.est_rec_t());
}
