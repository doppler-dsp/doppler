/// Finite impulse response filter (`native/inc/fir/fir_core.h`).
use crate::types::{DpCf32, DpCi16, DpCi32, DpCi8};
use num_complex::Complex;
use std::os::raw::c_int;

/// Opaque C FIR state.  Never construct directly — use [`Fir`].
#[repr(C)]
pub struct FirStateRaw {
    _priv: [u8; 0],
}

extern "C" {
    pub fn fir_create(
        taps: *const DpCf32,
        num_taps: usize,
    ) -> *mut FirStateRaw;
    pub fn fir_create_real(
        taps: *const f32,
        num_taps: usize,
    ) -> *mut FirStateRaw;
    pub fn fir_reset(f: *mut FirStateRaw);
    pub fn fir_destroy(f: *mut FirStateRaw);
    pub fn fir_execute_cf32(
        f: *mut FirStateRaw,
        input: *const DpCf32,
        output: *mut DpCf32,
        n: usize,
    ) -> c_int;
    pub fn fir_execute_ci8(
        f: *mut FirStateRaw,
        input: *const DpCi8,
        output: *mut DpCf32,
        n: usize,
    ) -> c_int;
    pub fn fir_execute_ci16(
        f: *mut FirStateRaw,
        input: *const DpCi16,
        output: *mut DpCf32,
        n: usize,
    ) -> c_int;
    pub fn fir_execute_ci32(
        f: *mut FirStateRaw,
        input: *const DpCi32,
        output: *mut DpCf32,
        n: usize,
    ) -> c_int;
    pub fn fir_execute_real_cf32(
        f: *mut FirStateRaw,
        input: *const DpCf32,
        output: *mut DpCf32,
        n: usize,
    ) -> c_int;
    pub fn fir_execute_real_ci8(
        f: *mut FirStateRaw,
        input: *const DpCi8,
        output: *mut DpCf32,
        n: usize,
    ) -> c_int;
    pub fn fir_execute_real_ci16(
        f: *mut FirStateRaw,
        input: *const DpCi16,
        output: *mut DpCf32,
        n: usize,
    ) -> c_int;
    pub fn fir_execute_real_ci32(
        f: *mut FirStateRaw,
        input: *const DpCi32,
        output: *mut DpCf32,
        n: usize,
    ) -> c_int;
}

/// RAII wrapper around `fir_state_t`.
///
/// Create with complex taps ([`Fir::new`]) or real taps
/// ([`Fir::new_real`]).
///
/// # Example
/// ```no_run
/// use doppler::fir::Fir;
/// use num_complex::Complex;
///
/// let mut fir = Fir::new_real(&[1.0_f32]); // identity
/// let input: Vec<Complex<f32>> =
///     (0..8).map(|i| Complex::new(i as f32, 0.0)).collect();
/// let output = fir.execute_real_cf32(&input).unwrap();
/// assert_eq!(output.len(), 8);
/// ```
pub struct Fir {
    ptr: *mut FirStateRaw,
    real_taps: bool,
}

unsafe impl Send for Fir {}

impl Fir {
    /// Create a FIR filter with complex CF32 taps.
    ///
    /// # Panics
    /// Panics if `fir_create` returns null.
    pub fn new(taps: &[Complex<f32>]) -> Self {
        let c_taps: Vec<DpCf32> =
            taps.iter().copied().map(DpCf32::from).collect();
        let ptr = unsafe { fir_create(c_taps.as_ptr(), c_taps.len()) };
        assert!(!ptr.is_null(), "fir_create returned null");
        Fir { ptr, real_taps: false }
    }

    /// Create a FIR filter with real (scalar) taps.
    ///
    /// Real taps cost 1 FMA per tap instead of 2, halving the multiply
    /// count for filters designed in the real domain (e.g. `firwin`).
    ///
    /// # Panics
    /// Panics if `fir_create_real` returns null.
    pub fn new_real(taps: &[f32]) -> Self {
        let ptr =
            unsafe { fir_create_real(taps.as_ptr(), taps.len()) };
        assert!(!ptr.is_null(), "fir_create_real returned null");
        Fir { ptr, real_taps: true }
    }

