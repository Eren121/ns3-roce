use std::cmp::Ordering;
use std::collections::BinaryHeap;
use super::Time;

// The lifetime is the one of the nodes.

/// Callback that takes one context parameter by mutable reference and returns nothing.
pub type Callback<T> = Box<dyn Fn(&mut T)>;

/// An EventContext stores the current node and can schedule events.
pub trait EventContext<T> {
    /// Get the node `i`.
    fn node(&mut self, i: i32) -> &mut T;

    /// Get the current time in the simulation.
    fn now(&self) -> Time;

    /// Schedule an event on node `node` from `rel` time relatively to the current time.
    /// The callback is `cb`.
    fn schedule(&mut self, rel: Time, cb: Callback<Self>);

    fn schedule_now(&mut self, cb: Callback<Self>) {
        self.schedule(self.now(), cb)
    }
}

/// Event is sorted by `when` field.
/// Since `BinaryHeap` is a max-heap, and we want a min-heap, the comparison `Ord` is reversed.
pub struct Event<T> {
    pub when: Time,
    pub cb: Callback<T>
}

impl<T> PartialOrd for Event<T> {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> { 
        other.when.partial_cmp(&self.when)
    }
}

impl<T> Ord for Event<T> {
    fn cmp(&self, other: &Self) -> Ordering {
        other.when.cmp(&self.when)
    }
}

impl<T> PartialEq for Event<T> {
    fn eq(&self, other: &Self) -> bool {
        self.when == other.when
    }
}

impl<T> Eq for Event<T> {
}

pub type EventQueue<T> = BinaryHeap<Event<T>>;
