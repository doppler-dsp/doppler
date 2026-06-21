# Getting Started with Fixed-Point Arithmetic

Fixed-point arithmetic is floating-point with the decimal point nailed to one
place. You trade dynamic range for speed, determinism, and hardware
compatibility. This guide builds intuition from first principles, then shows
every concept applied with `doppler.arith`.

______________________________________________________________________

## 1. The implied binary point

A binary integer has bits with place values 2⁰, 2¹, 2², …. Fixed-point
shifts those place values left or right by agreeing — in software — that a
certain bit position is "the one's place." The agreement is implicit: the
hardware stores plain integers; your code knows the scale.

```
Bit index:  7   6   5   4   3   2   1   0
Place value: 128  64  32  16   8   4   2   1    ← ordinary int8_t

Bit index:  7   6   5   4   3   2   1   0
Place value:  1  ½  ¼  ⅛  1/16 …              ← if the point sits ABOVE bit 7
             ↑
          integer part is 0 bits wide;
          all bits are fractional
```

The **Q-format notation** makes the agreement explicit.

______________________________________________________________________

## 2. Q notation — Qm.n

**Qm.n** means: `m` bits for the signed integer part (including the sign bit),
`n` bits for the fractional part, stored in an (m+n)-bit two's complement
integer.

| Format      | Stored type | Integer bits | Fraction bits | Range             | Resolution (LSB)   |
| ----------- | ----------- | :----------: | :-----------: | ----------------- | ------------------ |
| Q8 (Q1.7)   | `int8_t`    |      1       |       7       | `[−1, +1 − 2⁻⁷]`  | 2⁻⁷ ≈ 0.0078       |
| Q15 (Q1.15) | `int16_t`   |      1       |      15       | `[−1, +1 − 2⁻¹⁵]` | 2⁻¹⁵ ≈ 3.05 × 10⁻⁵ |

The real value of a raw integer `k` stored in Qm.n is:

```
real_value = k × 2⁻ⁿ
```

So `k = 16384` in Q15 represents `16384 × 2⁻¹⁵ = 0.5`.

```python
import numpy as np
from doppler.arith import add_q15

# Encode 0.5 as Q15
half_q15 = np.int16(16384)     # 0.5 × 32768

# Verify: raw integer divided by scale
print(half_q15 / 32768)        # 0.5
```

!!! note "Why not reach +1?"

    Two's complement gives 2ⁿ negative values but only 2ⁿ − 1 non-negative
    values. `int16_t` covers −32768 to +32767, so Q15 spans `[−1, +32767/32768]`.
    The missing upper bound is a fundamental property of the representation, not
    a bug.

______________________________________________________________________

## 3. Scaling — converting float to fixed-point

To encode a real value `x` in Qm.n, multiply by 2ⁿ and round:

```
k = round(x × 2ⁿ)
```

To decode back to float, divide by 2ⁿ:

```
x = k / 2ⁿ
```

```python
# Float → Q15 → float round-trip
x = 0.707           # -3 dBFS
scale = 32768       # 2^15

k = np.int16(round(x * scale))     # 23170
recovered = k / scale               # 0.706970... (quantisation error < 1 LSB)

# Float → Q8 → float
scale8 = 128        # 2^7
k8 = np.int8(round(x * scale8))    # 91
recovered8 = k8 / scale8           # 0.7109...  (larger error — 7 fraction bits)
```

The difference between `x` and `recovered` is the **quantisation error**,
bounded by ½ LSB when rounding (one full LSB when truncating).

______________________________________________________________________

## 4. Range and representable values

The representable range of a Qm.n integer type is determined entirely by the
integer range of the storage type.

| Format | Storage   |   Integer range   |       Real range       |
| ------ | --------- | :---------------: | :--------------------: |
| Q8     | `int8_t`  |   `[−128, 127]`   |   `[−1.0, +127/128]`   |
| Q15    | `int16_t` | `[−32768, 32767]` | `[−1.0, +32767/32768]` |

Any real value outside this range cannot be represented — it overflows.

______________________________________________________________________

## 5. Overflow vs saturation