    /// Reset the filter delay line to zero.
    pub fn reset(&mut self) {
        unsafe { fir_reset(self.ptr) }
    }

    /// Whether this filter was created with real (scalar) taps.
    pub fn is_real_taps(&self) -> bool {
        self.real_taps
    }

    // ── Complex-taps execute paths ────────────────────────────────────────

    /// Filter a block of `Complex<f32>` samples (complex taps).
    pub fn execute_cf32(
        &mut self,
        input: &[Complex<f32>],
    ) -> Result<Vec<Complex<f32>>, &'static str> {
        let n = input.len();
        let c_in: Vec<DpCf32> =
            input.iter().copied().map(DpCf32::from).collect();
        let mut c_out = vec![DpCf32::default(); n];
        let rc = unsafe {
            fir_execute_cf32(
                self.ptr,
                c_in.as_ptr(),
                c_out.as_mut_ptr(),
                n,
            )
        };
        if rc != 0 {
            return Err("fir_execute_cf32 failed");
        }
        Ok(c_out.into_iter().map(Complex::from).collect())
    }

    /// Filter a block of `DpCi8` samples (complex taps, CF32 output).
    pub fn execute_ci8(
        &mut self,
        input: &[DpCi8],
    ) -> Result<Vec<Complex<f32>>, &'static str> {
        let n = input.len();
        let mut c_out = vec![DpCf32::default(); n];
        let rc = unsafe {
            fir_execute_ci8(
                self.ptr,
                input.as_ptr(),
                c_out.as_mut_ptr(),
                n,
            )
        };
        if rc != 0 {
            return Err("fir_execute_ci8 failed");
        }
        Ok(c_out.into_iter().map(Complex::from).collect())
    }

    /// Filter a block of `DpCi16` samples (complex taps, CF32 output).
    pub fn execute_ci16(
        &mut self,
        input: &[DpCi16],
    ) -> Result<Vec<Complex<f32>>, &'static str> {
        let n = input.len();
        let mut c_out = vec![DpCf32::default(); n];
        let rc = unsafe {
            fir_execute_ci16(
                self.ptr,
                input.as_ptr(),
                c_out.as_mut_ptr(),
                n,
            )
        };
        if rc != 0 {
            return Err("fir_execute_ci16 failed");
        }
        Ok(c_out.into_iter().map(Complex::from).collect())
    }

    /// Filter a block of `DpCi32` samples (complex taps, CF32 output).
    pub fn execute_ci32(
        &mut self,
        input: &[DpCi32],
    ) -> Result<Vec<Complex<f32>>, &'static str> {
        let n = input.len();
        let mut c_out = vec![DpCf32::default(); n];
        let rc = unsafe {
            fir_execute_ci32(
                self.ptr,
                input.as_ptr(),
                c_out.as_mut_ptr(),
                n,
            )
        };
        if rc != 0 {
            return Err("fir_execute_ci32 failed");
        }
        Ok(c_out.into_iter().map(Complex::from).collect())
    }

    // ── Real-taps execute paths ───────────────────────────────────────────

    /// Filter CF32 samples through a real-tap filter.
    pub fn execute_real_cf32(
        &mut self,
        input: &[Complex<f32>],
    ) -> Result<Vec<Complex<f32>>, &'static str> {
        let n = input.len();
        let c_in: Vec<DpCf32> =
            input.iter().copied().map(DpCf32::from).collect();
        let mut c_out = vec![DpCf32::default(); n];
        let rc = unsafe {
            fir_execute_real_cf32(
                self.ptr,
                c_in.as_ptr(),
                c_out.as_mut_ptr(),
                n,
            )
        };
        if rc != 0 {
            return Err("fir_execute_real_cf32 failed");
        }
        Ok(c_out.into_iter().map(Complex::from).collect())
    }

    /// Filter CI8 samples through a real-tap filter.
    pub fn execute_real_ci8(
        &mut self,
        input: &[DpCi8],
    ) -> Result<Vec<Complex<f32>>, &'static str> {
        let n = input.len();
        let mut c_out = vec![DpCf32::default(); n];
        let rc = unsafe {
            fir_execute_real_ci8(
                self.ptr,
                input.as_ptr(),
                c_out.as_mut_ptr(),
                n,
            )
        };
        if rc != 0 {
            return Err("fir_execute_real_ci8 failed");
        }
        Ok(c_out.into_iter().map(Complex::from).collect())
    }

    /// Filter CI16 samples through a real-tap filter.
    pub fn execute_real_ci16(
        &mut self,
        input: &[DpCi16],
    ) -> Result<Vec<Complex<f32>>, &'static str> {
        let n = input.len();
        let mut c_out = vec![DpCf32::default(); n];
        let rc = unsafe {
            fir_execute_real_ci16(
                self.ptr,
                input.as_ptr(),
                c_out.as_mut_ptr(),
                n,
            )
        };
        if rc != 0 {
            return Err("fir_execute_real_ci16 failed");
        }
        Ok(c_out.into_iter().map(Complex::from).collect())
    }

    /// Filter CI32 samples through a real-tap filter.
    pub fn execute_real_ci32(
        &mut self,
        input: &[DpCi32],
    ) -> Result<Vec<Complex<f32>>, &'static str> {
        let n = input.len();
        let mut c_out = vec![DpCf32::default(); n];
        let rc = unsafe {
            fir_execute_real_ci32(
                self.ptr,
                input.as_ptr(),
                c_out.as_mut_ptr(),
                n,
            )
        };
        if rc != 0 {
            return Err("fir_execute_real_ci32 failed");
        }
        Ok(c_out.into_iter().map(Complex::from).collect())
    }
}

