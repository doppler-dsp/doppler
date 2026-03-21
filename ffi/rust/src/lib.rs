//! Rust FFI bindings for the doppler C library.
//!
//! Exposes the core DSP and streaming APIs as thin unsafe wrappers.
//! Safe, ergonomic wrappers live in the submodules below.
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
//! // 1. Bind plan to these exact pointers
//! fft::setup_1d(n, fft::Direction::Forward);
//!
//! // 2. Fill input
//! for (i, x) in input.iter_mut().enumerate() {
//!     let t = 2.0 * std::f64::consts::PI * i as f64 / n as f64;
//!     *x = Complex64::new(t.cos(), 0.0);
//! }
//!
//! // 3. Execute (zero-copy — library writes directly into output)
//! fft::execute_1d(&input, &mut output);
//! // Y[1] and Y[N-1] now contain the cosine energy (~N/2 each)
//! ```

use num_complex::Complex64;
use std::os::raw::{c_char, c_int};

// ---------------------------------------------------------------------------
// Raw C bindings
// ---------------------------------------------------------------------------

extern "C" {
    // --- Version ---
    pub fn dp_version() -> *const c_char;

    // --- FFT global plan ---
    pub fn dp_fft_global_setup(
        shape: *const usize,
        ndim: usize,
        sign: c_int,
        nthreads: c_int,
        planner: *const c_char,
        wisdom_path: *const c_char,
    );
    pub fn dp_fft1d_execute(input: *const Complex64, output: *mut Complex64);
    pub fn dp_fft1d_execute_inplace(data: *mut Complex64);
    pub fn dp_fft2d_execute(input: *const Complex64, output: *mut Complex64);
    pub fn dp_fft2d_execute_inplace(data: *mut Complex64);

    // --- SIMD ---
    pub fn dp_c16_mul(a: Complex64, b: Complex64) -> Complex64;
}

// ---------------------------------------------------------------------------
// Safe FFT wrappers
// ---------------------------------------------------------------------------

pub mod fft {
    use super::*;
    use std::ffi::CString;

    /// FFT direction.
    #[derive(Clone, Copy, Debug, PartialEq, Eq)]
    pub enum Direction {
        /// Forward DFT (sign = +1 in doppler convention).
        Forward,
        /// Inverse DFT (sign = -1 in doppler convention).
        Inverse,
    }

    impl Direction {
        fn sign(self) -> c_int {
            match self {
                Direction::Forward => 1,
                Direction::Inverse => -1,
            }
        }
    }

    /// Set up a 1-D global FFT plan for buffers of length `n`.
    ///
    /// Uses `"estimate"` planner so input data is not overwritten during planning.
    /// Must be called **before** [`execute_1d`] or [`execute_1d_inplace`].
    pub fn setup_1d(n: usize, dir: Direction) {
        let shape = [n];
        let planner = CString::new("estimate").unwrap();
        let wisdom  = CString::new("").unwrap();
        unsafe {
            dp_fft_global_setup(
                shape.as_ptr(),
                1,
                dir.sign(),
                1,
                planner.as_ptr(),
                wisdom.as_ptr(),
            );
        }
    }

    /// Set up a 2-D global FFT plan for an `ny × nx` grid.
    pub fn setup_2d(ny: usize, nx: usize, dir: Direction) {
        let shape = [ny, nx];
        let planner = CString::new("estimate").unwrap();
        let wisdom  = CString::new("").unwrap();
        unsafe {
            dp_fft_global_setup(
                shape.as_ptr(),
                2,
                dir.sign(),
                1,
                planner.as_ptr(),
                wisdom.as_ptr(),
            );
        }
    }

    /// Execute a 1-D out-of-place FFT.
    ///
    /// # Panics
    /// Panics if `input` and `output` have different lengths.
    pub fn execute_1d(input: &[Complex64], output: &mut [Complex64]) {
        assert_eq!(
            input.len(),
            output.len(),
            "input and output must have the same length"
        );
        unsafe {
            dp_fft1d_execute(input.as_ptr(), output.as_mut_ptr());
        }
    }

    /// Execute a 1-D in-place FFT.
    pub fn execute_1d_inplace(data: &mut [Complex64]) {
        unsafe {
            dp_fft1d_execute_inplace(data.as_mut_ptr());
        }
    }

    /// Execute a 2-D out-of-place FFT (row-major, `ny * nx` elements).
    pub fn execute_2d(input: &[Complex64], output: &mut [Complex64]) {
        assert_eq!(input.len(), output.len());
        unsafe {
            dp_fft2d_execute(input.as_ptr(), output.as_mut_ptr());
        }
    }

