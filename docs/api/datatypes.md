# Data Types

doppler uses a consistent set of named types for complex IQ samples
across the C library, Python bindings, and Rust FFI.  This page is the
authoritative reference for what each type is, where it lives, and how
it maps between languages.

---

## Complex sample types

All IQ types are interleaved structs with two fields named `i` (in-phase)
and `q` (quadrature).  No separate real/imaginary arrays — the struct
layout matches the on-wire format and the NumPy `complex64` / `complex128`
layout directly.

| C typedef | Bit width | Struct fields | NumPy dtype | Rust type |
| --------- | --------- | ------------- | ----------- | --------- |
| `dp_ci8_t` | 8+8 | `int8_t i, q` | — | `DpCi8` |
| `dp_ci16_t` | 16+16 | `int16_t i, q` | — | `DpCi16` |
| `dp_ci32_t` | 32+32 | `int32_t i, q` | — | `DpCi32` |
| `dp_cf32_t` | 32+32 | `float i, q` | `complex64` | `DpCf32` |
| `dp_cf64_t` | 64+64 | `double i, q` | `complex128` | `Complex64`¹ |
| `dp_cf128_t` | 128+128 | `long double i, q` | — | — |

¹ The Rust FFI uses `num_complex::Complex64` directly for `dp_cf64_t`
because it has the same memory layout (`[f64; 2]`).

All typedefs are defined in `dp/stream.h` and available via the umbrella
header `doppler.h`.

### Byte sizes

| Type | Bytes/sample |
| ---- | ------------ |
| `dp_ci8_t` | 2 |
| `dp_ci16_t` | 4 |
| `dp_ci32_t` | 8 |
| `dp_cf32_t` | 8 |
| `dp_cf64_t` | 16 |
| `dp_cf128_t` | 32 |

---

## Sample type enum (streaming)

The streaming API identifies sample types at runtime via an enum.

```c
typedef enum {
    DP_CI32  = 0,   /* int32_t  I/Q — 8 bytes/sample  */
    DP_CF64  = 1,   /* double   I/Q — 16 bytes/sample */
    DP_CF128 = 2,   /* long double I/Q — 32 bytes/sample */
    DP_CI8   = 3,   /* int8_t   I/Q — 2 bytes/sample  */
    DP_CI16  = 4,   /* int16_t  I/Q — 4 bytes/sample  */
    DP_CF32  = 5,   /* float    I/Q — 8 bytes/sample  */
} dp_sample_type_t;
```

The Python streaming API exposes these as integer constants:
`doppler.CI32`, `doppler.CF64`, `doppler.CF128`.

---

## Which type goes where

Each API uses the types that match its precision and throughput goal.

| API | Input type(s) | Output / internal type | Notes |
| --- | ------------- | ---------------------- | ----- |
| NCO (`dp/nco.h`) | — | `dp_cf32_t`, `uint32_t` | LUT output is CF32; raw accumulator is u32 |
| FIR (`dp/fir.h`) | `dp_ci8_t`, `dp_ci16_t`, `dp_ci32_t`, `dp_cf32_t` | `dp_cf32_t` | Integer inputs up-cast via SIMD; all compute in CF32 |
| FFT (`dp/fft.h`) | `double complex` | `double complex` | Uses C99 complex type, not `dp_cf64_t`² |
| Util (`dp/util.h`) | `double complex` | `double complex` | Same reason as FFT² |
| Accumulator f32 (`dp/accumulator.h`) | `float`, `float[]` | `float` | Scalar f32 only |
| Accumulator cf64 (`dp/accumulator.h`) | `dp_cf64_t`, `dp_cf64_t[]` | `dp_cf64_t` | Complex f64; real taps (`float`) for `madd` |
| Delay (`dp/delay.h`) | `dp_cf64_t` | `dp_cf64_t` | Double-precision delay line for polyphase resamplers |
| Resampler (`dp/resamp.h`) | `dp_cf32_t` | `dp_cf32_t` | SDR-grade cf32 end-to-end |
| DPMFS resampler (`dp/resamp_dpmfs.h`) | `dp_cf32_t` | `dp_cf32_t` | Same |
| Halfband decimator (`dp/hbdecim.h`) | `dp_cf32_t` | `dp_cf32_t` | Same |
| Streaming (`dp/stream.h`) | all types | all types | Negotiated at socket creation |
| Ring buffers (`dp/buffer.h`) | see below | see below | Raw element buffers |

