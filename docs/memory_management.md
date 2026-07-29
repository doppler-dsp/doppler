# Memory Management

Who allocates a buffer, who owns it, and how long it stays valid. The rules
differ between the C library and the Python bindings — not by accident, but
because the two APIs answer different questions.

The short version:

| Layer                                 | Who allocates the output | Caller must                          |
| ------------------------------------- | ------------------------ | ------------------------------------ |
| C, every DSP block                    | **the caller**           | nothing — you own the memory         |
| Python, `obj.method(x)`               | the binding              | nothing — the array stays valid      |
| Python, `obj.method(x, out=buf)`      | **the caller**           | pass the exact dtype and enough room |
| Python, `doppler.buffer` ring buffers | the buffer               | call `consume(n)` when finished      |

______________________________________________________________________

## C: the caller owns everything

Every DSP entry point in the C library writes into a buffer the caller
supplies. Nothing returns a pointer into internal state:

```c title="every block takes an out-parameter"
#include <complex.h>
#include <stddef.h>
#include <stdio.h>

#include "delay/delay_core.h"
#include "fir/fir_core.h"
#include "lo/lo_core.h"

int
main (void)
{
  /* Binding each to a spelled-out function pointer means this listing
     stops compiling if any of these signatures ever drifts. */
  size_t (*gen) (lo_state_t *, size_t, float complex *, size_t) = lo_steps;
  size_t (*flt) (fir_state_t *, const float complex *, size_t,
                 float complex *)
      = fir_execute;
  size_t (*win) (delay_state_t *, size_t, double complex *) = delay_ptr;

  printf ("out-parameter blocks: %d\n", (gen != 0) + (flt != 0) + (win != 0));
  return 0;
}
```

`delay_ptr` is worth singling out: despite the name, and despite its Python
docstring describing a "zero-copy view", the C function fills your buffer like
every other one.

This means there is **no lifetime contract to observe**. The memory is yours.
Hold it as long as you like, hand it to another thread, free it whenever. A
second call does not invalidate the result of the first unless you chose to
pass the same buffer twice.

Size the buffer with the block's `*_max_out()` companion, which exists for
exactly this purpose:

```c title="caller-owned output"
#include <complex.h>
#include <stdlib.h>
#include <stdio.h>
#include "lo/lo_core.h"

int main (void)
{
  lo_state_t *lo = lo_create (0.1);

  /* Ask how much room the block can need, then allocate once. */
  size_t cap = lo_steps_max_out (lo);
  float complex *out = malloc (cap * sizeof (float complex));

  /* Reuse the same buffer every call: no allocation in the loop. */
  for (int i = 0; i < 4; i++)
    {
      size_t n = lo_steps (lo, 256, out, 256);
      printf ("block %d: %zu samples\n", i, n);
    }

  free (out);
  lo_destroy (lo);
  return 0;
}
```

The state struct holds no output storage — `lo_state_t` is three scalars — so
a C caller allocates once, outside the loop, and never allocates again.

______________________________________________________________________

## Python: the binding allocates, and the result is always safe to keep

The convenience form returns an array you can hold indefinitely:

```python
>>> import numpy as np
>>> from doppler.source import LO
>>> lo = LO(0.1)
>>> first = lo.steps(8)
>>> snapshot = first.copy()
>>> second = lo.steps(8)                      # call again while `first` lives
>>> np.array_equal(first, snapshot)           # `first` is untouched
True
>>> np.shares_memory(first, second)           # distinct arrays
False
```

That is a **correctness guarantee**: a previously returned block is never
overwritten by a later call.

Each call allocates its own NumPy-owned result and shares nothing with the
next one, so there is no buffer to invalidate and no bookkeeping to get wrong.
Holding every block in a long-lived loop costs only the blocks you actually
keep:

```text
lo = LO(0.1)
for _ in range(10_000):
    block = lo.steps(65536)     # freed when you drop it, like any array
    process(block)
```

!!! note "It was not always this way"

    Until jm 0.33.15 the binding handed back a view onto a buffer the object
    owned, and holding one across the next call forced a fresh allocation while
    retaining the old — one buffer per held call, released only when the object
    was destroyed. Measured on the loop above: **1,548,388 KiB of growth, now
    1,376 KiB**, with the hold path 82% faster and the drop path no slower.

    That history is worth one line because it is the reason `out=` is
    documented below the way it is: the retention existed to make the plain
    form "as fast as" `out=`, and the fix was to stop trying.

    Three components — `DelayCf64`, `Farrow` and `FFT` — still carry the old
    path pending a hand-port, because their bindings hold methods the manifest
    cannot express yet. Their public contract is unchanged; only the retention
    differs.

