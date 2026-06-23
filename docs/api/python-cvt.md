# Python Type Converter API

The `doppler.cvt` module converts sample streams between **float32** and the
fixed-point / integer formats used at the edges of a DSP chain — ADC codes, Q15
fractions, Q15-in-wide-word for CIC — plus an ideal-quantiser **`ADC`** model for
characterisation. Every converter is a tiny stateful object with a `scale` (or
ADC depth) fixed at construction; `steps()` runs a whole block (optionally
in-place via `out=`), `step()` does one sample.

For the `F32To*` direction, `scale` is the gain applied before quantising
(`code = round(x · scale)`); the default `scale = 32768` maps the normalised
range `[-1, +1)` onto full-scale `int16` and saturates beyond it. The `*ToF32`
direction divides by the same `scale` to recover the float.

Source:
[`src/doppler/cvt/__init__.py`](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/cvt/__init__.py)

For the quantisation theory behind the scaling and the `UQ15` unsigned format,
see the [Quantization gallery page](../gallery/cvt-quantization.md).

______________________________________________________________________

## Converter map

| Class                         | In → Out         | Use                                         |
| ----------------------------- | ---------------- | ------------------------------------------- |
| `F32ToI16` / `I16ToF32`       | float32 ↔ int16  | signed Q15 / 16-bit PCM round-trip          |
| `I32ToF32`                    | int32 → float32  | 24/32-bit ADC codes to float                |
| `I8ToF32`                     | int8 → float32   | 8-bit codes to float                        |
| `F32ToUQ15` / `UQ15ToF32`     | float32 ↔ uint16 | **unsigned** Q15 (offset-binary) round-trip |
| `F32ToI16U32` / `I16U32ToF32` | float32 ↔ uint32 | one Q15 in the low 16 bits (CIC integer in) |
| `F32ToI16U64` / `I16U64ToF32` | float32 ↔ uint64 | one Q15 in the low 16 bits (CIC integer in) |
| `ADC`                         | float32 → int64  | ideal `bits`-bit quantiser (codes)          |

The `F32To*` directions expose a `clipped` flag that latches when an input
exceeded full scale during the last `steps()`.

______________________________________________________________________

## Examples

### float32 ↔ int16 round-trip

The default `scale = 32768` maps `[-1, +1)` to full-scale `int16`.

```python
import numpy as np
from doppler.cvt import F32ToI16, I16ToF32

x = np.array([0.0, 0.5, -1.0, 0.999], dtype=np.float32)

enc = F32ToI16()                # default scale = 32768
codes = enc.steps(x)            # array([0, 16384, -32768, 32735], dtype=int16)
if enc.clipped:                 # latches when an input exceeded full scale
    print("input exceeded full scale")

dec = I16ToF32()                # default scale = 32768
back = dec.steps(codes)         # ~= x  ([0.0, 0.5, -1.0, 0.999])
```

### In-place block conversion

Pass a pre-allocated `out=` to avoid an allocation in a hot loop.

```python
codes = np.empty(len(x), dtype=np.int16)
enc.steps(x, out=codes)
```

### Ideal ADC quantiser (characterisation)

`ADC(bits, dbfs, dithering)` models an ideal converter: a full-scale sine at
`dbfs=0.0` spans `±2**(bits-1)` codes. Feed its output straight into
[`ToneMeasure`](python-measure.md) to recover ENOB ≈ `bits`.

```python
from doppler.cvt import ADC

adc = ADC(12, 0.0, 0)                       # 12-bit, 0 dBFS, no dither
x = np.array([0.0, 0.5, 0.999, -1.0], dtype=np.float32)
adc.steps(x)                                # array([0, 1024, 2046, -2048])
```

### Unsigned Q15 (offset binary)

`F32ToUQ15` maps `[-1, 1)` onto `uint16` centred at `32768` (offset binary), the
convention used by many unsigned ADCs.

```python
from doppler.cvt import F32ToUQ15, UQ15ToF32

enc = F32ToUQ15()              # default scale = 32768
u = enc.steps(np.array([-1.0, 0.0, 0.999], dtype=np.float32))   # 0, 32768, 65503
back = UQ15ToF32().steps(u)    # ~= input
```

______________________________________________________________________

## Signed integer ↔ float

::: doppler.cvt.F32ToI16

::: doppler.cvt.I16ToF32

::: doppler.cvt.I32ToF32

::: doppler.cvt.I8ToF32

______________________________________________________________________

## Unsigned Q15 (offset binary)

::: doppler.cvt.F32ToUQ15

::: doppler.cvt.UQ15ToF32

______________________________________________________________________

## Q15 in a wide word (CIC integer input)

These pack a single saturated Q15 into the low 16 bits of a `uint32`/`uint64`
(upper bits zero) — the integer-input wire format the CIC filter expects, where
the headroom absorbs the bit-growth of the integrator cascade.

::: doppler.cvt.F32ToI16U32

::: doppler.cvt.I16U32ToF32

::: doppler.cvt.F32ToI16U64

::: doppler.cvt.I16U64ToF32

______________________________________________________________________

## Ideal ADC model

::: doppler.cvt.ADC
