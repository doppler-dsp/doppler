# `DsssBurstReceiver`: the burst chain, composed in C

**Status:** shipped — the object exists, is certified (32 limits, 11
findings, 0 open) and this document has been kept current with it.
**Looking for the API?** §8. Sections 1–7 are the design record: the problem,
what a prototype measured, and the four assumptions that did not survive
contact.
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

### 3.6 Acquisition returns crossings, not bursts

Refine answers *where* a burst starts (§3.4). Nothing yet answers **how many
bursts there are**, and that is a separate problem with its own failure
modes. Acquisition hands back a stream of threshold crossings, and three
kinds arrive:

| what fires                         | why                                                                                                        | what it is                              |
| ---------------------------------- | ---------------------------------------------------------------------------------------------------------- | --------------------------------------- |
| several frames of **one preamble** | the preamble spans up to `REPS` frames and each can clear the gate                                         | the **same** burst, reported repeatedly |
| the **payload** of a burst         | the data code correlates against the acquisition code at some non-zero level, usually off the Doppler peak | not a burst                             |
| **noise**                          | at the configured `pfa`, by construction                                                                   | not a burst                             |

Row 1 is an **identity** question — *are these two crossings one burst?* Rows
2 and 3 are an **exclusion** question — *is this a burst at all?* They look
like one problem and are not, and the rest of this section is what it costs
to treat them as one.

#### Treating them as one loses bursts

Suppress everything for one `burst_len` after a crossing and take the first
hit in that window, and both rows are handled at once. Measured on the
five-burst example capture, that loses **2 of 5 bursts** — each to a spurious
crossing inside the *preceding* burst's payload, roughly 10 dB weaker than
the burst it discarded. The exclusion rule fired on a crossing that was not a
burst, and the identity rule then hid the real one behind it.

