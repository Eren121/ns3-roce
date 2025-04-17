use std::ops::{Add, AddAssign, Mul, MulAssign, Div, DivAssign};
use std::fmt;
use bigdecimal::{BigDecimal, FromPrimitive, ToPrimitive};

pub type Quanta = i128;

#[derive(Copy, Clone, PartialOrd, Ord, PartialEq, Eq, Default)]
pub struct Time(Quanta);

macro_rules! pow10 {
    ($p:expr) => {
        {
            let mut res = 1i128;
            let mut i: i32 = 0;
            while i < $p {
                res *= 10i128;
                i += 1;
            }
            res
        }
    }
}

impl Time {
    pub const ONE_SECOND_BASE10: i32 = 15;
    pub const ONE_SECOND: Quanta = pow10!(Self::ONE_SECOND_BASE10);

    pub fn seconds(s: Quanta) -> Self {
        Self(s * pow10!(Self::ONE_SECOND_BASE10))
    }

    pub fn millis(ms: Quanta) -> Self {
        Self(ms * pow10!(Self::ONE_SECOND_BASE10 - 3))
    }

    pub fn micros(us: Quanta) -> Self {
        Self(us * pow10!(Self::ONE_SECOND_BASE10 - 6))
    }

    pub fn nanos(ns: Quanta) -> Self {
        Self(ns * pow10!(Self::ONE_SECOND_BASE10 - 9))
    }

    fn as_seconds_dec(&self) -> BigDecimal {
        let q = BigDecimal::from_i128(self.0).unwrap();
        q / Self::ONE_SECOND
    }

    pub fn as_seconds(&self) -> f64 {
        self.as_seconds_dec().to_f64().unwrap()
    }

    pub fn as_millis(&self) -> f64 {
        (self.as_seconds_dec() * pow10!(3)).to_f64().unwrap()
    }

    pub fn as_micros(&self) -> f64 {
        (self.as_seconds_dec() * pow10!(6)).to_f64().unwrap()
    }

    pub fn zero() -> Self {
        Self::seconds(0)
    }

    pub fn max<'a>(lhs: &'a Time, rhs: &'a Time) -> &'a Time {
        if lhs.0 > rhs.0 {
            lhs
        }
        else {
            rhs
        }
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

#[cfg(test)]
mod tests {
    #[test]
    fn pow10() {
        assert_eq!(pow10!(0), 1);
        assert_eq!(pow10!(1), 10);
        assert_eq!(pow10!(2), 100);
        assert_eq!(pow10!(14), 100_000_000_000_000);
    }
}