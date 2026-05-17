# Type System

doppler uses C99 standard types throughout — no custom struct wrappers
for complex samples in the DSP path.  Every public API takes or returns
types from `<stdint.h>` or `<complex.h>`, exactly as the C99 standard
defines them.  The `dp_*_t` family in `dp/stream.h` provides named
interleaved-struct aliases for use in streaming and FFI contexts.

---

## Integer types — `<stdint.h>`

| Type | Width | Used for |
| ---- | ----- | -------- |
| `int8_t` | 8-bit signed | CI8 samples (SDR hardware output) |
| `int16_t` | 16-bit signed | CI16 samples (baseband IQ from most SDRs) |
| `int32_t` | 32-bit signed | CI32 samples; streaming wire format |
| `uint32_t` | 32-bit unsigned | NCO phase accumulator; header fields |
| `uint64_t` | 64-bit unsigned | Sequence numbers; timestamps; sample counts |
| `float` | 32-bit IEEE 754 | Accumulator f32; polyphase tap coefficients |
| `double` | 64-bit IEEE 754 | High-precision accumulation |

Complex integers (e.g., `int16_t` I/Q pairs) are always passed as
interleaved pairs: `input[2*k]` = I sample, `input[2*k+1]` = Q sample.
Functions that accept them document the pointer as `const int16_t *samples`
with a `num_samples` argument counting complex samples (so the array
length is `2 × num_samples`).

---

## Complex floating-point types — `<complex.h>`

| C99 type | Alias | Bytes | Used for |
| -------- | ----- | ----- | -------- |
| `float _Complex` | CF32 | 8 | Signal path: NCO output, FIR, resamplers, DDC |
| `double _Complex` | CF64 | 16 | FFT backend; `dp_c16_mul`; streaming |
| `long double _Complex` | CF128 | 32 | Streaming wire format only |

These are the standard C99 complex types — not structs, not typedefs.
Call sites use the C99 spelling directly:

```c
#include <complex.h>
#include <dp/fir.h>

float _Complex in[1024];
float _Complex out[1024];

dp_fir_t *f = dp_fir_create(taps, 63);
dp_fir_execute_cf32(f, in, out, 1024);
```

### Creating complex constants: CMPLXF / CMPLX

```c
float _Complex tone = CMPLXF(0.5f, 0.866f);  /* e^{iπ/3} */
double _Complex dc  = CMPLX(1.0, 0.0);
```

---

## Named IQ types — `dp_*_t` (streaming & FFI)

Defined in `dp/stream.h`, available via `doppler.h`.  All are
`#[repr(C)]` interleaved structs with `i` (in-phase) and `q`
(quadrature) fields matching the on-wire format and NumPy complex layout.

| C typedef | Fields | Bytes | NumPy dtype | Rust type |
| --------- | ------ | ----- | ----------- | --------- |
| `dp_ci8_t` | `int8_t i, q` | 2 | — | `DpCi8` |
| `dp_ci16_t` | `int16_t i, q` | 4 | — | `DpCi16` |
| `dp_ci32_t` | `int32_t i, q` | 8 | — | `DpCi32` |
| `dp_cf32_t` | `float i, q` | 8 | `complex64` | `DpCf32` |
| `dp_cf64_t` | `double i, q` | 16 | `complex128` | `Complex64`¹ |
| `dp_cf128_t` | `long double i, q` | 32 | — | — |

¹ Rust FFI uses `num_complex::Complex64` directly for `dp_cf64_t`
because it has the same `[f64; 2]` memory layout — no wrapper needed.

The DSP path (NCO, FIR, resamplers, DDC) uses the C99 `float _Complex`
/ `double _Complex` types, not `dp_cf32_t` / `dp_cf64_t`.  They are
layout-compatible but distinct at the source level.

---

## `dp_sample_type_t` — runtime type tag

The streaming API identifies the wire type of a message at runtime:

```c
typedef enum {
    DP_CI32  = 0,   /* int32_t  I/Q — 8 bytes/sample  */
    DP_CF64  = 1,   /* double _Complex — 16 bytes/sample */
    DP_CF128 = 2,   /* long double _Complex — 32 bytes/sample */
    DP_CI8   = 3,   /* int8_t   I/Q — 2 bytes/sample  */
    DP_CI16  = 4,   /* int16_t  I/Q — 4 bytes/sample  */
    DP_CF32  = 5,   /* float _Complex — 8 bytes/sample */
} dp_sample_type_t;
```

Set at socket creation (`dp_pub_create`, `dp_push_create`) and embedded
in every `dp_header_t` frame.  Python exposes these as `doppler.CI32`,
`doppler.CF64`, `doppler.CF128`.

---

## Ring buffer element types

The ring buffer API (`dp/buffer.h`) is element-typed, not sample-typed.
Each buffer holds a flat array of scalars; how they are interpreted as
complex samples is the caller's responsibility.

