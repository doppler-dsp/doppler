# `BurstCapture`: acquisition's output, turned into bursts

**Status:** phase 1 — the Why. The object does not exist yet; this is what
it is for, what it owns, and what has to be measured before it is certified.
**Scope:** a new C object — `BurstCapture` — that sits between a detector
and whatever consumes a burst. It searches, refines, retains, and emits the
burst's SAMPLES. It stops there.
**Decision record:**
[`dsss-burst-receiver.md` §11](dsss-burst-receiver.md) — why this is being
built, including the count it was first declined on.

Tracked as [gh-1166](https://github.com/doppler-dsp/doppler/issues/1166).

______________________________________________________________________

## 1. Why — a shipped composition that dead-ends

`doppler.dsss.orchestrator.Acquirer` is a second composition over
`BurstAcquisition`: K coarse-Doppler channels, each `DDC → BurstAcquisition`,
fanned across a thread pool, with `get_state`/`set_state` per channel so the
bank ships to other pods. It feeds acquisition **continuously**, which is the
right way to drive it. Its output is `Detection` records — a Doppler and a
code phase — and there it stops.

A user of that bank who wants the burst has two routes, and both are bad.

- **Continue feeding continuously.** Anchors are stream-absolute and there is
    no sweep — and [§3.1](dsss-burst-receiver.md) bites in full:
    `acq_result_t::code_phase` measures `burst_start mod code_bins` exactly,
    so it fixes the alignment WITHIN a code period and never says WHICH
    repetition. A burst has a frame that begins in one specific repetition.
    Resolving that is a refine stage, a history ring to reach back into, a
    retention rule, and a claim rule — and
    [§3.2](dsss-burst-receiver.md) makes getting it wrong a **cliff**: a
    window one period out decodes at half the payload in error, which is
    noise, not degradation.
- **Sweep acquisition in `reset()`-per-dwell dwells**, as
    `src/doppler/examples/dsss_burst_pipeline_demo.py` does. That makes
    `pos + code_phase` a real position, so refine is not needed — paid for
    with `PRE_LEN/ACQ_HOP` overlapping FFT dwells over the whole stream, two
    window constants nothing derives, a hand-rolled cluster-and-keep-strongest
    rule, a `configure_search_raw` pin without which the auto-sizer makes a
    decision impossible, and the entire capture held in RAM to slice from.
    That last one means it does not stream.

So the ring, the refine and the claim rule exist exactly once, inside
`DsssBurstReceiver` — an object that also demodulates. The capability has no
home a caller can reach.

### 1.1 The criterion

> A caller holding a detector and a sample stream can get the burst's
> samples, aligned and never late, without writing epoch arithmetic.

"Never late" is the load-bearing half, and it is
[§3.2](dsss-burst-receiver.md)'s: the hand-off's obligation is not to be
exact, it is to never be late and to say how early it is. A capture that
emits a window starting after the preamble has destroyed the burst.

______________________________________________________________________

## 2. Use cases

| caller                      | supplies                    | wants back                                                    |
| --------------------------- | --------------------------- | ------------------------------------------------------------- |
| `DsssBurstReceiver`         | a sample stream             | aligned windows to hand to `BurstDemod`, with no copy         |
| an `Acquirer` channel       | a down-mixed sub-band       | the bursts that channel found, in that channel's own timebase |
| a recorder / corpus builder | a stream and a burst length | every burst, to disk, with the event that names where it was  |
| a second consumer           | the same stream             | windows to fan out to a despreader as well as a demodulator   |

The first row is the demanding one: it is an existing certified object whose
behaviour must not change, so the capture has to be reachable **without a
second copy of every burst**.

______________________________________________________________________

## 3. What it owns, and what it does not

| owns                                                 | does not own                                     |
| ---------------------------------------------------- | ------------------------------------------------ |
| the acquisition engine (§4)                          | the frame — what the symbols after the sync mean |
| the history ring, its span, and the retention rule   | demodulation, despreading, or any decision       |
| refine: the period-resolving correlation             | a CRC, an outer code, a randomiser               |
| CLAIM: which detections name one burst               | what a burst is FOR                              |
| the suppression window that follows an emitted burst | telemetry about a consumer's estimates           |

`burst_len` is a **construction parameter**, and that is the point of the
split. `acq_create_burst()` takes search parameters only and has no notion of
how long a burst is; teaching acquisition frame geometry to support refine
would be wrong. For a capture the burst length IS the parameter, because it
is what gets captured — and it is what sizes the retention.

______________________________________________________________________

## 4. It owns the engine — a correctness choice, not a convenience

[gh-1166](https://github.com/doppler-dsp/doppler/issues/1166) drew this
object as `push(samples, acq_results)`: the caller brings their own detector.
That face is rejected on one invariant.

> `acq_result_t::samples_consumed` is stream-absolute **only** for an engine
> fed continuously and never `reset()`, in the caller's own sample
> coordinates.

An object taking foreign results has to require that and **cannot check it**.
The failure is not a slightly wrong window: refine searches the wrong
repetition and §3.2's cliff returns noise. `push()` defining the coordinate
system makes the invariant internal and unbreakable.

What that costs is a caller with a foreign detector, and the loss is smaller
than it looks: the engine stays reachable as a child for read-backs and
`configure_search_raw()`, and a consumer receiving detections over a
transport needs the WINDOW as well — which is what
[§1.1](dsss-burst-receiver.md)'s sufficiency criterion already requires the
event to carry.

______________________________________________________________________

## 5. The surface, and the two faces of one mechanism

<!-- docs-snippet: skip=declarations quoted from burst_capture_core.h, not a compilable program -->

```c
burst_capture_state_t *burst_capture_create (
    const uint8_t *acq_code, size_t acq_code_len, size_t burst_len,
    size_t reps, size_t spc, double chip_rate, double cn0_dbhz,
    double doppler_uncertainty, double pfa, double pd);

size_t burst_capture_push (burst_capture_state_t *, const float complex *x,
                           size_t x_len, float complex *out, size_t max_out);
size_t burst_capture_events (burst_capture_state_t *, size_t n,
                             burst_capture_event_t *out, size_t max_out);

/* The C consumer's face: borrow, do not copy. */
size_t burst_capture_ready  (const burst_capture_state_t *);
const float complex *burst_capture_window (const burst_capture_state_t *,
                                           size_t i);
```

`push()` returns windows concatenated — burst `i` at `i*burst_len` — with
`events()` giving each its own record. Same shape, and the same reasoning, as
[`DsssBurstReceiver`'s §8.2/§8.3](dsss-burst-receiver.md): returning a list
means a caller *cannot* fail to drain, and two different output lengths need
two calls because the binding generator emits one variable-length output per
method.

### 5.1 A window is COPIED out of the ring, not borrowed from it

The receiver hands `burst_demod` a pointer straight into the double-mapped
ring — no copy, no seam. The obvious move is for the capture to keep doing
that and let its consumer borrow.

It does not, and the reason is lifetime. A borrow would have to stay valid
until the caller was done with it, so the retention rule would have to hold
every window emitted by the current call — and **one push can complete
several bursts**, while the ring's capacity is derived as
`2 · (refine_span + burst_len)`. Retaining an unbounded number of complete
bursts is not something that capacity promises.

So each emitted window is copied once into a grow-on-demand scratch, and both
faces read from there: `push()` copies scratch → the caller's buffer, and
`burst_capture_window()` borrows the scratch. A composing object therefore
pays **one memcpy per burst** and no more.

That is a real cost and it is named here rather than buried: it is one copy
per BURST, not per sample, which is a different order of magnitude from the
whole-stream copy [§6.1](dsss-burst-receiver.md) weighs. Whether it is
visible at all is [§8](#8-what-characterization-has-to-settle)'s first
question.

### 5.2 A partial window is never returned

`push()` truncates to a whole number of bursts. A caller handed 3.5 bursts
cannot tell where the truncation fell, and half a burst is not a burst.

______________________________________________________________________

## 6. The one behaviour that changes

`DsssBurstReceiver` arms its suppression window — the span after a burst in
which detections are the payload firing against the acquisition code, not new
bursts — when `burst_demod_demod()` returns a non-zero bit count.
[§10.3](dsss-burst-receiver.md) settled that: what arms it is the burst
having demodulated, which is a physical fact that object owns.

A capture does not demodulate. It arms the window when it **emits** — refine
resolved a start here and the whole span was handed out.

These differ in exactly one case: a full window on which the consumer's
demodulation returns nothing. In `burst_demod` that is reachable two ways —
an allocation failure, or fewer despread symbols than the frame needs — and
the second cannot happen for a window `burst_len` long by construction. So
the divergence is confined to allocation failure, where today's receiver
leaves the span unsuppressed and re-detects inside a burst it failed to
demodulate.

It is written down here because it is a behaviour change to a certified
object and must be **pinned by a test**, not argued about. Which behaviour is
better is not obvious — re-detecting inside a failed burst produces a second
attempt whose refined start lands inside the payload, which is not a
recovery — and the honest answer is that no measurement has been asked for
one.

______________________________________________________________________

## 7. What `DsssBurstReceiver` keeps

The receiver is not being thinned for its own sake; it keeps everything that
is about a decision rather than about finding a burst.

| moves to `BurstCapture`           | stays in `DsssBurstReceiver`              |
| --------------------------------- | ----------------------------------------- |
| the ring, its sizing, `chunk_max` | driving `BurstDemod`, re-seeded per burst |
| refine and its scratch            | `frame_syms` and the payload rows         |
| the retention/trim rule           | `llrs()` and the soft bits                |
| CLAIM and the detection queue     | the demodulator's own estimates           |
| the suppression window            | `events()` rows built from both halves    |

Its public API does not change. Its state blob does: the capture's blob
nests, and `DSSS_BURST_RECEIVER_STATE_VERSION` bumps. The format is
unreleased, so a layout change is free.

**The gate on the refactor is bit-exactness**, not a green suite: the
receiver's existing tests and its certification must produce the same
numbers, because every line moved is a line whose measurements were taken
against the old arrangement.

______________________________________________________________________

## 8. What characterization has to settle

Written down first, so a later sweep measures them rather than confirming a
decision already made.

- **Does the per-burst copy cost anything? — MEASURED at the test geometry,
    and no.** §5.1 chose a copy over a lifetime contract.
    `bench_burst_capture_core`, 64k blocks, minimum of 30 rounds, `ACQ_SF=31`,
    `REPS=4`, `burst_len=2448`:

    | row                    | min      | rate       |
    | ---------------------- | -------- | ---------- |
    | `push[quiet]`          | 2.242 ms | 29.2 MSa/s |
    | `push[4 bursts]`       | 2.276 ms | 28.8 MSa/s |
    | `push[4 bursts, file]` | 2.268 ms | 28.9 MSa/s |

    Four bursts — four refines and four window copies — cost 34 µs against a
    2.24 ms search floor, so the capture layer is **1.5%** on a block that is
    almost all acquisition. What is NOT settled is the same question at a real
    link geometry, where a window is 2.5 MB rather than 20 kB: the copy scales
    with `burst_len` and the search floor does not. If it becomes visible
    there, the answer is a bounded borrow window, not a bigger ring.

- **Does the queue depth still hold at a capture-only geometry?** `q_cap` is
    derived as `burst_len/refine_span`, and the receiver only ever exercised
    it where a burst was also demodulated. A recorder with a very long
    `burst_len` and a short code is a shape nothing has run.

- **What is the retention floor for a caller who never consumes?** `pending`
    is the read-back that says "a burst is held". A recorder that stops
    pushing has no way to flush; whether it needs one — a `drain()` that
    emits what it can — is unsettled, and the honest default is not to build
    it until a caller asks.

- **Is `burst_len` the right parameter, or is it two?** Retention is
    `refine_span + burst_len`, and a caller who wants a window WIDER than the
    burst (guard samples either side, for an offline analyst) currently has
    to inflate `burst_len` and get the suppression window inflated with it.
    Whether a separate capture span earns its knob is a question for a real
    recorder, not for this document.

______________________________________________________________________

## 9. Persistence — the ring in a file

§8's last question was whether a deployment could hold the look-back in RAM
**and** in the blob. `PersistentBurstCapture` is the answer for one that
cannot: the same object, with the history ring's pages backed by a file
instead of anonymous memory.

### 9.1 There is no write path

`buffer.h` builds its ring by mapping one fd **twice** at adjacent addresses,
which is what makes a window spanning the wrap contiguous. The mirror does not
care where the fd came from, so a persistent ring is that same double mapping
over an `open()`ed path — `MAP_SHARED`, so **the ring's samples ARE the file's
contents**.

That is the whole mechanism, and it is worth being explicit about what it is
not: there is no mirror buffer, no background flusher, no second copy of the
stream, and no way for the memory and the file to hold different bytes. Writes
land in the page cache and the kernel writes them back; `get_state()` calls
`msync` so a checkpoint and the history it names are consistent even across a
crash.

The primitive lives in `buffer.h` beside the anonymous one
(`dp_f32_create_backed`, `dp_f32_sync`), not in this object — a second private
ring is exactly the drift this repo forbids, and every `DECLARE_DP_BUFFER`
type gets the capability for free.

### 9.2 What it buys: the blob stops carrying the history

§8's measurement was that the look-back IS the blob. Backed, it is not.
Measured 2026-09-01 at `ACQ_SF=511`, `REPS=5`, `DATA_SF=63`, `spc=4`:

| frame    | `retain_span` | in-RAM blob | backed blob | ring file |
| -------- | ------------- | ----------- | ----------- | --------- |
| 61 sym   | 74 648        | 0.61 MB     | 17.0 kB     | 2.10 MB   |
| 1029 sym | 318 584       | 2.57 MB     | 17.5 kB     | 8.39 MB   |
| 8029 sym | 2 082 584     | 16.68 MB    | 21.6 kB     | 33.55 MB  |

A checkpoint at the long geometry drops from 16.68 MB to 21.6 kB — **770×** —
and what is left is the acquisition child plus the detection queue. The file
is larger than `retain_span` because the ring is twice the retained span
rounded up to a power of two; it is written once and reused, not accumulated.

The second thing it buys is that the history **outlives the process**. Point a
new capture at the same path and the samples are already there; restore the
blob and it reaches back across the restart into a burst that began before it.

**And it costs nothing measurable**, which is the claim the mechanism has to
earn: `push[4 bursts]` at 2.276 ms against `push[4 bursts, file-backed]` at
2.268 ms — the two rows are within each other's spread, because `MAP_SHARED`
means there is no second write to be slower than. `bench_burst_capture` keeps
both rows side by side, so the day that stops being true is the day the pair
separates.

### 9.3 Two refusals, both deliberate

- **A backed blob does not restore into an in-RAM capture, or the reverse.**
    `state_bytes()` differs, so jm's length check rejects it. They are
    different configurations, and silently accepting one for the other would
    resume a capture whose history was somewhere else entirely.
- **A blob claiming retained history, restored against a file that has none,
    is refused.** `create()` reports whether it adopted a ring of exactly this
    geometry or made a fresh (zeroed) one. Without that check the positions
    would be perfectly valid and the samples would be zeros, so the capture
    would simply never find another burst — indistinguishable from a quiet
    stream, which is the failure this object exists to prevent.

### 9.4 Why a view rather than an argument

The two constructors differ and nothing else does, so
`PersistentBurstCapture` is a `[[burst_capture.views]]` entry over the same
core — the mechanism `MatchedDDC` established: a difference in CONSTRUCTOR is
a flavour, a difference in METHOD SIGNATURE would be a separate type. The
methods are shared verbatim, so there is one algorithm rather than two.

An optional `path=None` on the base constructor was the alternative and is
worse twice over: a non-required string init-param renders its own
"create with defaults" doctest as `BurstCapture(path=0)`, which fails the
stub-doctest gate, and a backing file with a default is a capture quietly
persisting somewhere nobody named.

______________________________________________________________________

## 10. See also

- [`DsssBurstReceiver`](dsss-burst-receiver.md) — the composition this comes
    out of; §11 is the decision record, §3 the measurements the refine stage
    rests on
- [DSSS acquisition](dsss-acquisition.md) — the detector's own architecture
- [Adding an algorithm — the lifecycle](../dev/contributing/adding-algorithms.md) —
    the phase order this document is phase 1 of
- [State serialization](state-serialization.md) — the triplet this object owes