impl Drop for Fir {
    fn drop(&mut self) {
        unsafe { fir_destroy(self.ptr) }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn identity_real_taps_cf32() {
        let mut fir = Fir::new_real(&[1.0_f32]);
        let input: Vec<Complex<f32>> =
            (0..8).map(|i| Complex::new(i as f32, 0.0)).collect();
        let output = fir.execute_real_cf32(&input).unwrap();
        assert_eq!(output.len(), 8);
        for (i, (got, expected)) in
            output.iter().zip(input.iter()).enumerate()
        {
            assert!(
                (got.re - expected.re).abs() < 1e-6,
                "sample {i}: re mismatch"
            );
        }
    }

    #[test]
    fn identity_complex_taps_cf32() {
        let mut fir = Fir::new(&[Complex::new(1.0_f32, 0.0)]);
        let input: Vec<Complex<f32>> =
            (0..8).map(|i| Complex::new(i as f32, -(i as f32))).collect();
        let output = fir.execute_cf32(&input).unwrap();
        assert_eq!(output.len(), 8);
        for (i, (got, expected)) in
            output.iter().zip(input.iter()).enumerate()
        {
            assert!(
                (got.re - expected.re).abs() < 1e-6,
                "sample {i}: re mismatch"
            );
            assert!(
                (got.im - expected.im).abs() < 1e-6,
                "sample {i}: im mismatch"
            );
        }
    }

    #[test]
    fn real_taps_flag() {
        let real = Fir::new_real(&[1.0_f32]);
        let cmpl = Fir::new(&[Complex::new(1.0_f32, 0.0)]);
        assert!(real.is_real_taps());
        assert!(!cmpl.is_real_taps());
    }

    #[test]
    fn reset_clears_delay() {
        let mut fir = Fir::new_real(&[0.5_f32, 0.5]);
        let ones: Vec<Complex<f32>> =
            vec![Complex::new(1.0, 0.0); 4];
        let _ = fir.execute_real_cf32(&ones).unwrap();
        fir.reset();
        let zeros: Vec<Complex<f32>> = vec![Complex::default(); 1];
        let out = fir.execute_real_cf32(&zeros).unwrap();
        assert!(out[0].re.abs() < 1e-6, "delay should be cleared");
    }

    #[test]
    fn ci16_input_real_taps() {
        let mut fir = Fir::new_real(&[1.0_f32]);
        let input: Vec<DpCi16> =
            (0..8_i16).map(|i| DpCi16 { i, q: -i }).collect();
        let out = fir.execute_real_ci16(&input).unwrap();
        assert_eq!(out.len(), 8);
        assert!((out[3].re - 3.0).abs() < 1e-4);
        assert!((out[3].im - (-3.0)).abs() < 1e-4);
    }
}
