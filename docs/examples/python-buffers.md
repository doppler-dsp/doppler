# Ring Buffers

Lock-free SPSC (single-producer, single-consumer) ring buffers backed by a
virtual-memory double-mapping, so the consumer always sees a contiguous
window across the wrap boundary.

Every block of code on this page is a region of a script under
`src/doppler/examples/` that CI runs and that checks its own results, so
what you read here is what was executed.

| script                  | what it shows                                         |
| ----------------------- | ----------------------------------------------------- |
| `buffers_demo`          | three widths, one shape; the wrap; two threads        |
| `ring_lifecycle_demo`   | owning a ring, and owning a view of one               |
| `ring_chunking_demo`    | the producer's block size is not the consumer's       |
| `ring_nonblocking_demo` | one thread: `peek` / `write_some` / `space` / `reset` |
| `ring_iq16_demo`        | 16-bit I/Q as a record, without a copy                |
| `ring_refusals_demo`    | everything a ring says no to, and how it says it      |
| `ring_interrupt_demo`   | stopping a consumer blocked in `wait()`               |

The C face has its own, under `native/examples/`: `ring_threaded_demo`,
`ring_write_policy_demo`, `ring_drip_feed_demo`, `ring_chunking_demo`,
`ring_element_view_demo` and `ring_backed_demo` (a ring whose samples are a
file, which the Python face does not expose).

## Three widths, one shape

`write()` copies a block in; `wait(n)` **lends** the consumer a view of the
next `n` samples; `consume()` gives them back. Every count is in samples and
every view is 1-D, on every width — so this loop is written once:

```python
--8<-- "src/doppler/examples/buffers_demo.py:setup"
--8<-- "src/doppler/examples/buffers_demo.py:widths"
```

The double mapping is what makes a lent frame contiguous even when it
straddles the physical end of the ring — no copy, and no special case in
the consumer:

```python
--8<-- "src/doppler/examples/buffers_demo.py:wrap"
```

## Two threads, and an end

`wait()` releases the GIL while it spins, so a producer thread runs.
`write()` never blocks — it refuses — so backpressure is the producer's:
wait for `space`. And an empty ring cannot tell a slow producer from a
finished one, so the producer says so with `close()`, which ends the
consumer's `wait()` in `EOFError` once the ring is drained:

```python
--8<-- "src/doppler/examples/buffers_demo.py:threads"
```

## Owning a ring, and a view of one

A ring is a mapping, so `with` is the spelling that cannot leak it; after
it, every member raises rather than touching memory that is gone:

```python
--8<-- "src/doppler/examples/ring_lifecycle_demo.py:setup"
--8<-- "src/doppler/examples/ring_lifecycle_demo.py:ring"
```

A view is a loan: zero-copy, read-only, and it keeps the ring alive.
`consume()` with no argument releases exactly what was lent, so the count
is written once. Copy anything you want to keep *before* releasing it:

```python
--8<-- "src/doppler/examples/ring_lifecycle_demo.py:view"
```

Releasing **less** than was lent keeps the rest, which is how overlapped
frames are read:

```python
--8<-- "src/doppler/examples/ring_lifecycle_demo.py:overlap"
```

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
unless something batches for it. The consumer asks for a whole batch and
`wait()` is what tells it one is ready — it never polls `available`. When the
producer closes the ring, `EOFError` is what tells it the stream ended, and
*that* is the one moment to read `available`: the ring is closed, so the
count can no longer grow, and whatever is there is the tail.

```python
--8<-- "src/doppler/examples/ring_chunking_demo.py:setup"
--8<-- "src/doppler/examples/ring_chunking_demo.py:drip"
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
    something that cannot arrive. `capacity` is exactly what you passed the
    constructor, and it can be any size: pick the one your largest frame needs.

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
to. Time it with the benchmarks in `src/doppler/buffer/benchmarks/`.

## 16-bit I/Q: a record, not a pair of columns

numpy has no complex-integer dtype, so `I16Buffer` speaks a record — one
element per sample, like its siblings, and exactly the four bytes the
hardware delivered:

```python
--8<-- "src/doppler/examples/ring_iq16_demo.py:setup"
--8<-- "src/doppler/examples/ring_iq16_demo.py:record"
```

Fields, the interleaved form and the record are three views of the same
bytes; none of these lines copies:

```python
--8<-- "src/doppler/examples/ring_iq16_demo.py:read"
```

To floating point with doppler's own converter:

```python
--8<-- "src/doppler/examples/ring_iq16_demo.py:convert"
```

A record rather than two `int16` packed into an `int32`, because arithmetic
on the packed form carries across the I/Q boundary and corrupts I silently.
A record refuses instead — and so does the ring, given a bare `int16` array:

```python
--8<-- "src/doppler/examples/ring_iq16_demo.py:refused"
```

## What a ring says no to

Each refusal is a distinct exception, raised before anything is copied. A
ring exists to avoid copies, so an input that would need casting,
flattening or compacting is refused rather than quietly fixed:

```python
--8<-- "src/doppler/examples/ring_refusals_demo.py:setup"
--8<-- "src/doppler/examples/ring_refusals_demo.py:inputs"
```

**Full is not an error.** `write()` says `False`, `write_some()` says how
much it took. `dropped` adds up *refused* samples — the caller still holds
them, so it is a loss count only if they are then thrown away:

```python
--8<-- "src/doppler/examples/ring_refusals_demo.py:full"
```

So is releasing more than is there — refused, and nothing released, because
the two positions are all a ring knows about itself:

```python
--8<-- "src/doppler/examples/ring_refusals_demo.py:release"
```

A request nothing could ever satisfy is a caller bug, and says so with both
numbers instead of waiting forever. "Not yet" and "never" are different
answers — `None` and `EOFError`:

```python
--8<-- "src/doppler/examples/ring_refusals_demo.py:never"
--8<-- "src/doppler/examples/ring_refusals_demo.py:eos"
```

Which is why a producer closes the ring in `finally`. If it raises, the
consumer's `wait()` ends instead of spinning:

```python
--8<-- "src/doppler/examples/ring_refusals_demo.py:threads"
```

## Stopping a blocked `wait()`

`wait()` spins in C with the GIL released, so nothing in that loop is
running Python and an ordinary flag cannot stop it. `doppler.interrupt` is
the stop it listens to — process-wide, so a guard made anywhere reaches a
wait blocked anywhere — and `Interrupt([signal.SIGINT])` is how Ctrl-C is
wired to it:

```python
--8<-- "src/doppler/examples/ring_interrupt_demo.py:setup"
--8<-- "src/doppler/examples/ring_interrupt_demo.py:stop"
--8<-- "src/doppler/examples/ring_interrupt_demo.py:resume"
```

A consumer loop usually wants both endings:

```python
--8<-- "src/doppler/examples/ring_interrupt_demo.py:loop"
```

______________________________________________________________________

## Buffer types

| Type        | Import           | NumPy dtype           | Notes                    |
| ----------- | ---------------- | --------------------- | ------------------------ |
| `F32Buffer` | `doppler.buffer` | `complex64`           | CF32 IQ pairs            |
| `F64Buffer` | `doppler.buffer` | `complex128`          | CF64 IQ pairs            |
| `I16Buffer` | `doppler.buffer` | `(i, q)` int16 record | `view["i"]`, `view["q"]` |

Any capacity from 1 up, a power of two or not, and `capacity` is exactly what
you asked for on every machine. The mapping behind it is rounded up to a power
of two and to whole pages — address space, never room.
