# `DsssBurstReceiver`: the burst chain, composed in C

**Status:** draft / for discussion
**Scope:** a new composed C object — `DsssBurstReceiver` — that owns the
burst chain end to end, the way
[`DsssReceiver`](../api/python-dsss.md) already owns the continuous one.
It composes the existing, already-certified `Burst*` cores; it is not new
DSP. What is new is the **hand-off**, which today no object owns.

Tracked as [gh-1001](https://github.com/doppler-dsp/doppler/issues/1001).

______________________________________________________________________

## 1. Why — the thing that is missing

Every piece of the burst chain exists and is certified — see
`src/doppler/dsss/tests/validation/burst_acq/results.md`,
`.../burst_despreader/results.md` and `.../burst_demod/results.md`. What does
not exist is the object that puts them together.

The continuous chain has one. `dsss_receiver_core.c` calls
`acq_build_handoff()` — both convention inversions in one place — and then
derives the unprocessed tail (`samples_fed - samples_consumed`) so no sample
is lost or reprocessed across acquisition to tracking. Both of those are
hand-off concerns, and both live in C exactly once.

The burst chain has neither. Its hand-off is arithmetic every caller redoes
in Python, and the cost is already on the record: **one bin-to-frequency fold
was restated in four Python call sites, in three mutually inconsistent ways**,
one of them spelled `((bin + n/2) % n) - n/2` — the exact form `acq_core.h`
names as a past full-span sign inversion that surfaced as a receiver
reporting lock while decoding noise. That was fixed in `02b7714c` by giving
the fold one home (`dp_fftfreq`), but the fold was the symptom. Four
hand-rolled copies is what a seam with no owner produces, and the next
caller starts the count again.

Nor can it simply copy that shape, because it runs the other way. A tracking
receiver only ever goes **forward** — the unprocessed tail is exactly what it
needs. A burst receiver is handed an *end* anchor and must reach **back** to
a preamble start already gone past, so it needs bounded look-back where
`DsssReceiver` needs a tail (§7.1). That single reversal is what most of the
rest of this design follows from.

### 1.1 The criterion

> A burst event must contain everything the demodulator needs, when the
> event is all that is exchanged.

This is the requirement the component exists to satisfy, and it is stronger
than "the demo works". It is the standard
[`async-dsss-spec.md` §`DetectionEvent`](async-dsss-spec.md) already sets for
the continuous chain — "consumable by another process/service, not just
another Python object in the same interpreter" — applied to bursts.

______________________________________________________________________

## 2. Use cases — who calls this, and what they do with the answer

| caller                                 | supplies                                         | wants back                                                                                               |
| -------------------------------------- | ------------------------------------------------ | -------------------------------------------------------------------------------------------------------- |
| a burst link receiver                  | a sample stream, the two codes, the sync word    | payload bits + a CRC verdict, per burst                                                                  |
| a service pipeline (DDC → acq → demod) | one `DetectionEvent` per burst, over a transport | a decode that does not need the producer's config or its stream position                                 |
| a capture analyst                      | a file and a geometry                            | every burst found, scored, with the ones that failed distinguishable from the ones that were never there |
| an existing caller                     | today's hand-wired chain                         | the same results, with the epoch arithmetic deleted                                                      |

The second row is the demanding one and it is the reason this is a C object
rather than a Python helper: a consumer in another process has neither the
producer's `dwell_pos` nor its configuration, so anything a caller has to
compute from those cannot cross the boundary.

______________________________________________________________________

## 3. What a prototype measured

A throwaway Python prototype (per the
[lifecycle spine's](../dev/contributing/adding-algorithms.md) phase 1, not
committed) drove the real C objects across a compact geometry —
`ACQ_SF=127`, `REPS=4`, `SPC=4`, code period `P = 508` samples — to find out
which of this design's assumptions survive contact. Four did not.

### 3.1 `code_phase` is a residue, not a position

`acq_result_t.code_phase` measured exactly `burst_start mod code_bins` at
every offset tried, including one sample short of a period and one exactly
on it. The half of that nothing states is the **modulo**.

The consequence is structural. Place the search window one whole code period
early and the reported `code_phase` **does not move** — a repeated preamble
is periodic, so the lag cannot distinguish the repetitions, and neither can
the chip phase derived from it (`dll_init_chip_from_acq` returned the
identical value for windows a full period apart).

> **A phase seed resolves alignment WITHIN a code period and never WHICH
> period.**

For `DsssReceiver` that is harmless — a tracking loop runs on from wherever
it is. For a burst it is the whole problem, because a burst has a frame that
begins in one specific period.

### 3.2 An epoch error is recoverable in one direction only

`set_prior(f0, start)` takes the preamble's start *within the window*, so a
window that begins early can declare the residual:

| window placement   | `start` declared | `frame_valid` | payload errors |
| ------------------ | ---------------- | ------------- | -------------- |
| exact              | 0                | True          | 0 / 64         |
| late by 1 period   | 0                | False         | 29 / 64        |
| late by 2 periods  | 0                | False         | 37 / 64        |
| early by 1 period  | 0                | False         | 28 / 64        |
| early by 2 periods | 0                | False         | 27 / 64        |
| early by 1 period  | `P`              | **True**      | **0 / 64**     |
| early by 2 periods | `2P`             | **True**      | **0 / 64**     |

Every failure sits near half the payload — noise, not degradation. This is a
cliff, not a gradient, so there is no partial credit to design toward.

> **The hand-off's obligation is not "be exact". It is: never be late, and
> say how early you are.**

That is why the sufficient event is a **pair** — a never-late window plus
the offset from it to the preamble — and not a single position.

### 3.3 Timing is the fragile half; frequency is not

`BurstDemod` re-estimates frequency from the preamble, so `f0_coarse` only
has to land inside its pull-in. Measured: it pulls in a seed error of **4
Doppler bins** and fails at 8 — bounded, which is what makes the number worth
quoting. A hit on the bin grid is within **half a bin** by construction, so
the frequency path runs with roughly **8× margin** while a *single* code
period of timing error is fatal.

`est_freq_hz` also reports the truth rather than the seed: seeded a full bin
away, it landed within an eighth of a bin of the injected residual. A
consumer may therefore pass it on as a measurement, which is what makes the
event chainable rather than merely consumable.

### 3.4 The missing stage — refine resolves the period

§3.1 says a *code-period* correlation cannot name the repetition. The
**preamble** can, because it has finite extent — its edges break exactly the
periodicity a bare code correlation is blind to. Score a candidate offset by
correlating one code period at each of the `REPS` positions the preamble
would occupy and summing the **magnitudes**; the score at a whole-period
offset `k` follows the triangular overlap envelope `(REPS - abs(k)) / REPS`,
because only `REPS - abs(k)` of those positions still land on preamble.

**Combine non-coherently, one code period at a time — not the whole preamble
as one reference.** This correction cost a measurement: the first version
correlated all `REPS × P` samples coherently and was verified on a capture
with *zero* Doppler, where it cannot fail. Acquisition leaves up to half a
bin of residual, and a coherent integration that long does not survive it:

| residual Doppler | coherent whole preamble                                    | non-coherent per period |
| ---------------- | ---------------------------------------------------------- | ----------------------- |
| 0                | exact                                                      | exact                   |
| 0.25 bin         | **wrong by 2 periods** (true position 639× below the peak) | exact                   |
| 0.50 bin         | **wrong by 1 period** (310× below)                         | exact                   |

So the per-period form is not an optimization, it is the mechanism. Each
correlation spans one code period rather than `REPS` of them, so the phase
rotation a half-bin residual produces stays small enough to integrate
through, and the envelope survives. It is the same coherent-then-non-coherent
split `acq` itself uses, applied to a finer question.

Measured at three noise levels, against a coarse guess deliberately placed
two periods early:

| offset from true start | measured ratio | predicted |
| ---------------------- | -------------- | --------- |
| −2 periods             | 0.500          | 0.500     |
| −1 period              | 0.750          | 0.750     |
| **0 (true)**           | **1.000**      | **1.000** |
| +1 period              | 0.728          | 0.750     |
| +2 periods             | 0.465          | 0.500     |

The argmax landed on the true start **exactly** — to the sample, not to the
period — at σ = 0.02, 0.6 and 1.6 alike, and at 0, 0.25 and 0.5 bins of
residual Doppler.

> **This is the stage the chain is missing.** Acquisition's job is to get
> close; resolving the exact preamble start is a *refine*, and it has a
> primitive behind it rather than a tiebreak rule.

It is also the same stage
[`dsss-use-cases.md`](../dev/contributing/dsss-use-cases.md) already calls for on
the frequency axis ("refine only if needed: column-FFT over the reps within
the winning coarse bin"), and the chain
[`async-dsss-spec.md`](async-dsss-spec.md) records as validated end to end —
"search → handoff → **refine** → track". The burst path simply never grew
one, which is why §6.1 looked like an open question instead of an absent
component.

Note the discrimination this buys is `(REPS-1)/REPS` against the nearest
competitor — 2.5 dB at `REPS = 4`, improving with `REPS`. That margin is a
number to characterize (§6.1), not a risk to the mechanism.

### 3.5 Nothing in a hand-wired chain observes a broken hand-off

Windowed one code period off, the carrier is still genuinely present, so the
Costas metric is genuinely high: `lock_metric` read **0.99** while the
despread output carried a data-aided Es/N0 of **−14.8 dB** (38/93 bit
errors).

This is not a defect in the metric — its two documented ends are certified
in `burst_despreader`'s own report, and a carrier-lock indicator is only a
threshold test on the carrier. It is that a chain assembled by a caller has
no member whose job is to validate the seam. It is the same shape
`acq_core.h` records historically: a receiver reporting tracking while
decoding noise. A composed object can check its own hand-off; a hand-wired
one structurally cannot.

______________________________________________________________________

## 4. What this means for `DetectionEvent`

[`async-dsss-spec.md`](async-dsss-spec.md) specifies `DetectionEvent` with
`chip_phase` (chips), `samples_consumed`, `doppler_hz_est`, `doppler_res_hz`,
`cn0_dbhz_est` and three diagnostics. Measured against §3, that record is
**sufficient for a tracking consumer and insufficient for a burst one**, and
the reason is precise rather than an oversight:

- `chip_phase` is a residue (§3.1) — it cannot name a period.
- `samples_consumed` is an **end** anchor: the offset the detection's epoch
    *ended* at. Deriving a start from it (`samples_consumed - n + code_phase`)
    yields the epoch of whichever repetition that frame locked, which is
    never *early* and can be up to one period **late** — the unrecoverable
    direction (§3.2).

So every field in the record is either a residue or a never-early anchor,
and the burst needs a never-late one. The record therefore gains a burst
epoch. The proposal is a single field:

| field            | type       | description                                                                     |
| ---------------- | ---------- | ------------------------------------------------------------------------------- |
| `preamble_start` | `uint64_t` | Stream-absolute sample index at which this burst's preamble begins. Never late. |

One field rather than the (window, residual) pair, because the pair is an
*internal* mechanism: the composing object holds the stream position and can
resolve the pair into an exact start before the event is emitted. A consumer
should receive an answer, not a subtraction to perform. **This is exactly
the field a caller cannot compute** — it needs the engine's own stream
position, which is why it must be produced here and not by the caller.

`acq_build_handoff()` also has to learn the burst front door. Its doc comment
states the current assumption plainly — `state` built via
`acq_create_continuous()`, because it reads `doppler_bin` as a frequency-
*window* index — while an engine from `acq_create_burst()` reports a
*coherent* bin. Extending it is a precondition, not a side quest.

______________________________________________________________________

## 5. Design goals

1. **All C.** The composition, the fold, the epoch and the seeding live in
    `native/src/dsss_burst_receiver/`. The Python face is a thin binding, as
    with every other object.
1. **Compose, never re-implement.** `burst_acq`, `burst_despreader` and
    `burst_demod` cores are linked, not copied. The refine stage composes the
    existing correlation primitive against a reference the receiver already
    holds. No DSP is written here.
1. **Three stages, named.** `search → refine → demod`. Acquisition is not
    asked to produce an exact epoch; refine is what produces it (§3.4). A
    design that leaves refine implicit is the one that ends up doing it with
    arithmetic at every call site.
1. **Emit the event, and consume only the event.** The receiver's own
    internal path must go through the same `DetectionEvent` a remote consumer
    would get. If the record is insufficient, the in-process path must break
    too — otherwise the sufficiency criterion is enforced by nothing.
1. **Validate its own hand-off.** §3.5 is a gap the composition can close: it
    knows the expected preamble and can score the seam rather than trusting
    it.
1. **Mirror `DsssReceiver` where the problem is the same** — including the
    unprocessed-tail derivation, so no sample is lost across acquisition to
    demodulation.
1. **Retain, do not re-derive.** Every stage reaches backwards from an end
    anchor, so the receiver owns a bounded history and reuses the existing
    double-mapped ring rather than growing a new buffer type (§7.1). A
    dropped sample is a lost burst, so an overrun must be loud.

### 5.1 Non-goals

- No new detector, despreader or demodulator behaviour. If a measurement
    here wants one of those changed, that is an issue against that object.
- Not a replacement for the objects' individual use. A caller wiring them by
    hand stays supported; it just stops being the only option.
- No transport. The event is a flat POD; who serializes it is the caller's
    concern, exactly as `async-dsss-spec.md` has it.

______________________________________________________________________

## 6. The unknowns — what characterization has to settle

Written down first, so a later sweep measures them rather than confirming a
decision already made.

### 6.1 The refine stage's margin and span

The *mechanism* is settled (§3.4): correlate one code period at each of the
preamble's `REPS` positions, sum the magnitudes, and the triangular overlap
envelope names the repetition. What is not settled is its operating
envelope.

- **The discrimination margin — MEASURED** (phase 7,
    `src/doppler/dsss/tests/characterization/dsss_burst_receiver/`). At
    `REPS = 4`, `ACQ_SF = 31`: `refine_margin` sits at **0.774** against the
    predicted `(REPS-1)/REPS = 0.750` from 100 dB-Hz down to about 70, then
    climbs as the rival closes — 0.81 at 66 dB-Hz, 0.83 at 63, 0.87 at 56.5.
    The correct-period rate holds at 100% to roughly 60 dB-Hz and falls off a
    cliff below: 88% at 57.7, 58% at 56.5. So the knee for this geometry is
    **~58–60 dB-Hz**, and `refine_margin` is a usable health signal rather
    than an ornament: it reads ~0.87 where the stage is failing against
    ~0.775 where it is not.

    **How the floor moves with `REPS` — MEASURED, and it is a trade.** The
    floor is `(REPS-1)/REPS`, so it RISES with depth: 0.55 measured at
    `REPS=2`, 0.77 at 4, 0.89 at 8, 0.94 at 16. The separation between a
    resolved burst and an unresolved one therefore **halves with every
    doubling** — 0.37, 0.20, 0.10, 0.05 — while the acquisition knee
    improves from 66.0 to 54.9 dB-Hz over the same range. More repetitions
    buy sensitivity and cost discrimination. Concretely: compare
    `refine_margin` against `(REPS-1)/REPS`, never against a constant; the
    certification asserted a fixed 0.9 until this sweep was run, which is
    correct at `REPS=4` and wrong at 8 and 16.

    **A margin measured only where it does not fail is not a margin** — and
    this section's own first draft is the cautionary case: the coherent form
    was "verified" on a zero-Doppler capture and was wrong by two code
    periods the moment a quarter of a bin was present.

- **What actually loses bursts is the CODE, not the framing.** The same
    characterization swept the burst's start across a whole acquisition frame
    at 100 dB-Hz. With a good code every offset is found; with a structured
    code whose peak-to-worst-sidelobe ratio is 1.07, **47% are lost** —
    because the CFAR reference is then set by the code's own autocorrelation
    sidelobes, leaving no margin for a preamble that straddles two
    non-overlapping frames. This design nearly acquired an overlapping-dwell
    requirement on the strength of that first measurement; the framing was
    never the problem. Choose the preamble code on its autocorrelation.

- **The search span.** Acquisition bounds the error to whole code periods
    within the preamble, so a span of `± REPS · P` is sufficient and probably
    generous. The cost is one correlation per candidate offset; whether that
    is `REPS` coarse candidates (period-spaced, then interpolate) or a full
    dense search over the span is a cost/robustness trade to measure.

- **Whether refine also closes the frequency.** The same stage could refine
    sub-bin Doppler — either the column-FFT
    [`dsss-use-cases.md`](../dev/contributing/dsss-use-cases.md) describes, or
    `corr2d`'s interpolated inverse
    ([design](corr2d-interpolated-inverse.md)), which yields sub-chip delay
    and sub-bin Doppler at no extra forward cost. §3.3 says the frequency
    path already has ~8× margin without it, so this is an option to justify
    rather than a requirement — and the honest default is not to build it
    until a measurement asks for it.

- **Whether refine is a separate object.** It composes `Corr` against a
    reference the receiver already holds, so it may be a private function
    rather than a new public type. That decision should follow the phase-3
    implementation, not precede it.

- **Whether the history ring should be acq's.** §7.1 has the receiver keep
    its own, costing one `memcpy` of the stream, because `acq_push()`
    consumes eagerly. The alternative is a retention policy on `acq`'s
    existing ring so there is one copy instead of two. That is a change to a
    certified object, so it needs a measurement showing the copy matters
    before it is worth the blast radius — and at cf32 rates the copy may well
    disappear against the FFT.

### 6.2 Smaller unknowns

- **Does the receiver need the overlapping sweep at all?** The engine frames
    every code period, and the preamble is `REPS` periods, so several complete
    frames should land inside it regardless of alignment — suggesting a
    continuously-fed engine needs no `reset()`-per-dwell sweep, which would
    also make `samples_consumed` stream-absolute for free. The prototype saw
    this behaviour but did not test it as the primary path.
- **Which consumer is the default** — `BurstDespreader` (tracked) or
    `BurstDemod` (feedforward)? They have different burst-length regimes and
    the object should probably expose both rather than choose.
- **Serialization asymmetry.** `BurstAcquisition` is serializable;
    `BurstDemod` deliberately is not, and its report certifies that as correct
    (a burst completes within one `demod()` call or is lost). The composition
    must carry that asymmetry rather than paper over it — likely meaning the
    receiver's state is the acquisition's plus the in-flight burst window.
- **`cn0_dbhz_est` is a lower bound**, saturating at the code's
    autocorrelation-sidelobe floor and under-reporting true C/N0 by up to
    38 dB at the top of the measured range. It cannot seed `set_prior` — that
    signature has no SNR parameter at all — so its role here is gating and
    dwell sizing only. Whether the receiver should act on it is unsettled.

______________________________________________________________________

## 7. Implementation sketch

```text
native/inc/dsss_burst_receiver/dsss_burst_receiver_core.h
native/src/dsss_burst_receiver/dsss_burst_receiver_core.c
objects/dsss_burst_receiver.toml
```

State: the composed `burst_acq_state_t *`, the chosen consumer's state, the
history ring (§7.1), the in-flight burst, and `samples_fed`. Config: the two
codes, the sync word, the geometry, and which consumer to drive.

```text
push(x, n) ->
    SEARCH   feed burst_acq; collect hits with stream-absolute anchors
    per burst cluster:
        REFINE   score offsets over +/- REPS*P: one code-period
                 correlation per preamble position, magnitudes
                 summed non-coherently                            (§3.4)
                 -> exact preamble_start, to the sample
        EVENT    build DetectionEvent (fold via dp_fftfreq, one home)
        DEMOD    window the burst from the event ALONE            (goal 4)
                 drive the consumer, score the seam               (goal 5)
                 emit payload + verdict
    carry the unprocessed tail forward                            (goal 6)
```

The three stages are the point. `search → refine → demod` is the chain
`async-dsss-spec.md` records as validated; the burst path had the first and
last and was quietly asking acquisition to do refine's job with arithmetic.

### 7.1 Look-back — the receiver retains what acquisition releases

Every stage above reaches **backwards**, and that is the structural
difference from `DsssReceiver`. A tracking receiver only ever goes forward:
its hand-off derives the unprocessed *tail* and continues from there.
A burst receiver is handed an **end** anchor — `samples_consumed` is the
offset a detection's epoch *ended* at — and then has to reach back to a
preamble start that has already gone past.

How far back is bounded, and by the geometry rather than by a guess. A
detection fires on any frame lying inside the preamble, so the **last** one
can fire a full preamble-span after the burst began; refine then searches
about that. Two terms:

| term          | span         | why                                                              |
| ------------- | ------------ | ---------------------------------------------------------------- |
| detection lag | `REPS · P`   | the last frame inside the preamble is that far past its start    |
| refine span   | `± REPS · P` | §6.1 — bounded by the preamble, generously                       |
| forward hold  | `BURST_LEN`  | the demod needs the whole burst, which arrives *after* detection |

so the retained span is roughly `2·REPS·P + BURST_LEN`, rounded up to a
power of two.

**The primitive already exists, and `acq` already depends on it.**
`native/inc/buffer/buffer.h` generates a double-mapped SPSC ring
(`dp_f32_t` for cf32), and `acq_core.h` includes it — `acq_state_t` holds
`dp_f32_t *ring`, "the only ring". So this is not a new dependency for a DSP
core, and **no new type is needed**: `DECLARE_DP_BUFFER` already covers the
dtype. The double mapping is the property that matters here — a window
spanning the wrap comes back as one contiguous `float complex *`, so the
burst can be handed to `demod()` with no copy and no seam.

**What it does not already do is retain.** `acq_push()` calls
`dp_f32_consume(st->ring, frame_n)` on every frame it processes
(`acq_core.c:906`), so by the time a hit is emitted acquisition has
*released* the samples the receiver still needs. The receiver therefore
keeps its **own** history ring alongside acq's, rather than borrowing one it
does not own.

> **Do not read behind acq's tail.** The samples are still physically mapped
> after `consume()` and can be dereferenced, which makes this the tempting
> shortcut. It is reading released memory: the next `write()` overwrites it,
> on a schedule the reader does not control. The double mapping makes the
> bug silent rather than a fault.

Three things the receiver owns on top of the primitive, all policy rather
than mechanism (the primitive should not grow them):

- **Addressing.** The ring is indexed by its own head/tail; the receiver
    works in stream-absolute sample positions. One conversion, in one place,
    is what keeps `preamble_start` meaning the same thing everywhere.
- **Retention.** Nothing may be consumed while an in-flight burst or a
    pending refine still needs it. This is the receiver's bookkeeping, and it
    is what the sizing formula above has to be checked against.
- **A loud overrun.** `dp_f32_write()` returns `false` and bumps a `dropped`
    counter on overrun. For a streaming FIFO that is a statistic; here a
    dropped sample is a **lost burst**, so it has to surface as an error a
    caller sees, not a counter nobody reads. Same reasoning as
    `Report.capture`'s refusal to file a capture with a hole.

The cost is one `memcpy` of the stream into the receiver's ring, on top of
acq's own. Whether that is worth eliminating — by giving `acq` a retention
policy instead of consuming eagerly — is a real question, and the answer
should follow a measurement, not this paragraph.

______________________________________________________________________

The phases after this one are the spine's, unchanged: declare, implement,
pin with sabotage, bind, instrument, explore, certify. The certification at
phase 8 is where the prototype's measurements return — as evidence about a
component that exists, rather than about a demo.

______________________________________________________________________

## 8. See also

- [Adding an algorithm — the lifecycle](../dev/contributing/adding-algorithms.md) — the
    phase order this document is phase 1 of
- [`async-dsss-spec.md`](async-dsss-spec.md) — `DetectionEvent` and the
    service-boundary goals this extends to bursts
- [DSSS use cases](../dev/contributing/dsss-use-cases.md) — the burst regime's
    geometry and why the 2-D roll fits it
- [DSSS acquisition](dsss-acquisition.md) — the detector's own architecture
- [State serialization](state-serialization.md) — the triplet the composition
    inherits from its parts
