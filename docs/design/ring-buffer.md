# The ring buffer — one contiguous view of a stream

`native/inc/doppler/buffer/buffer.h` is a lock-free, single-producer /
single-consumer ring of complex samples. It makes four promises, and this
page says what each one means and where it stops.

| promise           | what it means                                                            | where it stops                                           |
| ----------------- | ------------------------------------------------------------------------ | -------------------------------------------------------- |
| **zero-copy**     | a reader gets a pointer *into the ring*, never a copy                    | the pointer is good until those samples are `consume()`d |
| **free wrapping** | a batch that crosses the end of the ring is still one contiguous pointer | `n` cannot exceed the capacity                           |
| **lock-free**     | producer and consumer never block each other                             | exactly one of each; everything here is single-sided     |
| **simple**        | six verbs, no modes                                                      | the ring does not own a thread, a clock or a policy      |

Not to be confused with [Ending a Wait](io-termination.md), which is the
contract for *when a blocking wait gives up*, across network, memory and
disk. This page is the memory transport itself.

______________________________________________________________________

## Who uses it, and how

| caller                                         | threads | how it reads                                                                     |
| ---------------------------------------------- | ------- | -------------------------------------------------------------------------------- |
| `acq`                                          | one     | writes into free space it computes, reads fixed frames off `data`, `consume()`s  |
| `detector`, `detector2d`                       | one     | chunks in through `write_some()`, fixed frames out through `peek()`              |
| `burst_capture`                                | one     | a **history**: samples addressed by absolute stream position; may be file-backed |
| Python `F32Buffer` / `F64Buffer` / `I16Buffer` | two     | a producer thread `write()`s, a consumer thread `wait()`s                        |

So the ring has two audiences with opposite needs: a single-threaded DSP
object that must **never** block, and a threaded pipeline stage that wants
to. §2 and §3 are the two surfaces.

## What is not known yet

