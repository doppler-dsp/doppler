# The ring buffer — one contiguous view of a stream

`native/inc/buffer/buffer.h` is a lock-free, single-producer /
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
| `acq`, `detector`, `detector2d`                | one     | chunks in through `write_some()`, fixed frames out through `peek()`              |
| `burst_capture`                                | one     | a **history**: samples addressed by absolute stream position; may be file-backed |
| Python `F32Buffer` / `F64Buffer` / `I16Buffer` | two     | a producer thread `write()`s, a consumer thread `wait()`s                        |

So the ring has two audiences with opposite needs: a single-threaded DSP
object that must **never** block, and a threaded pipeline stage that wants
to. §2 and §3 are the two surfaces.

## What is not known yet

- **What bounding `consume()` costs.** It would need the producer's index on
    the consumer's release path — a cross-core cache line per call. Unmeasured
    ([#1424](https://github.com/doppler-dsp/doppler/issues/1424)).
- **Two-thread throughput.** Every benchmark here is one thread alternating
    roles, which is the sequencing the acquire/release pairs are written for
    and not the case they exist for.
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

Capacity is a power of two (indexing is a mask, not a modulo) and is
**rounded up** to a whole page, so read `->capacity` back rather than
assuming the number you asked for.

## 2. Two ways to read, one way to release

|                      | blocks?                                                                                        | for                                                                                                                              |
| -------------------- | ---------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------- |
| `dp_<t>_wait(ab, n)` | **yes** — spins until `n` samples are there, the ring is closed, or the process is interrupted | a consumer on its **own thread**                                                                                                 |
| `dp_<t>_peek(ab, n)` | **never** — `n` samples or `NULL`, at once                                                     | a **single-threaded** user, where `wait()` would deadlock: the thread that would produce the samples is the one waiting for them |

Both return the same contiguous pointer and **neither consumes**. Releasing
is always `dp_<t>_consume(ab, k)`, and `k` need not equal `n` — releasing
fewer is how overlapped frames are read.

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
- **No bounds check in `consume()`.** Releasing more than `available()`
    moves the tail past the head and every later count is garbage. `peek()`
    and `wait()` return only what is there, so release no more than you were
    given.
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
