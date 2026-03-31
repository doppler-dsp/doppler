/// Scalar and complex accumulators (`dp/accumulator.h`).
use num_complex::Complex64;

/// Opaque C f32 accumulator handle.  Never construct directly — use
/// [`AccF32`].
#[repr(C)]
pub struct DpAccF32Raw {
    _priv: [u8; 0],
}

/// Opaque C cf64 accumulator handle.  Never construct directly — use
/// [`AccCf64`].
#[repr(C)]
pub struct DpAccCf64Raw {
    _priv: [u8; 0],
}

extern "C" {
    pub fn dp_acc_f32_create() -> *mut DpAccF32Raw;
    pub fn dp_acc_f32_destroy(acc: *mut DpAccF32Raw);
    pub fn dp_acc_f32_reset(acc: *mut DpAccF32Raw);
    pub fn dp_acc_f32_get(acc: *const DpAccF32Raw) -> f32;
    pub fn dp_acc_f32_dump(acc: *mut DpAccF32Raw) -> f32;
    pub fn dp_acc_f32_push(acc: *mut DpAccF32Raw, x: f32);
    pub fn dp_acc_f32_add(
        acc: *mut DpAccF32Raw,
        x: *const f32,
        n: usize,
    );
    pub fn dp_acc_f32_madd(
        acc: *mut DpAccF32Raw,
        x: *const f32,
        h: *const f32,
        n: usize,
    );
    pub fn dp_acc_f32_add2d(
        acc: *mut DpAccF32Raw,
        x: *const f32,
        rows: usize,
        cols: usize,
    );
    pub fn dp_acc_f32_madd2d(
        acc: *mut DpAccF32Raw,
        x: *const f32,
        h: *const f32,
        rows: usize,
        cols: usize,
    );

    pub fn dp_acc_cf64_create() -> *mut DpAccCf64Raw;
    pub fn dp_acc_cf64_destroy(acc: *mut DpAccCf64Raw);
    pub fn dp_acc_cf64_reset(acc: *mut DpAccCf64Raw);
    pub fn dp_acc_cf64_get(acc: *const DpAccCf64Raw) -> Complex64;
    pub fn dp_acc_cf64_dump(acc: *mut DpAccCf64Raw) -> Complex64;
    pub fn dp_acc_cf64_push(acc: *mut DpAccCf64Raw, x: Complex64);
    pub fn dp_acc_cf64_add(
        acc: *mut DpAccCf64Raw,
        x: *const Complex64,
        n: usize,
    );
    pub fn dp_acc_cf64_madd(
        acc: *mut DpAccCf64Raw,
        x: *const Complex64,
        h: *const f32,
        n: usize,
    );
    pub fn dp_acc_cf64_add2d(
        acc: *mut DpAccCf64Raw,
        x: *const Complex64,
        rows: usize,
        cols: usize,
    );
    pub fn dp_acc_cf64_madd2d(
        acc: *mut DpAccCf64Raw,
        x: *const Complex64,
        h: *const f32,
        rows: usize,
        cols: usize,
    );
}

/// RAII wrapper around `dp_acc_f32_t`.
///
/// Maintains a running `f32` sum.  All mutations require `&mut self`
/// so aliasing is impossible.
///
/// # Example
/// ```no_run
/// use doppler::acc::AccF32;
///
/// let x = [1.0_f32, 2.0, 3.0, 4.0];
/// let h = [0.25_f32; 4]; // uniform window
///
/// let mut acc = AccF32::new();
/// acc.madd(&x, &h); // acc = 0.25*(1+2+3+4) = 2.5
/// assert!((acc.dump() - 2.5).abs() < 1e-6);
/// assert!(acc.dump() == 0.0); // dump zeroed the accumulator
/// ```
pub struct AccF32 {
    ptr: *mut DpAccF32Raw,
}

unsafe impl Send for AccF32 {}

impl AccF32 {
    /// Allocate and zero a new f32 accumulator.
    ///
    /// # Panics
    /// Panics if `dp_acc_f32_create` returns null (out-of-memory).
    pub fn new() -> Self {
        let ptr = unsafe { dp_acc_f32_create() };
        assert!(!ptr.is_null(), "dp_acc_f32_create returned null");
        AccF32 { ptr }
    }

    /// Add one sample: `acc += x`.
    pub fn push(&mut self, x: f32) {
        unsafe { dp_acc_f32_push(self.ptr, x) }
    }

    /// Add a slice of samples: `acc += Σ x[k]`.
    pub fn add(&mut self, x: &[f32]) {
        unsafe { dp_acc_f32_add(self.ptr, x.as_ptr(), x.len()) }
    }

