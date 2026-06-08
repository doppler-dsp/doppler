# cvt Quantization Noise

![cvt quantization noise comparison](../assets/cvt_quantization_demo.png)

## What you're seeing

A multi-tone CF32 signal spanning 80 dB of dynamic range is passed through
all three `doppler.cvt` encoder/decoder pairs and the results are compared.

**Top — Input signal.** Five complex tones at 0, −20, −40, −60, and
−80 dBFS. The −60 dBFS tone is at a negative frequency (−0.31
cycles/sample) and only visible in the full complex spectrum.

**Middle — Quantised output (overlaid).** All three converters (I16,
I16U32, I16U64) are plotted. The traces are identical: every converter
uses the same Q15 quantization step (Δ = 2⁻¹⁵ ≈ 3.05 × 10⁻⁵), so the
quantization noise floor is the same regardless of the container width.
The −80 dBFS tone sits right at the Q15 noise floor (~−92 dBFS).

**Bottom — Quantisation error spectrum.** `xq − x` for each converter.
White quantization noise floor at ~−92 dBFS confirms Q15 SNR theory.
The tones themselves are absent from the error spectrum — only
quantization noise remains.

## The three cvt formats

| Format                        | Container  | Q15 bits             | Extra bits | Use                     |
| ----------------------------- | ---------- | -------------------- | ---------- | ----------------------- |
| `F32ToI16` / `I16ToF32`       | `int16_t`  | 15+sign              | —          | Audio, signed buffers   |
| `F32ToI16U32` / `I16U32ToF32` | `uint32_t` | 15+sign in bits 15:0 | 16 zero    | Legacy 32-bit pipelines |
| `F32ToI16U64` / `I16U64ToF32` | `uint64_t` | 15+sign in bits 15:0 | 48 zero    | CIC integrator input    |

All three carry the same Q15 bit pattern — the container width determines
downstream integer headroom, not quantization error. The CIC decimator
consumes `I16U64` directly: the 48 upper bits absorb up to `N·log₂(R)` = 48
bits of pipeline gain before overflow.

The encoders expose a sticky `.clipped` property that reads `True` if any
sample has been saturated since the last `reset()`:

```python
from doppler.cvt import F32ToI16, I16ToF32
import numpy as np

# Encode float32 → int16 (Q15)
enc = F32ToI16()
x = np.array([0.5, 0.8, -0.3, -0.9], dtype=np.float32)
q = enc.steps(x)                   # q.dtype == int16
print(enc.clipped)                  # False — all values within (-1, 1)

# Decode int16 → float32
dec = I16ToF32()
x_hat = dec.steps(q)               # x_hat.dtype == float32
print(np.max(np.abs(x - x_hat)))   # ~1.5e-5 (Q15 step size)

# sticky clipped flag
enc2 = F32ToI16()
enc2.steps(np.array([0.5, 1.2, -0.3], dtype=np.float32))
print(enc2.clipped)                 # True — 1.2 exceeded full scale
enc2.reset()
print(enc2.clipped)                 # False
```

```bash
python examples/python/cvt_quantization_demo.py   # → cvt_quantization_demo.png
```

See [`docs/design/QUANTIZATION.md`](../design/QUANTIZATION.md) for the
full storage-format specification and headroom budget analysis.
