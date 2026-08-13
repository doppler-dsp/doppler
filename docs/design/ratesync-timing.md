# Symbol Timing on a Rate Cascade

Symbol-timing recovery is usually built as two things: a matched filter, and
an interpolator steered by a timing loop. `RateSync` is built as **one**. It
owns a `RateConverter` whose terminal stage carries the pulse, and it closes
the timing loop around that stage's control port — so the matched filter is
the cascade's last dot product, and the polyphase arm that dot product
selects *is* the fractional timing delay.

This page is the **why**: what that fusion buys, which of its conventions are
load-bearing, and the two arguments that are easy to get wrong. The contract
itself — what each function promises, argument by argument — lives in
`native/inc/ratesync/ratesync_core.h`, and the measured envelope lives in
`src/doppler/track/tests/validation/ratesync/results.md`. This page does not
restate either.

Related: [The NCO](nco.md), [Continuously Variable Resampler](RESAMPLER.md),
[Timing Lock Detector](timing_lock_detector.md), [MPSK Receiver](mpsk.md),
[CIC Decimator](cic.md).

______________________________________________________________________

## 1. What it is for

A receiver is handed samples on the ADC's clock and needs symbols on the
transmitter's. Those two clocks are never the same, are usually not even
rationally related, and drift against each other. Timing recovery is the loop
that closes that gap.

The design bargain that shapes everything on this page:

> **Let the planner decide how to get down to a few samples per symbol, and
> steer only the last stage.**

Everything else follows from taking that seriously.

______________________________________________________________________

## 2. Why fuse the matched filter and the interpolator

`SymbolSync` separates the jobs — a matched FIR, then a Farrow interpolator
steered by a timing NCO. That is the textbook arrangement and it works. It
also does the same arithmetic twice: the matched filter computes a dot
product against the pulse, and the interpolator then computes a second dot
product to land between the samples the first one produced.

A polyphase bank already contains both. An arm of a `num_phases`-arm bank
*is* the pulse, sampled at a particular fractional offset. Selecting the arm
selects the delay; the dot product against it is the matched filter. One
filter, one pass, no Farrow.

The consequence that matters more than the saved multiplies is what sits in
**front** of the terminal stage. Because the cascade is a full
`RateConverter`, the planner is free to put halfband and CIC stages ahead of
it, and those do the bulk decimation for nearly nothing. The matched filter
is therefore sized by the **post-decimation** rate, not the input rate:

| input samples/symbol | planned cascade                         | terminal bank |
| -------------------- | --------------------------------------- | ------------- |
| 4                    | HalfbandDecimator + Resampler(1.0, rrc) | 34 taps       |
| 17.333               | CIC(8) + Resampler(0.923, rrc)          | 43 taps       |
| 64                   | CIC(32) + Resampler(1.0, rrc)           | 40 taps       |

A 16x span of input rates leaves the expensive filter the same size. A
matched filter at 256 samples per symbol costs what one at 4 costs.

### Arbitrary rate is not a feature, it is the default

`sps` is a `double`. 4, 17.33389, an irrational ratio and a slowly drifting
clock are all the same case, because the terminal stage's accumulator is a
double and the loop only has to steer the strobe. There is no integer path
and no special case for one — which is the honest arrangement, since a
free-running ADC clock against a symbol clock is the normal situation rather
than the exotic one.

______________________________________________________________________

## 3. The loop is a separate object, and that is load-bearing

`ratesync_loop_t` holds the strobe ring, the TED, the PI filter, the lock
detector and the telemetry. It holds **no filter and no cascade**. It
consumes a stream of terminal-stage outputs and produces a per-input rate
deviation for whoever owns the accumulator those outputs came from.

That split is why a receiver can reuse it verbatim. `RateSync` owns a
`RateConverter` and steers it directly; `MpskReceiver` owns a `Ddc`/`Ddcr`
— a mixer in front of the same cascade — and steers the *same* accumulator
through the DDC's `rate_ctrl` port. Both drive one implementation of the
timing loop, so a fix to the TED or the normaliser reaches both. They are not
peers that can drift apart, and `test_ratesync_core.c` §11 pins that by
driving a hand-owned cascade through the loop API and requiring the symbols
to match `RateSync`'s bit for bit.

The loop has to be told the geometry of the accumulator it steers, and there
are exactly two numbers:

- **the terminal stage's own rate**, because that is the scale `ctrl` is
    referenced to (§4 below);
- **the terminal bank's tap count**, because that is how many outputs are
    delay-line fill rather than signal (§5 below).

`ratesync_loop_bind_cascade()` reads both off a cascade so no owner has to
know how.

______________________________________________________________________

## 4. `ctrl` is referenced to the TERMINAL rate

This is the convention most likely to be got wrong, because the obvious
choice is wrong.

`loop_filter_step` returns a correction in symbols per symbol. `ctrl` is a
rate deviation the **terminal stage** adds to its accumulator once per one of
**its own** inputs — not once per cascade input. Those differ by the whole
integer decimation in front.