### `out=`: placement and determinism, not throughput

Every block method that returns an array also accepts `out=`, which makes
Python behave exactly like C — you allocate, the kernel fills it, nothing is
retained:

```python
>>> from doppler.filter import FIR
>>> taps = np.array([0.5, 0.25, 0.25], dtype=np.complex64)
>>> fir = FIR(taps)
>>> x = np.ones(64, dtype=np.complex64)
>>> buf = np.empty(max(fir.execute_max_out(), 64), dtype=np.complex64)
>>> y = fir.execute(x, out=buf)
>>> np.shares_memory(y, buf)                  # written in place
True
```

Two rules apply, and both are enforced rather than silently worked around:

**The dtype must match exactly, and the buffer must be C-contiguous.**
Either mismatch is rejected rather than silently cast:

```python
>>> wrong = np.empty(64, dtype=np.float32)    # FIR outputs complex64
>>> fir.execute(x, out=wrong)
Traceback (most recent call last):
    ...
TypeError: out must be a writable, C-contiguous ndarray of the output dtype
```

This matters more than it looks. A cast would write into a temporary copy, so
the call would return a correct-looking result while *your* buffer — the entire
reason you passed `out=` — was never touched. Rejecting it is the only way the
promise can be kept.

**The buffer must be large enough**, sized with the same `*_max_out()`
companion the C API uses. An undersized buffer raises `ValueError` rather than
truncating.

!!! warning "`*_max_out()` can demand far more than the data"

    That second rule is currently a sharp edge, and it cuts against the
    placement use case above. `*_max_out()` is not a per-call bound: it returns
    either `0` (no information — the binding then sizes from the input length)
    or a **fixed internal cap** unrelated to your call. For the resampler that
    cap is 65,536, so:

    ```pycon
    >>> from doppler.resample import Resampler
    >>> r = Resampler(0.5)
    >>> r.execute(np.ones(1024, np.complex64), out=np.empty(512, np.complex64))
    Traceback (most recent call last):
        ...
    ValueError: out has 512 elements, need >= 65536
    ```

    512 KiB of buffer to receive 512 samples — 128× the data. The check is
    correct given today's contract (without a per-call bound the binding cannot
    know 512 is safe), but it means a ring slot or `mmap` window sized to the
    data is rejected, which is exactly what `out=` exists to serve.

    Until this is fixed, size placement buffers with
    `max(obj.<method>_max_out(), n_in)` and accept the over-allocation, or use
    the plain form for blocks whose cap is disproportionate. Tracked upstream
    as just-makeit issue 607, which changes the contract to a per-call bound.

#### Reach for it for placement, or for determinism — not for speed

`out=` exists so you can decide *where* the samples land: an `mmap`, a shared
segment, a slot in a ring you already own, a buffer another library will read
without a copy. That is a capability the plain form cannot offer at any price.

The second real reason is jitter, **and only for large blocks**. `out=` means
no allocator call inside the loop, so whatever the allocator was contributing
to the tail of your latency distribution goes away. Whether that is worth
anything depends entirely on whether the allocator had a tail to begin with:

| `LO.steps`             | p50    | p99    | max    |
| ---------------------- | ------ | ------ | ------ |
| n = 65,536, allocating | 13,184 | 39,503 | 58,570 |
| n = 65,536, `out=`     | 13,244 | 15,350 | 19,116 |
| n = 1,024, allocating  | —      | 1,172  | —      |
| n = 1,024, `out=`      | —      | 1,543  | —      |

At 64k (512 KiB per block) the allocator goes to the OS — `mmap`/`munmap` and
the page faults that follow — and `out=` buys **2.6× on p99 and 3× on the max**
for the usual ~60 ns on the median. At 1k (8 KiB) the allocator never leaves
its free list, so there is no tail to remove and `out=` is worse at p99 *as
well as* p50: you pay the validation cost and get nothing back.

So the deadline argument is a large-block argument. Do not carry it down to
small blocks, where it inverts.