    /// Execute a 2-D in-place FFT.
    pub fn execute_2d_inplace(data: &mut [Complex64]) {
        unsafe {
            dp_fft2d_execute_inplace(data.as_mut_ptr());
        }
    }
}

// ---------------------------------------------------------------------------
// Safe SIMD wrapper
// ---------------------------------------------------------------------------

/// SIMD-accelerated complex multiplication (`a * b`).
///
/// Uses SSE2 on x86-64 and a scalar C99 fallback on other architectures.
/// Equivalent to `a * b` for [`num_complex::Complex64`] but guaranteed to
/// use the hardware-accelerated path when available.
///
/// # Example
/// ```
/// use num_complex::Complex64;
/// use doppler::c16_mul;
///
/// let a = Complex64::new(1.0, 2.0);
/// let b = Complex64::new(3.0, 4.0);
/// let c = c16_mul(a, b);
/// assert!((c.re - (-5.0)).abs() < 1e-12);
/// assert!((c.im - 10.0).abs() < 1e-12);
/// ```
pub fn c16_mul(a: Complex64, b: Complex64) -> Complex64 {
    unsafe { dp_c16_mul(a, b) }
}

/// Return the doppler C library version string.
pub fn version() -> &'static str {
    unsafe {
        let ptr = dp_version();
        std::ffi::CStr::from_ptr(ptr)
            .to_str()
            .unwrap_or("<invalid utf8>")
    }
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

#[cfg(test)]
mod tests {
    use super::*;
    use num_complex::Complex64;
    use std::f64::consts::PI;

    // -- SIMD tests ----------------------------------------------------------

    #[test]
    fn c16_mul_basic() {
        // (1+2i) * (3+4i) = (1*3 - 2*4) + (1*4 + 2*3)i = -5 + 10i
        let a = Complex64::new(1.0, 2.0);
        let b = Complex64::new(3.0, 4.0);
        let c = c16_mul(a, b);
        assert!((c.re - (-5.0)).abs() < 1e-12);
        assert!((c.im - 10.0).abs() < 1e-12);
    }

    #[test]
    fn c16_mul_identity() {
        // x * 1 = x
        let x = Complex64::new(3.14, -2.71);
        let one = Complex64::new(1.0, 0.0);
        let r = c16_mul(x, one);
        assert!((r.re - x.re).abs() < 1e-12);
        assert!((r.im - x.im).abs() < 1e-12);
    }

    #[test]
    fn c16_mul_zero() {
        // x * 0 = 0
        let x = Complex64::new(42.0, -99.0);
        let zero = Complex64::new(0.0, 0.0);
        let r = c16_mul(x, zero);
        assert!((r.re).abs() < 1e-12);
        assert!((r.im).abs() < 1e-12);
    }

    #[test]
    fn c16_mul_conjugate() {
        // x * conj(x) = |x|^2 (real, non-negative)
        let x = Complex64::new(3.0, 4.0);
        let xc = Complex64::new(3.0, -4.0);
        let r = c16_mul(x, xc);
        assert!((r.re - 25.0).abs() < 1e-12);
        assert!((r.im).abs() < 1e-12);
    }

    #[test]
    fn c16_mul_matches_num_complex() {
        // Compare against num-complex's native multiplication
        let a = Complex64::new(-1.5, 2.7);
        let b = Complex64::new(0.3, -4.1);
        let expected = a * b;
        let got = c16_mul(a, b);
        assert!((got.re - expected.re).abs() < 1e-12);
        assert!((got.im - expected.im).abs() < 1e-12);
    }

    // -- Version test --------------------------------------------------------

    #[test]
    fn version_string_valid() {
        let v = version();
        assert!(!v.is_empty(), "version string should not be empty");
        // Should contain a dot (major.minor.patch format)
        assert!(v.contains('.'), "version string should contain a dot: {v}");
    }

    // -- 1-D FFT tests -------------------------------------------------------

    // NOTE: FFT tests use `cargo test -- --test-threads=1` because the
    // global FFTW plan is process-wide mutable state and is not thread-safe.

    #[test]
    fn fft1d_impulse() {
        // FFT of unit impulse => flat spectrum (all ones)
        let n = 16;
        let mut input = vec![Complex64::default(); n];
        let mut output = vec![Complex64::default(); n];
        input[0] = Complex64::new(1.0, 0.0);

        fft::setup_1d(n, fft::Direction::Forward);
        fft::execute_1d(&input, &mut output);

        for (k, &val) in output.iter().enumerate() {
            assert!(
                (val.re - 1.0).abs() < 1e-10,
                "bin {k}: re = {} (expected 1.0)",
                val.re
            );
            assert!(
                val.im.abs() < 1e-10,
                "bin {k}: im = {} (expected 0.0)",
                val.im
            );
        }
    }

