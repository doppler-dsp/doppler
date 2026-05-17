/// Per-instance 1-D and 2-D FFT (`native/inc/fft/fft_core.h`).
///
/// Each [`Fft`] holds its own pocketfft plan — create once, reuse across
/// calls.  CF32 is roughly 2× faster than CF64 for the same transform
/// length.
///
/// # Example
/// ```no_run
/// use doppler::fft::{Fft, Direction};
/// use num_complex::Complex64;
///
/// let n = 1024_usize;
/// let mut input  = vec![Complex64::default(); n];
/// let mut output = vec![Complex64::default(); n];
///
/// input[1] = Complex64::new(1.0, 0.0);   // impulse at bin 1
///
/// let fft = Fft::new(n, Direction::Forward);
/// fft.execute_cf64(&input, &mut output);
/// assert!(output[1].norm() > 0.9);
/// ```
use num_complex::{Complex, Complex64};
use std::os::raw::c_int;

/// Opaque C FFT state.  Never construct directly — use [`Fft`].
#[repr(C)]
pub struct FftStateRaw {
    _priv: [u8; 0],
}

extern "C" {
    pub fn fft_create(
        n: usize,
        sign: c_int,
        nthreads: c_int,
    ) -> *mut FftStateRaw;
    pub fn fft_destroy(state: *mut FftStateRaw);
    pub fn fft_execute_cf64(
        state: *mut FftStateRaw,
        input: *const Complex64,
        n_in: usize,
        output: *mut Complex64,
    ) -> usize;
    pub fn fft_execute_cf32(
        state: *mut FftStateRaw,
        input: *const Complex<f32>,
        n_in: usize,
        output: *mut Complex<f32>,
    ) -> usize;
    pub fn fft_execute_inplace_cf64(
        state: *mut FftStateRaw,
        input: *const Complex64,
        n_in: usize,
        output: *mut Complex64,
    ) -> usize;
    pub fn fft_execute_inplace_cf32(
        state: *mut FftStateRaw,
        input: *const Complex<f32>,
        n_in: usize,
        output: *mut Complex<f32>,
    ) -> usize;
}

/// FFT direction.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Direction {
    /// Forward DFT (`e^{-2πi·k·n/N}`).
    Forward,
    /// Inverse DFT (`e^{+2πi·k·n/N}`).
    Inverse,
}

impl Direction {
    fn sign(self) -> c_int {
        match self {
            Direction::Forward => -1,
            Direction::Inverse => 1,
        }
    }
}

/// RAII wrapper around `fft_state_t`.
///
/// Create with [`Fft::new`], then call [`execute_cf64`](Fft::execute_cf64)
/// or [`execute_cf32`](Fft::execute_cf32) as many times as needed.
/// The state is freed on drop.
pub struct Fft {
    state: *mut FftStateRaw,
    n: usize,
}

unsafe impl Send for Fft {}

impl Fft {
    /// Create an FFT instance for transforms of length `n`.
    ///
    /// # Panics
    /// Panics if `fft_create` returns null (OOM).
    pub fn new(n: usize, dir: Direction) -> Self {
        let state = unsafe { fft_create(n, dir.sign(), 1) };
        assert!(!state.is_null(), "fft_create returned null");
        Fft { state, n }
    }

    /// Transform length.
    pub fn len(&self) -> usize {
        self.n
    }

    /// Out-of-place CF64 FFT.
    ///
    /// # Panics
    /// Panics if `input` or `output` length != `self.len()`.
    pub fn execute_cf64(
        &self,
        input: &[Complex64],
        output: &mut [Complex64],
    ) {
        assert_eq!(input.len(), self.n, "input length mismatch");
        assert_eq!(output.len(), self.n, "output length mismatch");
        unsafe {
            fft_execute_cf64(
                self.state,
                input.as_ptr(),
                self.n,
                output.as_mut_ptr(),
            );
        }
    }

    /// Out-of-place CF32 FFT (~2× faster than CF64).
    ///
    /// # Panics
    /// Panics if `input` or `output` length != `self.len()`.
    pub fn execute_cf32(
        &self,
        input: &[Complex<f32>],
        output: &mut [Complex<f32>],
    ) {
        assert_eq!(input.len(), self.n, "input length mismatch");
        assert_eq!(output.len(), self.n, "output length mismatch");
        unsafe {
            fft_execute_cf32(
                self.state,
                input.as_ptr(),
                self.n,
                output.as_mut_ptr(),
            );
        }
    }
}

impl Drop for Fft {
    fn drop(&mut self) {
        unsafe { fft_destroy(self.state) }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::f64::consts::PI;

    #[test]
    fn fft1d_impulse() {
        let n = 16;
        let input: Vec<Complex64> = (0..n)
            .map(|i| {
                if i == 0 {
                    Complex64::new(1.0, 0.0)
                } else {
                    Complex64::default()
                }
            })
            .collect();
        let mut output = vec![Complex64::default(); n];

        let fft = Fft::new(n, Direction::Forward);
        fft.execute_cf64(&input, &mut output);

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
        let n = 64;
        let input: Vec<Complex64> = (0..n)
            .map(|i| {
                let t = 2.0 * PI * i as f64 / n as f64;
                Complex64::new(t.cos(), t.sin() * 0.5)
            })
            .collect();
        let original = input.clone();

        let fwd = Fft::new(n, Direction::Forward);
        let inv = Fft::new(n, Direction::Inverse);

        let mut freq = vec![Complex64::default(); n];
        fwd.execute_cf64(&input, &mut freq);

        let mut recovered = vec![Complex64::default(); n];
        inv.execute_cf64(&freq, &mut recovered);

        let norm = 1.0 / n as f64;
        for v in &mut recovered {
            *v *= norm;
        }

        for (i, (orig, rec)) in
            original.iter().zip(recovered.iter()).enumerate()
        {
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
        let n = 128;
        let k = 7;
        let input: Vec<Complex64> = (0..n)
            .map(|i| {
                let t = 2.0 * PI * k as f64 * i as f64 / n as f64;
                Complex64::new(t.cos(), 0.0)
            })
            .collect();
        let mut output = vec![Complex64::default(); n];

        let fft = Fft::new(n, Direction::Forward);
        fft.execute_cf64(&input, &mut output);

        let half_n = n as f64 / 2.0;
        assert!(output[k].norm() > half_n - 1.0);
        assert!(output[n - k].norm() > half_n - 1.0);
        for (i, val) in output.iter().enumerate() {
            if i != k && i != n - k {
                assert!(
                    val.norm() < 1e-8,
                    "bin {i} should be near zero"
                );
            }
        }
    }

    #[test]
    fn fft1d_cf32_round_trip() {
        let n = 64_usize;
        let input: Vec<Complex<f32>> = (0..n)
            .map(|i| {
                let t =
                    2.0 * std::f32::consts::PI * i as f32 / n as f32;
                Complex::new(t.cos(), t.sin())
            })
            .collect();
        let original = input.clone();

        let fwd = Fft::new(n, Direction::Forward);
        let inv = Fft::new(n, Direction::Inverse);

        let mut freq = vec![Complex::<f32>::default(); n];
        fwd.execute_cf32(&input, &mut freq);

        let mut recovered = vec![Complex::<f32>::default(); n];
        inv.execute_cf32(&freq, &mut recovered);

        let norm = 1.0_f32 / n as f32;
        for v in &mut recovered {
            *v *= norm;
        }

        for (i, (orig, rec)) in
            original.iter().zip(recovered.iter()).enumerate()
        {
            assert!(
                (orig.re - rec.re).abs() < 1e-5,
                "sample {i}: re mismatch"
            );
        }
    }
}
