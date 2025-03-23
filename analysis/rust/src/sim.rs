pub mod time;
pub mod event;

use event::*;
use time::*;

pub struct Simulator<'a, T> {
    nodes: Vec<&'a mut T>,
    queue: EventQueue<Self>,
    now: Time,
}

impl<'a, T> Simulator<'a, T> {
    pub fn new(nodes_slice: &'a mut [T]) -> Self {
        let mut nodes = Vec::new();
        for node in nodes_slice {
            nodes.push(node)
        }
        Self {
            nodes: nodes,
            queue: EventQueue::new(),
            now: Time::zero()
        }
    }
}

impl<'a, T> EventContext<T> for Simulator<'a, T> {
    fn node(&mut self, i: i32) -> &mut T {
        self.nodes[i as usize]
    }

    fn now(&self) -> Time {
        self.now
    }

    fn schedule(&mut self, rel: Time, cb: Callback<Self>) {
        self.queue.push( Event {
            when: self.now + rel,
            cb: cb
        });
    }
}

impl<'a, T> Simulator<'a, T> {
    pub fn run(&mut self) {
        while let Some(ev) = self.queue.pop() {
            self.now = ev.when;
            (ev.cb)(self);
        }
    }
}