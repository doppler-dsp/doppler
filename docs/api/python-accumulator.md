# Python Accumulator API

Running-sum accumulators backed by `dp_acc_f32_t` and `dp_acc_cf64_t`.
Used as the integrate-and-dump register in polyphase resamplers.

Source:
[`src/doppler/accumulator/__init__.py`](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/accumulator/__init__.py)

______________________________________________________________________

## Classes

| Class     | Accumulator type | Coefficient type | Use when                           |
| --------- | ---------------- | ---------------- | ---------------------------------- |
| `AccF32`  | float32          | float32          | real-valued sums, power estimation |
| `AccCf64` | complex128       | float32          | polyphase resampler I&D path       |

______________________________________________________________________

## Examples

### AccF32 — running sum

```python
from doppler.accumulator import AccF32

acc = AccF32()
acc.push(1.0)
acc.push(2.5)
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

### add vs push

- `push(v)` — add a scalar to the accumulator.
- `add(x)` — add all elements of a NumPy array to the accumulator.

```python
acc = AccF32()
acc.push(1.0)           # acc = 1.0
acc.add(np.array([2.0, 3.0], dtype=np.float32))  # acc = 6.0
```

______________________________________________________________________

::: doppler.accumulator.AccF32

______________________________________________________________________

::: doppler.accumulator.AccCf64
