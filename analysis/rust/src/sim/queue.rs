use bigdecimal::Context;
use priority_queue::PriorityQueue;
use std::cmp::Reverse;
use std::hash::{Hash, Hasher};
use std::ops::Add;

/// T:
///     Time representation.
///     Should implement `Ord`.
///     Can be a integer type, eg. `i64` to represent seconds.
trait Time: Ord + Default {}
impl<T: Ord + Default> Time for T {}

/// Event callback that can be a closure (`|| {}`),
/// but any capture should not outlive lifetime `'a`.
trait EventCb<'a, C, T: Time>: FnOnce(&mut Queue<C, T>) + 'a {}

/// Automatically implement the `EventCb` trait for all `FnOnce` that have compatible lifetime.
impl<'a, C, T: Time, F: FnOnce(&mut Queue<C, T>) + 'a> EventCb<'a, C, T> for F {}

type Event<'a, C, T: Time> = Box<dyn EventCb<'a, C, T>>;
type EventId = i64;

struct EventEntry<'a, C, T: Time> {
    id: EventId,
    event: Event<'a, C, T>
}

/// Hash based on event ID.
impl<'a, C, T: Time> Hash for EventEntry<'a, C, T> {
    fn hash<H: Hasher>(&self, state: &mut H) {
        self.id.hash(state);
    }
}

/// Comparison based on event ID.
impl<'a, C, T: Time> PartialEq for EventEntry<'a, C, T> {
    fn eq(&self, other: &Self) -> bool {
        self.id.eq(&other.id)
    }
}

impl<'a, C, T: Time> Eq for EventEntry<'a, C, T> {}

struct Queue<'a, C, T: Time> {
    queue: PriorityQueue<EventEntry<'a, C, T>, Reverse<T>>,
    now: T,
    next_id: EventId,
    ctxt: C
}

/// When we implemented the custom logic `Context` to get the context, use it.
/// Prevent user having to type twice reference `**` for simple types (references and copiable types).
/// Allow getting a context without dereferencing twice with `**`, if the context is a reference.
/// 
/// - `GetContext<i32>`         => Allow sharing same syntax: `let x: i32 = *q.ctxt()`
/// - `GetContext<&mut i32>`    => Allow sharing same syntax: `let x: i32 = *q.ctxt()`
/// 
trait GetContext<C> {
    fn ctxt(&mut self) -> &mut C;
}

impl<'a, C, T: Time> GetContext<C> for Queue<'a, &mut C, T> {
    fn ctxt(&mut self) -> &mut C {
        self.ctxt
    }
}

impl<'a, C, T: Time> GetContext<C> for Queue<'a, C, T> {
    fn ctxt(&mut self) -> &mut C {
        &mut self.ctxt
    }
}

impl<'a, C, T: Time> Queue<'a, C, T> {
    fn new(ctxt: C) -> Queue<'a, C, T> {
        Queue {
            queue: PriorityQueue::new(),
            now: Default::default(),
            next_id: Default::default(),
            ctxt
        }
    }

    fn ctxt_ref(&mut self) -> &mut C {
        &mut self.ctxt
    }

    /// Run all the events until there are no more events.
    fn run(&mut self) {
        while !self.queue.is_empty() {
            let (entry, Reverse(when)) = self.queue.pop().expect("Queue was not empty in condition");
            self.now = when;
            (entry.event)(self);
        }
    }

    /// Schedule an event based on an absolute time.
    /// If time is already passed, then is scheduled immediately.
    fn schedule_abs(&mut self, ev: impl EventCb<'a, C, T>, when: T) {
        let ev = Box::new(ev);
        let entry = EventEntry {
            id: self.next_id,
            event: ev
        };
        self.next_id += 1;
        self.queue.push(entry, Reverse(when));
    }

    /// Schedule an event based on an relative time from now.
    /// Supported only if `T` implements addition.
    fn schedule_rel(&mut self, ev: impl EventCb<'a, C, T>, when: T)
    where
        T: Time + std::ops::Add<Output = T> + Copy
    {
        self.schedule_abs(ev, self.now + when);
    }
}

mod tests {
    use super::*;

    #[test]
    fn it_works() {
        let mut x = 0;
        {
            let mut q = Queue::<&mut i32, i64>::new(&mut x);
            q.schedule_abs(|q| {
                let x: i32 = *q.ctxt();
                assert!(x == 0);
                let y: &mut i32 = q.ctxt();
                *y += 1;
            }, 1);
            q.schedule_abs(|q| {
                let x: i32 = *q.ctxt();
                assert!(x == 0);
            }, 0);
            q.run();
        }
        assert!(x == 1);

        
        let mut q = Queue::<i32, i64>::new(10);
        q.schedule_abs(|q| {
            assert!(*q.ctxt() == 10);
            *q.ctxt() += 1;
        }, 1);
        q.run();
        assert!(*q.ctxt() == 11);
    }
}