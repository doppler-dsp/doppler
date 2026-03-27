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
// C-compatible sample types
// ---------------------------------------------------------------------------

/// Complex 32-bit float sample (`{float i; float q;}`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct DpCf32 {
    pub i: f32,
    pub q: f32,
}

impl From<DpCf32> for num_complex::Complex<f32> {
    fn from(s: DpCf32) -> Self {
        num_complex::Complex::new(s.i, s.q)
    }
}

impl From<num_complex::Complex<f32>> for DpCf32 {
    fn from(c: num_complex::Complex<f32>) -> Self {
        DpCf32 { i: c.re, q: c.im }
    }
}

/// Complex 8-bit integer sample (`{int8_t i; int8_t q;}`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct DpCi8 {
    pub i: i8,
    pub q: i8,
}

/// Complex 16-bit integer sample (`{int16_t i; int16_t q;}`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct DpCi16 {
    pub i: i16,
    pub q: i16,
}

/// Complex 32-bit integer sample (`{int32_t i; int32_t q;}`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct DpCi32 {
    pub i: i32,
    pub q: i32,
}

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

    // --- NCO ---
    pub fn dp_nco_create(norm_freq: f32) -> *mut DpNcoRaw;
    pub fn dp_nco_set_freq(nco: *mut DpNcoRaw, norm_freq: f32);
    pub fn dp_nco_reset(nco: *mut DpNcoRaw);
    pub fn dp_nco_destroy(nco: *mut DpNcoRaw);
    pub fn dp_nco_execute_cf32(
        nco: *mut DpNcoRaw,
        out: *mut DpCf32,
        n: usize,
    );
    pub fn dp_nco_execute_cf32_ctrl(
        nco: *mut DpNcoRaw,
        ctrl: *const f32,
        out: *mut DpCf32,
        n: usize,
    );
    pub fn dp_nco_execute_u32(
        nco: *mut DpNcoRaw,
        out: *mut u32,
        n: usize,
    );
    pub fn dp_nco_execute_u32_ovf(
        nco: *mut DpNcoRaw,
        out: *mut u32,
        ovf: *mut u8,
        n: usize,
    );
    pub fn dp_nco_execute_u32_ctrl(
        nco: *mut DpNcoRaw,
        ctrl: *const f32,
        out: *mut u32,
        n: usize,
    );
    pub fn dp_nco_execute_u32_ovf_ctrl(
        nco: *mut DpNcoRaw,
        ctrl: *const f32,
        out: *mut u32,
        ovf: *mut u8,
        n: usize,
    );

    // --- FIR ---
    pub fn dp_fir_create(
        taps: *const DpCf32,
        num_taps: usize,
    ) -> *mut DpFirRaw;
    pub fn dp_fir_create_real(
        taps: *const f32,
        num_taps: usize,
    ) -> *mut DpFirRaw;
    pub fn dp_fir_reset(fir: *mut DpFirRaw);
    pub fn dp_fir_destroy(fir: *mut DpFirRaw);
    pub fn dp_fir_execute_cf32(
        fir: *mut DpFirRaw,
        input: *const DpCf32,
        output: *mut DpCf32,
        n: usize,
    ) -> c_int;
    pub fn dp_fir_execute_ci8(
        fir: *mut DpFirRaw,
        input: *const DpCi8,
        output: *mut DpCf32,
        n: usize,
    ) -> c_int;
    pub fn dp_fir_execute_ci16(
        fir: *mut DpFirRaw,
        input: *const DpCi16,
        output: *mut DpCf32,
        n: usize,
    ) -> c_int;
    pub fn dp_fir_execute_ci32(
        fir: *mut DpFirRaw,
        input: *const DpCi32,
        output: *mut DpCf32,
        n: usize,
    ) -> c_int;
    pub fn dp_fir_execute_real_cf32(
        fir: *mut DpFirRaw,
        input: *const DpCf32,
        output: *mut DpCf32,
        n: usize,
    ) -> c_int;
    pub fn dp_fir_execute_real_ci8(
        fir: *mut DpFirRaw,
        input: *const DpCi8,
        output: *mut DpCf32,
        n: usize,
    ) -> c_int;
    pub fn dp_fir_execute_real_ci16(
        fir: *mut DpFirRaw,
        input: *const DpCi16,
        output: *mut DpCf32,
        n: usize,
    ) -> c_int;
    pub fn dp_fir_execute_real_ci32(
        fir: *mut DpFirRaw,
        input: *const DpCi32,
        output: *mut DpCf32,
        n: usize,
    ) -> c_int;
}

