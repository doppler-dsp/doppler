/// FFT global-plan interface (`dp/fft.h`).
use num_complex::Complex64;
use std::ffi::CString;
use std::os::raw::{c_char, c_int};

extern "C" {
    pub fn dp_fft_global_setup(
        shape: *const usize,
        ndim: usize,
        sign: c_int,
        nthreads: c_int,
        planner: *const c_char,
        wisdom_path: *const c_char,
    );
    pub fn dp_fft1d_execute(
        input: *const Complex64,
        output: *mut Complex64,
    );
    pub fn dp_fft1d_execute_inplace(data: *mut Complex64);
    pub fn dp_fft2d_execute(
        input: *const Complex64,
        output: *mut Complex64,
    );
    pub fn dp_fft2d_execute_inplace(data: *mut Complex64);
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
            Direction::Forward => 1,
            Direction::Inverse => -1,
        }
    }
}

/// Set up a 1-D global FFT plan for buffers of length `n`.
///
/// Uses `"estimate"` planner — input data is not overwritten during
/// planning.  Must be called before [`execute_1d`] or
/// [`execute_1d_inplace`].
pub fn setup_1d(n: usize, dir: Direction) {
    let shape = [n];
    let planner = CString::new("estimate").unwrap();
    let wisdom = CString::new("").unwrap();
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
    let wisdom = CString::new("").unwrap();
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

/// Execute a 1-D FFT in-place.
pub fn execute_1d_inplace(data: &mut [Complex64]) {
    unsafe { dp_fft1d_execute_inplace(data.as_mut_ptr()) }
}

/// Execute a 2-D out-of-place FFT (row-major, `ny * nx` elements).
///
/// # Panics
/// Panics if `input` and `output` have different lengths.
pub fn execute_2d(input: &[Complex64], output: &mut [Complex64]) {
    assert_eq!(input.len(), output.len());
    unsafe {
        dp_fft2d_execute(input.as_ptr(), output.as_mut_ptr());
    }
}

/// Execute a 2-D FFT in-place.
pub fn execute_2d_inplace(data: &mut [Complex64]) {
    unsafe { dp_fft2d_execute_inplace(data.as_mut_ptr()) }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::f64::consts::PI;

    // NOTE: run with `--test-threads=1` — the global FFTW plan is
    // process-wide mutable state and is not thread-safe.

    #[test]
    fn fft1d_impulse() {
        let n = 16;
        let mut input = vec![Complex64::default(); n];
        let mut output = vec![Complex64::default(); n];
        input[0] = Complex64::new(1.0, 0.0);

        setup_1d(n, Direction::Forward);
        execute_1d(&input, &mut output);

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

        let mut freq = vec![Complex64::default(); n];
        setup_1d(n, Direction::Forward);
        execute_1d(&input, &mut freq);

        let mut recovered = vec![Complex64::default(); n];
        setup_1d(n, Direction::Inverse);
        execute_1d(&freq, &mut recovered);

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

        setup_1d(n, Direction::Forward);
        execute_1d(&input, &mut output);

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
    fn fft2d_impulse() {
        let ny = 8;
        let nx = 8;
        let n = ny * nx;
        let mut input = vec![Complex64::default(); n];
        let mut output = vec![Complex64::default(); n];
        input[0] = Complex64::new(1.0, 0.0);

        setup_2d(ny, nx, Direction::Forward);
        execute_2d(&input, &mut output);

        for (k, &val) in output.iter().enumerate() {
            assert!(
                (val.re - 1.0).abs() < 1e-10,
                "2D bin {k}: re = {}",
                val.re
            );
            assert!(val.im.abs() < 1e-10, "2D bin {k}: im = {}", val.im);
        }
    }

    #[test]
    fn fft2d_round_trip() {
        let ny = 16;
        let nx = 16;
        let n = ny * nx;
        let input: Vec<Complex64> = (0..n)
            .map(|i| Complex64::new(i as f64 * 0.01, -(i as f64) * 0.005))
            .collect();
        let original = input.clone();

        let mut freq = vec![Complex64::default(); n];
        setup_2d(ny, nx, Direction::Forward);
        execute_2d(&input, &mut freq);

        let mut recovered = vec![Complex64::default(); n];
        setup_2d(ny, nx, Direction::Inverse);
        execute_2d(&freq, &mut recovered);

        let norm = 1.0 / n as f64;
        for v in &mut recovered {
            *v *= norm;
        }

        for (i, (orig, rec)) in
            original.iter().zip(recovered.iter()).enumerate()
        {
            assert!(
                (orig - rec).norm() < 1e-10,
                "2D sample {i}: mismatch ({orig} vs {rec})"
            );
        }
    }
}
