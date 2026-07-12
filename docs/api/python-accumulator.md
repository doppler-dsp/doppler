# Python Accumulator API

Running-sum accumulators backed by `dp_acc_f32_t` and `dp_acc_cf64_t`.
Used as the integrate-and-dump register in polyphase resamplers.

Source:
[`src/doppler/accumulator/__init__.py`](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/accumulator/__init__.py)

______________________________________________________________________

## Classes

| Class      | Accumulator type  | Coefficient type | Use when                            |
| ---------- | ----------------- | ---------------- | ----------------------------------- |
| `AccF32`   | float32 scalar    | float32          | real-valued sums, power estimation  |
| `AccCf64`  | complex128 scalar | float32          | polyphase resampler I&D path        |
| `AccTrace` | float64 per-bin   | —                | trace averaging (mean / EMA / hold) |

`AccTrace` differs from the scalar accumulators above: it keeps one running
value **per bin** over a fixed-length frame rather than reducing the frame to a
single sum. Choose the reduction with `mode` — `"mean"` (linear average),
`"exp"` (exponential moving average using `alpha`), `"maxhold"`, or
`"minhold"`. It is the averaging engine behind
[`doppler.spectral.PSD`](python-spectral.md), and is reusable for waterfall /
spectrogram and video-averaged displays.

______________________________________________________________________

## Examples

### AccF32 — running sum

```python
from doppler.accumulator import AccF32

acc = AccF32()
acc.step(1.0)
acc.step(2.5)
print(acc.get())    # 3.5 — read without clearing
print(acc.dump())   # 3.5 — read and zero
print(acc.dump())   # 0.0 — cleared by previous dump
```

### AccF32 — multiply-accumulate (dot product)

`madd(x, h)` computes `acc += sum(x[k] * h[k])` in C — the inner loop
of a polyphase FIR branch.

```python
from doppler.accumulator import AccF32
import numpy as np

x = np.array([1, 2, 3, 4], dtype=np.float32)
h = np.array([0.25, 0.25, 0.25, 0.25], dtype=np.float32)

acc = AccF32()
acc.madd(x, h)
print(acc.dump())   # 2.5 = mean([1,2,3,4])
```

### AccCf64 — complex accumulator for resampler I&D

```python
from doppler.accumulator import AccCf64
import numpy as np

x = np.array([1+2j, 3+4j], dtype=np.complex128)
h = np.array([0.5, 0.5], dtype=np.float32)

acc = AccCf64()
acc.madd(x, h)
print(acc.dump())   # (2+3j) = mean([1+2j, 3+4j])
```

### steps vs step

- `step(v)` — add a scalar to the accumulator.
- `steps(x)` — add all elements of a NumPy array to the accumulator.

```python
acc = AccF32()
acc.step(1.0)           # acc = 1.0
acc.steps(np.array([2.0, 3.0], dtype=np.float32))  # acc = 6.0
```

### AccTrace — per-bin trace averaging

```python
from doppler.accumulator import AccTrace
import numpy as np

# Linear (PSD (PSD-method)) average of two power frames, per bin.
acc = AccTrace(n=4, mode="mean")
acc.accumulate(np.array([1, 3, 5, 7], dtype=np.float32))
acc.accumulate(np.array([3, 5, 7, 9], dtype=np.float32))
print(acc.value())   # [2. 4. 6. 8.]
print(acc.count)     # 2

# Max-hold catches per-bin transients across frames.
mh = AccTrace(n=3, mode="maxhold")
mh.accumulate(np.array([1, 5, 2], dtype=np.float32))
mh.accumulate(np.array([4, 3, 6], dtype=np.float32))
print(mh.value())    # [4. 5. 6.]
```

`value()` returns `None` until the first frame is accumulated.

______________________________________________________________________

::: doppler.accumulator.AccF32

______________________________________________________________________

::: doppler.accumulator.AccCf64

______________________________________________________________________

::: doppler.accumulator.AccTrace

## Related pages

<!-- related-pages:start -->

**Gallery** — [Gallery](../gallery/index.md), [Four WCDMA Carriers — `PSD`, `band_power`, `AccTrace`](../gallery/wcdma-carriers.md)
**Design** — [API taxonomy: the DSP building-block hierarchy and its naming axis](../design/api-taxonomy.md), [State Serialization — the standard bytes interface](../design/state-serialization.md)

<!-- related-pages:end -->
