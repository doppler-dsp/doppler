//! Rust FFI bindings for the doppler C library.
//!
//! Each submodule mirrors one C header under `native/inc/`:
//!
//! | Module   | C header                  | Key types / functions            |
//! |----------|---------------------------|----------------------------------|
//! | [`acc`]  | `accumulator/accumulator.h` | [`acc::AccF32`], [`acc::AccCf64`] |
//! | [`fft`]  | `fft/fft_core.h`          | [`fft::Fft`]                     |
//! | [`fir`]  | `fir/fir_core.h`          | [`fir::Fir`]                     |
//! | [`lo`]   | `lo/lo_core.h`            | [`lo::Lo`]                       |
//! | [`nco`]  | `nco/nco_core.h`          | [`nco::Nco`]                     |
//! | [`util`] | —                         | [`c16_mul`]                      |
//!
//! # Quick FFT example
//!
//! ```no_run
//! use doppler::fft::{Fft, Direction};
//! use num_complex::Complex64;
//!
//! let n = 1024_usize;
//! let input:  Vec<Complex64> = (0..n).map(|i| {
//!     let t = 2.0 * std::f64::consts::PI * i as f64 / n as f64;
//!     Complex64::new(t.cos(), 0.0)
//! }).collect();
//! let mut output = vec![Complex64::default(); n];
//!
//! let fft = Fft::new(n, Direction::Forward);
//! fft.execute_cf64(&input, &mut output);
//! // output[1] and output[N-1] contain the cosine energy (~N/2 each)
//! ```

/// Error returned by `set_state` when a serialized state blob is rejected.
///
/// State blobs are native-endian and versioned (see
/// `docs/design/state-serialization.md`): a blob from a different object,
/// format version, endianness, or configuration is rejected rather than
/// silently reinterpreted.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum StateError {
    /// `blob.len()` does not match the object's `state_bytes()`.
    Size {
        /// The size the object expects (its `state_bytes()`).
        expected: usize,
        /// The size of the blob that was passed.
        got: usize,
    },
    /// The envelope was rejected: wrong object tag, version, or endianness.
    Invalid,
}

impl std::fmt::Display for StateError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            StateError::Size { expected, got } => write!(
                f,
                "state blob size mismatch: expected {expected}, got {got}"
            ),
            StateError::Invalid => write!(f, "state blob envelope rejected"),
        }
    }
}

impl std::error::Error for StateError {}

/// Generate the `state_bytes` / `get_state` / `set_state` triplet for an RAII
/// wrapper over a C object implementing the `dp_state.h` bytes interface.
///
/// Expects the three C functions (declared in the module's `extern "C"` block)
/// and the wrapper's private `ptr` field.
macro_rules! impl_serializable {
    ($wrapper:ty, $bytes:ident, $get:ident, $set:ident) => {
        impl $wrapper {
            /// Serialized state size in bytes (envelope + payload).
            pub fn state_bytes(&self) -> usize {
                unsafe { $bytes(self.ptr) }
            }

            /// Serialize the running state to a portable byte blob.
            ///
            /// Restore it into a fresh, identically-built instance with
            /// [`Self::set_state`] to resume bit-for-bit — across a thread,
            /// process, or pod.
            pub fn get_state(&self) -> Vec<u8> {
                let mut buf = vec![0u8; self.state_bytes()];
                unsafe { $get(self.ptr, buf.as_mut_ptr()) };
                buf
            }

            /// Restore running state from a [`Self::get_state`] blob.
            ///
            /// # Errors
            /// [`StateError`](crate::StateError)`::Size` if `blob.len()`
            /// differs from [`Self::state_bytes`]; `::Invalid` if the
            /// envelope is rejected (wrong object / version / endianness).
            pub fn set_state(
                &mut self,
                blob: &[u8],
            ) -> Result<(), $crate::StateError> {
                let expected = self.state_bytes();
                if blob.len() != expected {
                    return Err($crate::StateError::Size {
                        expected,
                        got: blob.len(),
                    });
                }
                match unsafe { $set(self.ptr, blob.as_ptr()) } {
                    0 => Ok(()),
                    _ => Err($crate::StateError::Invalid),
                }
            }
        }
    };
}

pub mod acc;
pub mod fft;
pub mod fir;
pub mod lo;
pub mod nco;
pub mod util;

mod types;
pub use types::{DpCf32, DpCi16, DpCi32, DpCi8};

/// Complex multiplication — re-exported from [`util`].
pub use util::c16_mul;
