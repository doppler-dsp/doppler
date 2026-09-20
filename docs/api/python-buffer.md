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
minimum therefore depends on the system mapping granularity (4 KiB on Linux
x86-64, 16 KiB on macOS arm64, and the 64 KiB allocation granularity on
Windows — so the Windows minimums are 16× the 4 KiB column). Always read the
real size back from `.capacity`; it may exceed what you asked for.

______________________________________________________________________

## Two surfaces, one ring

| You are                               | Read with          | Write with        |
| ------------------------------------- | ------------------ | ----------------- |
| a consumer **thread**, producer apart | `wait(n)` — blocks | `write(arr)`      |
| **one thread** doing both             | `peek(n)` — never  | `write_some(arr)` |

**Threaded.** One producer thread calls `write`; one consumer thread calls
`wait` / `consume`. `write` is non-blocking and **refuses** the whole call if
the ring has no room — it copies nothing and leaves your array untouched, so
you still hold the data and can retry. Nothing is dropped unless you discard
it; `dropped` counts refused calls, not lost samples. `wait` blocks the
consumer and releases the GIL so the producer can run concurrently.

**Single-threaded.** `wait` would deadlock a caller that is its own
producer, so `peek(n)` returns the same zero-copy view when `n` samples are
there and `None` when they are not yet. `None` means *not yet* and nothing
else: a closed ring with fewer than `n` left raises `EOFError`, and
`n > capacity` raises `ValueError`, exactly as `wait` does. `write_some`
takes what fits and returns the count, never refusing and never touching
`dropped` — the only way to feed a chunk larger than the ring. `space` is
the room a `write` is guaranteed to find; `reset()` empties **and reopens**
the ring (both sides must be idle).

Both surfaces release with `consume(k)`; `k < n` keeps the overlap, which is
how overlapped frames are read.

______________________________________________________________________

## Examples

### Producer / consumer (threaded)

<!-- docs-snippet: skip=threaded producer/consumer with infinite loops -->

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

### Draining without blocking

`wait(n)` returns a zero-copy view immediately when `n` samples are already
buffered; it only blocks the consumer while fewer than `n` are available. So a
producer that has filled the ring lets the consumer drain without waiting.

```python
from doppler.buffer import F32Buffer
import numpy as np

buf = F32Buffer(4096)
buf.write(np.ones(2048, dtype=np.complex64))   # producer filled the ring

view = buf.wait(1024)          # 1024 already buffered -> returns at once
np.abs(view).mean()            # process the zero-copy view
buf.consume(1024)
```

### One thread: a chunk larger than the ring, overlapped frames out

From `src/doppler/examples/ring_nonblocking_demo.py`, which checks every
frame against the input:

```python
--8<-- "src/doppler/examples/ring_nonblocking_demo.py:setup"
--8<-- "src/doppler/examples/ring_nonblocking_demo.py:chunk"
```

### End of stream, then the same ring again

```python
--8<-- "src/doppler/examples/ring_nonblocking_demo.py:eos"
```

### I16Buffer — raw ADC samples

`I16Buffer` stores interleaved int16 IQ pairs. The returned array from
`wait` has shape `(n, 2)`: column 0 is I, column 1 is Q.

```python
from doppler.buffer import I16Buffer
import numpy as np

buf = I16Buffer(4096)
adc_bytes = np.zeros(2048 * 2, dtype=np.int16).tobytes()   # ADC byte stream
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
print(ok)                   # True if written, False if refused (retry)
```

______________________________________________________________________

::: doppler.buffer.F32Buffer

______________________________________________________________________

::: doppler.buffer.F64Buffer

______________________________________________________________________

::: doppler.buffer.I16Buffer

## Related pages

<!-- related-pages:start -->

**Guides** — [Real-Time Pacing & Timestamping](../guide/timing.md)
**Design** — [The ring buffer — one contiguous view of a stream](../design/ring-buffer.md)

<!-- related-pages:end -->