**Overflow** happens when an arithmetic result exceeds the storage type's range.
In two's complement the bits wrap: the result "rolls over" and the sign can flip
without warning.

```
Q15 example: 0.75 + 0.75 = 1.5  (out of range)

  0.75 = 0x6000 =  24576
+ 0.75 = 0x6000 =  24576
────────────────────────
         0xC000 = -16384  ← int16 overflow!  Wrong sign.
```

**Saturation** clamps the result at the representable limits instead:

```
0.75 + 0.75  →  saturate  →  0x7FFF = 32767 = +0.9999...
```

`doppler.arith` saturates all add, subtract, multiply, and shift operations.

```python
import numpy as np
from doppler.arith import add_q15

Q15_MAX = np.int16(32767)

a = np.array([Q15_MAX], dtype=np.int16)
b = np.array([1],       dtype=np.int16)

print(add_q15(a, b)[0])   # 32767 — clamped, not wrapped
```

!!! tip "When NOT to saturate"

    Integrating accumulators in CIC filters intentionally rely on two's
    complement wrap-around: the overflow at each integrator cancels in the
    subsequent comb section. `AccQ15` and `AccQ8` use a wider integer
    accumulator (int64_t and int32_t respectively) instead of Q-format
    saturation, so the partial sums stay exact until you `dump()` them.

______________________________________________________________________

## 6. Bit growth on arithmetic operations

Every arithmetic operation changes the number of bits you need to represent
the exact result.

### 6a. Addition and subtraction

Adding two n-bit numbers requires n+1 bits to hold every possible result
without overflow. In general, adding k numbers of n bits each requires
n + ⌈log₂ k⌉ bits.

```
Example: adding 4 Q15 values

  Each value fits in 16 bits.
  Exact sum needs 16 + ⌈log₂ 4⌉ = 16 + 2 = 18 bits.
  Accumulating 1024 Q15 values needs 16 + 10 = 26 bits.
  Accumulating 2³² values needs 16 + 32 = 48 bits (fits in int64).
```

```python
from doppler.arith import AccQ15

acc = AccQ15()                          # int64_t internal accumulator
data = np.ones(1024, dtype=np.int16) * 32767   # all at full scale
acc.steps(data)
print(acc.dump())   # 33,553,408 — far outside int16 range, exact in int64
```

### 6b. Multiplication

Multiplying a Qm.n value by a Qp.q value produces a Q(m+p).(n+q) result.
The product needs m+n+p+q bits for the integer part AND the fraction part.

```
Q15 × Q15 → Q1.15 × Q1.15 → Q2.30

  Each operand: 1 sign + 15 fraction = 16 bits
  Product:      2 sign + 30 fraction = 32 bits (but stored in int32 or int64)
  To return to Q15: shift right 15, saturate to int16
```

The 15-bit right shift is what `mul_q15` does after rounding:

```python
from doppler.arith import mul_q15

half = np.array([16384], dtype=np.int16)    # 0.5 in Q15
quarter = mul_q15(half, half)               # 0.5 × 0.5 = 0.25
print(quarter[0])                           # 8192  (= 0.25 × 32768)
print(quarter[0] / 32768)                   # 0.25
```

### 6c. The dot product

`dot_q15(a, b)` returns the **raw Q30 accumulation** as int64 — no shift, no
saturation. This lets you inspect the full precision before deciding how to
normalise.

```python
from doppler.arith import dot_q15, shr_i64

a = np.full(4, 16384, dtype=np.int16)   # [0.5, 0.5, 0.5, 0.5]
b = np.full(4, 16384, dtype=np.int16)

raw = dot_q15(a, b)          # Q30: 4 × (0.5 × 0.5 × 32768²) = 536_870_912
print(raw)                   # 536870912

# Normalise to Q15: shift right 15
q15_scalar = shr_i64(np.array([raw], dtype=np.int64), 15)
print(q15_scalar[0])         # 16384  (= 1.0 in Q15 — correct: Σ 0.5² = 1.0)
```

______________________________________________________________________

## 7. Required accumulator width (no precision loss)

