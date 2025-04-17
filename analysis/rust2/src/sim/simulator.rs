use super::*;

type Simulator<'a, C> = Queue<'a, C, Time>;

mod tests {
    use super::*;

    #[test]
    fn it_works() {
        let mut x = 0;
        {
            let mut q = Simulator::new(&mut x);
            q.schedule_abs(|q| {
                let x: i32 = *q.ctxt();
                assert!(x == 0);
                let y: &mut i32 = q.ctxt();
                *y += 1;
            }, Time::seconds(1));
            q.schedule_abs(|q| {
                let x: i32 = *q.ctxt();
                assert!(x == 0);
            }, Time::zero());
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