| Buffer type | Element | Bytes/elem | Python class |
| ----------- | ------- | ---------- | ------------ |
| `dp_f32` | `float` | 4 | `doppler.buffer.F32Buffer` |
| `dp_f64` | `double` | 8 | `doppler.buffer.F64Buffer` |
| `dp_i16` | `int16_t` | 2 | `doppler.buffer.I16Buffer` |

Note: buffer types omit the `_t` suffix — `dp_f32` not `dp_f32_t`.
This is a known naming inconsistency with the rest of the API.

To stream complex IQ through an `F32Buffer`, write interleaved floats:

```python
from doppler.buffer import F32Buffer
import numpy as np

buf = F32Buffer(2048)   # 2048 floats → 1024 complex64 samples

iq = np.ones(1024, dtype=np.complex64)
buf.write(iq.view(np.float32))   # reinterpret as float32 pairs
```

---

## Which type goes where

| Header | Input type(s) | Output / state type | Notes |
| ------ | ------------- | ------------------- | ----- |
| `dp/nco.h` | — | `float _Complex`, `uint32_t` | LUT → CF32; raw phase = u32 |
| `dp/fir.h` | `dp_ci8_t`, `dp_ci16_t`, `dp_ci32_t`, `float _Complex` | `float _Complex` | Integer inputs up-cast via SIMD |
| `dp/fft.h` | `double _Complex`, `float _Complex` | same | C99 complex types, not `dp_cf*_t` |
| `dp/util.h` | `double _Complex` | `double _Complex` | `dp_c16_mul` SIMD multiply |
| `dp/resamp.h` | `float _Complex` | `float _Complex` | Polyphase resampler |
| `dp/resamp_dpmfs.h` | `float _Complex` | `float _Complex` | DPMFS resampler |
| `dp/hbdecim.h` | `float _Complex` or `float` | `float _Complex` | CF32 or real→CF32 |
| `dp/ddc.h` | `float _Complex` | `float _Complex` | Complex DDC (Arch A) |
| `dp/ddc.h` | `float` | `float _Complex` | Real DDC (Arch D2) |
| `dp/accumulator.h` | `float` or `double _Complex` | same | Separate f32 and cf64 accumulators |
| `dp/delay.h` | `double _Complex` | `double _Complex` | Polyphase tap delay |
| `dp/buffer.h` | `float`, `double`, `int16_t` | same | Scalar ring buffer elements |
| `dp/stream.h` | all `dp_*_t` types | all types | Wire type set at socket creation |

---

## Precision design rationale

Three levels of precision are used deliberately:

**`float _Complex` (CF32)** — default signal path.
Matches native SDR hardware output (RTL-SDR, HackRF, LimeSDR, PlutoSDR,
USRP).  SIMD-friendly: AVX-512 processes eight CF32 pairs per instruction.
Used by NCO, FIR, polyphase resampler, halfband decimator, DDC.

**`double _Complex` (CF64)** — spectral and accumulation paths.
The FFT backend (FFTW or pocketfft) works in `double _Complex` throughout.
`dp_c16_mul` (SIMD complex multiply) uses `double _Complex` to match.
Running sums over many taps accumulate in `double` to prevent rounding
error before rounding back to `float`.

**`int8_t` / `int16_t` / `int32_t`** — input-only integer formats.
Real SDR hardware delivers quantized samples.  FIR filters accept integer
inputs and up-cast to `float _Complex` via SIMD as part of execution.

Rule: *compute in the cheapest type that keeps the math clean.*

---

## Python equivalents

| C type | NumPy dtype | Notes |
| ------ | ----------- | ----- |
| `dp_ci8_t` | — | streaming / manual only |
| `dp_ci16_t` | `np.dtype([('i','i2'),('q','i2')])` | manual; streaming use only |
| `dp_ci32_t` | streaming only (`CI32`) | `doppler.CI32` |
| `dp_cf32_t` / `float _Complex` | `np.complex64` | memory layout identical |
| `dp_cf64_t` / `double _Complex` | `np.complex128` | memory layout identical |
| `dp_cf128_t` | streaming only (`CF128`) | `doppler.CF128` |
| `uint32_t` | `np.uint32` | NCO phase |
| `uint8_t` | `np.uint8` | NCO carry/overflow flag |
| `float` | `np.float32` | f32 accumulator, tap coefficients |

---

## Rust equivalents

| C type | Rust type | Notes |
| ------ | --------- | ----- |
| `dp_ci8_t` | `types::DpCi8` | `#[repr(C)]` struct |
| `dp_ci16_t` | `types::DpCi16` | `#[repr(C)]` struct |
| `dp_ci32_t` | `types::DpCi32` | `#[repr(C)]` struct |
| `dp_cf32_t` | `types::DpCf32` | `From<Complex<f32>>` impl |
| `dp_cf64_t` | `num_complex::Complex64` | same layout, no wrapper needed |
| `double _Complex` | `num_complex::Complex64` | FFT and util APIs |
| `uint32_t` | `u32` | NCO phase accumulator |

---

## See also

- [just-makeit — State Variable Types](https://just-buildit.github.io/just-makeit/types/) —
  maps these C types to `--state name:type` scaffold tokens and generated
  NumPy stubs.
