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
| `double _Complex` | CF64 | 16 | FFT backend; accumulators; streaming |
| `long double _Complex` | CF128 | 32 | Streaming wire format only |

These are the standard C99 complex types — not structs, not typedefs.
Call sites use the C99 spelling directly:

```c
#include <complex.h>
#include <fir/fir_core.h>

float taps[63] = { ... };
fir_state_t *f = fir_create_real(taps, 63);

float _Complex in[1024], out[1024];
fir_execute(f, in, 1024, out);
fir_destroy(f);
```

### Creating complex constants: CMPLXF / CMPLX

```c
float _Complex tone = CMPLXF(0.5f, 0.866f);  /* e^{iπ/3} */
double _Complex dc  = CMPLX(1.0, 0.0);
```

---

## Named IQ types — Rust FFI structs

The Rust FFI (`ffi/rust/src/types.rs`) defines `#[repr(C)]` interleaved
structs for passing IQ samples across the C boundary:

| Rust type | Fields | Bytes | NumPy equiv | Notes |
| --------- | ------ | ----- | ----------- | ----- |
| `DpCi8` | `i: i8, q: i8` | 2 | — | SDR raw int8 IQ |
| `DpCi16` | `i: i16, q: i16` | 4 | — | SDR raw int16 IQ |
| `DpCi32` | `i: i32, q: i32` | 8 | — | SDR raw int32 IQ |
| `DpCf32` | `i: f32, q: f32` | 8 | `complex64` | Implements `From<Complex<f32>>` |

The C DSP API uses `float complex` and `double complex` directly (C99
types) rather than `i/q` structs. `DpCf32` is layout-compatible with
`float complex` and `numpy.complex64`.

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

The ring buffer API (`buffer/buffer.h`) is element-typed, not sample-typed.
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
| `lo/lo_core.h` | — | `float complex` | LO + 2¹⁶-entry LUT → CF32 phasors |
| `nco/nco_core.h` | — | `uint32_t` | Raw phase accumulator |
| `fir/fir_core.h` | `float complex` | `float complex` | Real or complex taps |
| `fft/fft_core.h` | `double complex`, `float complex` | same | C99 complex types |
| `fft2d/fft2d_core.h` | `double complex`, `float complex` | same | 2-D FFT |
| `Resampler/Resampler_core.h` | `float complex` | `float complex` | Polyphase resampler |
| `HalfbandDecimator/HalfbandDecimator_core.h` | `float complex` | `float complex` | Fixed 2:1 halfband |
| `ddc/ddc_core.h` | `float complex` | `float complex` | Complex DDC (`DDC`) |
| `ddc/ddc_core.h` | `float` | `float complex` | Real DDC (`DDCR`, Arch D2) |
| `acc_f32/acc_f32_core.h` | `float` | `float` | Running sum / dot product |
| `acc_cf64/acc_cf64_core.h` | `double complex` | `double complex` | Complex accumulator (I&D) |
| `delay/delay_core.h` | `double complex` | `double complex` | Polyphase tap delay |
| `dp/buffer.h` | `float`, `double`, `int16_t` | same | Scalar ring buffer elements |
| `stream/stream.h` | all `dp_sample_type_t` wire types | all types | Wire type set at socket creation |

---

## Precision design rationale

Three levels of precision are used deliberately:

**`float _Complex` (CF32)** — default signal path.
Matches native SDR hardware output (RTL-SDR, HackRF, LimeSDR, PlutoSDR,
USRP).  SIMD-friendly: AVX-512 processes eight CF32 pairs per instruction.
Used by NCO, FIR, polyphase resampler, halfband decimator, DDC.

**`double _Complex` (CF64)** — spectral and accumulation paths.
The FFT backend (FFTW or pocketfft) works in `double _Complex` throughout.
The accumulator types use `double _Complex` to match.
Running sums over many taps accumulate in `double` to prevent rounding
error before rounding back to `float`.

**`int8_t` / `int16_t` / `int32_t`** — streaming wire types.
Real SDR hardware delivers quantized samples.  The streaming API carries
these as `dp_sample_type_t` values (`DP_CI8`, `DP_CI16`, `DP_CI32`).
Convert to `float complex` before processing with the DSP library.

Rule: *compute in the cheapest type that keeps the math clean.*

---

## Python equivalents

| C type | NumPy dtype | Notes |
| ------ | ----------- | ----- |
| `float complex` | `np.complex64` | memory layout identical |
| `double complex` | `np.complex128` | memory layout identical |
| `uint32_t` | `np.uint32` | NCO phase |
| `uint8_t` | `np.uint8` | NCO carry/overflow flag |
| `float` | `np.float32` | f32 accumulator, tap coefficients |
| `DP_CI8` wire type | — | streaming only; convert before DSP |
| `DP_CI16` wire type | — | streaming only; convert before DSP |
| `DP_CI32` wire type | — | streaming only; convert before DSP |

---

## Rust equivalents

| C type | Rust type | Notes |
| ------ | --------- | ----- |
| `float complex` | `types::DpCf32` | `#[repr(C)]` `{i: f32, q: f32}`; `From<Complex<f32>>` |
| `double complex` | `num_complex::Complex64` | same layout, no wrapper needed |
| IQ wire types | `types::DpCi8`, `DpCi16`, `DpCi32` | `#[repr(C)]` structs for FFI boundary |
| `uint32_t` | `u32` | NCO phase accumulator |

---

## See also

- [just-makeit — State Variable Types](https://just-buildit.github.io/just-makeit/types/) —
  maps these C types to `--state name:type` scaffold tokens and generated
  NumPy stubs.
