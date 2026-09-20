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
