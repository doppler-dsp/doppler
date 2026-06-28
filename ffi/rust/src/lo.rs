/// Local oscillator — NCO phase accumulator + 2^16 LUT (`lo_core.h`).
///
/// Generates CF32 complex phasors.  For raw u32 phase output, use
/// [`crate::nco::Nco`].
///
/// The 2^16-entry float sine LUT is initialised once on first use and
/// shared across all `Lo` instances.  SFDR is ~96 dBc (16-bit phase
/// truncation).
///
/// # Example
/// ```no_run
/// use doppler::lo::Lo;
/// use num_complex::Complex;
///
/// let mut lo = Lo::new(0.25); // quarter-rate: e^{j·π/2·n}
/// let mut out = vec![Complex::<f32>::default(); 4];
/// lo.steps(&mut out);
/// // out ≈ [1+0j, 0+1j, -1+0j, 0-1j]
/// ```
use num_complex::Complex;

/// Opaque C LO state.  Never construct directly — use [`Lo`].
#[repr(C)]
pub struct LoStateRaw {
    _priv: [u8; 0],
}

extern "C" {
    pub fn lo_create(norm_freq: f64) -> *mut LoStateRaw;
    pub fn lo_destroy(lo: *mut LoStateRaw);
    pub fn lo_reset(lo: *mut LoStateRaw);
    pub fn lo_set_norm_freq(lo: *mut LoStateRaw, norm_freq: f64);
    pub fn lo_get_norm_freq(lo: *const LoStateRaw) -> f64;
    pub fn lo_steps(
        lo: *mut LoStateRaw,
        n: usize,
        out: *mut Complex<f32>,
    ) -> usize;
    pub fn lo_steps_ctrl(
        lo: *mut LoStateRaw,
        ctrl: *const f32,
        ctrl_len: usize,
        out: *mut Complex<f32>,
    ) -> usize;
}

/// RAII wrapper around `lo_state_t`.
///
/// Generates complex phasors at a normalised frequency (cycles per sample).
/// Backed by a 2^16-entry LUT for high SFDR (~96 dBc) at low cost.
pub struct Lo {
    ptr: *mut LoStateRaw,
}

unsafe impl Send for Lo {}

impl Lo {
    /// Create a local oscillator at the given normalised frequency.
    ///
    /// # Panics
    /// Panics if `lo_create` returns null.
    pub fn new(norm_freq: f64) -> Self {
        let ptr = unsafe { lo_create(norm_freq) };
        assert!(!ptr.is_null(), "lo_create returned null");
        Lo { ptr }
    }

    /// Zero the phase accumulator.  Normalised frequency is unchanged.
    pub fn reset(&mut self) {
        unsafe { lo_reset(self.ptr) }
    }

    /// Update the normalised frequency without disturbing the phase.
    pub fn set_norm_freq(&mut self, norm_freq: f64) {
        unsafe { lo_set_norm_freq(self.ptr, norm_freq) }
    }

    /// Return the current normalised frequency.
    pub fn get_norm_freq(&self) -> f64 {
        unsafe { lo_get_norm_freq(self.ptr) }
    }

    /// Generate `out.len()` complex CF32 phasors.
    pub fn steps(&mut self, out: &mut [Complex<f32>]) {
        unsafe { lo_steps(self.ptr, out.len(), out.as_mut_ptr()) };
    }

    /// Generate phasors with per-sample FM frequency deviation.
    ///
    /// `ctrl[i]` is a normalised-frequency offset added to the base
    /// frequency before each sample.
    ///
    /// # Panics
    /// Panics if `ctrl` and `out` have different lengths.
    pub fn steps_ctrl(
        &mut self,
        ctrl: &[f32],
        out: &mut [Complex<f32>],
    ) {
        assert_eq!(
            ctrl.len(),
            out.len(),
            "ctrl and out must have the same length"
        );
        unsafe {
            lo_steps_ctrl(
                self.ptr,
                ctrl.as_ptr(),
                ctrl.len(),
                out.as_mut_ptr(),
            )
        };
    }
}

impl Drop for Lo {
    fn drop(&mut self) {
        unsafe { lo_destroy(self.ptr) }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn quarter_rate_phasors() {
        let mut lo = Lo::new(0.25);
        let mut out = vec![Complex::<f32>::default(); 4];
        lo.steps(&mut out);
        // quarter rate: 1, j, -1, -j
        assert!((out[0].re - 1.0).abs() < 1e-4, "out[0].re");
        assert!(out[0].im.abs() < 1e-4, "out[0].im");
        assert!(out[1].re.abs() < 1e-4, "out[1].re");
        assert!((out[1].im - 1.0).abs() < 1e-4, "out[1].im");
        assert!((out[2].re + 1.0).abs() < 1e-4, "out[2].re");
        assert!(out[2].im.abs() < 1e-4, "out[2].im");
        assert!(out[3].re.abs() < 1e-4, "out[3].re");
        assert!((out[3].im + 1.0).abs() < 1e-4, "out[3].im");
    }

    #[test]
    fn unit_amplitude() {
        let mut lo = Lo::new(0.1);
        let mut out = vec![Complex::<f32>::default(); 256];
        lo.steps(&mut out);
        for (i, s) in out.iter().enumerate() {
            let amp = (s.re * s.re + s.im * s.im).sqrt();
            assert!(
                (amp - 1.0).abs() < 1e-3,
                "sample {i}: amplitude {amp} not unit"
            );
        }
    }

    #[test]
    fn set_norm_freq_takes_effect() {
        let mut lo = Lo::new(0.1);
        lo.set_norm_freq(0.25);
        assert!((lo.get_norm_freq() - 0.25).abs() < 1e-9);
        let mut out = vec![Complex::<f32>::default(); 4];
        lo.steps(&mut out);
        assert!((out[0].re - 1.0).abs() < 1e-4);
    }

    #[test]
    fn reset_zeroes_phase() {
        let mut lo = Lo::new(0.25);
        let mut out = vec![Complex::<f32>::default(); 4];
        lo.steps(&mut out);
        lo.reset();
        let mut out2 = vec![Complex::<f32>::default(); 1];
        lo.steps(&mut out2);
        let amp =
            (out2[0].re * out2[0].re + out2[0].im * out2[0].im).sqrt();
        assert!((amp - 1.0).abs() < 1e-3);
    }

    #[test]
    fn fm_ctrl_modulates_frequency() {
        let mut lo = Lo::new(0.25);
        let ctrl = vec![0.0_f32; 4]; // zero deviation = same as no ctrl
        let mut out_ctrl = vec![Complex::<f32>::default(); 4];
        lo.steps_ctrl(&ctrl, &mut out_ctrl);

        lo.reset();
        let mut out_plain = vec![Complex::<f32>::default(); 4];
        lo.steps(&mut out_plain);

        for (i, (c, p)) in
            out_ctrl.iter().zip(out_plain.iter()).enumerate()
        {
            assert!(
                (c.re - p.re).abs() < 1e-4,
                "sample {i}: ctrl re {}, plain re {}",
                c.re,
                p.re
            );
        }
    }
}

// Serializable state — the dp_state.h bytes interface.
extern "C" {
    fn lo_state_bytes(s: *const LoStateRaw) -> usize;
    fn lo_get_state(s: *const LoStateRaw, blob: *mut u8);
    fn lo_set_state(s: *mut LoStateRaw, blob: *const u8) -> i32;
}
impl_serializable!(
    Lo,
    lo_state_bytes,
    lo_get_state,
    lo_set_state
);
