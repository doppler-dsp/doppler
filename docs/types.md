# Type System

doppler uses C99 standard types throughout — no custom struct wrappers for
complex samples in the DSP path.  Every public API takes and returns types
exactly as the C99 standard defines them.

---

## C99 type system

<figure markdown>
| C99 type | Bytes | Alias | NumPy | Rust |
| -------- | ----- | ----- | ----- | ---- |
| `float` | 4 | F32 | `np.float32` | `f32` |
| `double` | 8 | F64 | `np.float64` | `f64` |
| `int8_t` | 1 | CI8 | `np.int8` | `i8` |
| `int16_t` | 2 | CI16 | `np.int16` | `i16` |
| `int32_t` | 4 | CI32 | `np.int32` | `i32` |
| `uint32_t` | 4 | UI32 | `np.uint32` | `u32` |
| `uint64_t` | 8 | UI64 | `np.uint64` | `u64` |
| `float _Complex` | 8 | CF32 | `np.complex64` | `DpCf32` |
| `double _Complex` | 16 | CF64 | `np.complex128` | `DpCf64` |
| `long double _Complex` | 32 | CF128 | `np.clongdouble` | — |
<figcaption>C99 type system — Built-in, &lt;stdint.h&gt;, &lt;complex.h&gt;</figcaption>
</figure>

!!! note "Rust FFI types"
    `DpCf32` and `DpCf64` are `#[repr(C)]` structs (`{f32 i, f32 q}` /
    `{f64 i, f64 q}`) that mirror the C ABI exactly.  Both implement
    `From<Complex<f32>>` / `From<Complex<f64>>` for zero-cost conversion
    to and from `num_complex`:

    ```rust
    let c: Complex<f32> = DpCf32 { i: 1.0, q: 0.0 }.into();
    let ffi: DpCf64 = Complex::new(0.0_f64, 1.0_f64).into();
    ```

Aliases (CF32, CI16, etc.) are shorthand used in documentation and the
streaming API (`CF32`, `CI16`, …).  They are not typedefs — the C
API always spells the full C99 type.

Complex integers (CI8 / CI16 / CI32) are passed as interleaved arrays:
`input[2*k]` = I, `input[2*k+1]` = Q, with `num_samples` counting complex
pairs.  The Rust FFI uses `#[repr(C)]` structs (`DpCi8`, `DpCi16`,
`DpCi32`) at the boundary; the C side uses the plain integer pointer.

---

## Creating complex constants

```c
#include <complex.h>

float _Complex tone = CMPLXF(0.5f, 0.866f);   /* e^{iπ/3} */
double _Complex dc  = CMPLX(1.0, 0.0);
```

---

## Which type goes where

| Module | Input | Output / state |
| ------ | ----- | -------------- |
| `lo` | — | `float _Complex` |
| `nco` | — | `uint32_t` |
| `fir` | `float _Complex` | `float _Complex` |
| `fft`, `fft2d` | `float _Complex`, `double _Complex` | same |
| `corr`, `corr2d` | `float _Complex` | `double _Complex` |
| `detector`, `detector2d` | `float _Complex` | detections |
| `ddc` (complex) | `float _Complex` | `float _Complex` |
| `ddc` (real) | `float` | `float _Complex` |
| `resampler` | `float _Complex` | `float _Complex` |
| `halfband_decimator` | `float _Complex` | `float _Complex` |
| `acc_f32` | `float` | `float` |
| `acc_cf64` | `double _Complex` | `double _Complex` |
| `delay` | `double _Complex` | `double _Complex` |
| `buffer` | `float`, `double`, `int16_t` | same (scalar elements) |
| `stream` | any `dp_sample_type_t` wire type | same |

---

## Precision design rationale

**CF32 (`float _Complex`)** — default signal path.  Matches native SDR
hardware output (RTL-SDR, HackRF, LimeSDR, PlutoSDR, USRP).
SIMD-friendly: AVX-512 processes eight CF32 pairs per instruction.

**CF64 (`double _Complex`)** — spectral and accumulation paths.  The FFT
backend (pocketfft) works in `double _Complex` throughout.  Running sums
accumulate in `double` to prevent rounding error before rounding back to
`float`.

**CI8 / CI16 / CI32** — streaming wire types.  Real SDR hardware delivers
quantized samples; convert to CF32 before processing with the DSP library.

Rule: *compute in the cheapest type that keeps the math clean.*

---

## See also

- [just-makeit — State Variable Types](https://just-buildit.github.io/just-makeit/types/) —
  maps these C types to `--state name:type` scaffold tokens and generated
  NumPy stubs.