²  `dp_fft_*` and `dp_c16_mul` take `double complex` (the C99 complex
arithmetic type) instead of `dp_cf64_t`.  These are layout-compatible
— both are `[double; 2]` — but the types differ at the C source level.
This is an intentional choice: the FFT backend (FFTW / pocketfft) works
in terms of `double complex`, and spelling it that way avoids an extra
cast at every call site.

---

## Ring buffer element types

The ring buffer API (`dp/buffer.h`) is element-typed, not sample-typed.
Each buffer holds a flat array of scalar elements; how those elements are
interpreted as complex samples is left to the caller.

| Buffer type | Element | C++ analogy | Python class |
| ----------- | ------- | ----------- | ------------ |
| `dp_f32` | `float` | `vector<float>` | `doppler.buffer.F32Buffer` |
| `dp_f64` | `double` | `vector<double>` | `doppler.buffer.F64Buffer` |
| `dp_i16` | `int16_t` | `vector<int16_t>` | `doppler.buffer.I16Buffer` |

**Note:** The buffer types do not use the `_t` suffix — `dp_f32` not
`dp_f32_t`.  This diverges from the rest of the API and is a known
naming inconsistency.

To stream complex IQ data through a `dp_f32` buffer, write interleaved
`float` values (I₀, Q₀, I₁, Q₁, …) and `2N` floats per complex sample.
In Python, pass a `complex64` array cast to `float32`:

```python
from doppler.buffer import F32Buffer
import numpy as np

buf = F32Buffer(2048)   # 2048 floats → 1024 complex64 samples

iq = np.ones(1024, dtype=np.complex64)
buf.write(iq.view(np.float32))   # reinterpret as float32 pairs
```

---

## Precision design rationale

The library uses three distinct precision levels deliberately:

**CF32 (32-bit complex float)** — default signal path precision.
Used by NCO output, FIR input/output, and resamplers.  Matches the
native format of most SDR hardware (RTL-SDR, HackRF, LimeSDR, PlutoSDR,
USRP).  SIMD-friendly: AVX-512 processes 8 CF32 pairs per instruction.

**CF64 (64-bit complex double)** — accumulation and delay precision.
Used by the accumulator and delay line.  Polyphase resampler inner
products accumulate in CF64 to avoid rounding error across many taps,
then round down to CF32 on output.

**double complex (C99)** — FFT and SIMD utility precision.
The FFT backend operates in double precision throughout.  `dp_c16_mul`
uses `double complex` to match.

The rule: *compute in the cheapest type that keeps the math clean*.
Signal paths are CF32.  Running sums that accumulate many samples
step up to CF64.  Spectral transforms go to double.

---

## Python equivalents

| C type | Python / NumPy | Import path |
| ------ | -------------- | ----------- |
| `dp_ci8_t` | — | — |
| `dp_ci16_t` | `np.dtype([('i','i2'),('q','i2')])` | manual |
| `dp_ci32_t` | streaming only (`CI32`) | `doppler.CI32` |
| `dp_cf32_t` | `np.complex64` | standard |
| `dp_cf64_t` | `np.complex128` | standard |
| `dp_cf128_t` | streaming only (`CF128`) | `doppler.CF128` |
| `uint32_t` (NCO phase) | `np.uint32` | standard |
| `uint8_t` (NCO carry) | `np.uint8` | standard |

---

## Rust equivalents

| C type | Rust type | Crate module | Notes |
| ------ | --------- | ------------ | ----- |
| `dp_ci8_t` | `types::DpCi8` | `doppler::types` | `#[repr(C)]` struct |
| `dp_ci16_t` | `types::DpCi16` | `doppler::types` | `#[repr(C)]` struct |
| `dp_ci32_t` | `types::DpCi32` | `doppler::types` | `#[repr(C)]` struct |
| `dp_cf32_t` | `types::DpCf32` | `doppler::types` | `From<Complex<f32>>` impl |
| `dp_cf64_t` | `num_complex::Complex64` | `num_complex` | Same layout, no wrapper needed |
| `double complex` | `num_complex::Complex64` | `num_complex` | FFT and util APIs |