/// Opaque C NCO handle.  Never construct directly — use [`nco::Nco`].
#[repr(C)]
pub struct DpNcoRaw {
    _priv: [u8; 0],
}

/// Opaque C FIR handle.  Never construct directly — use [`fir::Fir`].
#[repr(C)]
pub struct DpFirRaw {
    _priv: [u8; 0],
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

// ---------------------------------------------------------------------------
// Safe NCO wrapper
// ---------------------------------------------------------------------------

/// Numerically-controlled oscillator (NCO).
///
/// Wraps `dp_nco_t`.  The NCO generates complex or raw-phase samples at a
/// normalised frequency `f` (cycles per sample, range `[0, 1)`).
///
/// # Example
/// ```no_run
/// use doppler::nco::Nco;
/// use num_complex::Complex;
///
/// // 0.1 cycles/sample → period of 10 samples
/// let mut nco = Nco::new(0.1);
/// let samples = nco.execute_cf32(64);
/// assert_eq!(samples.len(), 64);
/// ```
pub mod nco {
    use super::{
        dp_nco_create, dp_nco_destroy, dp_nco_execute_cf32,
        dp_nco_execute_cf32_ctrl, dp_nco_execute_u32, dp_nco_execute_u32_ctrl,
        dp_nco_execute_u32_ovf, dp_nco_execute_u32_ovf_ctrl, dp_nco_reset,
        dp_nco_set_freq, DpCf32, DpNcoRaw,
    };
    use num_complex::Complex;

    /// RAII wrapper around `dp_nco_t`.
    pub struct Nco {
        ptr: *mut DpNcoRaw,
    }

    // dp_nco_t owns no thread-local state after creation; all mutations go
    // through &mut self so there is no aliasing.
    unsafe impl Send for Nco {}

    impl Nco {
        /// Create an NCO running at `norm_freq` cycles per sample.
        ///
        /// # Panics
        /// Panics if `dp_nco_create` returns null.
        pub fn new(norm_freq: f32) -> Self {
            let ptr = unsafe { dp_nco_create(norm_freq) };
            assert!(!ptr.is_null(), "dp_nco_create returned null");
            Nco { ptr }
        }

        /// Change the oscillator frequency (cycles per sample).
        pub fn set_freq(&mut self, norm_freq: f32) {
            unsafe { dp_nco_set_freq(self.ptr, norm_freq) }
        }

        /// Reset the phase accumulator to zero.
        pub fn reset(&mut self) {
            unsafe { dp_nco_reset(self.ptr) }
        }

        /// Generate `n` complex samples (`Complex<f32>`, unit magnitude).
        pub fn execute_cf32(&mut self, n: usize) -> Vec<Complex<f32>> {
            let mut buf: Vec<DpCf32> = vec![DpCf32::default(); n];
            unsafe { dp_nco_execute_cf32(self.ptr, buf.as_mut_ptr(), n) }
            buf.into_iter().map(Complex::from).collect()
        }

        /// Generate `n` complex samples with per-sample frequency control.
        ///
        /// `ctrl` must have length `n`; each entry is a frequency offset
        /// (cycles per sample) added to the base frequency for that sample.
        ///
        /// # Panics
        /// Panics if `ctrl.len() != n`.
        pub fn execute_cf32_ctrl(
            &mut self,
            ctrl: &[f32],
            n: usize,
        ) -> Vec<Complex<f32>> {
            assert_eq!(ctrl.len(), n, "ctrl length must equal n");
            let mut buf: Vec<DpCf32> = vec![DpCf32::default(); n];
            unsafe {
                dp_nco_execute_cf32_ctrl(
                    self.ptr,
                    ctrl.as_ptr(),
                    buf.as_mut_ptr(),
                    n,
                )
            }
            buf.into_iter().map(Complex::from).collect()
        }

        /// Generate `n` raw 32-bit phase accumulator values.
        pub fn execute_u32(&mut self, n: usize) -> Vec<u32> {
            let mut buf: Vec<u32> = vec![0u32; n];
            unsafe { dp_nco_execute_u32(self.ptr, buf.as_mut_ptr(), n) }
            buf
        }

