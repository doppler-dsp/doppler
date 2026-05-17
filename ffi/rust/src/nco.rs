/// Pure phase-accumulator NCO (`native/inc/nco/nco_core.h`).
///
/// Produces raw `u32` accumulator values or per-sample carry flags.
/// For complex (CF32) phasors, use [`crate::lo::Lo`].
///
/// # Example
/// ```no_run
/// use doppler::nco::Nco;
///
/// let mut nco = Nco::new(0.25); // quarter-rate: wraps every 4 samples
/// let mut out = vec![0u32; 8];
/// nco.execute_u32(&mut out);
/// // out[3] wraps back (≈ 0xC000_0000 → overflow every 4th sample)
/// ```
use std::os::raw::c_uint;

/// Opaque C NCO state.  Never construct directly — use [`Nco`].
#[repr(C)]
pub struct NcoStateRaw {
    _priv: [u8; 0],
}

extern "C" {
    pub fn nco_create(norm_freq: f32, nmax: c_uint) -> *mut NcoStateRaw;
    pub fn nco_destroy(nco: *mut NcoStateRaw);
    pub fn nco_reset(nco: *mut NcoStateRaw);
    pub fn nco_set_freq(nco: *mut NcoStateRaw, norm_freq: f32);
    pub fn nco_get_freq(nco: *const NcoStateRaw) -> f32;
    pub fn nco_execute_u32(
        nco: *mut NcoStateRaw,
        out: *mut u32,
        n: usize,
    );
    pub fn nco_execute_u32_scaled(
        nco: *mut NcoStateRaw,
        out: *mut u32,
        n: usize,
    );
    pub fn nco_execute_u32_ovf(
        nco: *mut NcoStateRaw,
        out: *mut u32,
        carry: *mut u8,
        n: usize,
    );
}

/// RAII wrapper around `nco_state_t`.
///
/// Generates raw 32-bit phase accumulator values.  For complex CF32 phasors,
/// use [`crate::lo::Lo`] instead.
pub struct Nco {
    ptr: *mut NcoStateRaw,
}

unsafe impl Send for Nco {}

impl Nco {
    /// Create an NCO at the given normalised frequency.
    ///
    /// `nmax` sets the wrap target for [`execute_u32_scaled`](Nco::execute_u32_scaled).
    /// Pass `0` to always return the raw accumulator.
    ///
    /// # Panics
    /// Panics if `nco_create` returns null.
    pub fn new(norm_freq: f32) -> Self {
        let ptr = unsafe { nco_create(norm_freq, 0) };
        assert!(!ptr.is_null(), "nco_create returned null");
        Nco { ptr }
    }

    /// Create an NCO with a scaled output range `[0, nmax)`.
    ///
    /// # Panics
    /// Panics if `nco_create` returns null.
    pub fn new_scaled(norm_freq: f32, nmax: u32) -> Self {
        let ptr = unsafe { nco_create(norm_freq, nmax) };
        assert!(!ptr.is_null(), "nco_create returned null");
        Nco { ptr }
    }

    /// Zero the phase accumulator.
    pub fn reset(&mut self) {
        unsafe { nco_reset(self.ptr) }
    }

    /// Update the normalised frequency without disturbing the phase.
    pub fn set_freq(&mut self, norm_freq: f32) {
        unsafe { nco_set_freq(self.ptr, norm_freq) }
    }

    /// Return the current normalised frequency.
    pub fn get_freq(&self) -> f32 {
        unsafe { nco_get_freq(self.ptr) }
    }

    /// Write `n` raw 32-bit accumulator values into `out`.
    pub fn execute_u32(&mut self, out: &mut [u32]) {
        unsafe { nco_execute_u32(self.ptr, out.as_mut_ptr(), out.len()) }
    }

    /// Write `n` accumulator values scaled to `[0, nmax)` into `out`.
    pub fn execute_u32_scaled(&mut self, out: &mut [u32]) {
        unsafe {
            nco_execute_u32_scaled(self.ptr, out.as_mut_ptr(), out.len())
        }
    }

    /// Write raw accumulator values and per-sample carry flags.
    ///
    /// `carry[i]` is `1` when the accumulator wrapped on sample `i`
    /// (one full cycle elapsed), `0` otherwise.
    ///
    /// # Panics
    /// Panics if `out` and `carry` have different lengths.
    pub fn execute_u32_ovf(&mut self, out: &mut [u32], carry: &mut [u8]) {
        assert_eq!(
            out.len(),
            carry.len(),
            "out and carry must have the same length"
        );
        unsafe {
            nco_execute_u32_ovf(
                self.ptr,
                out.as_mut_ptr(),
                carry.as_mut_ptr(),
                out.len(),
            )
        }
    }
}

impl Drop for Nco {
    fn drop(&mut self) {
        unsafe { nco_destroy(self.ptr) }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn quarter_rate_period() {
        let mut nco = Nco::new(0.25);
        let n = 16;
        let mut phase = vec![0u32; n];
        nco.execute_u32(&mut phase);
        // phase increments by ~2^30 each sample; after 4 samples it wraps
        let inc = phase[1].wrapping_sub(phase[0]);
        assert_eq!(inc, phase[2].wrapping_sub(phase[1]));
        let expected_inc = (0.25_f64 * (1u64 << 32) as f64) as u32;
        assert!(
            (inc as i64 - expected_inc as i64).abs() < 2,
            "phase_inc {inc} != expected {expected_inc}"
        );
    }

    #[test]
    fn carry_fires_at_wrap() {
        let mut nco = Nco::new(0.25); // wraps every 4 samples
        let n = 12;
        let mut phase = vec![0u32; n];
        let mut carry = vec![0u8; n];
        nco.execute_u32_ovf(&mut phase, &mut carry);
        // carry should fire at samples 3, 7, 11 (0-based)
        for (i, &c) in carry.iter().enumerate() {
            let expected = if (i + 1) % 4 == 0 { 1 } else { 0 };
            assert_eq!(c, expected, "carry[{i}] = {c}, expected {expected}");
        }
    }

    #[test]
    fn set_freq_takes_effect() {
        let mut nco = Nco::new(0.1);
        assert!((nco.get_freq() - 0.1).abs() < 1e-6);
        nco.set_freq(0.25);
        assert!((nco.get_freq() - 0.25).abs() < 1e-6);
    }

    #[test]
    fn reset_is_deterministic() {
        // Two fresh NCOs at the same frequency must produce identical
        // output — which proves reset() fully restores initial state.
        let mut nco1 = Nco::new(0.25);
        let mut nco2 = Nco::new(0.25);

        let mut out1 = vec![0u32; 8];
        let mut out2 = vec![0u32; 8];
        nco1.execute_u32(&mut out1);
        nco2.execute_u32(&mut out2);
        assert_eq!(out1, out2, "fresh NCOs must agree");

        // Advance nco1 further, then reset it back.
        let mut extra = vec![0u32; 4];
        nco1.execute_u32(&mut extra);
        nco1.reset();

        let mut out1r = vec![0u32; 8];
        let mut out2r = vec![0u32; 8];
        nco1.execute_u32(&mut out1r);
        nco2.reset();
        nco2.execute_u32(&mut out2r);
        assert_eq!(out1r, out2r, "reset NCOs must agree");
    }
}
