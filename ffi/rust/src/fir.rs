/// Finite impulse response filter (`dp/fir.h`).
use crate::types::{DpCf32, DpCi16, DpCi32, DpCi8};
use num_complex::Complex;
use std::os::raw::c_int;

/// Opaque C FIR handle.  Never construct directly — use [`Fir`].
#[repr(C)]
pub struct DpFirRaw {
    _priv: [u8; 0],
}

extern "C" {
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

/// RAII wrapper around `dp_fir_t`.
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
        let ptr =
            unsafe { dp_fir_create(c_taps.as_ptr(), c_taps.len()) };
        assert!(!ptr.is_null(), "dp_fir_create returned null");
        Fir { ptr, real_taps: false }
    }

    /// Create a FIR filter with real (scalar) taps.
    ///
    /// # Panics
    /// Panics if `dp_fir_create_real` returns null.
    pub fn new_real(taps: &[f32]) -> Self {
        let ptr =
            unsafe { dp_fir_create_real(taps.as_ptr(), taps.len()) };
        assert!(!ptr.is_null(), "dp_fir_create_real returned null");
        Fir { ptr, real_taps: true }
    }

    /// Reset the filter delay line to zero.
    pub fn reset(&mut self) {
        unsafe { dp_fir_reset(self.ptr) }
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
        let mut c_out = vec![DpCf32::default(); n];
        let rc = unsafe {
            dp_fir_execute_ci8(
                self.ptr,
                input.as_ptr(),
                c_out.as_mut_ptr(),
                n,
            )
        };
        if rc != 0 {
            return Err("dp_fir_execute_ci8 failed");
        }
        Ok(c_out.into_iter().map(Complex::from).collect())
    }

    /// Filter a block of `DpCi16` samples (complex taps, cf32 output).
    pub fn execute_ci16(
        &mut self,
        input: &[DpCi16],
    ) -> Result<Vec<Complex<f32>>, &'static str> {
        let n = input.len();
        let mut c_out = vec![DpCf32::default(); n];
        let rc = unsafe {
            dp_fir_execute_ci16(
                self.ptr,
                input.as_ptr(),
                c_out.as_mut_ptr(),
                n,
            )
        };
        if rc != 0 {
            return Err("dp_fir_execute_ci16 failed");
        }
        Ok(c_out.into_iter().map(Complex::from).collect())
    }

    /// Filter a block of `DpCi32` samples (complex taps, cf32 output).
    pub fn execute_ci32(
        &mut self,
        input: &[DpCi32],
    ) -> Result<Vec<Complex<f32>>, &'static str> {
        let n = input.len();
        let mut c_out = vec![DpCf32::default(); n];
        let rc = unsafe {
            dp_fir_execute_ci32(
                self.ptr,
                input.as_ptr(),
                c_out.as_mut_ptr(),
                n,
            )
        };
        if rc != 0 {
            return Err("dp_fir_execute_ci32 failed");
        }
        Ok(c_out.into_iter().map(Complex::from).collect())
    }

    // ── Real-taps execute paths ───────────────────────────────────────────

    /// Filter a block of `Complex<f32>` samples (real taps).
    pub fn execute_real_cf32(
        &mut self,
        input: &[Complex<f32>],
    ) -> Result<Vec<Complex<f32>>, &'static str> {
        let n = input.len();
        let c_in: Vec<DpCf32> =
            input.iter().copied().map(DpCf32::from).collect();
        let mut c_out = vec![DpCf32::default(); n];
        let rc = unsafe {
            dp_fir_execute_real_cf32(
                self.ptr,
                c_in.as_ptr(),
                c_out.as_mut_ptr(),
                n,
            )
        };
        if rc != 0 {
            return Err("dp_fir_execute_real_cf32 failed");
        }
        Ok(c_out.into_iter().map(Complex::from).collect())
    }

    /// Filter a block of `DpCi8` samples (real taps, cf32 output).
    pub fn execute_real_ci8(
        &mut self,
        input: &[DpCi8],
    ) -> Result<Vec<Complex<f32>>, &'static str> {
        let n = input.len();
        let mut c_out = vec![DpCf32::default(); n];
        let rc = unsafe {
            dp_fir_execute_real_ci8(
                self.ptr,
                input.as_ptr(),
                c_out.as_mut_ptr(),
                n,
            )
        };
        if rc != 0 {
            return Err("dp_fir_execute_real_ci8 failed");
        }
        Ok(c_out.into_iter().map(Complex::from).collect())
    }

    /// Filter a block of `DpCi16` samples (real taps, cf32 output).
    pub fn execute_real_ci16(
        &mut self,
        input: &[DpCi16],
    ) -> Result<Vec<Complex<f32>>, &'static str> {
        let n = input.len();
        let mut c_out = vec![DpCf32::default(); n];
        let rc = unsafe {
            dp_fir_execute_real_ci16(
                self.ptr,
                input.as_ptr(),
                c_out.as_mut_ptr(),
                n,
            )
        };
        if rc != 0 {
            return Err("dp_fir_execute_real_ci16 failed");
        }
        Ok(c_out.into_iter().map(Complex::from).collect())
    }

    /// Filter a block of `DpCi32` samples (real taps, cf32 output).
    pub fn execute_real_ci32(
        &mut self,
        input: &[DpCi32],
    ) -> Result<Vec<Complex<f32>>, &'static str> {
        let n = input.len();
        let mut c_out = vec![DpCf32::default(); n];
        let rc = unsafe {
            dp_fir_execute_real_ci32(
                self.ptr,
                input.as_ptr(),
                c_out.as_mut_ptr(),
                n,
            )
        };
        if rc != 0 {
            return Err("dp_fir_execute_real_ci32 failed");
        }
        Ok(c_out.into_iter().map(Complex::from).collect())
    }
}