The obvious explanation is wrong, and the wrong one is instructive.
Acquisition frames without overlap, so a preamble landing mid-frame is split
between two — which looks exactly like a cause. It is not: sweeping the
global frame phase moves every burst's single-frame coverage over 50 %–100 %,
*including an exact half-split*, and the count does not move — pinned at 3/5
at every phase, while the worst-straddled burst in the capture is found and a
near-fully-covered one is lost. What predicts the loss, 1:1 across every
phase, is the number of spurious crossings. Non-overlapping framing does cost
a band of offsets **near the knee**, where the margin is thin, but that is a
separate and much smaller effect
([doppler#1006](https://github.com/doppler-dsp/doppler/issues/1006)).

#### The obvious tie-break is the wrong quantity

Ranking two crossings so the stronger keeps the slot needs a measure of
strength, and the gating statistic is not one. `test_stat` is
`peak_mag / noise_est`, and `noise_est` is a *mean over the correlation
surface* — so a signal that spreads energy across the surface raises its own
denominator. A **bare** preamble raises no floor; a real burst's payload
does. Measured: a decoy preamble at **0.35 amplitude** outranks a full burst
at 1.0 on `test_stat`. `cn0_dbhz_est` is backed out of `test_stat` and
inherits it.

The normalized statistic answers *did this cross the gate*, which is what it
exists for and what prices `pfa`. It does not answer *which of these two is
the stronger signal*.

#### An exclusion window has a far edge — so the object has a re-arm time

A window that runs to the end of a burst is compared against an **epoch**,
and §3.1 already says an epoch is a residue: a code position inside the frame
that detected it. So a crossing can name a position up to one acquisition
frame **earlier than the burst it belongs to**, and the far edge of one
burst's window can therefore reach the next burst's anchor.

The object consequently has a **re-arm time**, and with
[doppler#1008](https://github.com/doppler-dsp/doppler/issues/1008) fixed it
can finally be measured — every earlier attempt was measuring that defect
instead, because block size dominated the result.

Paired control: the same scene at a given gap and spread three burst-lengths
apart, so a burst lost at *both* is sensitivity rather than re-arm and is
excluded. Randomised burst size, phase, seed, noise, count and block size.

| inter-burst gap | losses, all C/N0 | losses, clean C/N0 |
| --------------- | ---------------- | ------------------ |
| 0 % (abutting)  | 8 / 35           | 3 / 26             |
| 2 %             | 4 / 36           | **0 / 20**         |
| 5 %             | 3 / 25           | **0 / 24**         |
| 10 %            | 4 / 41           | 1 / 25             |
| 25 %            | 3 / 33           | **0 / 35**         |
| 50 %            | 1 / 31           | **0 / 30**         |

**A zero gap fails and any real separation does not.** At clean C/N0 — about
40 dB above the knee, where sensitivity cannot confound the reading — losses
are confined to the abutting case. Zero gap is not a burst link anyway: bursts
that abut with no separation are a continuous stream, which is
`DsssReceiver`'s problem (§5.1).

The residual in the left column is **sensitivity, not re-arm**, and the
experiment cannot fully separate the two by construction: changing the gap
re-lays the whole capture, so each burst sees a different noise realization
and one near threshold can flip either way. That is why the clean column is
the one that answers the question.

**Two false answers preceded this one, and both were the measurement rather
than the object.** The first sweep varied only the frame phase — one seed,
one noise level, one block size — and made "clean above 3 % separation" look
established; widening it destroyed that. The second reported a flat ~11 % loss
at every gap, which looked structural and was not: the harness recorded
`preamble_start`, a scalar read-back that describes only the **last** burst of
a call, so it under-counted precisely when a push returned several. Reading
`events()` instead took it from 17/160 to 4/160. A harness written against a
one-result API silently under-reports a list-returning one.

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

1. **Claim bursts by two rules, never one.** §3.6's identity and exclusion
    questions get separate answers, because one rule cannot serve both:

    - **Identity** — two crossings are the same burst when refine could map
        both onto one start. That is computable, not a tunable: refine's
        candidate grid is `anchor + k·P` over `k ∈ [−k_lo, k_hi]`, so
        `refine_span` *is* the window. Within it the **larger raw
        `peak_mag`** keeps the slot, so a weak crossing that merely arrived
        first cannot own it — and it must be the raw peak, never `test_stat`
        or `cn0_dbhz_est`, for the reason §3.6 measures.
    - **Exclusion** — a burst's payload keeps firing for a whole
        `burst_len`, and suppressing that long is right only if a burst is
        really there. Nothing at detection time knows. **The CRC knows**, so
        the long window is armed by a burst that *decoded* and by nothing
        else, and candidates already queued inside the confirmed span are
        dropped with it.

    The consequence is deliberate: the demodulator's verdict feeds
    **backwards** into the search. It is the one place in this object where a
    later stage informs an earlier one, and it is what keeps a crossing that
    turns out to be nothing from costing the next real burst.

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
- **Not a continuous-stream receiver.** Bursts are *separated* — that is what
    makes them bursts, and the search, the look-back and the claim rules all
    assume it. A stream whose bursts abut with no gap at all is
    `DsssReceiver`'s problem, and the re-arm bound in §3.6 is the edge where
    the two meet.

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

- **Whether refine is a separate object — SETTLED, private** (2026-09-01).
    It is `dsss_br_refine()`, a static function in
    `dsss_burst_receiver_core.c`. It does not compose `Corr` as this bullet
    guessed: the candidates are `anchor + k·P` and nothing between, so the
    stage correlates one code period at each preamble position against a
    sign reference the receiver holds, and sums magnitudes (§3.4). The
    decision followed phase 3, as this bullet asked — the wording stayed
    open long after the code closed it. Where refine LIVES is a separate
    question, and §11 answers it: out of this object, into `BurstCapture`,
    still a private function of the object that owns the ring.

- **Whether the history ring should be acq's — SETTLED for now, the
    receiver's.** §7.1 has the receiver keep its own, costing one `memcpy` of
    the stream, because `acq_push()` consumes eagerly. The alternative is a
    retention policy on `acq`'s existing ring so there is one copy instead of
    two. That is a change to a certified object, so it needs a measurement
    showing the copy matters before it is worth the blast radius — and at
    cf32 rates the copy may well disappear against the FFT. **That
    measurement has not been made**, so the second copy stands — and §11
    moves it rather than removing it: the ring becomes `BurstCapture`'s, one
    layer out, and the acq-retention question is untouched and still gated on
    the same measurement.

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
- **Serialization asymmetry — SETTLED, and it cost nothing.**
    `BurstAcquisition` is serializable; `BurstDemod` deliberately is not, and
    its report certifies that as correct (a burst completes within one
    `demod()` call or is lost). The composition carries the asymmetry by
    serializing the acquisition child, the retained look-back and the
    detection queue, and **nothing of the demodulator** — which is what the
    guess above predicted. `dsss_burst_receiver_state_bytes()` stays a pure
    function of configuration because both variable regions are fixed-size
    with a length prefix. The blob is already the capture half alone, which is
    why §11's split does not simplify serialization — the same bytes are
    written one envelope deeper.
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
    CLAIM    turn a stream of hits into distinct bursts           (§3.6)
             within refine_span of a queued candidate -> SAME burst,
                 keep the one with the larger raw peak_mag
             inside a burst that already DECODED -> its payload, drop
             otherwise -> a new candidate
             (acq is re-fed until it has absorbed the whole chunk: it
              stops once its result array fills and abandons the rest)
    per claimed burst WHOSE WINDOW HAS ARRIVED -- all of them, not one:
        REFINE   score offsets over +/- REPS*P: one code-period
                 correlation per preamble position, magnitudes
                 summed non-coherently                            (§3.4)
                 -> exact preamble_start, to the sample
        EVENT    build DetectionEvent (fold via dp_fftfreq, one home)
        DEMOD    window the burst from the event ALONE            (goal 4)
                 drive the consumer, score the seam               (goal 5)
                 append payload + its own event to this call's list
    return EVERY completed burst; every sample of x was consumed  (goal 6)
```

The three stages are the point. `search → refine → demod` is the chain
`async-dsss-spec.md` records as validated; the burst path had the first and
last and was quietly asking acquisition to do refine's job with arithmetic.

`CLAIM` is not a fourth stage so much as the bookkeeping between the first
two, but it is written out because it was once a single line of policy that
cost bursts (§3.6). Its two rules answer two different questions and are
deliberately not merged: identity is settled by refine's own reach, a
payload's existence is settled by the CRC.

Both windows are derived, and neither is a knob:

| window    | value                                    | why that value                                                                                                                    |
| --------- | ---------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------- |
| identity  | `refine_span` = `(k_lo + k_hi + REPS)·P` | the exact span over which refine can map two anchors onto one start — wider would merge distinct bursts, narrower would split one |
| exclusion | `burst_len`, armed only on `frame_valid` | exactly how long a payload can keep firing                                                                                        |

**The re-arm bound is settled** (§3.6): a zero gap fails, any real separation
does not, and a zero gap is a continuous stream rather than a burst link
(§5.1). Getting there took three wrong turns, recorded so nobody repeats
them:

- backing the exclusion window's far edge off by one acquisition frame —
    **measured to fix nothing**;
- bounding the identity window by `burst_len`, so a short payload cannot
    merge two adjacent bursts — the derivation predicted it cleanly and the
    measurement refused it: failing trials span `spacing/refine_span` from
    0.67 to 5.97, the same range as the passing ones. Neither was kept;
- the third was not a wrong turn but the actual defect
    ([doppler#1008](https://github.com/doppler-dsp/doppler/issues/1008)):
    `push()` abandoned the rest of its input after an emit. Fixing it needed
    the retention bound restored at the same time — consuming the input alone
    makes the ring refuse samples (`dropped=5632`), because `trim` clamps to
    the oldest queued detection and draining one per chunk lets the backlog
    grow. Draining **every** arrived detection is what makes `dropped` 0 by
    construction.

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

## 8. The API that shipped

Design phase 1 is above; this is what it became, kept here because the shape
of the surface *is* a design decision and two of its choices were arrived at
by measurement rather than by taste.

### 8.1 The C surface

<!-- docs-snippet: skip=declarations quoted from dsss_burst_receiver_core.h, not a compilable program -->

```c
/* Lifecycle */
dsss_burst_receiver_state_t *dsss_burst_receiver_create (
    const uint8_t *acq_code, size_t acq_code_len,
    const uint8_t *data_code, size_t data_code_len,
    const uint8_t *sync, size_t sync_len,
    size_t reps, size_t spc, double chip_rate, size_t frame_syms,
    double cn0_dbhz, double doppler_uncertainty, double pfa, double pd,
    double carrier_hz, double max_rate, size_t est_segments);
void   dsss_burst_receiver_destroy (dsss_burst_receiver_state_t *);
void   dsss_burst_receiver_reset   (dsss_burst_receiver_state_t *);

/* The stream: samples in, the payload of EVERY completed burst out */
size_t dsss_burst_receiver_push_max_out (dsss_burst_receiver_state_t *,
                                         size_t x_len);
size_t dsss_burst_receiver_push (dsss_burst_receiver_state_t *,
                                 const float complex *x, size_t x_len,
                                 uint8_t *out, size_t max_out);

/* One event per burst that push() just returned */
size_t dsss_burst_receiver_events_max_out (dsss_burst_receiver_state_t *);
size_t dsss_burst_receiver_events (dsss_burst_receiver_state_t *, size_t n,
                                   dsss_br_event_t *out, size_t max_out);

/* Escape hatch, read-backs, state */
int    dsss_burst_receiver_configure_search_raw (dsss_burst_receiver_state_t *,
                                                 size_t doppler_bins,
                                                 size_t n_noncoh);
size_t dsss_burst_receiver_state_bytes (const dsss_burst_receiver_state_t *);
void   dsss_burst_receiver_get_state   (const dsss_burst_receiver_state_t *,
                                        void *blob);
int    dsss_burst_receiver_set_state   (dsss_burst_receiver_state_t *,
                                        const void *blob);
```

### 8.2 `push()` returns a LIST, and that is the load-bearing choice

Payloads come back concatenated — burst `i` occupies
`out` from `i*frame_syms` — with `events()` giving each its
own record.

The alternative, one burst per call, is what shipped first and it was wrong
in a way worth recording. A call can *complete* many bursts while *returning*
one, and that mismatch has to be absorbed somewhere: a store for the surplus,
a capacity for the store, a drain protocol telling callers to push again, and
back-pressure when the store fills. Every one of those is a way for a caller
to lose a burst by not following a protocol. Returning the list deletes all
of them — collection is forced by the return value, and a caller *cannot*
fail to drain.

It also deleted the defect. The one-per-call version abandoned the rest of
its input to preserve its own contract, so a block carrying several bursts
lost all but the first — 6/6 decoded with 333-sample blocks against 1/6 with
one large one ([doppler#1008](https://github.com/doppler-dsp/doppler/issues/1008)).

**`push_max_out(x_len)` uses its argument**, which the one-per-call version
accepted and ignored. Distinct bursts cannot overlap, so `x_len` samples
complete at most `x_len/burst_len + 1` of them, plus whatever is already
queued:

```
push_max_out(x_len) = (x_len/burst_len + 1 + q_cap) * frame_syms
```

### 8.3 Why `events()` is a second call

Each burst needs its own event: the scalar read-backs
(`preamble_start`, `frame_valid`, `doppler_hz_est`, …) still exist and still
describe the **last** burst returned, but they cannot speak for the others.
§4's sufficiency argument is about what *one* event must contain, not how
many are returned per call, so a list preserves it exactly.

It is a companion rather than a second output of `push()` for a mechanical
reason: bits are `k · frame_syms` long and events are `k`, and the binding
generator emits one variable-length output per method — a second parallel
array is allocated at the *first* one's length. Splitting the call is the
honest way to express two different lengths, and it leaves every existing
caller of `bits = rx.push(x)` working unchanged.

```python
import numpy as np
from doppler.dsss import DsssBurstReceiver

rng = np.random.default_rng(0)
PAYLOAD_LEN = 32
FRAME_SYMS = 13 + PAYLOAD_LEN + 16       # sync | payload | CRC-16
rx = DsssBurstReceiver(
    rng.integers(0, 2, 31).astype(np.uint8),   # acquisition code
    rng.integers(0, 2, 8).astype(np.uint8),    # data code
    np.zeros(13, dtype=np.uint8),              # sync word
    reps=4, spc=4, frame_syms=FRAME_SYMS,
)

bits = rx.push(np.zeros(4096, dtype=np.complex64))   # k * frame_syms
for i, ev in enumerate(rx.events()):                 # k records
    frame = bits[i * FRAME_SYMS:(i + 1) * FRAME_SYMS]
    print(ev.preamble_start, frame.size)

# One record per FRAME, always -- silence here, so k == 0.
assert len(rx.events()) == bits.size // FRAME_SYMS
```

`events()` is scratch describing the most recent `push()`, valid until the
next `push()`, `reset()` or `set_state()`. It is deliberately **not**
serialized — keeping one call's transient out of the blob is what holds
`state_bytes()` to a pure function of configuration.

### 8.4 What the caller is not asked to size

The history ring, the detection queue and the search grid are all derived
from the geometry at `create()`. A caller asked to size a look-back buffer is
a caller handed a way to lose bursts silently (§7.1), and a caller asked for
a queue depth is the same thing one level up: the depth scales with
`burst_len/refine_span`, which is ~1 at a short-payload test geometry and
5.5× at a real link, so any constant is wrong somewhere.

`configure_search_raw()` remains the one escape hatch, and it pins the
acquisition grid only.

______________________________________________________________________

## 9. See also

- [Adding an algorithm — the lifecycle](../dev/contributing/adding-algorithms.md) — the
    phase order this document is phase 1 of
- [`async-dsss-spec.md`](async-dsss-spec.md) — `DetectionEvent` and the
    service-boundary goals this extends to bursts
- [DSSS use cases](../dev/contributing/dsss-use-cases.md) — the burst regime's
    geometry and why the 2-D roll fits it
- [DSSS acquisition](dsss-acquisition.md) — the detector's own architecture
- [State serialization](state-serialization.md) — the triplet the composition
    inherits from its parts

## 10. The receiver stops at decisions (2026-08-27)

Sections 1–9 describe a receiver whose frame is `sync | payload | CRC-16`.
That shape was written down in **four** places, and two consequences were
measured: a burst generated with `crc="none"` decoded bit-exactly and was
reported *invalid*, and `wfmgen --type dsss --conv` produced a waveform
byte-identical to no `--conv` at all.

The first fix ([#1017](https://github.com/doppler-dsp/doppler/issues/1017))
gave both ends one `wfm_frame_desc_t`. It fixed the assumption and put the
frame in the wrong object: a receiver then held `crc`/`rs_depth`/
`randomise`/`attach_asm`, ran `wfm_frame_check()` and published
`frame_valid` — none of which is a physical-layer fact — and a CCSDS
coverage policy had moved into `wfm/wfm_frame.h`, whose own header says it
"knows nothing about CCSDS".

### 10.1 Three layers, each knowing one thing

| layer    | object                              | knows                                                                   |
| -------- | ----------------------------------- | ----------------------------------------------------------------------- |
| physical | `BurstDemod`, `DsssBurstReceiver`   | the codes, the sync word, and `frame_syms` — how many symbols follow it |
| frame    | `frame` (`wfm.Frame` / `FrameDesc`) | the fields, the stages, and the span each covers                        |
| codes    | `conv`, `rs` via `ccsds_tm`'s ops   | the arithmetic a stage calls                                            |

`push()` returns `frame_syms` bits per burst and `llrs()` the matching soft
values. That is the entire output. `Frame.deframe()` is the receive
counterpart of assembling one: it undoes the stages in place order and
reports through `rx_ok` / `rx_units` / `rx_checked` / `rx_symbols`.

**The receiver got smaller.** Gone: four constructor knobs, two read-backs,
an event field, and the `ccsds_tm`/`conv`/`rs`/`wfm_frame` link line. What
is left is what a demodulator is for.

### 10.2 Where the cover policy lives

`wfm/wfm_frame.h` knows what a field and a stage are and **not** which
covers which, because the answer is a specification's. That table —
marker/preamble/sync are found-not-decoded, payload/CRC/parity is the data
group, the inner code covers everything — is CCSDS 131.0-B-6 10.3.4
generalised, so it lives in `ccsds_tm_frame_desc_of()`, next to the ASM
bits and the RS parity size it needs. The generator's bridge is an adapter
over it; a receiver calls neither.

### 10.3 What the split cost, and what it bought

The **suppression window** was armed by `frame_valid`. It is armed by the
burst having demodulated — the sync word correlated here, which is a fact
this object owns. A frame whose check fails is still a frame that was
transmitted at that position.

*2026-09-01:* once the capture owned the window (§11) it armed the span on
every **emission**, and this promise was masked — a decoy ending just after
a real burst began swallowed it (§2.10, lead 2100). The capture now holds
the detections inside a span rather than dropping them, and this receiver
checks its trailer in C and calls `burst_capture_release()` on every window
that fails, so the span is owned exactly by the frames that decoded. See
[`burst-capture.md` §11.2](burst-capture.md) and #1181.

Callers slice their own payload: `push()`'s rows are frames, so a payload
is `frame[field_off(i):][:field_bits(i)]` after `deframe()`. That is one
line, and it is the line that makes the receiver reusable for a frame
nobody has designed yet.

### 10.4 The inner code, still

A convolutional stage covers the sync word, so on the wire the bits a
hard-decision correlator looks for are coded, and frame synchronisation
would have to run after the Viterbi. The soft bits that needs exist now
(`llrs()`, [#1018](https://github.com/doppler-dsp/doppler/issues/1018));
the ordering does not. `deframe()` reports such a stage as **not checked**,
never as passed.

## 11. `BurstCapture` — the look-back and the refine get a home (2026-09-01)

[gh-1166](https://github.com/doppler-dsp/doppler/issues/1166) named a third
option §6.1 does not list: an object **between** acquisition and
demodulation, owning the history ring and the refine, and emitting aligned
burst windows. `DsssBurstReceiver` composes it and stops owning a ring.

**Accepted.** It was first declined here, on the count that no caller wants
the windows, and that count was wrong — established by grepping for this
object's own vocabulary (`look-back`, `history`, `retain`), which is not how
a caller spells it. Recorded rather than quietly fixed, because the search
that produced it is the reusable mistake: **look for the CAPABILITY under
the caller's name for it, not the implementation's.**

### 11.1 What a different composition costs today

`doppler.dsss.orchestrator.Acquirer` is already a second composition over
`BurstAcquisition` — K coarse-Doppler channels, each `DDC → BurstAcquisition`,
fanned across a thread pool, `get_state`/`set_state` per channel so the bank
ships to other pods. It feeds acquisition **continuously**, which is the
right way to drive it, and its output is `Detection` records. It stops there.

A user of that bank who wants the burst has two routes, and both are bad.

- **Feed acquisition continuously**, as the bank already does. Anchors are
    stream-absolute and there is no sweep — and §3.1 bites in full:
    `code_phase` is `burst_start mod code_bins`, so the caller must write
    refine themselves (one code-period correlation at each preamble position
    across ±`REPS·P` candidates, non-coherently summed), plus the ring to
    reach back into, plus a retention rule, plus a claim rule keyed on
    `refine_span`. §3.2 says getting it wrong is a cliff: a burst one period
    out decodes as noise, not as a degraded frame.
- **Sweep acquisition in dwells**, as
    `src/doppler/examples/dsss_burst_pipeline_demo.py` does. `reset()` per
    dwell makes `pos + code_phase` a real position, so refine is not needed —
    paid for with `PRE_LEN/ACQ_HOP` overlapping FFT dwells over the whole
    stream, two window constants nothing derives, a hand-rolled
    cluster-and-keep-strongest rule with a different window than
    `refine_span`, a `configure_search_raw` pin without which the auto-sizer
    makes a decision impossible, and the entire capture held in RAM to slice
    from. That last one means it does not stream.

So the ring, the refine and the claim rule exist exactly once, inside an
object that also demodulates. That is the reuse case, and it is present.

### 11.2 The input face: it owns the engine

`push(samples) → windows + events`. The capture composes its own
`burst_acq` and exposes it as a child, the way this receiver already does.

The alternative gh-1166 draws — `push(samples, acq_results)`, so a caller
brings their own detector — was rejected on one invariant.
`acq_result_t::samples_consumed` is stream-absolute **only** for an engine
fed continuously and never `reset()`, in the caller's own sample
coordinates. An object taking foreign results has to require that and cannot
check it, and a violation is not a degraded window: refine searches the wrong
period and §3.2's cliff returns noise. Owning the engine makes `push()`
itself define the coordinate system, so the invariant is internal and
unbreakable rather than documented and hoped for.

What a caller loses is a foreign detector, and the loss is smaller than it
looks: the engine stays reachable as a child for read-backs and
`configure_search_raw()`, and a consumer receiving detections over a
transport (§2's second row) needs the WINDOW as well, which is what §1.1
already requires the event to be sufficient for.

### 11.3 What moves, and what the split does not buy

Out of `dsss_burst_receiver_core.c` and into the capture: the history ring
and its sizing, `dsss_br_refine()`, `dsss_br_trim()`, the CLAIM bookkeeping
and the detection queue. What stays: driving the demodulator, the suppression
window armed on a burst having demodulated (§10.3), and the payload/event
list `push()` returns. `DsssBurstReceiver` becomes capture + demod;
`CoarseChannel` becomes `DDC → BurstCapture`.

Two arguments made for the split do **not** hold, and are recorded so they
are not re-made:

- **It does not simplify serialization.** The receiver's blob is already the
    acquisition child, the look-back and the queue, with nothing of the
    demodulator (§6.2). The buffer is necessarily in the blob — a resume has
    to reach back to a burst start already gone past — and after the split
    the same fixed `retain_span` region is written, one envelope deeper,
    behind the capture's own `state_bytes()`.
- **It needs no new accessor.** `acq_state_t::doppler_res_hz` is a public
    field and `BurstAcquisition.doppler_res_hz` is a published property
    (`objects/burst_acq.toml`), so nothing re-derives the bin width.

### 11.4 The look-back IS the blob

Measured 2026-09-01 by constructing
`DsssBurstReceiver(acq_code, data_code, sync13, reps, spc=4, chip_rate=1e6, frame_syms=13+payload+16)` and reading its own `retain_span`
and `state_bytes()`:

| geometry                | `retain_span` | `state_bytes()` |
| ----------------------- | ------------- | --------------- |
| SF31×4, 61-sym frame    | 4 928         | 0.05 MB         |
| SF511×5, 1029-sym frame | 318 584       | 2.57 MB         |
| SF511×5, 8029-sym frame | 2 082 584     | 16.68 MB        |

`retain_span · 8 B` is all but ~20 kB of each, so a checkpoint is the history
and little else. In a microservices deployment that is plausibly fine — a
blob is written on migration, not per `push()`. Where it is not, the answer
is a different **retention backend**: a circular file the blob references by
offset instead of carrying, or a ring shared out of the object's address
space. That is a policy about where retained samples live, and it is
precisely what the capture is the right owner of — today's ring is RAM the
object owns and no knob changes that. **Not built now**: no deployment has
asked, and the object earns its certification on the reuse case above
without it.

### 11.5 The name

`BurstCapture`, matching `BurstAcquisition` / `BurstDemod` /
`BurstDespreader`, which carry no `Dsss` infix — that appears only on the
composed `DsssBurstReceiver`. `doppler.telemetry`'s `Capture` and
`MemoryCapture` are a different domain, module-qualified at every use, and "a
capture is a recording of samples" is the DSP reading of the word; the
collision is not disqualifying. Decided rather than defaulted into.