    /// Multiply-accumulate: `acc += Σ x[k]·h[k]`.
    ///
    /// # Panics
    /// Panics if `x` and `h` have different lengths.
    pub fn madd(&mut self, x: &[f32], h: &[f32]) {
        assert_eq!(x.len(), h.len(), "x and h must be the same length");
        unsafe {
            dp_acc_f32_madd(self.ptr, x.as_ptr(), h.as_ptr(), x.len())
        }
    }

    /// 2-D accumulate: `acc += Σᵢⱼ x[i][j]` (row-major, `rows×cols`).
    ///
    /// # Panics
    /// Panics if `x.len() != rows * cols`.
    pub fn add2d(&mut self, x: &[f32], rows: usize, cols: usize) {
        assert_eq!(x.len(), rows * cols, "x.len() must equal rows*cols");
        unsafe { dp_acc_f32_add2d(self.ptr, x.as_ptr(), rows, cols) }
    }

    /// 2-D MAC: `acc += Σᵢⱼ x[i][j]·h[i][j]` (row-major).
    ///
    /// # Panics
    /// Panics if `x` or `h` lengths do not match `rows × cols`.
    pub fn madd2d(
        &mut self,
        x: &[f32],
        h: &[f32],
        rows: usize,
        cols: usize,
    ) {
        assert_eq!(x.len(), rows * cols);
        assert_eq!(h.len(), rows * cols);
        unsafe {
            dp_acc_f32_madd2d(
                self.ptr, x.as_ptr(), h.as_ptr(), rows, cols,
            )
        }
    }

    /// Read the current accumulated value without clearing it.
    pub fn get(&self) -> f32 {
        unsafe { dp_acc_f32_get(self.ptr) }
    }

    /// Read the current value *and* zero the accumulator.
    ///
    /// This is the canonical operation for polyphase decimators: read
    /// the branch output and immediately prepare for the next window.
    pub fn dump(&mut self) -> f32 {
        unsafe { dp_acc_f32_dump(self.ptr) }
    }

    /// Zero the accumulator without reading it.
    pub fn reset(&mut self) {
        unsafe { dp_acc_f32_reset(self.ptr) }
    }
}

impl Default for AccF32 {
    fn default() -> Self {
        Self::new()
    }
}

impl Drop for AccF32 {
    fn drop(&mut self) {
        unsafe { dp_acc_f32_destroy(self.ptr) }
    }
}

/// RAII wrapper around `dp_acc_cf64_t`.
///
/// Maintains a running `Complex64` sum.  Coefficients for `madd` are
/// always real (`f32`), matching the polyphase FIR structure where
/// filter taps are real-valued.
///
/// # Example
/// ```no_run
/// use doppler::acc::AccCf64;
/// use num_complex::Complex64;
///
/// let mut acc = AccCf64::new();
/// acc.push(Complex64::new(1.0, 0.0));
/// acc.push(Complex64::new(0.0, 1.0));
/// let v = acc.get(); // non-destructive read
/// assert!((v.re - 1.0).abs() < 1e-12);
/// assert!((v.im - 1.0).abs() < 1e-12);
/// ```
pub struct AccCf64 {
    ptr: *mut DpAccCf64Raw,
}

unsafe impl Send for AccCf64 {}

impl AccCf64 {
    /// Allocate and zero a new cf64 accumulator.
    ///
    /// # Panics
    /// Panics if `dp_acc_cf64_create` returns null.
    pub fn new() -> Self {
        let ptr = unsafe { dp_acc_cf64_create() };
        assert!(!ptr.is_null(), "dp_acc_cf64_create returned null");
        AccCf64 { ptr }
    }

    /// Add one complex sample: `acc += x`.
    pub fn push(&mut self, x: Complex64) {
        unsafe { dp_acc_cf64_push(self.ptr, x) }
    }

    /// Add a slice of complex samples: `acc += Σ x[k]`.
    pub fn add(&mut self, x: &[Complex64]) {
        unsafe { dp_acc_cf64_add(self.ptr, x.as_ptr(), x.len()) }
    }

    /// Multiply-accumulate: `acc += Σ x[k]·h[k]` (real taps).
    ///
    /// This is the hot-path operation for polyphase FIR resamplers.
    ///
    /// # Panics
    /// Panics if `x` and `h` have different lengths.
    pub fn madd(&mut self, x: &[Complex64], h: &[f32]) {
        assert_eq!(x.len(), h.len(), "x and h must be the same length");
        unsafe {
            dp_acc_cf64_madd(self.ptr, x.as_ptr(), h.as_ptr(), x.len())
        }
    }

    /// 2-D accumulate (row-major, `rows×cols` complex samples).
    ///
    /// # Panics
    /// Panics if `x.len() != rows * cols`.
    pub fn add2d(
        &mut self,
        x: &[Complex64],
        rows: usize,
        cols: usize,
    ) {
        assert_eq!(x.len(), rows * cols);
        unsafe { dp_acc_cf64_add2d(self.ptr, x.as_ptr(), rows, cols) }
    }

