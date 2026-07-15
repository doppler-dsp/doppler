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

Downstream despread and demod (`Dll(segments) -> MpskReceiver`) are later
stages of this story, each getting their own page once built.

## Run it

```sh
python -m doppler.examples.dsss_acq_async_data_demo   # ~few seconds -> PNG
```

Source: `src/doppler/examples/dsss_acq_async_data_demo.py`. See also
[Continuous Async DSSS Receiver](async-dsss-receiver.md) (the full
downstream chain, currently on a plain PN code — due for a revisit once
this story's later stages land) and
[Streaming Async Despreader](async-despread.md) (the despread-only half
at toy parameters).
