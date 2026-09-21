# The ring buffer — measurements and the record

The dated companion to [The Ring Buffer](ring-buffer.md), which states what
*is*. This page keeps what was measured, in the order it was measured, and
the guesses that were wrong — section numbers are shared between the two, so
`§4` means the same thing on both.

All timings: AMD Ryzen AI 9 465, GCC 16.2.1, `-O3 -march=x86-64-v2 -ffast-math`, `bench_buffer_core`, **pinned to a Zen 5 core**
(`taskset -c 0-3,10-13`; unpinned, this part's two core classes differ 1.6×
and the rows below are not comparable — see `make bench-interleaved`).

______________________________________________________________________

## 0. How the non-blocking surface came to exist (2026-09-20)

The work started as a binding migration and three of its premises were wrong.

| assumed                                                               | measured                                                                                                                                                                                                                                                                                                                                                              |
| --------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| "The C API returns a bare `NULL`, so the *reasons* must move into C." | They were already there: `wait()` returns `NULL` for `n > capacity` (checked first, #1335), for closed-and-drained, and for an interrupt, and a caller tells them apart with `closed()`, `dp_interrupted()` and `n` against capacity. The Python binding merely re-derived that, three times. What was missing was **one owner of the precedence** — `wait_status()`. |
| "The blocking `wait()` is the ring's main interface."                 | **No C consumer calls it.** Only the tests and Python do.                                                                                                                                                                                                                                                                                                             |
| "Migrating the binding is the work."                                  | The larger defect was in C: the surface every real consumer needs did not exist.                                                                                                                                                                                                                                                                                      |

Every C consumer is single-threaded and rebuilt that surface from the struct:

| private copy of…                      | acq | detector | detector2d | burst_capture |
| ------------------------------------- | --- | -------- | ---------- | ------------- |
| free space (`capacity - (h - t)`)     | ✔   | ✔        | ✔          | —             |
| clamped partial write                 | ✔   | ✔        | ✔          | ✔             |
| frame pointer `data + (t & mask) * 2` | ✔   | ✔        | ✔          | ✔             |
| reset by storing `head` / `tail`      | ✔   | ✔        | ✔          | ✔             |
| direct `head` / `tail` loads + stores | 10  | 10       | 10         | 9             |

`detector_push()` is `write_some()` + `while (peek (n)) … consume (n)`,
written out by hand. Moving the four onto the API, with a lint rule against
struct access, is [#1426](https://github.com/doppler-dsp/doppler/issues/1426).

## 1. Wrapping is free

```text
write_wait_consume[f32,chunk=1024]   0.328 ns/sample   3050 Msample/s   never straddles
write_wait_consume[f32,chunk=1000]   0.329 ns/sample   3043 Msample/s   usually does      1.00x
write_wait_consume[f64,chunk=1024]   0.690 ns/sample   1450 Msample/s
write_wait_consume[f64,chunk=1000]   0.710 ns/sample   1408 Msample/s                     1.03x
write_wait_consume[i16,chunk=1024]   0.218 ns/sample   4584 Msample/s   (first i16 row)
write_wait_consume[i16,chunk=1000]   0.222 ns/sample   4508 Msample/s                     1.02x
```

`f64` costs 2.1× `f32` and `i16` 0.66×: the ring's cost is **per byte**, which
is what a `memcpy` plus a read should be. Adding the five functions did not
move the existing rows (they are separate `static inline`s; nothing on the
`write` / `wait` / `consume` path changed).

## 4. What the non-blocking surface costs

`write_some_peek_consume[f32,in=3000,frame=1024]`: **0.354 ns/sample, 1.08×**
the `write` + `wait` reference. The bench's first comment said it "should cost
nothing". It does not, and the 8% is two effects, separated by changing one
variable (three runs each, identical to the digit):

| input chunk | frame | ratio     | what it is                                                                                                                                          |
| ----------- | ----- | --------- | --------------------------------------------------------------------------------------------------------------------------------------------------- |
| 1024        | 1024  | **1.03×** | the loop: each drain ends on one `peek()` that returns `NULL` — ~10 ns against a ~340 ns frame. The price of not knowing the chunk size.            |
| 3000        | 1024  | **1.08×** | the other 5% is cache geometry: 24 KB is written before any is read back, where the reference reads each 8 KB while it is hot. No API removes that. |

## 5. The tests, and the sabotage that was missed

Eight sabotages of the new functions, run against a scratch copy of the
header: `space` off by one, `write_some` ignoring the room, `peek` returning
without checking, `wait_status` with *closed* and *interrupted* swapped, with
no too-large case, `reset` keeping `closed`, `reset` clearing `dropped` — all
red. **One survived the first pass:** `write_some` counting a drop on a full
ring left the suite green, because `dropped` was asserted *before* the
full-ring call rather than after it. The assertion was added and the sabotage
then failed at `test_buffer_core.c:441`.

## 6. `consume()` is bounded after all, and two threads (2026-09-21)

§6 of the design page used to list "no bounds check in `consume()`" among the
things the ring deliberately does not do. This is why it no longer does.

Both were listed as unknown. They were one question: the bound reads the
producer's index on the consumer's release path, which is a cross-core cache
line — *if* the consumer does not already hold it.

It does. The `wait()` or `peek()` that precedes every release has just loaded
`head`, so the bound re-reads a line the core pulled a few instructions ago.

Measured as an A/B on one header, producer and consumer on **separate cores**
(`taskset` 4 and 2), `wait` + `consume` at a **64-sample frame** — the
release-heaviest shape, where memcpy cannot hide a per-call cost — 200 M
samples per run, alternating builds, five each:

| `consume()`                | min ns/frame | the five runs                     |
| -------------------------- | ------------ | --------------------------------- |
| unbounded                  | 53.30        | 56.36, 53.30, 54.85, 54.84, 53.88 |
| bounded (`n <= available`) | 53.32        | 53.46, 53.32, 53.72, 53.64, 53.39 |

No difference; the run-to-run spread (±1.5 ns) is fifty times the gap. One
thread says the same: `write_wait_consume[f32,chunk=1024]` 85.9 → 85.9 µs over
six alternations. So `consume()` is bounded, in the header, as the one
implementation ([#1424](https://github.com/doppler-dsp/doppler/issues/1424)).

It was not an optimisation question in the end. Unbounded, the tail could pass
the head; `space()` then exceeds `capacity`, `write()` believed it, and copied
past the mapping. From the Python face that was two lines —
`buf.consume(1_000_000)` then a large `write()` — and a SIGSEGV. `write()` and
`write_some()` now also refuse to copy more than `capacity` whatever the
indices claim: one compare against a field already loaded, measured at no
cost over five alternations.

**The two-thread row** is `two_thread_write_wait_consume[f32,frame=64]` in
`bench_buffer_core`: **17.9 ns/frame, ~3.6 GSa/s** unpinned. That is three
times faster than the pinned A/B above, and the difference is *placement*:
left alone the scheduler puts the pair on SMT siblings that share a cache,
while cores 2 and 4 do not. Quote either number only with its placement.

## 7. Any capacity, and what it costs (2026-09-21)

The ring required a power-of-two capacity and rounded a sub-page one **up**,
reporting the larger number. Both were the mapping's constraints leaking into
the contract: a mask needs a power of two and a page mirror needs whole pages,
but neither is about how many samples the caller wants held.

`capacity` and `mask` were already separate fields and every guard already
used `capacity`, so honouring the request needed **no change to the index
arithmetic** — only `create()` deciding the two numbers separately, and
`sync()` / `destroy()` sizing the mapping from `mask + 1` rather than from
`capacity`.

Measured before writing it, with a 1024 mapping whose capacity was set to 1000
by hand:

|                                                                  |                     |
| ---------------------------------------------------------------- | ------------------- |
| model walk, 2 M `write_some` / `peek` + `consume`, 273,388 wraps | **0** disagreements |
| two threads, frame 250, capacity 1024 (min of 4)                 | 98.77 ns/frame      |
| two threads, frame 250, capacity **1000** over the same mapping  | 98.70 ns/frame      |

So the cost is address space alone: under 2× in the worst case (a capacity one
past a power of two), 2.4% for 1000, none for a power of two that spans a
page. `next_pow_two` is doppler's own (`util/util_core.h`), called once in
`create()`.

Two consequences worth recording. A **file-backed** ring is recognised by its
*mapped* size and the file does not record the capacity, so two requests that
round to one mapping re-attach the same file — the caller knows how much of it
was in use. And `detector`'s state blob is sized from `ring->capacity`, so it
used to depend on the page size of the machine that wrote it; it no longer
does.

The unmap was the one place this could go wrong in silence — sized from
`capacity` it frees about half of a non-power-of-two ring and nothing fails —
so the C test cycles a 65,537-sample ring 200 times and checks the process did
not grow. That sabotage was invisible to every other test.