The accumulator must be wide enough to hold the sum of all products before any
normalising shift.

| Operation     | Operand type | Product width | Max terms | Min accumulator |
| ------------- | ------------ | :-----------: | :-------: | :-------------: |
| `dot_q15`     | Q15 (int16)  |  Q30 (int32)  |   ≤ 2³³   |      int64      |
| `dot_q8`      | Q8 (int8)    |  Q14 (int16)  |   ≤ 2¹⁷   |      int32      |
| `AccQ15.madd` | Q15          |      Q30      |   ≤ 2³³   |      int64      |
| `AccQ8.madd`  | Q8           |      Q14      |   ≤ 2¹⁷   |      int32      |

The guard-bits formula:

```
accumulator bits needed = product bits + ⌈log₂(max_terms)⌉

For dot_q15 with n=1024:
  product bits = 30 (Q15×Q15 → Q30, fits in int32)
  guard bits   = ⌈log₂ 1024⌉ = 10
  total needed = 40 bits → use int64 (64 bits — plenty of headroom)

For dot_q8 with n=1024:
  product bits = 14 (Q8×Q8 → Q14, fits in int16)
  guard bits   = 10
  total needed = 24 bits → use int32 (32 bits)
```

______________________________________________________________________

## 8. Truncation vs rounding

After a multiply or shift you must decide how to discard the fractional bits
you are throwing away.

