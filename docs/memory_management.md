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
  size_t (*gen) (lo_state_t *, size_t, float complex *) = lo_steps;
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
      size_t n = lo_steps (lo, 256, out);
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

It is not free, though, and the cost is worth knowing because it is invisible.
The returned array may be a view onto a buffer the object owns. When you hold
that view across the next call, the object cannot reuse the buffer in place, so
it allocates a fresh one and keeps the old one until the object is destroyed.
Holding every block therefore costs one retained buffer per call:

```text
lo = LO(0.1)
for _ in range(10_000):
    block = lo.steps(65536)     # each retained block pins its own buffer
    process(block)
```

If you are processing large blocks in a long-lived loop, prefer one of the two
forms below.

!!! note

    This retention is a property of the current binding, not of the C library
    (which allocates nothing per call) and not of the API contract. It is
    tracked upstream as just-makeit issue 604.

### `out=`: the C contract, exposed to Python

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

**The dtype must match exactly.** A mismatched buffer is rejected, not cast:

```python
>>> wrong = np.empty(64, dtype=np.float32)    # FIR outputs complex64
>>> fir.execute(x, out=wrong)
Traceback (most recent call last):
    ...
TypeError: out must be a writable ndarray of the output dtype
```

This matters more than it looks. A cast would write into a temporary copy, so
the call would return a correct-looking result while *your* buffer — the entire
reason you passed `out=` — was never touched. Rejecting it is the only way the
promise can be kept.

**The buffer must be large enough**, sized with the same `*_max_out()`
companion the C API uses. An undersized buffer raises `ValueError` rather than
truncating.

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

- **`delay` and `hbdecim` allocate twice the capacity** and keep two copies of
    the window, so *any* `num_taps`-length span is contiguous and a read is one
    `memcpy` with no wrap-around branch. `delay` first rounds capacity up to a
    power of two so the index wrap is a mask rather than a modulo — a 33-tap
    line allocates 128 slots to hold 33.
- **The ring buffer separates `head`, `tail` and `dropped` by a full cache
    line** (`DP_ALIGN(DP_CACHELINE)`). Packed together they would share a line,
    and producer and consumer would invalidate each other's copy on every
    update — the false-sharing ping-pong.
- **The ring buffer's storage is double-mapped** into adjacent virtual pages,
    which costs address space to buy a consumer view that never wraps.

The pattern is the same each time: spend memory to make the *access pattern*
regular. A power-of-two stride is not always the win, though — when many rows
share one stride, a power of two makes them collide in the same cache sets, and
padding the stride to break that up is the fix. Measure the access pattern, not
just the footprint.

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
- **Writing Python, small blocks or throwaway results** — use the plain form.
    It is the clearest, and a block that dies before the next call costs nothing.
- **Writing Python, large blocks in a long-lived loop** — use `out=`. It is the
    C contract with a numpy face: one allocation, no retention, no surprises.
- **Streaming between threads** — use `doppler.buffer`, and copy anything that
    must outlive `consume(n)`.
