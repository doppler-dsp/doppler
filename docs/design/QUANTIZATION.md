# Quantization Design

Doppler's signal chain begins with real hardware that delivers quantized
samples — typically 8, 12, or 16-bit IQ pairs. The `cvt` module converts
between the floating-point compute type (`float _Complex` / CF32) and the
integer wire types the hardware delivers. This document defines the
mathematical model, the specific encoding conventions, and the precision
budget for each format.

______________________________________________________________________

## 1. The signal model

All internal DSP operates on **CF32** (`float _Complex`): two 32-bit IEEE 754
single-precision floats per sample. A sample is written as:

```
x = xᵣ + j·xᵢ,    xᵣ, xᵢ ∈ ℝ,   |xᵣ| ≤ 1,  |xᵢ| ≤ 1
```

The full-scale convention is **±1.0 per component**. Each component is
clipped independently at the encoder. A unit-amplitude complex exponential
reaches full scale on each component at distinct times:

```
x[n] = e^{j2πfn} = cos(2πfn) + j·sin(2πfn)

|xᵣ[n]| ≤ 1,  |xᵢ[n]| ≤ 1,  but |x[n]| = 1 ∀n
```

______________________________________________________________________

## 2. C99 conversion semantics

Both quantization paths (signed and offset-binary) depend on a sequence of
numeric casts. Getting the cast chain right is not optional: an out-of-range
float-to-integer cast is **undefined behaviour** in C99, not a saturating
operation. This section documents what the standard guarantees — and what it
does not — for every cast pattern used in `cvt` and `cic_core`.

### 2.1 Float-to-integer casts (§6.3.1.4)

> If the value of the integral part cannot be represented by the integer type,
> the behaviour is **undefined**.

This applies regardless of whether the target is signed or unsigned.

| Cast          | In-range                       | Out-of-range                           |
| ------------- | ------------------------------ | -------------------------------------- |
| `(int16_t)f`  | Defined: truncates toward zero | **UB** for `f < -32768` or `f > 32767` |
| `(uint16_t)f` | Defined: truncates toward zero | **UB** for `f < 0` or `f > 65535`      |

**Consequence**: saturation of the float value to the target integer range must
occur *before* any float-to-integer cast. Compilers are permitted to assume
UB never happens and may emit code that produces arbitrary results without it.

### 2.2 Integer-to-integer conversions (§6.3.1.3)

Conversions between integer types have well-defined behaviour in all cases
when the target is unsigned. Signed targets may be implementation-defined for
out-of-range values.

| Cast                    | Guarantee               | Notes                                                                               |
| ----------------------- | ----------------------- | ----------------------------------------------------------------------------------- |
| `(int64_t)(int16_t)v`   | Always defined          | Sign-extends bit 15 into bits 16–63                                                 |
| `(uint64_t)(int64_t)v`  | Always defined          | Mod 2⁶⁴ (negative → large positive)                                                 |
| `(uint64_t)(uint16_t)v` | Always defined          | Zero-extends bits 16–63                                                             |
| `(uint16_t)(uint64_t)v` | Always defined          | Truncates to bits 15:0                                                              |
| `(int16_t)(uint16_t)v`  | Defined for `v ≤ 32767` | **Implementation-defined** for `v ≥ 32768` in C99; two's-complement mandated in C23 |

All C99 implementations targeting any platform relevant to `doppler` use
two's complement, so `(int16_t)0x8000 == -32768` in practice. C23 mandates
this universally (§6.2.6.2).

### 2.3 Integer arithmetic overflow (§6.5 / §6.2.5¶9)

| Operand type                         | Overflow                | Result                  |
| ------------------------------------ | ----------------------- | ----------------------- |
| Signed (`int32_t`, `int64_t`, …)     | **Undefined behaviour** | Do not rely on wrapping |
| Unsigned (`uint32_t`, `uint64_t`, …) | Defined                 | Wraps modulo 2ⁿ         |