        /// Generate `n` phase values and an overflow (cycle) flag per sample.
        ///
        /// Returns `(phases, overflows)` where `overflows[i] == 1` whenever
        /// the accumulator wrapped on sample `i`.
        pub fn execute_u32_ovf(&mut self, n: usize) -> (Vec<u32>, Vec<u8>) {
            let mut phase: Vec<u32> = vec![0u32; n];
            let mut ovf: Vec<u8> = vec![0u8; n];
            unsafe {
                dp_nco_execute_u32_ovf(
                    self.ptr,
                    phase.as_mut_ptr(),
                    ovf.as_mut_ptr(),
                    n,
                )
            }
            (phase, ovf)
        }

        /// Generate `n` phase values with per-sample frequency control.
        pub fn execute_u32_ctrl(
            &mut self,
            ctrl: &[f32],
            n: usize,
        ) -> Vec<u32> {
            assert_eq!(ctrl.len(), n, "ctrl length must equal n");
            let mut buf: Vec<u32> = vec![0u32; n];
            unsafe {
                dp_nco_execute_u32_ctrl(
                    self.ptr,
                    ctrl.as_ptr(),
                    buf.as_mut_ptr(),
                    n,
                )
            }
            buf
        }

        /// Generate `n` phase values with frequency control and overflow flags.
        pub fn execute_u32_ovf_ctrl(
            &mut self,
            ctrl: &[f32],
            n: usize,
        ) -> (Vec<u32>, Vec<u8>) {
            assert_eq!(ctrl.len(), n, "ctrl length must equal n");
            let mut phase: Vec<u32> = vec![0u32; n];
            let mut ovf: Vec<u8> = vec![0u8; n];
            unsafe {
                dp_nco_execute_u32_ovf_ctrl(
                    self.ptr,
                    ctrl.as_ptr(),
                    phase.as_mut_ptr(),
                    ovf.as_mut_ptr(),
                    n,
                )
            }
            (phase, ovf)
        }
    }

    impl Drop for Nco {
        fn drop(&mut self) {
            unsafe { dp_nco_destroy(self.ptr) }
        }
    }
}

// ---------------------------------------------------------------------------
// Safe FIR wrapper
// ---------------------------------------------------------------------------

/// Finite impulse response (FIR) filter.
///
/// Wraps `dp_fir_t`.  Create with complex taps ([`Fir::new`]) or real taps
/// ([`Fir::new_real`]).
///
/// # Example
/// ```no_run
/// use doppler::fir::Fir;
/// use num_complex::Complex;
///
/// // Single tap at 1.0 = identity filter
/// let mut fir = Fir::new_real(&[1.0_f32]);
/// let input: Vec<Complex<f32>> = (0..8)
///     .map(|i| Complex::new(i as f32, 0.0))
///     .collect();
/// let output = fir.execute_cf32(&input).unwrap();
/// assert_eq!(output.len(), input.len());
/// ```
pub mod fir {
    use super::{
        dp_fir_create, dp_fir_create_real, dp_fir_destroy,
        dp_fir_execute_cf32, dp_fir_execute_ci16, dp_fir_execute_ci32,
        dp_fir_execute_ci8, dp_fir_execute_real_cf32, dp_fir_execute_real_ci16,
        dp_fir_execute_real_ci32, dp_fir_execute_real_ci8, dp_fir_reset,
        DpCf32, DpCi16, DpCi32, DpCi8, DpFirRaw,
    };
    use num_complex::Complex;

    /// RAII wrapper around `dp_fir_t`.
    pub struct Fir {
        ptr: *mut DpFirRaw,
        real_taps: bool,
    }

    unsafe impl Send for Fir {}

    impl Fir {
        /// Create a FIR filter with complex taps.
        ///
        /// # Panics
        /// Panics if `dp_fir_create` returns null.
        pub fn new(taps: &[Complex<f32>]) -> Self {
            let c_taps: Vec<DpCf32> =
                taps.iter().copied().map(DpCf32::from).collect();
            let ptr = unsafe {
                dp_fir_create(c_taps.as_ptr(), c_taps.len())
            };
            assert!(!ptr.is_null(), "dp_fir_create returned null");
            Fir { ptr, real_taps: false }
        }

        /// Create a FIR filter with real (scalar) taps.
        ///
        /// # Panics
        /// Panics if `dp_fir_create_real` returns null.
        pub fn new_real(taps: &[f32]) -> Self {
            let ptr = unsafe {
                dp_fir_create_real(taps.as_ptr(), taps.len())
            };
            assert!(!ptr.is_null(), "dp_fir_create_real returned null");
            Fir { ptr, real_taps: true }
        }

        /// Reset the filter delay line to zero.
        pub fn reset(&mut self) {
            unsafe { dp_fir_reset(self.ptr) }
        }

