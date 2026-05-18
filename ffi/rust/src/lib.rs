//! Rust FFI bindings for the doppler C library.
//!
//! Each submodule mirrors one C header under `native/inc/`:
//!
//! | Module   | C header                  | Key types / functions            |
//! |----------|---------------------------|----------------------------------|
//! | [`acc`]  | `accumulator/accumulator.h` | [`acc::AccF32`], [`acc::AccCf64`] |
//! | [`fft`]  | `fft/fft_core.h`          | [`fft::Fft`]                     |
//! | [`fir`]  | `fir/fir_core.h`          | [`fir::Fir`]                     |
//! | [`lo`]   | `lo/lo_core.h`            | [`lo::Lo`]                       |
//! | [`nco`]  | `nco/nco_core.h`          | [`nco::Nco`]                     |
//! | [`util`] | —                         | [`c16_mul`]                      |
//!
//! # Quick FFT example
//!
//! ```no_run
//! use doppler::fft::{Fft, Direction};
//! use num_complex::Complex64;
//!
//! let n = 1024_usize;
//! let input:  Vec<Complex64> = (0..n).map(|i| {
//!     let t = 2.0 * std::f64::consts::PI * i as f64 / n as f64;
//!     Complex64::new(t.cos(), 0.0)
//! }).collect();
//! let mut output = vec![Complex64::default(); n];
//!
//! let fft = Fft::new(n, Direction::Forward);
//! fft.execute_cf64(&input, &mut output);
//! // output[1] and output[N-1] contain the cosine energy (~N/2 each)
//! ```

pub mod acc;
pub mod fft;
pub mod fir;
pub mod lo;
pub mod nco;
pub mod util;

mod types;
pub use types::{DpCf32, DpCi16, DpCi32, DpCi8};

/// Complex multiplication — re-exported from [`util`].
pub use util::c16_mul;