Over one symbol the terminal stage sees `N = m / rate_term` inputs, so the
accumulator gains `N * ctrl` output periods, i.e. `N * ctrl / m` symbols.
Setting that equal to the requested correction gives

```text
ctrl = correction * rate_term
```

with no reference to `sps` or to the decimation at all. Scaling by the
*cascade* rate `m/sps` instead under-drives the loop by exactly the
decimation factor — 32x at `sps = 64` behind a CIC(32).

The cost is measured rather than argued. `test_ratesync_core.c` §12 binds the
cascade rate where the terminal rate belongs and measures **18 dB** of EVM
lost. Note what does *not* catch it: given a long enough record the
under-driven loop still crawls to a nominally open eye and `locked` reads
true. The lock detector answers "is the eye open", which it eventually is; it
was never a check on loop gain.

This is also why `bn` means the same thing whatever the planner built. The
validation report sweeps `bn` across all three cascades above and finds them
within a decibel of each other at the useful settings — which is the whole
point of referencing the control to the terminal rate.

______________________________________________________________________

## 5. The loop stays open until the cascade is primed

A cascade's first outputs are its delay lines filling, not signal. The eye
statistic swings over its whole ±2 range through them, and steering on that
is meaningless — measured, it was worth one lost acquisition in sixteen.

So the loop discards `prime_taps + 1` terminal outputs before it closes, and
`prime_taps` is not a tuned constant: it is the terminal bank's own tap
count, read off the stage at construction. Counting **outputs** rather than
strobes is what makes it rate-independent — the bank spans `num_taps` of its
own input samples, and since the terminal stage never interpolates here
(`rate = m/sps ≤ 1`) it cannot emit more than `num_taps` outputs while those
inputs arrive. Discarding `num_taps` outputs therefore always covers the
fill, whatever `m` and `sps` are.

The strobe phase is deliberately *not* advanced during the discard, so the
first real strobe starts a clean count.

______________________________________________________________________

## 6. Two things that are easy to get wrong

### 6.1 Normalising the TED

A Gardner error is a timing error multiplied by three things the detector did
not choose: the signal amplitude, the transition density, and the detector's
own slope against the pulse. Only the last of those is the detector's to
remove.

`RateSync` divides it out with a **construct-time reciprocal**
(`symsync_ted_slope()`, computed once by `ratesync_loop_bind_cascade()`); the
hot path multiplies by it. Amplitude does not appear — it enters the raw
error as `A²` for Gardner and `A¹` for DTTL, and a unity-gain matched cascade
delivers the amplitude it was sent, so levelling is an AGC's job upstream.
Transition density does not appear either; it is data, and whatever slope it
yields is the honest slope.

What this replaced was a running 1%-per-symbol average of `|on|² + |mid|²`,
and it is worth recording why that was wrong, because the intuition behind it
is appealing:

- It is an `A²` quantity, so it was right for Gardner's amplitude law and
    left DTTL's gain proportional to `1/A` — a 4x swing over a 4x level
    change, in the detector BPSK selects.
- Being an average, it **lagged**. Seeded on the first post-prime strobe,
    which lands in the cascade's amplitude ramp, it ran the loop at up to
    thousands of times its designed gain for exactly the interval that
    decides acquisition. Measured: that wound the integrator past pull-in and
    cost 7000–25000 symbols to recover across a 0.3-symbol-wide band of
    initial offsets. With the lag gone the same band acquires in 133–266.

A separate and *earlier* mistake is worth keeping on the record because it is
the more tempting one: normalising by `|on|²` **alone**. The on-time energy
vanishes exactly when the strobe sits on the symbol transitions — precisely
the state the loop must recover *from* — so that divides by zero at the worst
possible moment. Measured, the error reached −91, the control drove the
terminal stage's effective rate negative, its accumulator stopped advancing,
and the cascade emitted nothing ever again: 2 symbols where 4000 were
expected. A permanent death, not a transient.

The energy **sum** `|on|² + |mid|²` survives as the lock statistic's
normaliser, where it belongs: the two energies are the same signal half a
symbol apart, so their sum is bounded away from zero at every timing phase.

**Why there is no clamp on the control anywhere.** With a construct-time
constant there is nothing that can vanish, so there is no runaway to clamp;
the error is bounded by the detector's own S-curve, which is bounded by
construction. Measured from the worst offset at `bn = 0.02`, `ctrl` stays
inside a few parts in a thousand.

### 6.2 The T/2 role ambiguity resolves itself

Gardner needs an on-time strobe and a transition gate half a symbol earlier.
Running at `rate = m/sps` and taking every m-th output as on-time makes that
assignment a **parity count**, which looks like it should be ambiguous — and
a half-symbol error genuinely is an equilibrium of the detector.

It is an **unstable** one. Swept over a fine grid of standing timing offsets,
the S-curve has exactly two zeros per symbol, crossing in opposite senses:
one at the eye centre, which restores, and one at the T/2 point, which does
not. The loop runs away from the wrong one on its own.