        // ── Complex-taps execute paths ────────────────────────────────────

        /// Filter a block of `Complex<f32>` samples (complex taps).
        ///
        /// Returns an error string if the underlying C call fails.
        pub fn execute_cf32(
            &mut self,
            input: &[Complex<f32>],
        ) -> Result<Vec<Complex<f32>>, &'static str> {
            let n = input.len();
            let c_in: Vec<DpCf32> =
                input.iter().copied().map(DpCf32::from).collect();
            let mut c_out: Vec<DpCf32> = vec![DpCf32::default(); n];
            let rc = unsafe {
                dp_fir_execute_cf32(
                    self.ptr,
                    c_in.as_ptr(),
                    c_out.as_mut_ptr(),
                    n,
                )
            };
            if rc != 0 {
                return Err("dp_fir_execute_cf32 failed");
            }
            Ok(c_out.into_iter().map(Complex::from).collect())
        }

        /// Filter a block of `DpCi8` samples (complex taps, cf32 output).
        pub fn execute_ci8(
            &mut self,
            input: &[DpCi8],
        ) -> Result<Vec<Complex<f32>>, &'static str> {
            let n = input.len();
            let mut c_out: Vec<DpCf32> = vec![DpCf32::default(); n];
            let rc = unsafe {
                dp_fir_execute_ci8(
                    self.ptr,
                    input.as_ptr(),
                    c_out.as_mut_ptr(),
                    n,
                )
            };
            if rc != 0 { return Err("dp_fir_execute_ci8 failed"); }
            Ok(c_out.into_iter().map(Complex::from).collect())
        }

