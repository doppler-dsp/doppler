# Ring Buffers

Lock-free SPSC (single-producer, single-consumer) ring buffers backed by a
virtual-memory double-mapping, so the consumer always sees a contiguous
window across the wrap boundary.

## Producer / consumer pattern

`wait(n)` blocks until `n` samples are available and returns a zero-copy
view. Call `consume(n)` when done to release the slots back to the producer.

```python
from doppler.buffer import F64Buffer
import numpy as np
import threading

buf = F64Buffer(256)

def producer():
    data = (np.ones(256) + 2j * np.ones(256)).astype(np.complex128)
    ok = buf.write(data)          # non-blocking; False if full
    print(f"write ok: {ok}")

def consumer():
    view = buf.wait(256)          # blocks until 256 samples available
    print(f"received[:2]: {view[:2]}")
    buf.consume(256)              # release slots back to producer

t_c = threading.Thread(target=consumer)
t_p = threading.Thread(target=producer)
t_c.start()
t_p.start()
t_p.join()
t_c.join()

print(f"dropped: {buf.dropped}")
```

```text
write ok: True
received[:2]: [1.+2.j 1.+2.j]
dropped: 0
```

Start the consumer before the producer so `wait()` is already blocking when
data arrives. Always `join()` both threads — the consumer holds a view into
shared memory and must call `consume()` before the buffer can be reused.

## The producer's block size is not the consumer's

This is what the ring is *for*, and it works in both directions. The
double-mapping is what makes it free: `wait(n)` returns a **contiguous** view
of `n` samples even when those `n` straddle the physical end of the ring, so
a consumer with a fixed block size hands the view straight to the next stage
with no copy and no special case for the wrap.

**Large in, fixed out.** A capture device gives you whatever its driver
batched; an FFT of length `N` cannot take `N-1`. Write the irregular blocks,
read exact frames:

```python
from doppler.buffer import F32Buffer
import numpy as np

FFT_N, ring = 1024, F32Buffer(8192)
cap = ring.capacity

for block in (3000, 5000, 1700, 4096):        # nothing is a multiple of N
    while cap - ring.available < block:       # backpressure: make room first
        ring.wait(FFT_N); ring.consume(FFT_N)
    ring.write(np.zeros(block, dtype=np.complex64))
    while ring.available >= FFT_N:            # take every whole frame
        frame = ring.wait(FFT_N)
        assert len(frame) == FFT_N and frame.flags["C_CONTIGUOUS"]
        ring.consume(FFT_N)
```

**Small in, drain when full.** A chatty producer costs one wake-up per write
unless something batches for it. Let the backlog build, then take it in one
go:

```python
BATCH, drip = 2048, F32Buffer(4096)
for _ in range(64):                            # a drip of small writes
    drip.write(np.zeros(32, dtype=np.complex64))
while drip.available >= BATCH:
    view = drip.wait(BATCH)                    # one wake-up, not 64
    drip.consume(BATCH)
```

Three things that bite, all of them measurable:

- **`write` is all-or-nothing and never blocks.** A block that does not fit
    is rejected whole — so check for room first, or check the return value.
    There is no blocking write; backpressure is the caller's.
- **`dropped` counts the length of every *rejected call*, not samples lost.**
    A producer that spins on `write` until it succeeds inflates it without
    losing anything: a 60,000-sample run written that way reported 5,960,438
    "dropped". Wait for room if you want the counter to mean what it says.
- **Size a block from `capacity`, not from the producer's chunk.** `wait(n)`
    with `n > capacity` can never be satisfied — the ring cannot hold that many
    — so it raises `ValueError` naming both numbers rather than waiting for
    something that cannot arrive. Read `capacity` back to compare against: it is
    *rounded up* from what you asked the constructor for.

The runnable version of both directions, with the assertions that keep it
honest, is `src/doppler/examples/ring_chunking_demo.py`.

## One thread: `peek` and `write_some`

`wait(n)` spins until a producer on *another* thread delivers, so a caller
that is its own producer — a read loop, a callback, a notebook cell — would
wait forever. `peek(n)` is the same zero-copy view without the wait: the
frame if it is there, `None` if it is not yet.

**Drip-feed until a frame is there.** Arrivals smaller than a frame; ask
after each one:

```python
--8<-- "src/doppler/examples/ring_nonblocking_demo.py:setup"
--8<-- "src/doppler/examples/ring_nonblocking_demo.py:drip"
```

**A chunk larger than the ring, read as overlapped frames.** `write` is
all-or-nothing, so it can never accept this chunk. `write_some` takes what
fits and says how much; alternate it with the drain. `consume(HOP)` with
`HOP < NFFT` releases a hop and keeps the overlap:

```python
--8<-- "src/doppler/examples/ring_nonblocking_demo.py:chunk"
```

**End of stream, then reuse.** `None` only ever means *not yet*. Once the
ring is closed a frame that cannot arrive raises `EOFError`; what is there
can still be read, and `reset()` reopens the same mapping for a second
stream:

```python
--8<-- "src/doppler/examples/ring_nonblocking_demo.py:eos"
```

`space` is the room a `write` is guaranteed to find — use it instead of
deriving `capacity - available`. From Python this pair is also the cheaper
one per frame: `wait` releases and retakes the GIL, `peek` has no reason
to. Time it with `src/doppler/buffer/benchmarks/bench_buffer.py`.

______________________________________________________________________

## Buffer types

| Type        | Import           | NumPy dtype          | Min capacity | Notes                |
| ----------- | ---------------- | -------------------- | ------------ | -------------------- |
| `F32Buffer` | `doppler.buffer` | `complex64`          | 512          | CF32 IQ pairs        |
| `F64Buffer` | `doppler.buffer` | `complex128`         | 256          | CF64 IQ pairs        |
| `I16Buffer` | `doppler.buffer` | `int16, shape=(n,2)` | 1024         | col 0 = I, col 1 = Q |

!!! note "Min capacity is page-size dependent"

    These are the minima on a 4 KiB-page system (x86_64). The mmap-backed ring
    sizes to a whole page, so on 16 KiB-page systems (e.g. macOS arm64) the
    minima double — `F32Buffer` 1024, `F64Buffer` 512, `I16Buffer` 2048.

```python
from doppler.buffer import F32Buffer, I16Buffer

# F32 — half the memory footprint of F64
buf32 = F32Buffer(512)

# I16 — raw SDR output; wait() returns shape (n, 2)
buf16 = I16Buffer(1024)
```

## Overflow detection

`buf.dropped` adds up the length of every **refused** `write` — the caller
still holds those samples, so it is a loss count only if they are then
discarded. `write_some` never refuses, so it never moves it:

```python
buf = F64Buffer(256)
buf.write(np.zeros(256, dtype=np.complex128))
buf.write(np.zeros(256, dtype=np.complex128))  # refused whole: no room
print(f"dropped: {buf.dropped}")
```

```text
dropped: 256
```
