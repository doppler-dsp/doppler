# Python Ring Buffer API

Lock-free SPSC ring buffers backed by `dp_buffer_*`. Uses virtual-memory
double-mapping so the consumer always sees a contiguous window across the
wrap boundary — zero-copy, branch-free.

Source:
[`src/doppler/buffer/__init__.py`](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/buffer/__init__.py)

______________________________________________________________________

## Buffer types

| Class       | NumPy dtype                 | Bytes/sample |
| ----------- | --------------------------- | ------------ |
| `F32Buffer` | `complex64`                 | 8            |
| `F64Buffer` | `complex128`                | 16           |
| `I16Buffer` | `[('i','<i2'),('q','<i2')]` | 4            |

**Any capacity from 1 up, and `capacity` is exactly the number you passed —
on every machine.** It need not be a power of two.

What *is* rounded is the mapping behind the ring: up to a power of two,
because indexing is a mask, and up to a whole number of pages, because the
double mapping is built from them (4 KiB on Linux x86-64, 16 KiB on macOS
arm64, 64 KiB allocation granularity on Windows). That is address space, not
room — a ring of 1,000 holds 1,000 and refuses the next sample — and it costs
nothing per call. A capacity just past a power of two maps nearly twice what
it holds; one that *is* a power of two and spans a page maps exactly itself.

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

From `src/doppler/examples/buffers_demo.py`. `write()` never blocks, so the
producer waits for `space`; `close()` is how the consumer learns there is no
more, and it arrives as `EOFError` once the ring is drained.

```python
--8<-- "src/doppler/examples/buffers_demo.py:setup"
--8<-- "src/doppler/examples/buffers_demo.py:threads"
```

### One thread: any block in, exact frames out

`wait(n)` blocks until a producer on *another* thread delivers, so a caller
that is its own producer would wait forever. `peek(n)` is the same zero-copy
view without the wait — the frame, or `None` for "not yet" — and `write_some`
takes what fits. Together they are the whole single-threaded pattern: feed,
take every whole frame, repeat until the block is gone.

```python
--8<-- "src/doppler/examples/ring_chunking_demo.py:setup"
--8<-- "src/doppler/examples/ring_chunking_demo.py:reblock"
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

numpy has no complex-integer dtype, so one q15 sample is a **record**,
`[("i", "<i2"), ("q", "<i2")]`. Both faces speak it, which keeps the ring 1-D
with one element per sample like its float siblings. The storage underneath
is still interleaved int16, so `.view()` converts either way with no copy.

```python
from doppler.buffer import I16Buffer
import numpy as np

IQ16 = np.dtype([("i", "<i2"), ("q", "<i2")])

buf = I16Buffer(4096)
adc_bytes = np.zeros(2048 * 2, dtype=np.int16).tobytes()   # ADC byte stream
raw = np.frombuffer(adc_bytes, dtype=IQ16)                  # zero-copy
buf.write(raw)

view = buf.wait(1024)       # shape (1024,), dtype IQ16
I = view["i"]               # strided int16 views, no copy
Q = view["q"]
flat = view.view(np.int16)  # interleaved I, Q, I, Q, ... — also no copy
buf.consume()
```

A bare `int16` array — flat or `(n, 2)` — is refused with `TypeError`: it is
not an array of samples. And a record refuses arithmetic (`view + 1` raises)
where a packed `int32` would carry across the I/Q boundary and corrupt I
silently, which is why it is a record.

### A full ring

A full ring is not an error: `write()` refuses the block and returns `False` — you still
hold it — and `dropped` counts what was refused.

```python
buf = F32Buffer(1000)          # any size; not a power of two here
cap = buf.capacity            # 1000

buf.write(np.ones(cap, dtype=np.complex64))        # True
buf.write(np.ones(1, dtype=np.complex64))          # False: no room
buf.dropped                                         # 1 refused, none lost
```

The full walk through every refusal is
`src/doppler/examples/ring_refusals_demo.py`; the rest of the suite is indexed
on [Ring Buffers](../examples/python-buffers.md).

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
