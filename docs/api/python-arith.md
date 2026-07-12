# Python Fixed-Point Arithmetic API

The `doppler.arith` module is the **saturating fixed-point** toolkit: elementwise
Q15 (1.15, `int16`) and Q8 (1.7, `int8`) add / subtract / multiply, fractional
dot products, saturating shifts, and small running accumulators. Every operation
**saturates** on overflow rather than wrapping, matching how fixed-point DSP
hardware and the rest of doppler's Q-format paths behave.

Source:
[`src/doppler/arith/__init__.py`](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/arith/__init__.py)

______________________________________________________________________

## Fixed-point formats

| Format | Storage | Range      | Resolution | One-liner                      |
| ------ | ------- | ---------- | ---------- | ------------------------------ |
| Q15    | `int16` | `[-1, +1)` | `2⁻¹⁵`     | `1.0 → 32767`, `-1.0 → -32768` |
| Q8     | `int8`  | `[-1, +1)` | `2⁻⁷`      | `1.0 → 127`, `-1.0 → -128`     |

A Q15 value `v` represents the real number `v / 32768`. Multiplication is the
fractional product (`(a·b) >> 15`), so `0.5 × 0.5 = 0.25`; addition saturates so
`0.5 + 0.5` reads the codec ceiling `32767` (≈ `+1.0`), never wrapping to a
negative.

______________________________________________________________________

## Examples

### Saturating Q15 elementwise ops

```python
import numpy as np
from doppler.arith import add_q15, mul_q15

a = np.array([16384, 16384, -32768], dtype=np.int16)   # 0.5, 0.5, -1.0
b = np.array([16384, 32767,  16384], dtype=np.int16)   # 0.5, ~1.0, 0.5

add_q15(a, b)    # array([32767, 32767, -16384])  -> 0.5+0.5 saturates to +1
mul_q15(a, b)    # array([ 8192, 16384, -16384])  -> 0.5*0.5 = 0.25
```

### Fractional dot product

`dot_q15` accumulates the Q15 products in a wide integer and returns a Python
`int`, so a long inner product never overflows mid-sum.

```python
from doppler.arith import dot_q15

taps   = np.array([ 8192,  8192,  8192,  8192], dtype=np.int16)  # 0.25 each
sample = np.array([32767, 16384, -16384, 0],    dtype=np.int16)
acc = dot_q15(taps, sample)     # wide integer accumulator (no overflow)
```

### Saturating shifts

```python
from doppler.arith import shl_q15, shr_q15

x = np.array([16384, -32768], dtype=np.int16)   # 0.5, -1.0
shl_q15(x, 1)   # array([32767, -32768]) -> 0.5<<1 saturates to +1, -1<<1 to -1
shr_q15(x, 2)   # array([ 4096,  -8192]) -> arithmetic right shift
```

### Running Q15 / Q8 accumulators

`AccQ15` / `AccQ8` keep a 64-bit running sum across calls — drive them
sample-by-sample (`step`), in bulk (`steps`), or as a multiply-accumulate
(`madd`), then read with `get()` (peek) or `dump()` (read-and-reset). The wide
register means a long stream cannot overflow.

```python
from doppler.arith import AccQ15

acc = AccQ15()
acc.steps(np.array([8192, 8192, 8192], dtype=np.int16))   # accumulate a block
acc.madd(np.array([16384], dtype=np.int16),               # acc += sum(a*b)
         np.array([16384], dtype=np.int16))
running = acc.get()      # peek without resetting
final   = acc.dump()     # read and reset to zero
```

______________________________________________________________________

## Accumulators

::: doppler.arith.AccQ15

::: doppler.arith.AccQ8

______________________________________________________________________

## Q15 operations (1.15, `int16`)

::: doppler.arith.add_q15

::: doppler.arith.sub_q15

::: doppler.arith.mul_q15

::: doppler.arith.dot_q15

::: doppler.arith.shl_q15

::: doppler.arith.shr_q15

______________________________________________________________________

## Q8 operations (1.7, `int8`)

::: doppler.arith.add_q8

::: doppler.arith.sub_q8

::: doppler.arith.mul_q8

::: doppler.arith.dot_q8

::: doppler.arith.shl_q8

::: doppler.arith.shr_q8

______________________________________________________________________

## 64-bit saturating shifts

::: doppler.arith.shl_i64

::: doppler.arith.shr_i64

## Related pages

<!-- related-pages:start -->

**Guides** — [Getting Started with Fixed-Point Arithmetic](../guide/fixed-point.md)
**Design** — [API taxonomy: the DSP building-block hierarchy and its naming axis](../design/api-taxonomy.md)

<!-- related-pages:end -->
