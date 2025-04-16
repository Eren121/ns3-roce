/// Gilbert-Elliot Markov chains model with 2 states and 4 parameters.
/// 
///

use rand::Rng;

#[derive(Copy, Clone)]
pub struct Model {
    /// Probability to transition to bad state.
    bad_pr: f64,
    /// Probability to transition to good state.
    good_pr: f64,
    /// Probability to lose a packet in bad state.
    bad_loss: f64,
    /// Probability to lose a packet in good state.
    good_loss: f64
}

impl Model {
    /// Construct the model from the average length in each state, following a Bernouilli distribution.
    /// Arguments:
    ///     - bad_len: Average length of bad state
    ///     - good_len: Average length of good state
    ///     - bad_loss: Probability to lose a packet in bad state
    ///     - good_loss: Probability to lose a packet in good state
    pub fn from_bernouilli(bad_len: f64, good_len: f64, bad_loss: f64, good_loss: f64) -> Self {
        assert!(good_len >= 0.);
        assert!(bad_len >= 0.);
        assert!(bad_loss >= 0. && bad_loss <= 1.);
        assert!(good_loss >= 0. && good_loss <= 1.);
        Self {
            bad_pr: bad_len / (bad_len + good_len),
            good_pr: good_len / (bad_len + good_len),
            bad_loss,
            good_loss
        }
    }
}

enum State {
    Good,
    Bad
}

/// Markov chain model.
pub struct Chain<R: Rng> {
    model: Model,
    state: State,
    rng: R
}

impl<R: Rng> Chain<R> {
    pub fn new(model: Model, rng: R) -> Self {
        Self {
            model,
            state: State::Good,
            rng
        }
    }

    /// Advance the markov chain.
    /// Returns:
    ///     - true: Packet is received
    ///     - false: Packet is lost
    pub fn next(&mut self) -> bool {
        let ret;
        match self.state {
            State::Good => {
                ret = self.rng.random_bool(1. - self.model.good_loss);
                if self.rng.random_bool(self.model.bad_pr) {
                    self.state = State::Bad;
                }
            },
            State::Bad => {
                ret = self.rng.random_bool(1. - self.model.bad_loss);
                if self.rng.random_bool(self.model.good_pr) {
                    self.state = State::Good;
                }
            },
        }
        ret
    }
}