!!! note "Don't expect a clean threshold"

    The mechanism is the allocator's `mmap` cutoff — 128 KiB by default in
    glibc — but that cutoff is **dynamic**: glibc raises it (up to 32 MiB) once
    it sees mmap'd blocks being freed, precisely so a steady-state loop stops
    paying for `mmap`. The tail can therefore be large early in a run and
    shrink later, and the crossover moves with allocation size, run length and
    what else the process is doing. Treat the two rows above as the shape of
    the effect, not as a lookup table, and measure your own workload if a
    deadline depends on it.

What it is *not* is a throughput optimization. Against letting the binding
allocate the result, passing `out=` is consistently **slower** — the binding
still has to validate the buffer, check its capacity and build a view over it:

| n      | binding allocates | `out=`    | cost of `out=` |
| ------ | ----------------- | --------- | -------------- |
| 64     | 85 ns             | 157 ns    | +72 ns         |
| 1,024  | 377 ns            | 420 ns    | +43 ns         |
| 65,536 | 16,934 ns         | 17,003 ns | +69 ns         |

It is a roughly **fixed ~60 ns per call**, so it reads as 85% overhead on a
64-sample block and 0.4% on a 64k one. Either way, allocation is not what makes
a block call expensive — the kernel is.

This distinction is worth being pedantic about, because getting it wrong has
already cost real work: `out=` being *described* as the fast path is what
motivated an internal reuse buffer to make the plain form "just as fast",
which bought a returned view that aliased object state, which needed deferred
frees and liveness tracking to stay safe, which is [issue
604](https://github.com/just-buildit/just-makeit/issues/604). The chain starts
with treating an allocation as the thing to eliminate.

!!! warning "If you place it, align it"

    The buffers `out=` exists to serve — `mmap` regions, shared segments, ring
    slots — are exactly the ones most likely to be misaligned or offset, and
    a NumPy **slice** is misaligned too. That costs 16% on an FFT (see
    [Alignment](#alignment)). Allocate placement buffers whole, on a 16-byte
    boundary.

### Releasing the block instead of keeping it

If you only need a reduction, let the array die inside the expression. The
object then reuses its buffer in place and nothing accumulates:

```python
>>> lo = LO(0.1)
>>> total = sum(float(abs(lo.steps(1024)).sum()) for _ in range(4))
>>> total > 0
True
```

______________________________________________________________________

## The address matters, not just the ownership

"The memory is yours" settles *who frees it*. It does not settle where it
lives, and for a DSP kernel the address is part of the performance contract.
Two consequences are worth knowing, because both are silent.

### Alignment

doppler's float FFT path is [PFFFT](https://bitbucket.org/jpommier/pffft),
which requires 16-byte-aligned buffers. `pocketfft_execute_1d_cf32` checks
`aligned16(in) && aligned16(out)` and transforms straight in→out when both
qualify; otherwise it bounces the unaligned side through internal aligned
scratch. The 2-D path never touches caller alignment at all — it copies into
`pffft_aligned_malloc`'d storage first, and its per-row sub-pointers stay
aligned because `nx`/`ny` are multiples of 16.

**Results are identical either way.** The cost is speed:

| `FFT(4096).execute_cf32(x, out=…)` | ns/call      |
| ---------------------------------- | ------------ |
| 16-byte-aligned `out`              | 5,756        |
| misaligned `out`                   | 6,681 (+16%) |

A fresh NumPy array is suitably aligned; a **slice is not**. `buf[1:n+1]` is
offset by 8 bytes, so it takes the bounce path with nothing to tell you:

```python
>>> import numpy as np
>>> base = np.empty(4100, dtype=np.complex64)
>>> base.__array_interface__["data"][0] % 16     # a fresh array: aligned
0
>>> base[1:4097].__array_interface__["data"][0] % 16   # a slice: not
8
```

If you are pre-allocating an `out=` buffer for a hot loop, allocate it whole
rather than slicing it out of something larger.

!!! note

    This is the tamer cousin of a classic FFTW footgun, where the *planner*
    takes the buffer pointers and specialises the plan to their alignment, so
    executing against a differently-aligned array is undefined rather than
    merely slow. doppler cannot reproduce that: `fft_create(n, sign, nthreads)`
    takes no pointers, so a plan is never tied to one.

### Over-allocating on purpose

The corollary is that the cheapest buffer is often bigger than the data. Three
places in doppler deliberately allocate more than they use, and the extra space
is the whole point:

- **The polyphase resampler's delay line is a power-of-two dual buffer.**
    `resamp` rounds capacity up to a power of two (so the index wrap is a mask,
    not a modulo) and then allocates *twice* that, writing every input sample
    into both halves:

    ```text
    s->delay_head = (s->delay_head - 1) & s->delay_mask;
    s->delay_buf[s->delay_head]                = x;
    s->delay_buf[s->delay_head + s->delay_cap] = x;   /* mirror */
    ```

    That is one extra store per input sample. What it buys is the read side:
    `dl_ptr()` hands back a **plain `const float _Complex *`**, so each arm's
    dot product is a straight contiguous walk — no mask in the inner loop, no
    split into two segments, and a shape a vectorizer can actually use. An
    interpolating resampler runs that dot product several times per input, so
    the read side is what dominates.

- **`delay` and `hbdecim` use the same trick** for the same reason: any
    `num_taps`-length span is contiguous, so a window read is one `memcpy` with
    no wrap-around branch. A 33-tap `delay` line allocates 128 slots to hold 33.

- **The ring buffer separates `head`, `tail` and `dropped` by a full cache
    line** (`DP_ALIGN(DP_CACHELINE)`). Packed together they would share a line,
    and producer and consumer would invalidate each other's copy on every
    update — the false-sharing ping-pong.

- **The ring buffer's storage is double-mapped** into adjacent virtual pages,
    which costs address space to buy a consumer view that never wraps.

The pattern is the same each time: spend memory to make the *access pattern*
regular, so the hot loop gets a shape the compiler and the prefetcher can both
exploit.

Two cautions, because neither of these is a rule you can apply blind.

**The win is conditional on the loop it feeds.** Isolating the dual buffer in a
synthetic dot product, the contiguous form beats masked indexing by ~1.5× at
32–64 taps — but *loses* below 16 taps, where the extra store costs more than
the mask saves, and the margin narrows again once the working set outgrows L1.
It pays when the loop vectorizes and the read is repeated per input sample,
which is exactly the polyphase case and not, say, a one-shot copy.

**A power-of-two stride is not automatically the win either.** It makes the
wrap a mask, which is why the delay lines use it — but when many *rows* share
one power-of-two stride they collide in the same cache sets, and there the fix
is the opposite: pad the stride to break the collision up. Same principle
(shape the addresses), opposite adjustment.

Measure the access pattern, not the footprint.

______________________________________________________________________

## Ring buffers: the one genuine zero-copy contract

`doppler.buffer` is the exception, and the only place where "use it before you
call again" is a real rule. `wait(n)` hands back a view directly onto the
buffer's storage, and the slots are not released until you say so:

```text
view = buf.wait(256)        # zero-copy view into the ring
process(view)               # use it...
buf.consume(256)            # ...then release the slots to the producer
```

Copy the view if you need the data to outlive the `consume(n)` call. See
[Ring Buffers](examples/python-buffers.md) for the full producer/consumer
pattern.

______________________________________________________________________

## Choosing

- **Writing C** — allocate once with `*_max_out()`, reuse it, forget about
    lifetimes. There is nothing else to know.
- **Writing Python** — use the plain form. It is the clearest, it is the
    fastest, and the result is yours to keep. This is the default and it should
    stay the default.
- **You need the samples in a particular place** — `mmap`, a shared segment, a
    ring slot, a buffer another library reads without copying — use `out=`.
    Allocate it whole and 16-byte aligned.
- **You have a latency deadline and your blocks are large** — use `out=` to keep
    the allocator out of the loop, accepting ~60 ns on the median to cut p99 by
    ~2.6×. For small blocks this argument does not apply: `out=` is worse at
    p99 too, because there is no allocator tail to remove.
- **You are holding large blocks in a long-lived loop today** — release each
    block, or pass `out=`, until [604](https://github.com/just-buildit/just-makeit/issues/604)
    lands. This one is a workaround with an expiry date, not a rule.
- **Streaming between threads** — use `doppler.buffer`, and copy anything that
    must outlive `consume(n)`.

The through-line: reach for `out=` when you need control over *where* memory is
or *when* it is acquired. Reaching for it to avoid an allocation is optimizing
the one term that was never the expensive part.
