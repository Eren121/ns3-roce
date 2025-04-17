use std::ops::{Add, AddAssign, Mul, MulAssign, Div, DivAssign};
use std::fmt;
use bigdecimal::{BigDecimal, FromPrimitive, ToPrimitive};

type Quanta = i128;

#[derive(Copy, Clone, PartialOrd, Ord, PartialEq, Eq)]
pub struct Time(Quanta);

const TEN: Quanta = 10;
const ONE_SECOND_BASE10: u32 = 15;
const ONE_SECOND: Quanta = TEN.pow(ONE_SECOND_BASE10);

impl Time {
    pub fn seconds(s: Quanta) -> Self {
        Self(s * TEN.pow(ONE_SECOND_BASE10))
    }

    pub fn millis(ms: Quanta) -> Self {
        Self(ms * TEN.pow(ONE_SECOND_BASE10 - 3))
    }

    pub fn micros(us: Quanta) -> Self {
        Self(us * TEN.pow(ONE_SECOND_BASE10 - 6))
    }

    pub fn nanos(ns: Quanta) -> Self {
        Self(ns * TEN.pow(ONE_SECOND_BASE10 - 9))
    }

    fn as_seconds_dec(&self) -> BigDecimal {
        let q = BigDecimal::from_i128(self.0).unwrap();
        q / ONE_SECOND
    }

    pub fn as_seconds(&self) -> f64 {
        self.as_seconds_dec().to_f64().unwrap()
    }

    pub fn as_millis(&self) -> f64 {
        (self.as_seconds_dec() * TEN.pow(3)).to_f64().unwrap()
    }

    pub fn as_micros(&self) -> f64 {
        (self.as_seconds_dec() * TEN.pow(6)).to_f64().unwrap()
    }

    pub fn zero() -> Self {
        Self::seconds(0)
    }
}

impl fmt::Display for Time {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{}s", self.as_seconds())
    }
}

impl Add for Time {
    type Output = Self;
    fn add(self, rhs: Self) -> Self::Output {
        Time(self.0 + rhs.0)
    }
}

impl AddAssign for Time {
    fn add_assign(&mut self, rhs: Self) {
        self.0 += rhs.0;
    }
}

impl Mul<Quanta> for Time {
    type Output = Self;
    fn mul(self, rhs: Quanta) -> Self::Output {
        Time(self.0 * rhs)
    }
}

impl MulAssign<Quanta> for Time {
    fn mul_assign(&mut self, rhs: Quanta) {
        self.0 *= rhs;
    }
}

impl Div<Quanta> for Time {
    type Output = Self;
    fn div(self, rhs: Quanta) -> Self::Output {
        Time(self.0 / rhs)
    }
}

impl DivAssign<Quanta> for Time {
    fn div_assign(&mut self, rhs: Quanta) {
        self.0 /= rhs;
    }
}

#[derive(Copy, Clone)]
pub struct Bw {
    bps: Bits
}

impl Bw {
    pub fn from_bits(bps: Bits) -> Bw {
        Bw {
            bps: bps
        }
    }

    pub fn from_gbits(gbps: i64) -> Bw {
        Self::from_bits(gbps * 1_000_000_000)
    }

    pub fn bytes_tx_time(&self, bytes: Bytes) -> Time {
        Time(ONE_SECOND * bytes as Quanta * 8 / self.bps as Quanta)
    }

    pub fn max<'a>(lhs: &'a Time, rhs: &'a Time) -> &'a Time {
        if lhs.0 > rhs.0 {
            lhs
        }
        else {
            rhs
        }
    }

    pub fn min<'a>(lhs: &'a Time, rhs: &'a Time) -> &'a Time {
        if lhs.0 < rhs.0 {
            lhs
        }
        else {
            rhs
        }
    }
}

pub type Bytes = i64;
pub type Bits = i64;