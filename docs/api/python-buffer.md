# Python Ring Buffer API

Lock-free SPSC ring buffers backed by `dp_buffer_*`. Uses virtual-memory
double-mapping so the consumer always sees a contiguous window across the
wrap boundary — zero-copy, branch-free.

Source:
[`src/doppler/buffer/__init__.py`](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/buffer/__init__.py)

______________________________________________________________________

## Buffer types

| Class       | NumPy dtype              | Bytes/sample | Min (4 KiB page) | Min (16 KiB page) |
| ----------- | ------------------------ | ------------ | ---------------- | ----------------- |
| `F32Buffer` | `complex64`              | 8            | 512 samples      | 2048 samples      |
| `F64Buffer` | `complex128`             | 16           | 256 samples      | 1024 samples      |
| `I16Buffer` | `int16` (shape `(n, 2)`) | 4            | 1024 samples     | 4096 samples      |

`n_samples` must be a power of two. The double-mapping trick builds the mirror
at page granularity, so the buffer must span at least one whole page — a
sub-page request is rounded **up** to the smallest power-of-two that does. The
minimum therefore depends on the system page size (4 KiB on Linux x86-64,
16 KiB on macOS arm64). Always read the real size back from `.capacity`; it may
exceed what you asked for.

______________________________________________________________________

## Threading model

One producer thread calls `write`; one consumer thread calls `wait` /
`consume`. `write` is non-blocking and drops samples if the buffer is
full. `wait` blocks the consumer and releases the GIL so the producer
can run concurrently.

______________________________________________________________________

## Examples

### Producer / consumer (threaded)

```python
from doppler.buffer import F32Buffer
import numpy as np
import threading

buf = F32Buffer(4096)

def producer():
    for block in iq_source:                         # complex64 arrays
        buf.write(block)                            # non-blocking

def consumer():
    while True:
        view = buf.wait(1024)                       # blocks; zero-copy
        process(view)
        buf.consume(1024)

t_prod = threading.Thread(target=producer, daemon=True)
t_cons = threading.Thread(target=consumer, daemon=True)
t_prod.start()
t_cons.start()
```

### Check available samples without blocking

```python
if buf.available >= 1024:
    view = buf.wait(1024)
    process(view)
    buf.consume(1024)
```

### I16Buffer — raw ADC samples

`I16Buffer` stores interleaved int16 IQ pairs. The returned array from
`wait` has shape `(n, 2)`: column 0 is I, column 1 is Q.

```python
from doppler.buffer import I16Buffer
import numpy as np

buf = I16Buffer(4096)
raw = np.frombuffer(adc_bytes, dtype=np.int16).reshape(-1, 2)
buf.write(raw)

view = buf.wait(1024)       # shape (1024, 2), dtype int16
I = view[:, 0]
Q = view[:, 1]
buf.consume(1024)
```

### Capacity and overflow

```python
buf = F32Buffer(1024)
print(buf.capacity)         # 1024 (or next power of two)

ok = buf.write(np.ones(1024, dtype=np.complex64))
print(ok)                   # True if written, False if dropped
```

______________________________________________________________________

::: doppler.buffer.F32Buffer

______________________________________________________________________

::: doppler.buffer.F64Buffer

______________________________________________________________________

::: doppler.buffer.I16Buffer
