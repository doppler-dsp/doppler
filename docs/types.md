# Type System

doppler uses C99 standard types throughout — no custom struct wrappers for
complex samples in the DSP path. Every public API takes and returns types
exactly as the C99 standard defines them.

______________________________________________________________________

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
    `{f64 i, f64 q}`) that mirror the C ABI exactly. Both implement
    `From<Complex<f32>>` / `From<Complex<f64>>` for zero-cost conversion
    to and from `num_complex`:

    ```rust
    let c: Complex<f32> = DpCf32 { i: 1.0, q: 0.0 }.into();
    let ffi: DpCf64 = Complex::new(0.0_f64, 1.0_f64).into();
    ```

Aliases (CF32, CI16, etc.) are shorthand used in documentation and the
streaming API (`CF32`, `CI16`, …). They are not typedefs — the C
API always spells the full C99 type.

Complex integers (CI8 / CI16 / CI32) are passed as interleaved arrays:
`input[2*k]` = I, `input[2*k+1]` = Q, with `num_samples` counting complex
pairs. The Rust FFI uses `#[repr(C)]` structs (`DpCi8`, `DpCi16`,
`DpCi32`) at the boundary; the C side uses the plain integer pointer.

______________________________________________________________________

## Creating complex constants

```c
#include <complex.h>
#include <stdio.h>

int main(void)
{
  float _Complex tone = 0.5f + 0.866f * I;   /* e^{iπ/3} */
  double _Complex dc  = 1.0 + 0.0 * I;

  printf("tone = %.3f%+.3fi\n", crealf(tone), cimagf(tone));
  printf("dc   = %.3f%+.3fi\n", creal(dc), cimag(dc));
  return 0;
}
```

______________________________________________________________________

## Which type goes where

| Module                   | Input                               | Output / state         |
| ------------------------ | ----------------------------------- | ---------------------- |
| `lo`                     | —                                   | `float _Complex`       |
| `nco`                    | —                                   | `uint32_t`             |
| `fir`                    | `float _Complex`                    | `float _Complex`       |
| `fft`, `fft2d`           | `float _Complex`, `double _Complex` | same                   |
| `corr`, `corr2d`         | `float _Complex`                    | `double _Complex`      |
| `detector`, `detector2d` | `float _Complex`                    | detections             |
| `ddc` (complex)          | `float _Complex`                    | `float _Complex`       |
| `ddc` (real)             | `float`                             | `float _Complex`       |
| `resampler`              | `float _Complex`                    | `float _Complex`       |
| `halfband_decimator`     | `float _Complex`                    | `float _Complex`       |
| `acc_f32`                | `float`                             | `float`                |
| `acc_cf64`               | `double _Complex`                   | `double _Complex`      |
| `delay`                  | `double _Complex`                   | `double _Complex`      |
| `buffer`                 | `float`, `double`, `int16_t`        | same (scalar elements) |
| `stream`                 | any `dp_sample_type_t` wire type    | same                   |

______________________________________________________________________

## Precision design rationale

**CF32 (`float _Complex`)** — default signal path. Matches native SDR
hardware output (RTL-SDR, HackRF, LimeSDR, PlutoSDR, USRP).
SIMD-friendly: AVX-512 processes eight CF32 pairs per instruction.

**CF64 (`double _Complex`)** — spectral and accumulation paths. The FFT
backend (pocketfft) works in `double _Complex` throughout. Running sums
accumulate in `double` to prevent rounding error before rounding back to
`float`.

**CI8 / CI16 / CI32** — streaming wire types. Real SDR hardware delivers
quantized samples; convert to CF32 before processing with the DSP library.

Rule: *compute in the cheapest type that keeps the math clean.*

______________________________________________________________________

## Quantization schemes

The `cvt` module converts between CF32 and fixed-point integer formats.
All formats derive from **Q15** (15-bit signed fractional, Δ = 2⁻¹⁵).

<figure markdown>
| Scheme | Container  | `0.0` code | Description |
|--------|------------|------------|-------------|
| Q15    | `int16_t`  | `0x0000`   | Bipolar two's-complement |
| I16U32 | `uint32_t` | `0x00000000` | Q15 zero-extended to 32 bits |
| I16U64 | `uint64_t` | `0x0000000000000000` | Q15 zero-extended to 64 bits |
| UQ15   | `uint16_t` | `0x8000`   | Offset-binary (0.0 → 32768) |
| UQ16   | `uint64_t` | `0x0000000000008000` | UQ15 in uint64 — CIC pipeline format |
<figcaption>Quantization schemes — all derived from Q15</figcaption>
</figure>

See [Quantization Design](design/QUANTIZATION.md) for encoding formulas,
C99 cast semantics, and the CIC headroom budget.

______________________________________________________________________

## Reading interleaved I/Q in Python

`wavegen` / `wfmgen` write **interleaved** I/Q (`I Q I Q …`) in the chosen
`--sample-type`. A naive `np.fromfile` gets the layout wrong — and for the
integer types, the scale too — so it's worth knowing what each type costs:

| `--sample-type`         | NumPy           | natural form                                      | cost                    |
| ----------------------- | --------------- | ------------------------------------------------- | ----------------------- |
| `cf32`                  | `np.complex64`  | complex **view** (interleaved f32 *is* complex64) | zero-copy               |
| `cf64`                  | `np.complex128` | complex **view**                                  | zero-copy               |
| `ci8` / `ci16` / `ci32` | `np.int8/16/32` | full-scale ints; **no** complex-int dtype         | copy to rescale to ±1.0 |

There is no complex-integer dtype, so integer captures deinterleave and rescale
to `complex64` (±1.0) on read; the float types are already the memory layout of
a complex array. [`Reader`](api/python-wfmgen.md) does the conversion in C — any
wire type to unit-scale `complex64` — and auto-detects the container (BLUE /
SigMF / CSV / raw), so it also recovers `fs`/`fc`/sample-type from a
self-describing header:

<!-- docs-snippet: skip=illustrative: reads an I/Q capture file you supply -->

```python
from doppler.wfm import Reader

# headerless raw: pass the on-disk sample_type as a hint
with Reader("capture.iq", sample_type="ci16") as r:
    iq = r.read(r.num_samples)                     # complex64, rescaled to ±1.0

# self-describing container: sample_type comes from the metadata
with Reader("capture.blue") as r:
    iq = r.read(r.num_samples)

# the float types are also a plain reinterpretation, no reader needed:
iq = np.fromfile("capture.iq", dtype="<c8")        # cf32 → complex64
iq = np.memmap("huge.iq", dtype="<c8", mode="r")   # zero-copy view of a big capture
```

`Reader` uses the writer's exact full-scale (`2³¹−1 / 32767 / 127`), so
`generate → Reader.read` is bit-faithful.

______________________________________________________________________

## See also

- [just-makeit — State Variable Types](https://just-buildit.github.io/just-makeit/types/) —
    maps these C types to `--state name:type` scaffold tokens and generated
    NumPy stubs.