- **Whether `burst_capture`'s position-addressed history belongs in the
    ring** (`at(pos)`, `restore(head, tail)`) or stays that object's own
    ([#1425](https://github.com/doppler-dsp/doppler/issues/1425)).

## 1. Why a batch never splits

The pages are mapped **twice**, back to back. Address `base + i` and address
`base + capacity + i` are the same physical sample, so a batch that starts
near the end of the ring simply runs on into the second mapping: the MMU does
the wrap, not the code. There is no "two halves" case to write, to test, or
to get wrong in a SIMD kernel.

That is a measured claim, not a hoped-for one. `bench_buffer_core` reads a
batch size that never straddles the seam against one that usually does:

```text
write_wait_consume[f32,chunk=1024]   0.328 ns/sample     (never straddles)
write_wait_consume[f32,chunk=1000]   0.329 ns/sample     (usually does)     1.00x
```

**The capacity is whatever was asked for; the mapping is what gets
rounded.** Indexing is a mask, not a modulo, so `->mask + 1` samples are
mapped — a power of two — and the mirror is built from whole pages, so at
least one. `->capacity` stays the caller's number and every guard uses it:
the slack between the two is address space, never room. It costs nothing per
call, because the two were already separate fields, and it makes the capacity
the same on every machine — it used to be the *capacity* that was rounded, so
`512` asked was 512 on Linux x86, 2048 on macOS arm64 and 8192 on Windows
([measurements §7](ring-buffer-measurements.md#7-any-capacity-and-what-it-costs-2026-09-21)).

## 2. Two ways to read, one way to release

|                      | blocks?                                                                                        | for                                                                                                                              |
| -------------------- | ---------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------- |
| `dp_<t>_wait(ab, n)` | **yes** — spins until `n` samples are there, the ring is closed, or the process is interrupted | a consumer on its **own thread**                                                                                                 |
| `dp_<t>_peek(ab, n)` | **never** — `n` samples or `NULL`, at once                                                     | a **single-threaded** user, where `wait()` would deadlock: the thread that would produce the samples is the one waiting for them |

Both return the same contiguous pointer and **neither consumes**. Releasing
is always `dp_<t>_consume(ab, k)`, and `k` need not equal `n` — releasing
fewer is how overlapped frames are read.

`consume(k)` refuses `k > available()` — `DP_ERR_INVALID`, nothing released —
because past that the read position overtakes the write position and the two
indices stop describing a ring. A caller that releases only what it was lent
never meets it. The bound is free: the read that preceded it already loaded
the producer's index
([measurements §6](ring-buffer-measurements.md#6-consume-is-bounded-after-all-and-two-threads-2026-09-21)).

`NULL` has more than one meaning, and they call for different responses, so
one function owns the precedence — `dp_<t>_wait_status(ab, n)`:

| status                | meaning                          | the right response                       |
| --------------------- | -------------------------------- | ---------------------------------------- |
| `DP_WAIT_OK`          | `n` samples are readable         | read them                                |
| `DP_WAIT_PENDING`     | fewer than `n` so far            | nothing is wrong; come back              |
| `DP_WAIT_TOO_LARGE`   | `n` exceeds the capacity         | a caller bug — it can never be satisfied |
| `DP_WAIT_CLOSED`      | closed, with fewer than `n` left | the normal end of a stream               |
| `DP_WAIT_INTERRUPTED` | the process was asked to stop    | stop                                     |

Too-large is checked first; readable samples win over *closed* (a closed ring
still drains); *closed* wins over *interrupted*.

## 3. Two ways to write

|                                 | when the ring lacks room                                           | for                                                                                     |
| ------------------------------- | ------------------------------------------------------------------ | --------------------------------------------------------------------------------------- |
| `dp_<t>_write(ab, src, n)`      | **refuses the whole call**, returns `false`, adds `n` to `dropped` | a frame that is meaningless in part                                                     |
| `dp_<t>_write_some(ab, src, n)` | writes what fits, returns how many                                 | a **stream** — including a chunk larger than the ring, which `write()` can never accept |

`dp_<t>_space(ab)` is the room `write()` is guaranteed to accept. `dropped`
counts samples in *refused calls*, not samples lost: a producer that retries
keeps its data and still moves the counter. `write_some()` never touches it.

Both policies, `space()`, and the answers `wait_status()` gives, in one
program:

```c
--8<-- "native/examples/ring_write_policy_demo.c"
```

## 4. The two patterns

### Drip-feed until a frame is there, then take it

Samples arrive in pieces smaller than a frame. `peek()` answers "is there a
frame yet?" without ever blocking.

```c
--8<-- "native/examples/ring_drip_feed_demo.c"
```

### Stream chunking: any chunk in, fixed frames out

The shape every FFT front end needs: input arrives in large or irregular
chunks, the transform wants exactly `nfft`. Feed with `write_some()`, drain
with `peek()`, and alternate — which is also how a chunk **larger than the
ring** goes through it.

```c
--8<-- "native/examples/ring_chunking_demo.c"
```

The loop costs about **3%** over `write()` + `wait()` at a 1024-sample frame,
and chunks much larger than the frame cost a further ~5% in cache geometry
that no API can remove
([measurements §4](ring-buffer-measurements.md#4-what-the-non-blocking-surface-costs)).

That loop is the same in every consumer of fixed frames, so it has a home:
[§9, the framed face](#9-the-framed-face-any-chunk-in-fixed-frames-out).

## 5. Threading, and what "single-sided" buys

Every function belongs to one side, and reads the *other* side's index in
the direction that can only go stale safely:

| producer side                           | consumer side                                         | both sides quiescent |
| --------------------------------------- | ----------------------------------------------------- | -------------------- |
| `write`, `write_some`, `space`, `close` | `wait`, `peek`, `available`, `consume`, `wait_status` | `reset`, `destroy`   |

`space()` may under-report (the consumer frees room concurrently) and
`available()` may under-report (the producer adds samples concurrently);
neither can over-report, so neither can license an overrun. `reset()`
empties the ring and **reopens** it (`closed` is cleared; `dropped` is a
lifetime count and is kept). It writes both indices, so it is for the
single-threaded user and for between runs.

The two-thread shape whole — backpressure from `space()`, and an end said
with `close()`, after which `wait()` returns `NULL` and `wait_status()`
says why:

```c
--8<-- "native/examples/ring_threaded_demo.c"
```

## 6. What the ring deliberately does not do

- **No thread, no timeout.** `wait()` spins; it is for a consumer that has a
    core to spend. A single-threaded user calls `peek()`.
- **No ownership of the pointer's lifetime.** A pointer from `wait()` or
    `peek()` is valid until those samples are consumed; after that the
    producer may overwrite them. Zero-copy means exactly that.

## 7. The element-typed face

The ring stores scalars, two per complex sample. `DECLARE_DP_BUFFER_VIEW`
stamps the same four calls typed as one **element** per sample —
`float _Complex`, `double _Complex`, and for 16-bit I/Q the record
`dp_iq16_t {i, q}`, since C has no complex integer. Each is a cast: every
count in the header is already in samples, so the two faces cannot disagree
about a length. It is the face the Python binding is generated over.

```c
--8<-- "native/examples/ring_element_view_demo.c"
```

## 8. A ring whose samples are a file

`dp_<t>_create_backed()` is `create()` over a path. The mapping is shared, so
the ring's samples **are** the file's contents — no write-to-disk step, no
copy, nothing to disagree. `existed` says whether the file already held a
ring of this size; `dp_<t>_sync()` is the checkpoint. What persists is the
samples: the positions live in the struct, so resuming from them is the
caller's bookkeeping.

```c
--8<-- "native/examples/ring_backed_demo.c"
```

## 9. The framed face: any chunk in, fixed frames out

The loop in §4 — write what fits, peek a frame, consume the hop — is the same
in every consumer of fixed, possibly overlapping frames, and its order is easy
to get wrong. `DECLARE_DP_BUFFER_FRAMES` stamps it once, beside the
element-typed face, as a **framer** that owns the ring:

| call                                 | what it does                                                                                                                                                                                              |
| ------------------------------------ | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `dp_<t>_framer_init(fr, ring, N, H)` | frames of `N` samples, `H` apart; frame *k* covers stream samples `[k·H, k·H + N)`                                                                                                                        |
| `dp_<t>_framer_feed(fr, in, n, max)` | takes input — **only as far as the frames it yields fit `max`** — and returns how many it took                                                                                                            |
| `dp_<t>_framer_next(fr)`             | the next frame, zero-copy, or `NULL`; the pointer holds until the next framer call                                                                                                                        |
| `dp_<t>_framer_flush(fr, row)`       | ends the stream: the one zero-padded row it still owes, **on the hop grid** (1), or none (0); refused (negative) while whole frames are still buffered                                                    |
| `dp_<t>_framer_drained(fr)`          | true once `next()` would return `NULL`: the precondition of `flush` and of a snapshot                                                                                                                     |
| `dp_<t>_framer_frames_in(fr, n)`     | frames the stream yields once `n` more samples are in, however many feed-and-drain rounds that takes: the bound a consumer sizes its output by (`frames_for` is the same count capped by one feed's room) |
| `dp_<t>_framer_state_bytes/get/set`  | the carry as a standard state blob, fixed-size for a given `N`                                                                                                                                            |

Three properties are the point, and each is tested rather than hoped for:

- **The frames are a function of the stream, not of the chunking.** One giant
    chunk, seven samples at a time and room for a single frame per call produce
    the same frames, bit for bit.
- **Once drained, fewer than `N` samples are left.** `feed()` never admits a
    frame the caller has no room for, so a short output buffer slows the
    stream down and loses nothing — and the leftover is small enough to snapshot
    at a size that depends on `N` alone, so a fresh framer accepts the blob.
- **`flush()` lands on the grid.** The last row starts at a multiple of `H`,
    not at the first uncovered sample, and exists only if it holds a sample no
    earlier frame covered. Afterwards the framer starts again at sample 0.

The framer owns its ring: nothing else writes to it or reads it, which is what
makes `feed()` the only way in. A reader that needs absolute positions into
history (`burst_capture`) uses the ring directly.

```c
--8<-- "native/examples/ring_framer_demo.c"
```

It costs what the loop it replaces costs: the same 0.356 ns/sample at a
1024-sample frame with no overlap, and the same 1.190 at 75% overlap
([measurements §9](ring-buffer-measurements.md#9-the-framed-face-costs-what-the-hand-loop-costs-2026-10-08)).

Its claims are certified like the ring's, in C, because it has no Python face:
`native/validation/framer_certify.c` measures and
`src/doppler/tests/validation/framer/results.md` is the generated report, with
the header's eighteen claims inventoried against their pins and the sabotage
that turned each red.