| Mode                       | Rule                  |    Error range     |      Bias       |
| -------------------------- | --------------------- | :----------------: | :-------------: |
| Truncation (floor)         | `x >> n`              |    \[−1 LSB, 0)    |    negative     |
| Round half-up              | `(x + 2^(n−1)) >> n`  | `[−½ LSB, +½ LSB]` | slight positive |
| Round half-even (banker's) | round to nearest even | `[−½ LSB, +½ LSB]` |      none       |

`doppler.arith` uses **round-half-up** throughout: the bias of ½ LSB is added
before the shift. This is the standard choice for DSP and matches the
behaviour of most hardware DSPs.

```
Q15 multiply, round-half-up:

  raw product = a × b                      (int32, Q30)
  bias        = 2^14 = 16384
  rounded     = (raw + bias) >> 15         (int16, Q15)
```

Truncation is faster but introduces a systematic downward bias on all results.
For a FIR filter this shifts the DC gain by ~−0.5 LSB per output sample.

```python
from doppler.arith import shr_q15

# Truncation (floor): 3/4 is exactly between two Q15 values → floor gives 0.5
a = np.array([24576], dtype=np.int16)  # 0.75 in Q15

# shr_q15 uses round-half-up: (3/4) >> 1 → (24576 + 16384) >> 1 = 20480
print(shr_q15(a, 1)[0])                # 20480 / 32768 = 0.625  (= 0.75/2, rounded)

# Manual truncation (C right-shift behaviour):
print(np.int16(np.int32(24576) >> 1))  # 12288 / 32768 = 0.375  (biased low)
```

______________________________________________________________________

## 9. Saturation arithmetic — a closer look

Saturation replaces the modular wrap of two's complement with a clamp. Every
`add`, `sub`, `mul`, and `shl` in `doppler.arith` saturates.

### Why shifts can overflow too

A left shift multiplies by a power of two. Shifting a Q15 value left by 1
doubles it, which can overflow:

```python
from doppler.arith import shl_q15

a = np.array([24576, -24576], dtype=np.int16)   # ±0.75

# Without saturation: 24576 << 1 = 49152 > 32767 → wraps to -16384
# With saturation:                            → clamps to 32767
print(shl_q15(a, 1))   # [ 32767, -32768]
```

### The sign-flip hazard

Without saturation, overflow is invisible — the number *looks* valid but has
the wrong sign. Saturation makes clipping audible/visible without the far worse
artefact of a full sign flip.

______________________________________________________________________

## 10. Working with the doppler.arith API

### Module-level functions (stateless)

All stateless operations take NumPy arrays and return a new array (or scalar).

```python
import numpy as np
from doppler.arith import (
    add_q15, sub_q15, mul_q15, dot_q15,
    shl_q15, shr_q15,
    shr_i64,
)

# --- Q15 example: FIR output sample ---
# Compute one output sample of a symmetric FIR with coefficient vector h
# (Q15) applied to a delay line x (Q15).
def fir_sample(x: np.ndarray, h: np.ndarray) -> np.int16:
    """Compute Σ h[k]·x[k] in Q15 with full precision accumulation."""
    raw_q30 = dot_q15(x, h)                    # int64, Q30
    q30_arr = np.array([raw_q30], dtype=np.int64)
    q15_arr = shr_i64(q30_arr, 15)             # normalise to Q15
    # Saturate to int16 range
    return np.int16(min(max(q15_arr[0], -32768), 32767))

h = np.array([4096, 8192, 16384, 8192, 4096], dtype=np.int16)  # symmetric
x = np.array([8192, 8192, 16384, 8192, 4096], dtype=np.int16)  # delay line
y = fir_sample(x, h)
print(y, '→', y / 32768)
```

### Stateful accumulators

`AccQ15` and `AccQ8` maintain a running sum across calls. Use `madd` for
multiply-accumulate (the MAC operation at the heart of every FIR filter).

```python
from doppler.arith import AccQ15

# --- Running MAC: feed one block at a time ---
mac = AccQ15()

for block in blocks:                    # streaming blocks of int16
    coeffs = get_coeffs_for_block()     # matched filter
    mac.madd(block, coeffs)             # acc += Σ block[i] × coeffs[i]

raw_sum = mac.dump()                    # int64, Q30; resets accumulator

# Normalise to Q15 scalar
q15_out = np.int16(np.clip(
    (raw_sum + (1 << 14)) >> 15, -32768, 32767
))
```

### Q8 vs Q15 — when to choose which

| Criterion                  | Q8 (int8)                              | Q15 (int16)                |
| -------------------------- | -------------------------------------- | -------------------------- |
| Resolution                 | 7 fraction bits (~0.8% of full scale)  | 15 fraction bits (~0.003%) |
| SQNR (thermal noise limit) | ~48 dB                                 | ~96 dB                     |
| Memory / bandwidth         | 1 byte/sample                          | 2 bytes/sample             |
| AVX2 throughput            | 32 int8 per register                   | 16 int16 per register      |
| Typical use                | rough quantization, neural-net weights | audio, SDR, precision DSP  |

A 6 dB/bit rule of thumb: each additional bit buys ~6 dB of
signal-to-quantisation-noise ratio (SQNR).

______________________________________________________________________

## 11. Summary: the five rules

1. **Know your Q.** Track the binary point through every operation. If two
    operands have different Q, align them before adding.

1. **Widen on multiply.** Q15 × Q15 = Q30. Keep the product in at least
    int32 before shifting back. Use int64 for accumulators.

1. **Round, don't truncate.** Add `2^(n−1)` before the right-shift to get
    round-half-up. It costs one addition; it buys unbiased output.

1. **Saturate at the output.** Carry full precision through the pipeline; only
    clamp at the final output word. Intermediate saturation destroys precision.

1. **Size your accumulator.** Accumulating k products of n-bit width needs
    n + ⌈log₂ k⌉ bits. For k = 1024 Q15 products that is 26 bits — int32 is
    tight; int64 is safe.

______________________________________________________________________

## 12. Further reading

- `doppler.arith` module — `add_q15`, `sub_q15`, `mul_q15`, `dot_q15`,
    `shl_q15`, `shr_q15`, `AccQ15`, `AccQ8`, and Q8 counterparts.
- [Quantization design note](../design/QUANTIZATION.md) — doppler's encoding
    conventions for the `cvt` module (Q15 ↔ float, UQ15, I16).
- [ADC gallery walkthrough](../gallery/adc.md) — 3–8 bit quantisation noise
    visualised: time-domain staircase and 6 dB/bit noise-floor descent.
- [HBDecimQ15 gallery walkthrough](../gallery/hbdecim_q15.md) — Q15 halfband
    decimator: the `_mm256_madd_epi16` inner loop and symmetric-fold trick.