        /// Filter a block of `DpCi16` samples (complex taps, cf32 output).
        pub fn execute_ci16(
            &mut self,
            input: &[DpCi16],
        ) -> Result<Vec<Complex<f32>>, &'static str> {
            let n = input.len();
            let mut c_out: Vec<DpCf32> = vec![DpCf32::default(); n];
            let rc = unsafe {
                dp_fir_execute_ci16(
                    self.ptr,
                    input.as_ptr(),
                    c_out.as_mut_ptr(),
                    n,
                )
            };
            if rc != 0 { return Err("dp_fir_execute_ci16 failed"); }
            Ok(c_out.into_iter().map(Complex::from).collect())
        }

        /// Filter a block of `DpCi32` samples (complex taps, cf32 output).
        pub fn execute_ci32(
            &mut self,
            input: &[DpCi32],
        ) -> Result<Vec<Complex<f32>>, &'static str> {
            let n = input.len();
            let mut c_out: Vec<DpCf32> = vec![DpCf32::default(); n];
            let rc = unsafe {
                dp_fir_execute_ci32(
                    self.ptr,
                    input.as_ptr(),
                    c_out.as_mut_ptr(),
                    n,
                )
            };
            if rc != 0 { return Err("dp_fir_execute_ci32 failed"); }
            Ok(c_out.into_iter().map(Complex::from).collect())
        }

        // ── Real-taps execute paths ───────────────────────────────────────

        /// Filter a block of `Complex<f32>` samples (real taps).
        pub fn execute_real_cf32(
            &mut self,
            input: &[Complex<f32>],
        ) -> Result<Vec<Complex<f32>>, &'static str> {
            let n = input.len();
            let c_in: Vec<DpCf32> =
                input.iter().copied().map(DpCf32::from).collect();
            let mut c_out: Vec<DpCf32> = vec![DpCf32::default(); n];
            let rc = unsafe {
                dp_fir_execute_real_cf32(
                    self.ptr,
                    c_in.as_ptr(),
                    c_out.as_mut_ptr(),
                    n,
                )
            };
            if rc != 0 { return Err("dp_fir_execute_real_cf32 failed"); }
            Ok(c_out.into_iter().map(Complex::from).collect())
        }

        /// Filter a block of `DpCi8` samples (real taps, cf32 output).
        pub fn execute_real_ci8(
            &mut self,
            input: &[DpCi8],
        ) -> Result<Vec<Complex<f32>>, &'static str> {
            let n = input.len();
            let mut c_out: Vec<DpCf32> = vec![DpCf32::default(); n];
            let rc = unsafe {
                dp_fir_execute_real_ci8(
                    self.ptr,
                    input.as_ptr(),
                    c_out.as_mut_ptr(),
                    n,
                )
            };
            if rc != 0 { return Err("dp_fir_execute_real_ci8 failed"); }
            Ok(c_out.into_iter().map(Complex::from).collect())
        }

        /// Filter a block of `DpCi16` samples (real taps, cf32 output).
        pub fn execute_real_ci16(
            &mut self,
            input: &[DpCi16],
        ) -> Result<Vec<Complex<f32>>, &'static str> {
            let n = input.len();
            let mut c_out: Vec<DpCf32> = vec![DpCf32::default(); n];
            let rc = unsafe {
                dp_fir_execute_real_ci16(
                    self.ptr,
                    input.as_ptr(),
                    c_out.as_mut_ptr(),
                    n,
                )
            };
            if rc != 0 { return Err("dp_fir_execute_real_ci16 failed"); }
            Ok(c_out.into_iter().map(Complex::from).collect())
        }

        /// Filter a block of `DpCi32` samples (real taps, cf32 output).
        pub fn execute_real_ci32(
            &mut self,
            input: &[DpCi32],
        ) -> Result<Vec<Complex<f32>>, &'static str> {
            let n = input.len();
            let mut c_out: Vec<DpCf32> = vec![DpCf32::default(); n];
            let rc = unsafe {
                dp_fir_execute_real_ci32(
                    self.ptr,
                    input.as_ptr(),
                    c_out.as_mut_ptr(),
                    n,
                )
            };
            if rc != 0 { return Err("dp_fir_execute_real_ci32 failed"); }
            Ok(c_out.into_iter().map(Complex::from).collect())
        }

        /// Whether this filter was created with real (scalar) taps.
        pub fn is_real_taps(&self) -> bool {
            self.real_taps
        }
    }

    impl Drop for Fir {
        fn drop(&mut self) {
            unsafe { dp_fir_destroy(self.ptr) }
        }
    }
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

    // -- NCO tests -----------------------------------------------------------

    #[test]
    fn nco_cf32_output_length() {
        let mut osc = nco::Nco::new(0.1);
        let out = osc.execute_cf32(64);
        assert_eq!(out.len(), 64);
    }

    #[test]
    fn nco_cf32_unit_magnitude() {
        // Every sample should lie on the unit circle (magnitude ≈ 1.0)
        let mut osc = nco::Nco::new(0.1);
        let out = osc.execute_cf32(256);
        for (i, s) in out.iter().enumerate() {
            let mag = (s.re * s.re + s.im * s.im).sqrt();
            assert!(
                (mag - 1.0).abs() < 1e-5,
                "sample {i}: magnitude = {mag} (expected 1.0)"
            );
        }
    }

    #[test]
    fn nco_cf32_phase_advances() {
        // At f = 0.25 cycles/sample the phasor should rotate 90° per step.
        // arg(s[k]) ≈ 2π·0.25·k, so arg(s[1]) ≈ π/2.
        use std::f32::consts::PI;
        let mut osc = nco::Nco::new(0.25);
        let out = osc.execute_cf32(4);
        // s[0] angle ~ 0  (initial phase)
        // s[1] angle ~ π/2
        let angle1 = out[1].im.atan2(out[1].re);
        assert!(
            (angle1.abs() - PI / 2.0).abs() < 0.01,
            "expected ~π/2, got {angle1}"
        );
    }

    #[test]
    fn nco_reset_restores_phase() {
        let mut osc = nco::Nco::new(0.1);
        let first = osc.execute_cf32(16);
        osc.reset();
        let second = osc.execute_cf32(16);
        for (i, (a, b)) in first.iter().zip(second.iter()).enumerate() {
            assert!(
                (a.re - b.re).abs() < 1e-6 && (a.im - b.im).abs() < 1e-6,
                "sample {i} after reset: {a:?} != {b:?}"
            );
        }
    }

    #[test]
    fn nco_set_freq_changes_output() {
        let mut osc = nco::Nco::new(0.1);
        let a = osc.execute_cf32(32);
        osc.reset();
        osc.set_freq(0.3);
        let b = osc.execute_cf32(32);
        // With a different frequency the outputs should differ beyond noise.
        let diff: f32 = a
            .iter()
            .zip(b.iter())
            .map(|(x, y)| (x.re - y.re).abs() + (x.im - y.im).abs())
            .sum();
        assert!(diff > 1.0, "set_freq had no effect (diff={diff})");
    }

    #[test]
    fn nco_u32_output_length() {
        let mut osc = nco::Nco::new(0.1);
        let phases = osc.execute_u32(128);
        assert_eq!(phases.len(), 128);
    }

    #[test]
    fn nco_u32_ovf_wraps_at_full_cycle() {
        // At f = 0.5, the accumulator should overflow every 2 samples.
        let mut osc = nco::Nco::new(0.5);
        let (_, ovf) = osc.execute_u32_ovf(64);
        // Every other sample should be a wrap
        let wrap_count = ovf.iter().filter(|&&x| x != 0).count();
        assert!(
            wrap_count >= 20,
            "expected ~32 wraps at f=0.5, got {wrap_count}"
        );
    }

    // -- FIR tests -----------------------------------------------------------

    #[test]
    fn fir_real_identity() {
        // A single tap of 1.0 is an identity filter.
        let mut f = fir::Fir::new_real(&[1.0_f32]);
        let input: Vec<num_complex::Complex<f32>> = (0..8)
            .map(|i| num_complex::Complex::new(i as f32, -(i as f32)))
            .collect();
        let output = f.execute_real_cf32(&input).unwrap();
        for (i, (inp, out)) in input.iter().zip(output.iter()).enumerate() {
            assert!(
                (inp.re - out.re).abs() < 1e-6
                    && (inp.im - out.im).abs() < 1e-6,
                "sample {i}: {inp:?} != {out:?}"
            );
        }
    }

    #[test]
    fn fir_complex_identity() {
        // A single complex tap of (1+0j) is also an identity filter.
        use num_complex::Complex;
        let taps = [Complex::new(1.0_f32, 0.0_f32)];
        let mut f = fir::Fir::new(&taps);
        let input: Vec<Complex<f32>> = (0..8)
            .map(|i| Complex::new(i as f32, 0.5 * i as f32))
            .collect();
        let output = f.execute_cf32(&input).unwrap();
        for (i, (inp, out)) in input.iter().zip(output.iter()).enumerate() {
            assert!(
                (inp.re - out.re).abs() < 1e-5
                    && (inp.im - out.im).abs() < 1e-5,
                "sample {i}: {inp:?} != {out:?}"
            );
        }
    }

    #[test]
    fn fir_real_impulse_response() {
        // Taps [1, 2, 3]; impulse in => taps out (convolution with δ).
        let taps = [1.0_f32, 2.0, 3.0];
        let mut f = fir::Fir::new_real(&taps);
        let mut input = vec![
            num_complex::Complex::new(0.0_f32, 0.0);
            8
        ];
        input[0] = num_complex::Complex::new(1.0, 0.0); // impulse
        let output = f.execute_real_cf32(&input).unwrap();
        // The filter has 3 taps; after the impulse:
        // y[0] = tap[0]*1 = 1, y[1] = tap[1]*1 = 2, y[2] = tap[2]*1 = 3
        assert!((output[0].re - 1.0).abs() < 1e-5, "y[0]={}", output[0].re);
        assert!((output[1].re - 2.0).abs() < 1e-5, "y[1]={}", output[1].re);
        assert!((output[2].re - 3.0).abs() < 1e-5, "y[2]={}", output[2].re);
    }

    #[test]
    fn fir_reset_clears_state() {
        // Run filter, reset, run again — outputs should match from the start.
        let taps = [0.25_f32, 0.5, 0.25];
        let mut f = fir::Fir::new_real(&taps);
        let signal: Vec<num_complex::Complex<f32>> = (0..16)
            .map(|i| num_complex::Complex::new(i as f32, 0.0))
            .collect();
        let first = f.execute_real_cf32(&signal).unwrap();
        f.reset();
        let second = f.execute_real_cf32(&signal).unwrap();
        for (i, (a, b)) in first.iter().zip(second.iter()).enumerate() {
            assert!(
                (a.re - b.re).abs() < 1e-6,
                "sample {i} after reset: {a} != {b}"
            );
        }
    }

    #[test]
    fn fir_ci8_input() {
        use super::DpCi8;
        use num_complex::Complex;
        // Identity filter: single complex tap at (1+0j)
        let taps = [Complex::new(1.0_f32, 0.0_f32)];
        let mut f = fir::Fir::new(&taps);
        let input: Vec<DpCi8> = (0..8_i8)
            .map(|i| DpCi8 { i, q: -i })
            .collect();
        let output = f.execute_ci8(&input).unwrap();
        assert_eq!(output.len(), 8);
        for (k, (inp, out)) in input.iter().zip(output.iter()).enumerate() {
            assert!(
                (out.re - inp.i as f32).abs() < 1e-5,
                "ci8 sample {k}: re mismatch"
            );
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
