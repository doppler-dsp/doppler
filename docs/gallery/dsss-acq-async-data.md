# DSSS Acquisition — Continuous Async-Data Modulation

![Acquisition alone against asynchronous BPSK data, four panels](../assets/dsss_acq_async_data_demo.png)

Stage 1 of a story told across several pages: *"Acquisition ->
Dll(segments) -> MpskReceiver"* is the full continuous-DSSS receive
chain, validated one stage at a time instead of as a single end-of-run
BER number. This page asks the narrowest possible question: with a
**continuous** (non-bursty) 1023-chip CCSDS Gold code at 3 Mchips/s
carrying BPSK data at 2100 sym/s — `chips/symbol ~= 1428.6`, not an
integer, so the data-symbol clock is **asynchronous** to the code-epoch
clock — does [`Acquisition`](../api/python-dsss.md) still land on the
*exact* right code phase and Doppler bin, and does its per-epoch test
statistic survive a data-bit transition landing mid-epoch?

Not to be confused with [DSSS Acquisition Characterization](dsss-acq-characterization.md),
which measures Pd/Pfa vs Es/N0 for a **bursty** preamble+payload link with
synchronous spreading. This page's code is always present and its data
clock is deliberately misaligned to the code clock.

## What you're seeing

**Top left — chip-level zoom at the hit.** The raw received signal
(genie-derotated by the exactly-known injected Doppler — for plotting
only, not part of the real receiver), the TRUE transmitted chip×data
sequence, and Acquisition's own reported chip-phase reconstruction, all
overlaid for 400 samples right where the engine fired. All three match
bit-for-bit — asserted, not eyeballed. This isolates Acquisition's own
`code_phase`/`doppler_bin` estimate from every stage downstream (despread,
carrier recovery) and confirms it is exact.

**Top right — test statistic vs. epoch, Monte-Carlo over random data and
code phase.** At this operating point `doppler_bins == 1`, so every
`push()` evaluates exactly one code epoch — a direct per-epoch window onto
how the asynchronous data modulation affects the search. 200 independent
trials (random data, random starting code phase, the same 97 dB-Hz C/N0,
100 code epochs each) are summarized as a min/mean/max band against the
engine's own CFAR `threshold`. The regular scalloping (period ≈ 2.5
epochs, matching the fixed 1.396 epochs/symbol ratio) is real: a data-bit
transition landing mid-epoch partially cancels that epoch's coherent sum.
The worst epochs dip to within a hair of the threshold line — but across
20 000 epoch-trials, detection never actually failed.

**Bottom left — code-phase error vs. epoch, same Monte Carlo.** Tight,
near-zero error almost everywhere — except at the *exact same* epochs
where the test statistic dipped near threshold above, where the reported
peak occasionally jumps hundreds of chips away (a gross mislock, not a
small error). This is a real, quantified finding: crossing the CFAR gate
does not by itself guarantee an accurate code-phase estimate near
threshold. 3.1% of epochs in this run show a >5-chip mislock, and the
mislocks are provably concentrated at the low-test-stat epochs (asserted,
not just visually correlated). **The mislocks are not noise** — see below.

**Bottom right — Doppler error vs. epoch, same Monte Carlo.** Flat at the
full injected 50 Hz, with zero trial-to-trial variance. Not a bug: at
`doppler_bins == 1` the single Doppler bin spans the engine's *entire*
native ±1.47 kHz span, so this operating point makes no attempt at fine
Doppler resolution at all — that refinement is a downstream tracking-loop
job (`MpskReceiver`'s carrier NCO), not acquisition's.

## Why the mislocks happen

Naively you'd expect a weak epoch to fail closed (miss detection) or fail
randomly (lock onto AWGN). Neither is what happens. A data-bit transition
landing mid-epoch splits that epoch's coherent sum into two
*oppositely-signed* partial segments. At the true code phase this costs
correlation gain — the two segments partially cancel, which is the dip
seen above. The Gold code's 3-valued **full-period** correlation bound
(`{-1, -65, 63}` off-peak) does not apply to these unequal-length
**partial**-period segments: at some *other* candidate phase, the two
mis-signed partial sums can happen to add constructively instead,
producing a peak that beats the (already weakened) true-phase peak.

The example proves this directly rather than asserting it: it replays one
identified bad epoch with **all injected noise removed**. The identical
mislock reproduces bit-for-bit —

```
mislock root cause: trial=0 epoch=26 noisy_error=121.0 chips,
noiseless_replay_error=121.0 chips -- identical mislock with zero noise
proves it's structural (a data-transition splitting the coherent
window), not noise
```

— confirming the mislock is a deterministic consequence of the code's
partial-window self-correlation structure and where the transition falls
within the epoch, not a noise event. Stripping the noise couldn't have
"fixed" a noise-driven failure; it did nothing here, because noise was
never the cause.

## Fixing it: combine across epochs

![Epoch-diversity comparison: coherent vs. non-coherent combining](../assets/dsss_acq_async_data_demo_diversity.png)

If a single epoch is fragile, does combining more than one fix it — and
does it matter *how* they're combined? Four configurations, same 200-trial
sweep, x-axis in real epochs so they're directly comparable (mislock
defined the same way as above, >5 chips):