    /// 2-D MAC with real taps (row-major).
    ///
    /// # Panics
    /// Panics if `x` or `h` lengths do not match `rows × cols`.
    pub fn madd2d(
        &mut self,
        x: &[Complex64],
        h: &[f32],
        rows: usize,
        cols: usize,
    ) {
        assert_eq!(x.len(), rows * cols);
        assert_eq!(h.len(), rows * cols);
        unsafe {
            dp_acc_cf64_madd2d(
                self.ptr, x.as_ptr(), h.as_ptr(), rows, cols,
            )
        }
    }

    /// Read the current accumulated value without clearing it.
    pub fn get(&self) -> Complex64 {
        unsafe { dp_acc_cf64_get(self.ptr) }
    }

    /// Read the current value *and* zero the accumulator.
    pub fn dump(&mut self) -> Complex64 {
        unsafe { dp_acc_cf64_dump(self.ptr) }
    }

    /// Zero the accumulator without reading it.
    pub fn reset(&mut self) {
        unsafe { dp_acc_cf64_reset(self.ptr) }
    }
}

impl Default for AccCf64 {
    fn default() -> Self {
        Self::new()
    }
}

impl Drop for AccCf64 {
    fn drop(&mut self) {
        unsafe { dp_acc_cf64_destroy(self.ptr) }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use num_complex::Complex64;

    // ── AccF32 ───────────────────────────────────────────────────────────────

    #[test]
    fn acc_f32_push_and_get() {
        let mut a = AccF32::new();
        a.push(1.0);
        a.push(2.0);
        a.push(3.0);
        assert!((a.get() - 6.0).abs() < 1e-6, "get = {}", a.get());
        assert!((a.get() - 6.0).abs() < 1e-6); // non-destructive
    }

    #[test]
    fn acc_f32_dump_zeros() {
        let mut a = AccF32::new();
        a.push(5.0);
        let v = a.dump();
        assert!((v - 5.0).abs() < 1e-6);
        assert!(a.get().abs() < 1e-7, "after dump acc should be 0");
    }

    #[test]
    fn acc_f32_reset_zeros() {
        let mut a = AccF32::new();
        a.push(42.0);
        a.reset();
        assert!(a.get().abs() < 1e-7, "after reset acc should be 0");
    }

    #[test]
    fn acc_f32_add_slice() {
        let mut a = AccF32::new();
        a.add(&[1.0, 2.0, 3.0, 4.0]);
        assert!((a.dump() - 10.0).abs() < 1e-6);
    }

    #[test]
    fn acc_f32_madd_dot_product() {
        // [1,2,3,4] · [0.25; 4] = 2.5
        let mut a = AccF32::new();
        a.madd(&[1.0, 2.0, 3.0, 4.0], &[0.25; 4]);
        assert!((a.dump() - 2.5).abs() < 1e-6);
    }

    #[test]
    fn acc_f32_madd_identity_tap() {
        let mut a = AccF32::new();
        a.madd(&[7.0_f32], &[1.0_f32]);
        assert!((a.dump() - 7.0).abs() < 1e-6);
    }

    // ── AccCf64 ──────────────────────────────────────────────────────────────

    #[test]
    fn acc_cf64_push_and_get() {
        let mut a = AccCf64::new();
        a.push(Complex64::new(1.0, 2.0));
        a.push(Complex64::new(3.0, 4.0));
        let v = a.get();
        assert!((v.re - 4.0).abs() < 1e-12);
        assert!((v.im - 6.0).abs() < 1e-12);
    }

    #[test]
    fn acc_cf64_dump_zeros() {
        let mut a = AccCf64::new();
        a.push(Complex64::new(1.5, -2.5));
        let v = a.dump();
        assert!((v.re - 1.5).abs() < 1e-12);
        assert!((v.im - (-2.5)).abs() < 1e-12);
        let z = a.get();
        assert!(z.norm() < 1e-12, "after dump acc should be zero");
    }

    #[test]
    fn acc_cf64_madd_real_weights() {
        // 0.5*(1+0j) + 0.5*(0+1j) = 0.5 + 0.5j
        let x = [Complex64::new(1.0, 0.0), Complex64::new(0.0, 1.0)];
        let h = [0.5_f32, 0.5_f32];
        let mut a = AccCf64::new();
        a.madd(&x, &h);
        let v = a.dump();
        assert!((v.re - 0.5).abs() < 1e-12);
        assert!((v.im - 0.5).abs() < 1e-12);
    }

    #[test]
    fn acc_cf64_add_slice() {
        let x = [
            Complex64::new(1.0, -1.0),
            Complex64::new(2.0, -2.0),
        ];
        let mut a = AccCf64::new();
        a.add(&x);
        let v = a.dump();
        assert!((v.re - 3.0).abs() < 1e-12);
        assert!((v.im - (-3.0)).abs() < 1e-12);
    }
}
