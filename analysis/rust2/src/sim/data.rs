use super::Time;
use super::time::Quanta;

pub type Bytes = usize;
pub type Bits = usize;

#[derive(Copy, Clone)]
pub struct Bw {
    bytes_per_s: Bytes
}

impl Bw {
    pub fn from_bits(bps: Bits) -> Bw {
        Bw {
            bytes_per_s: bps / 8
        }
    }

    pub fn from_gbits(gbps: usize) -> Bw {
        Self::from_bits(gbps * 1_000_000_000)
    }

    pub fn bytes_tx_time(&self, bytes: Bytes) -> Time {
        Time::seconds(Time::ONE_SECOND * bytes as Quanta / self.bytes_per_s as Quanta)
    }
}