| config                                    | `doppler_bins` | `n_noncoh` | mislock rate |
| ----------------------------------------- | -------------- | ---------- | ------------ |
| 1 epoch/decision (baseline)               | 1              | 1          | 3.07%        |
| 2 epochs/decision, coherent               | 2              | 1          | 0.00%        |
| 3 epochs/decision, coherent (1 dump)      | 3              | 1          | 0.00%        |
| 3 epochs/decision, non-coherent (3 looks) | 1              | 3          | 0.00%        |

**Any combining across ≥2 independent epochs clears the mislock** — even
plain coherent stacking (`doppler_bins=2`) gets to zero, which at first
looks like it should suppress the fix's mechanism: not "non-coherent
specifically defeats it", but epoch **diversity** in general. The engine's
coherent combining isn't one long correlation over the concatenated window
— it's a per-code-phase-column FFT *across* the epoch axis (a coherent sum,
sample-position by sample-position, of each epoch's own raw samples at
that same within-epoch offset). A transition corrupts only a narrow,
transition-position-dependent slice of that sum, and because the
transition's position drifts epoch to epoch (the fixed 1.396
epochs/symbol ratio never repeats the same offset), that corrupted slice
never dominates once ≥2 epochs are combined, coherently or not.

Look closer at the panels, though, and non-coherent (bottom right) is
still the visibly *tightest* band of the four — no periodic dips at all,
where the coherent configs (top right, bottom left) still show real
periodic scalloping, just clear of their (also higher) thresholds rather
than hugging them. Two independent reasons favor non-coherent as the
robust default, not just "ties on this metric":

1. **Variance.** Power-summing (`|·|²` accumulate) can never be dragged
    down by destructive combination the way a coherent (complex-amplitude)
    sum can — it only ever adds. A coherent stack's gain is real, but its
    floor is set by however badly the worst single epoch in the stack
    disagrees in *phase*, not just magnitude.
1. **Drift immunity — the more important point for a real channel.**
    Coherent stacking across `doppler_bins` epochs is a single-frequency
    -hypothesis slow-time FFT — it implicitly assumes the carrier's phase
    evolves predictably across the *entire* window. Any unmodeled dynamics
    inside that window (residual acceleration, oscillator phase noise, a
    Doppler rate finer than the bin grid resolves) bleeds coherent gain
    away as the window lengthens — a cost this controlled simulation
    cannot show, because it has none of those drift sources. Non-coherent
    combining is blind to phase *between* looks by construction, so it is
    immune to that failure mode regardless of what the simulation does or
    doesn't model. The price is the classic non-coherent combining loss
    (roughly 1–3 dB versus the same total energy combined ideally
    coherently, for small `N`) — invisible here only because of the huge
    operating margin (real C/N0 = 97 dB-Hz vs. a ≤55 dB-Hz sizing target).

See the [DSSS acquisition guide](../guide/dsss-acquisition.md#continuous-data-modulated-signals-the-asynchronous-symbol-clock-case)
for the resulting recommendation: given `code`, `chip_rate`, and a
`symbol_rate` (the signal that continuous data modulation is present),
cap `reps=1` and size sensitivity through `max_noncoh` by default.

```python
--8<-- "src/doppler/examples/dsss_acq_async_data_demo.py:diversity_configs"
```

```python
--8<-- "src/doppler/examples/dsss_acq_async_data_demo.py:diversity_acq"
```

## How it works

```python
--8<-- "src/doppler/examples/dsss_acq_async_data_demo.py:signal"
```

1. Build a continuous capture: silence, then the Gold-code-spread BPSK
    stream at real chip/symbol rates with an independent, non-integer
    symbol clock and a residual 50 Hz carrier.
1. Stream it through `Acquisition.push()` — no `reset()`-hopping sweep
    needed, since (unlike a sparse burst) the code is always present.
1. At the hit, invert `code_phase` (a correlation *lag*) into the code's
    actual phase (`chip_phase = (sf - code_phase/spc) % sf`) and rebuild a
    local chip replica to compare against ground truth.
1. Separately, run the Monte-Carlo sweep: fresh `Acquisition` instances,
    100 epochs of continuous signal each (no silence — this measures
    per-epoch search behaviour in steady transmission, not acquisition
    latency), random data and starting code phase per trial, recording
    test statistic, code-phase error, and Doppler error at every epoch.
1. Then the epoch-diversity comparison: the same signal construction and
    trial loop, parametrized over `(cn0_dbhz, reps, max_noncoh)` so
    `doppler_bins`/`n_noncoh` land on each of the four configurations in
    the table above, at a fixed real C/N0 the whole time.

Downstream despread and demod (`Dll(segments) -> MpskReceiver`) are later
stages of this story, each getting their own page once built.

## Run it

```sh
python -m doppler.examples.dsss_acq_async_data_demo   # ~15 s -> two PNGs
```

Writes both figures on this page (`dsss_acq_async_data_demo.png` and
`dsss_acq_async_data_demo_diversity.png`); both are wired into `make gallery`.

Source: `src/doppler/examples/dsss_acq_async_data_demo.py`. See also the
[DSSS acquisition guide](../guide/dsss-acquisition.md) (the recommendation
this page's epoch-diversity comparison backs),
[Continuous Async DSSS Receiver](async-dsss-receiver.md) (the full
downstream chain, currently on a plain PN code — due for a revisit once
this story's later stages land) and
[Streaming Async Despreader](async-despread.md) (the despread-only half
at toy parameters).