    #[test]
    fn fft1d_round_trip() {
        // Forward then inverse FFT should recover the original signal
        let n = 64;
        let input: Vec<Complex64> = (0..n)
            .map(|i| {
                let t = 2.0 * PI * i as f64 / n as f64;
                Complex64::new(t.cos(), t.sin() * 0.5)
            })
            .collect();
        let original = input.clone();

        // Forward
        let mut freq = vec![Complex64::default(); n];
        fft::setup_1d(n, fft::Direction::Forward);
        fft::execute_1d(&input, &mut freq);

        // Inverse
        let mut recovered = vec![Complex64::default(); n];
        fft::setup_1d(n, fft::Direction::Inverse);
        fft::execute_1d(&freq, &mut recovered);

        // Normalise
        let norm = 1.0 / n as f64;
        for v in &mut recovered {
            *v *= norm;
        }

        for (i, (orig, rec)) in original.iter().zip(recovered.iter()).enumerate() {
            assert!(
                (orig.re - rec.re).abs() < 1e-10,
                "sample {i}: re mismatch ({} vs {})",
                orig.re,
                rec.re
            );
            assert!(
                (orig.im - rec.im).abs() < 1e-10,
                "sample {i}: im mismatch ({} vs {})",
                orig.im,
                rec.im
            );
        }
    }

    #[test]
    fn fft1d_cosine_energy() {
        // cos(2*pi*k*n/N) should put energy at bins k and N-k
        let n = 128;
        let k = 7;
        let input: Vec<Complex64> = (0..n)
            .map(|i| {
                let t = 2.0 * PI * k as f64 * i as f64 / n as f64;
                Complex64::new(t.cos(), 0.0)
            })
            .collect();
        let mut output = vec![Complex64::default(); n];

        fft::setup_1d(n, fft::Direction::Forward);
        fft::execute_1d(&input, &mut output);

        let half_n = n as f64 / 2.0;
        assert!(
            output[k].norm() > half_n - 1.0,
            "bin {k} should have ~N/2 energy"
        );
        assert!(
            output[n - k].norm() > half_n - 1.0,
            "bin {} should have ~N/2 energy",
            n - k
        );

        // All other bins near zero
        for (i, val) in output.iter().enumerate() {
            if i != k && i != n - k {
                assert!(
                    val.norm() < 1e-8,
                    "bin {i} should be near zero, got {}",
                    val.norm()
                );
            }
        }
    }

    // -- 2-D FFT tests -------------------------------------------------------

    #[test]
    fn fft2d_impulse() {
        let ny = 8;
        let nx = 8;
        let n = ny * nx;
        let mut input = vec![Complex64::default(); n];
        let mut output = vec![Complex64::default(); n];
        input[0] = Complex64::new(1.0, 0.0);

        fft::setup_2d(ny, nx, fft::Direction::Forward);
        fft::execute_2d(&input, &mut output);

        for (k, &val) in output.iter().enumerate() {
            assert!(
                (val.re - 1.0).abs() < 1e-10,
                "2D bin {k}: re = {} (expected 1.0)",
                val.re
            );
            assert!(
                val.im.abs() < 1e-10,
                "2D bin {k}: im = {} (expected 0.0)",
                val.im
            );
        }
    }

    #[test]
    fn fft2d_round_trip() {
        let ny = 16;
        let nx = 16;
        let n = ny * nx;
        let input: Vec<Complex64> = (0..n)
            .map(|i| Complex64::new((i as f64) * 0.01, -(i as f64) * 0.005))
            .collect();
        let original = input.clone();

        let mut freq = vec![Complex64::default(); n];
        fft::setup_2d(ny, nx, fft::Direction::Forward);
        fft::execute_2d(&input, &mut freq);

        let mut recovered = vec![Complex64::default(); n];
        fft::setup_2d(ny, nx, fft::Direction::Inverse);
        fft::execute_2d(&freq, &mut recovered);

        let norm = 1.0 / n as f64;
        for v in &mut recovered {
            *v *= norm;
        }

        for (i, (orig, rec)) in original.iter().zip(recovered.iter()).enumerate() {
            assert!(
                (orig - rec).norm() < 1e-10,
                "2D sample {i}: mismatch ({orig} vs {rec})"
            );
        }
    }
}
