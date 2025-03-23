pub mod time;

use std::cmp::Ordering;
use std::sync::atomic::AtomicI32;
use std::sync::atomic;
use std::thread;
use std::sync::mpsc;
use std::sync::Arc;
use std::collections::BinaryHeap;
use time::Time;

type Callback<T> = Box<dyn Fn(
    &mut Context<T>,
) + Send>;

struct Action<T> {
    when: Time,
    cb: Callback<T>
}

impl<T> PartialOrd for Action<T> {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> { 
        self.when.partial_cmp(&other.when)
    }
}

impl<T> Ord for Action<T> {
    fn cmp(&self, other: &Self) -> Ordering {
        self.when.cmp(&other.when)
    }
}

impl<T> PartialEq for Action<T> {
    fn eq(&self, other: &Self) -> bool {
        self.when == other.when
    }
}

impl<T> Eq for Action<T> {}

enum Event<T> {
    Call(Action<T>),
    /// Last to have least priority when pushed to the event queue.
    Stop
}

impl<T> Ord for Event<T> {
    fn cmp(&self, other: &Self) -> Ordering {
        match (self, other) {
            // Stop has least priority
            (Event::Stop, Event::Stop) => Ordering::Equal,
            (Event::Stop, _) => Ordering::Less,
            (_, Event::Stop) => Ordering::Greater,
            (Event::Call(a), Event::Call(b)) => a.when.cmp(&b.when),
        }
    }
}

impl<T> PartialOrd for Event<T> {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}

impl<T> PartialEq for Event<T> {
    fn eq(&self, other: &Self) -> bool {
        self.cmp(other) == Ordering::Equal
    }
}

impl<T> Eq for Event<T> {}

type EventQueue<T> = BinaryHeap<Event<T>>;

pub struct Simulator {
}

/// One context per node, potentially in its own thread
pub struct Context<T> {
    queue: EventQueue<T>,
    rx: mpsc::Receiver<Event<T>>,
    txes: Vec<mpsc::Sender<Event<T>>>,
    /// Count of alive contexts
    /// When this count is zero, all event queues are finished,
    /// and no more event can be pushed. Then, the simulation stops.
    alive: Arc<AtomicI32>,
    now: Time,
    node: T,
    running: bool,
    /// If anything was scheduled this iteration,
    /// Cannot stop
    scheduled: bool
}

impl<T> Context<T> {
    fn new(nodes: Vec<T>) -> Vec<Context<T>> {

        // Create one channel per node
        // `tx[i]` and `rx[i]` store the channel of the node `i`
        let mut txes = Vec::new();
        let mut rxes = Vec::new();
        
        for _ in nodes.iter() {
            let (tx, rx) = mpsc::channel();
            txes.push(tx);
            rxes.push(rx);
        }

        // At first all contexts are alive
        let alive = Arc::new(
            AtomicI32::new(nodes.len() as i32)
        );

        // Create the contexts
        let mut ctxts = Vec::new();

        for node in nodes.into_iter().rev() {
            ctxts.push(Self {
                queue: EventQueue::<T>::new(),
                rx: rxes.pop().unwrap(),
                txes: txes.clone(),
                alive: alive.clone(),
                now: Time::zero(),
                node: node,
                running: true,
                scheduled: false
            });
        }
        ctxts
    }

    fn drain(&mut self) {
        while let Some(ev) = self.queue.pop() {
            match ev {
                Event::Call(action) => {
                    self.now = action.when;
                    (action.cb)(self);
                },
                Event::Stop => {
                    self.running = false;
                }
            }
        }
    }

    fn recv(&mut self) {
        loop {
            match self.rx.try_recv() {
                Ok(ev) => self.queue.push(ev),
                Err(_) => {
                    break
                }
            }
        }
    }

    fn notify_stop(&mut self) {
        for tx in self.txes.iter_mut() {
            tx.send(Event::Stop).unwrap();
        }
    }

    fn run(mut self) -> Time {
        loop {
            self.scheduled = false;
            self.recv();
            self.drain();
            
            if !self.running {
                break;
            }

            if self.queue.is_empty() {
                if !self.scheduled {
                    let prev = self.alive.fetch_sub(1, atomic::Ordering::SeqCst);
                    assert!(prev > 0);
                    
                    if prev == 1 {
                        // All dead!
                        self.notify_stop();
                    }
                }

                // No more work to do, just wait the next message.
                // So this will consume no CPU time.
                // To notify the end, also a message should arrive.
                self.queue.push(self.rx.recv().unwrap());

                if !self.scheduled {
                    // Revive!
                    self.alive.fetch_add(1, atomic::Ordering::SeqCst);
                }

                //println!("{}", self.alive.load(atomic::Ordering::SeqCst));
            }
        }
        self.now
    }

    pub fn now(&self) -> Time {
        self.now
    }

    pub fn node(&mut self) -> &mut T {
       &mut self.node
    }
    
    pub fn schedule<'b>(&mut self, node: i64, rel: Time, cb: Callback<T>) {
        self.scheduled = true;
        self.txes[node as usize].send(Event::Call(Action {
            when: self.now + rel,
            cb: cb,
        })).unwrap();
        // println!("{}s", self.now.as_seconds());
    }
}

impl Simulator {
    /// At the begining of the simulation, calls `start` on every node.
    pub fn run<T, F>(
        nodes: Vec<T>,
        start: F
    ) -> Time where
        F: Fn(&mut Context<T>) + Send + Copy + 'static,
        T: Send
    {
        let n = nodes.len();
        
        let ctxts = Context::new(nodes);
        
        // to aggregate result
        let (tx, rx) = mpsc::channel();

        thread::scope(|s| {
            for (i, mut ctxt) in ctxts.into_iter().enumerate() {
                let cb = Box::new(start.clone());
                let tx = tx.clone();
                s.spawn(move || {
                    ctxt.schedule(
                        i as i64,
                        Time::zero(),
                        cb
                    );
                    tx.send(ctxt.run()).unwrap();
                });
            }
        });

        let mut elapsed = Time::zero();
        for _ in 0..n {
            elapsed = Time::max(elapsed, rx.recv().unwrap());
        }
        elapsed
    }
}