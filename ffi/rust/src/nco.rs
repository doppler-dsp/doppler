/// Numerically-controlled oscillator (`dp/nco.h`).
use crate::types::DpCf32;
use num_complex::Complex;

/// Opaque C NCO handle.  Never construct directly — use [`Nco`].
#[repr(C)]
pub struct DpNcoRaw {
    _priv: [u8; 0],
}

extern "C" {
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
}

/// RAII wrapper around `dp_nco_t`.
///
/// Generates complex or raw-phase samples at a normalised frequency `f`
/// (cycles per sample, range `[0, 1)`).
///
/// # Example
/// ```no_run
/// use doppler::nco::Nco;
///
/// // 0.1 cycles/sample → period of 10 samples
/// let mut nco = Nco::new(0.1);
/// let samples = nco.execute_cf32(64);
/// assert_eq!(samples.len(), 64);
/// ```
pub struct Nco {
    ptr: *mut DpNcoRaw,
}

// dp_nco_t owns no thread-local state after creation; all mutations go
// through &mut self so there is no aliasing.
unsafe impl Send for Nco {}

impl Nco {
    /// Create an NCO at `norm_freq` cycles per sample.
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
    /// Each entry in `ctrl` is a frequency offset (cycles per sample)
    /// added to the base frequency for that sample.
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
        let mut buf = vec![0u32; n];
        unsafe { dp_nco_execute_u32(self.ptr, buf.as_mut_ptr(), n) }
        buf
    }

    /// Generate `n` phase values and a per-sample overflow (cycle) flag.
    ///
    /// Returns `(phases, overflows)` where `overflows[i] == 1` whenever
    /// the accumulator wrapped on sample `i`.
    pub fn execute_u32_ovf(&mut self, n: usize) -> (Vec<u32>, Vec<u8>) {
        let mut phase = vec![0u32; n];
        let mut ovf = vec![0u8; n];
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
    ///
    /// # Panics
    /// Panics if `ctrl.len() != n`.
    pub fn execute_u32_ctrl(
        &mut self,
        ctrl: &[f32],
        n: usize,
    ) -> Vec<u32> {
        assert_eq!(ctrl.len(), n, "ctrl length must equal n");
        let mut buf = vec![0u32; n];
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

    /// Generate `n` phase values with frequency control and overflow
    /// flags.
    ///
    /// # Panics
    /// Panics if `ctrl.len() != n`.
    pub fn execute_u32_ovf_ctrl(
        &mut self,
        ctrl: &[f32],
        n: usize,
    ) -> (Vec<u32>, Vec<u8>) {
        assert_eq!(ctrl.len(), n, "ctrl length must equal n");
        let mut phase = vec![0u32; n];
        let mut ovf = vec![0u8; n];
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

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn nco_cf32_output_length() {
        let mut osc = Nco::new(0.1);
        assert_eq!(osc.execute_cf32(64).len(), 64);
    }

    #[test]
    fn nco_cf32_unit_magnitude() {
        let mut osc = Nco::new(0.1);
        let out = osc.execute_cf32(256);
        for (i, s) in out.iter().enumerate() {
            let mag = (s.re * s.re + s.im * s.im).sqrt();
            assert!(
                (mag - 1.0).abs() < 1e-5,
                "sample {i}: magnitude = {mag}"
            );
        }
    }

    #[test]
    fn nco_cf32_phase_advances() {
        use std::f32::consts::PI;
        let mut osc = Nco::new(0.25);
        let out = osc.execute_cf32(4);
        let angle1 = out[1].im.atan2(out[1].re);
        assert!(
            (angle1.abs() - PI / 2.0).abs() < 0.01,
            "expected ~π/2, got {angle1}"
        );
    }

    #[test]
    fn nco_reset_restores_phase() {
        let mut osc = Nco::new(0.1);
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
        let mut osc = Nco::new(0.1);
        let a = osc.execute_cf32(32);
        osc.reset();
        osc.set_freq(0.3);
        let b = osc.execute_cf32(32);
        let diff: f32 = a
            .iter()
            .zip(b.iter())
            .map(|(x, y)| (x.re - y.re).abs() + (x.im - y.im).abs())
            .sum();
        assert!(diff > 1.0, "set_freq had no effect (diff={diff})");
    }

    #[test]
    fn nco_u32_output_length() {
        let mut osc = Nco::new(0.1);
        assert_eq!(osc.execute_u32(128).len(), 128);
    }

    #[test]
    fn nco_u32_ovf_wraps_at_full_cycle() {
        let mut osc = Nco::new(0.5);
        let (_, ovf) = osc.execute_u32_ovf(64);
        let wrap_count = ovf.iter().filter(|&&x| x != 0).count();
        assert!(wrap_count >= 20, "expected ~32 wraps, got {wrap_count}");
    }
}