impl Drop for Fir {
    fn drop(&mut self) {
        unsafe { dp_fir_destroy(self.ptr) }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use num_complex::Complex;

    #[test]
    fn fir_real_identity() {
        let mut f = Fir::new_real(&[1.0_f32]);
        let input: Vec<Complex<f32>> =
            (0..8).map(|i| Complex::new(i as f32, -(i as f32))).collect();
        let output = f.execute_real_cf32(&input).unwrap();
        for (i, (inp, out)) in input.iter().zip(output.iter()).enumerate()
        {
            assert!(
                (inp.re - out.re).abs() < 1e-6
                    && (inp.im - out.im).abs() < 1e-6,
                "sample {i}: {inp:?} != {out:?}"
            );
        }
    }

    #[test]
    fn fir_complex_identity() {
        let taps = [Complex::new(1.0_f32, 0.0_f32)];
        let mut f = Fir::new(&taps);
        let input: Vec<Complex<f32>> = (0..8)
            .map(|i| Complex::new(i as f32, 0.5 * i as f32))
            .collect();
        let output = f.execute_cf32(&input).unwrap();
        for (i, (inp, out)) in input.iter().zip(output.iter()).enumerate()
        {
            assert!(
                (inp.re - out.re).abs() < 1e-5
                    && (inp.im - out.im).abs() < 1e-5,
                "sample {i}: {inp:?} != {out:?}"
            );
        }
    }

    #[test]
    fn fir_real_impulse_response() {
        // Taps [1, 2, 3]; impulse in → taps out
        let taps = [1.0_f32, 2.0, 3.0];
        let mut f = Fir::new_real(&taps);
        let mut input = vec![Complex::new(0.0_f32, 0.0); 8];
        input[0] = Complex::new(1.0, 0.0);
        let output = f.execute_real_cf32(&input).unwrap();
        assert!((output[0].re - 1.0).abs() < 1e-5);
        assert!((output[1].re - 2.0).abs() < 1e-5);
        assert!((output[2].re - 3.0).abs() < 1e-5);
    }

    #[test]
    fn fir_reset_clears_state() {
        let taps = [0.25_f32, 0.5, 0.25];
        let mut f = Fir::new_real(&taps);
        let signal: Vec<Complex<f32>> =
            (0..16).map(|i| Complex::new(i as f32, 0.0)).collect();
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
        use crate::types::DpCi8;
        let taps = [Complex::new(1.0_f32, 0.0_f32)];
        let mut f = Fir::new(&taps);
        let input: Vec<DpCi8> =
            (0..8_i8).map(|i| DpCi8 { i, q: -i }).collect();
        let output = f.execute_ci8(&input).unwrap();
        assert_eq!(output.len(), 8);
        for (k, (inp, out)) in input.iter().zip(output.iter()).enumerate()
        {
            assert!(
                (out.re - inp.i as f32).abs() < 1e-5,
                "ci8 sample {k}: re mismatch"
            );
        }
    }
}
