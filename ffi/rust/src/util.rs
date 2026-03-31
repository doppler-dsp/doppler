/// SIMD-accelerated complex arithmetic (`dp/util.h`).
use num_complex::Complex64;

extern "C" {
    pub fn dp_c16_mul(a: Complex64, b: Complex64) -> Complex64;
}

/// SIMD-accelerated complex multiplication (`a * b`).
///
/// Uses SSE2 on x86-64 and a scalar C99 fallback on other architectures.
/// Equivalent to `a * b` for [`Complex64`] but guaranteed to use the
/// hardware-accelerated path when available.
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

#[cfg(test)]
mod tests {
    use super::*;
    use num_complex::Complex64;

    #[test]
    fn c16_mul_basic() {
        // (1+2i) * (3+4i) = -5 + 10i
        let a = Complex64::new(1.0, 2.0);
        let b = Complex64::new(3.0, 4.0);
        let c = c16_mul(a, b);
        assert!((c.re - (-5.0)).abs() < 1e-12);
        assert!((c.im - 10.0).abs() < 1e-12);
    }

    #[test]
    fn c16_mul_identity() {
        let x = Complex64::new(3.14, -2.71);
        let one = Complex64::new(1.0, 0.0);
        let r = c16_mul(x, one);
        assert!((r.re - x.re).abs() < 1e-12);
        assert!((r.im - x.im).abs() < 1e-12);
    }

    #[test]
    fn c16_mul_zero() {
        let x = Complex64::new(42.0, -99.0);
        let r = c16_mul(x, Complex64::new(0.0, 0.0));
        assert!(r.re.abs() < 1e-12);
        assert!(r.im.abs() < 1e-12);
    }

    #[test]
    fn c16_mul_conjugate() {
        // x * conj(x) = |x|^2 (real, non-negative)
        let x = Complex64::new(3.0, 4.0);
        let xc = Complex64::new(3.0, -4.0);
        let r = c16_mul(x, xc);
        assert!((r.re - 25.0).abs() < 1e-12);
        assert!(r.im.abs() < 1e-12);
    }

    #[test]
    fn c16_mul_matches_num_complex() {
        let a = Complex64::new(-1.5, 2.7);
        let b = Complex64::new(0.3, -4.1);
        let expected = a * b;
        let got = c16_mul(a, b);
        assert!((got.re - expected.re).abs() < 1e-12);
        assert!((got.im - expected.im).abs() < 1e-12);
    }
}