The CIC integrators and comb stages use `uint64_t` arithmetic intentionally so
that intermediate overflow is defined and cancels exactly across the N stages.

### 2.4 Cast chains used in this codebase

**UQ16 encode (CIC encoder)**

```
float sr → [saturate to [-32768, 32767]] → (int16_t)sr
         → (int32_t)sr + 32768 → (uint64_t)
```

1. Saturation makes the `(int16_t)` cast defined (§6.3.1.4 safe zone).
1. Widen to `int32_t` before adding 32768: range `[-32768+32768, 32767+32768] = [0, 65535]` — no signed overflow (§6.5).
1. `(uint64_t)` from `int32_t` in `[0, 65535]`: always defined (§6.3.1.3).
    All inputs are non-negative; no sign extension; no signed integers anywhere
    in the hot path.

**UQ16 decode (CIC decoder)**

```
(uint64_t re >> shift) → (uint16_t) → float - 32768.0f → · (1/32768)
```

1. Right-shift of `uint64_t`: defined (logical, zero-fills high bits).
1. `uint64_t → uint16_t`: always defined; truncates to bits 15:0.
1. Subtract 32768.0f in floating-point: removes the offset-binary bias;
    no signed integer cast, no implementation-defined behaviour.

**UQ15 encode (offset-binary)**

```
float x → [saturate to [-32768, 32767]] → (int16_t)v
        → (int32_t)v + 32768 → (uint16_t) → (uint32_t or uint64_t)
```

1. Saturation makes the `(int16_t)` cast defined.
1. Widen to `int32_t` before adding 32768: the range `[-32768 + 32768, 32767 + 32768] = [0, 65535]` fits in `int32_t` with no signed overflow.
1. `(uint16_t)` cast from the in-range `int32_t` value `[0, 65535]`: always
    defined (§6.3.1.3, to unsigned).
1. Zero-extend into `uint32_t` / `uint64_t`: always defined.

______________________________________________________________________

## 3. Q15 — the quantization basis

Every integer format in `cvt` derives from a single quantization step:
**Q15 (signed 15-bit fractional, 1-bit sign)**.

### 3.1 Encoder

Given a real sample `x ∈ [-1, +1]`, the Q15 encoder is:

```
v = round(x · 2¹⁵),    v ∈ [-2¹⁵, 2¹⁵ - 1]
```

The input contract `|x| ≤ 1` guarantees the result fits in int16 without
overflow. The implementation saturates at the boundary as a guard against
out-of-range inputs, but saturation is not part of the mathematical model.

```
x = +1.0  →  v =  32767   (2¹⁵ - 1,  max positive)
x =  0.0  →  v =      0
x = -1.0  →  v = -32768   (-2¹⁵,     min negative)
```

The quantization step is `Δ = 2⁻¹⁵ ≈ 3.05 × 10⁻⁵`.

### 3.2 Decoder

The inverse maps `v ∈ [-32768, 32767]` back to `x ∈ [-1, 1 - Δ]`:

```
x̂ = v · 2⁻¹⁵
```

The maximum reconstruction error (half-step) is `Δ/2 ≈ 1.53 × 10⁻⁵`.

### 3.3 Quantization noise

For a broadband input uniformly distributed over `[-1, +1]`, the
quantization error `e = x̂ - x` is approximately uniform over
`[-Δ/2, +Δ/2]`. The theoretical signal-to-quantization-noise ratio
for a full-scale sinusoid is:

```
SNR_Q15 = 6.02 · 15 + 1.76 ≈ 92 dB
```

In practice, the error spectrum is **white** only when the input is
broadband or dithered. A narrowband tone produces periodic quantization
error, which appears as harmonics of the input frequency in the error
spectrum.

______________________________________________________________________

## 4. CF32 quantization — two independent channels

A CF32 sample `x = xᵣ + j·xᵢ` contains **two independent F32 values**.
Each channel is quantized separately using the same Q15 encoder:

