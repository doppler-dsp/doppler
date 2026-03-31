//! Rust FFI bindings for the doppler C library.
//!
//! Each submodule mirrors one C header under `dp/`:
//!
//! | Module        | C header              | Key types / functions          |
//! |---------------|-----------------------|--------------------------------|
//! | [`acc`]       | `dp/accumulator.h`    | [`acc::AccF32`], [`acc::AccCf64`] |
//! | [`fft`]       | `dp/fft.h`            | [`fft::setup_1d`], [`fft::execute_1d`] |
//! | [`fir`]       | `dp/fir.h`            | [`fir::Fir`]                   |
//! | [`nco`]       | `dp/nco.h`            | [`nco::Nco`]                   |
//! | [`util`]      | `dp/util.h`           | [`c16_mul`]                    |
//!
//! # Zero-copy FFT contract
//!
//! The global-plan FFT API binds to specific buffer addresses at setup time:
//!
//! ```no_run
//! use doppler::fft;
//! use num_complex::Complex64;
//!
//! let n = 1024_usize;
//! let mut input  = vec![Complex64::default(); n];
//! let mut output = vec![Complex64::default(); n];
//!
//! fft::setup_1d(n, fft::Direction::Forward);
//!
//! for (i, x) in input.iter_mut().enumerate() {
//!     let t = 2.0 * std::f64::consts::PI * i as f64 / n as f64;
//!     *x = Complex64::new(t.cos(), 0.0);
//! }
//!
//! fft::execute_1d(&input, &mut output);
//! // Y[1] and Y[N-1] now contain the cosine energy (~N/2 each)
//! ```

use std::ffi::CStr;
use std::os::raw::c_char;

pub mod acc;
pub mod fft;
pub mod fir;
pub mod nco;
pub mod util;

mod types;
pub use types::{DpCf32, DpCi16, DpCi32, DpCi8};

/// SIMD-accelerated complex multiplication — re-exported from [`util`].
pub use util::c16_mul;

extern "C" {
    fn dp_version() -> *const c_char;
}

/// Return the doppler C library version string.
pub fn version() -> &'static str {
    unsafe {
        CStr::from_ptr(dp_version())
            .to_str()
            .unwrap_or("<invalid utf8>")
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn version_string_valid() {
        let v = version();
        assert!(!v.is_empty(), "version string should not be empty");
        assert!(v.contains('.'), "version should be major.minor.patch: {v}");
    }
}