So **the parity does not matter**, and no eye-sign detector or counter flip
is needed. An earlier prototype ran two displaced banks to pin the roles
structurally; measurement showed that buys nothing and costs double the
multiplies.

**The object never inspects the eye, and must not.** The escape from the T/2
point is feedback: at that offset the crossing has the non-restoring sense,
so any perturbation is amplified rather than corrected, and the loop leaves
on its own. That argument holds for whatever is on the input — a modulated
signal, noise, or a buffer of zeros — because it is a property of the loop's
sign, not of the signal's quality. A receiver that gated on an open eye
before trusting its own timing loop would stall on exactly the inputs it most
needs to survive: an unmodulated dwell, a squelched channel, the fill between
bursts. There is no such gate here and none is wanted.

### What the validator has to do instead, and why it is not this

The validation report faces a question the object does not. It measures
**open-loop** (`bn = 0`), so nothing pulls the strobe anywhere: it has to
name the equilibrium it is differentiating before it can check that the
normalised slope there is unity (§6.1). Getting that wrong is not
hypothetical — it produced the retired **F15**, an apparent 8.7x roll-off
dependence in DTTL's normaliser that was the T/2 zero all along.

It cannot name it by the sign of the slope. A sign is only meaningful
relative to a timing axis, and the two harnesses run theirs in opposite
senses — the Python validator offsets the decimation phase, the C one offsets
the transmitter — so a hard-coded `slope <= 0` test picks the eye centre in
one and the T/2 point in the other, while both report identical numbers.
Gardner's near-sinusoidal S-curve carries the same slope magnitude at both
zeros, so nothing on the default detector reveals the mistake.

Eye opening does distinguish them, without reference to any axis: mean
`|symbol|` measures 1.000 at the eye centre against 0.53–0.79 half a symbol
away. That is a **measurement-time** discrimination, run on a known modulated
stimulus the validator generated itself, and it belongs there and nowhere
else — it is available precisely because a validator controls its own input,
which is the one thing the object can never assume.

______________________________________________________________________

## 7. The caller owns the input level

There is no AGC in `RateSync`, deliberately: a receiver composing it already
levels in its own front-end cascade (`RateConverter_enable_agc()`, one per
receiver), so an AGC here would be a second one integrating against the
first. The level to hit is not a tuned number — it is unit-amplitude symbols,
the reference the bank already defines.

The consequence is sharper than "present a sensible level", and the
validation report measures it: because the Gardner error carries an `A²`
factor, **the input level multiplies the loop gain**. The level axis *is* the
`bn` axis. Too hot tracks noisily; too cold has not settled. EVM is therefore
not monotone in level — it improves slightly below the contracted amplitude
before collapsing — so a receiver tuned against EVM alone is rewarded for
drifting toward a cliff.

`ratesync_get_clipped()` reports the subset of over-drive a CIC's input
quantiser sees (a CIC bounds its input to ±1.0 and clips silently past that).
It is not a level check: on a plan the planner built without a CIC there is
nothing to clip, and over-driving costs EVM with the flag reading clean. The
missing under-drive report is tracked as
[gh-661](https://github.com/doppler-dsp/doppler/issues/661).

______________________________________________________________________

## 8. One input can complete two outputs

The cascade rate is `m/sps ≤ 1`, so it is tempting to assume the terminal
stage emits at most one output per input. It does not. Whenever the terminal
stage's own rate is at or near 1.0 — a cascade like HB + Resampler(1.0),
which is exactly what an integer `sps` plans — the control can push the
accumulator over and that input emits **two**.

Asking for only one silently drops the second, which permanently shifts the
strobe parity and leaves the loop sliding: measured as `rate_est` walking
monotonically away while the eye never opens. `ratesync_step_ted()`
therefore drains up to four outputs per input.

Two is the bound, since the cascade rate is at most 1, and with `m ≥ 2` two
consecutive outputs can contain at most one on-time strobe — so the
single-symbol return of `ratesync_step()` is still correct.

______________________________________________________________________

## 9. What is deliberately not here

- **A (pfa, pd) sizing entry point.** `symsync_configure_lock()`'s constants
    were calibrated against symsync's own geometry by Monte Carlo. Re-exposing
    the formula for a different front end without repeating that validation
    would be asserting a calibration nobody measured, so `RateSync` ships
    symsync's empirically validated operating point as the default
    (`avgs = 133`, threshold 0.311, `n_up = 1`, `n_down = 8`) and exposes
    `ratesync_configure_lock_raw()` for a caller that sizes its own. See
    [Timing Lock Detector](timing_lock_detector.md).
- **A second timing mechanism.** The terminal stage's accumulator is the only
    thing that decides when a sample is taken. Nothing else adjusts timing.
- **Droop compensation as a choice.** `ratesync_create()` builds the cascade
    with CIC droop compensation on unconditionally: it folds into the terminal
    bank at six taps per arm, costs no extra stage and no extra pass, and is
    worth ~28 dB of EVM on any plan containing a CIC. There is no
    configuration under which paying for it is wrong.