```
vᵣ = round(xᵣ · 2¹⁵)
vᵢ = round(xᵢ · 2¹⁵)
```

The reconstruction is:

```
x̂ = vᵣ · 2⁻¹⁵ + j · vᵢ · 2⁻¹⁵
```

Because `xᵣ` and `xᵢ` are encoded identically and independently, the
noise floor is the same for both channels and the error is uncorrelated
between them.

______________________________________________________________________

## 5. Storage formats

The `cvt` module provides three integer storage formats for the same Q15
bit pattern, differentiated by the **container width**. All three
produce identical quantization noise; the choice of container is
determined by the downstream consumer.

### 5.1 `F32ToI16` — signed 16-bit (I16)

The Q15 value is stored directly as a **signed `int16_t`**:

```
container = (int16_t) v
```

Memory layout (2 bytes per sample):

```
 15  14  13  12  11  10   9   8   7   6   5   4   3   2   1   0
[sgn|                  magnitude bits                           ]
```

Use when the consumer expects a native signed integer (e.g., POSIX audio,
`int16_t *` buffers).

### 5.2 `F32ToI16U32` — Q15-in-uint32 (I16U32)

The Q15 bit pattern (16-bit two's-complement) is **zero-extended** into
the lower 16 bits of a **`uint32_t`**; the upper 16 bits are always zero:

```
container = (uint32_t)(uint16_t) v
```

Memory layout (4 bytes per sample):

```
 31      16 15  14  ...   1   0
[  0 … 0  |sgn|  magnitude   ]
```

Examples:

```
x = +1.0  →  v =  32767  →  uint32  0x00007FFF
x =  0.0  →  v =      0  →  uint32  0x00000000
x = -1.0  →  v = -32768  →  uint32  0x00008000
```

Note that `-1.0` maps to `0x00008000`, **not** `0xFFFF8000` — the upper
16 bits are zero-extended, not sign-extended. The CIC filter's integrator
cascade uses the upper bits as **accumulation headroom**; the zero upper
bits leave the full 48 bits available for bit-growth.

### 5.3 `F32ToI16U64` — Q15-in-uint64 (I16U64)

Identical to I16U32 but in a **`uint64_t`** container; bits 63:16 are
always zero:

```
container = (uint64_t)(uint16_t) v
```

Memory layout (8 bytes per sample):

```
 63           16 15  14  ...   1   0
[  0 …………… 0  |sgn|  magnitude   ]
```

Examples:

```
x = +1.0  →  uint64  0x0000000000007FFF
x =  0.0  →  uint64  0x0000000000000000
x = -1.0  →  uint64  0x0000000000008000
```

The CIC decimator (`resample.CIC`) consumes this format directly. Each
sample enters the first integrator stage; the 48 bits above the Q15
value absorb up to `N · log₂(R)` bits of bit-growth without overflow,
where `N = 4` (stages) and `R ≤ 4096` (decimation ratio), giving a
maximum bit-growth of 48 bits — exactly filling the 64-bit word.

______________________________________________________________________

## 6. Encoding comparison table

| Format | Container  | Range                       | `+1.0`           | `0.0`                | `-1.0`               |
| ------ | ---------- | --------------------------- | ---------------- | -------------------- | -------------------- |
| I16    | `int16_t`  | −32768 … 32767              | `0x7FFF`         | `0x0000`             | `0x8000`             |
| I16U32 | `uint32_t` | 0 … 0x00007FFF / 0x00008000 | `0x00007FFF`     | `0x00000000`         | `0x00008000`         |
| I16U64 | `uint64_t` | 0 … 0x7FFF / 0x8000         | `0x000000007FFF` | `0x0000000000000000` | `0x0000000000008000` |

All three are **bipolar**: the zero input maps to the zero code. The
sign information is preserved in bit 15 of the container in every format.

______________________________________________________________________

## 7. Bipolar vs. unipolar (offset binary)

The formats above are all **bipolar** (two's-complement): `0.0 → 0`.

Some hardware and integer pipelines require **unipolar** (offset binary):
`0.0 → 2¹⁵` so that the output swings between `0` and `2¹⁶ - 1`:

```
v_unipolar = v_Q15 + 32768
```

```
x = +1.0  →  65535   (2¹⁶ - 1,  positive full scale)
x =  0.0  →  32768   (2¹⁵,      mid-scale)
x = -1.0  →      0   (zero,      negative full scale)
```

The inverse is:

```
x̂ = (v_unipolar - 32768) · 2⁻¹⁵
```

Conversion between the two is an integer add/subtract — no precision is
lost.

### 7.1 UQ15 — offset-binary in uint16

When the offset-binary value is stored in a `uint16_t` container, the
format is called **UQ15**:

```
encode: u = (uint16_t)((int32_t)v_Q15 + 32768),  u ∈ [0, 65535]
decode: x̂ = ((int32_t)u − 32768) · 2⁻¹⁵
```

| Input        | Q15 (`int16_t`) | UQ15 (`uint16_t`) |
| ------------ | --------------- | ----------------- |
| +32767/32768 | `0x7FFF`        | `0xFFFF` (65535)  |
| 0.0          | `0x0000`        | `0x8000` (32768)  |
| −1.0         | `0x8000`        | `0x0000` (0)      |

The `cvt` module provides `F32ToUQ15` / `UQ15ToF32` for this encoding.
For the CIC pipeline's UQ16 variant (same bias, stored in `uint64_t` with
48 bits of headroom) see §2.4 and §8.

______________________________________________________________________

## 8. Headroom budget for the CIC pipeline

The CIC decimator's integrators accumulate Q15 samples in `uint64_t`
accumulators using modular (wrapping) unsigned arithmetic. The unsigned
modular-arithmetic CIC property guarantees that every intermediate overflow
in the N integrator stages cancels exactly in the N comb stages, provided
the **true result** of each comb difference fits within 64 bits.

For N = 4 stages, M = 1 (one-sample comb delay), and R-to-1 decimation:

```
max comb output = max sum of R consecutive Q15 inputs
               ≤ 32768 · R                          (per comb stage)
               ≤ 32768 · 4096   for R_max = 4096
               = 2²⁷            (27 bits)
```

After N = 4 comb stages accumulate:

```
max output ≤ 32768 · R^N = 2¹⁵ · 2^(4·12) = 2¹⁵ · 2⁴⁸ = 2⁶³
```

This fits in 63 bits — within the 64-bit uint64 without ambiguity, so the
modular arithmetic gives exact results for all `R ≤ 4096`. The output
is right-shifted by `shift = N · log₂(R)` bits before conversion back to
CF32, restoring unit gain for DC.

______________________________________________________________________

## 9. Precision summary

| Stage                    | Precision               | Dynamic range   |
| ------------------------ | ----------------------- | --------------- |
| CF32 compute path        | 23-bit mantissa + sign  | ~138 dB         |
| Q15 quantization         | 15-bit magnitude + sign | ~92 dB          |
| CIC accumulator (uint64) | 15 + 48 bits headroom   | exact (modular) |
| CIC output (after shift) | Q15 (15 bits)           | ~92 dB          |

______________________________________________________________________

## See also

- [`docs/types.md`](../types.md) — CF32, CI16, and the full type table including
    the quantization scheme summary
- [`cic_core.h`](../c-api/cic__core_8h.md) — CIC integer pipeline
    implementation (UQ16 encode/decode)
- `native/inc/cvt/f32_to_uq15/` — F32ToUQ15 C header (UQ15 encode)
- `native/inc/cvt/uq15_to_f32/` — UQ15ToF32 C header (UQ15 decode)
- `examples/python/cvt_quantization_demo.py` — spectral comparison of all
    three bipolar quantization formats
