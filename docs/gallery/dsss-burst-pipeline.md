# A 5-Burst DSSS Link — wfmgen's Three Faces, the Full Receiver Chain

![Es/N0 vs time, all 5 bursts](../assets/dsss_burst_pipeline_demo.png)

Two things in one example: exercise every way `wfmgen` can produce a
waveform against the *same* declarative scene, then run the resulting
capture through all three DSSS receiver objects — `Acquisition`,
`BurstDespreader`, `BurstDemod` — each demonstrated on its own before
they're chained, with every downstream stage seeded from what `Acquisition`
actually *finds*, not from ground truth.

## The scenario

5 bursts, each `[ 5x512-chip unmodulated preamble | 1000-symbol BPSK payload spread 50 chips/symbol ]`, payload Es/N0 = 10 dB, separated by a
variable (burst-to-burst distinct) inter-burst gap. The payload frame is
`sync word | 1000 payload bits | CRC-16`, spread by a *second*, short code
distinct from the preamble code — the same frame layout
[`BurstDemod`'s own test suite](../api/python-dsss.md) uses, just longer.
Each gap is an explicit, distinct sample count (not `repeats=N`'s randomized
redraws) so every burst's ground truth position stays exactly known for
scoring below; `gap_noise="off"` pins the inter-burst floor to silence for
the same reason — the [DSSS bursts](../guide/wfmgen/dsss-bursts.md) guide
page shows the realistic variant with the AWGN floor running through gaps.

## Generating the capture — three faces, one scene

`wfmgen` is one C engine reachable three ways: the CLI binary, the
`Composer.from_file` Python binding loading the identical JSON scene, and
the `Composer`/`Segment` object API built with no JSON at all. Each burst is
**one declarative `type="dsss"` segment** — the engine tiles the preamble,
XOR-spreads the `sync | payload | CRC-16` frame with the second code, sizes
the segment to exactly one burst, and interprets `snr_mode="esno"` as the
payload *data-symbol* Es/N0 (see the
[waveforms guide](../guide/wfmgen/waveforms.md#dsss-two-code-spread-spectrum-bursts)).
All three faces should produce byte-identical samples for the same scene —
and here that's not just documentation, it's an assertion:

```python
import tempfile

from doppler.examples.dsss_burst_pipeline_demo import (
    generate_waveform,
    burst_starts,
)

with tempfile.TemporaryDirectory() as tmp:
    rx, acq_code, data_code, payload_bits, frame_bits = generate_waveform(
        tmp
    )
starts = burst_starts()
len(rx), len(starts)
```

`generate_waveform` raises if any two faces diverge — reaching the return
value means the CLI, `Composer.from_file`, and `Composer([Segment(...)])`
all agree, bit for bit, on this multi-segment DSSS scene.

## Acquisition — alone, and actually blind

`Acquisition`'s real job is running continuously against a channel that's
mostly silence and noise, finding a signal (or not) with zero prior
knowledge of its timing — so this section runs it that way: ONE instance,
swept blindly across the *entire* capture:

```python
from doppler.examples.dsss_burst_pipeline_demo import demo_acquisition

hits, acq = demo_acquisition(rx, acq_code)
len(hits)
```

Every one of the 5 real bursts is discovered at Doppler bin 0 (no Doppler
was injected in this scene), at exactly its true sample position, purely
from CFAR threshold crossings — no ground truth passed in. A handful of
false alarms in the noise-only regions typically also show up; see
[API notes](#api-notes) for why, and how the pipeline handles them.

### How `push()` actually buffers and frames samples

Worth walking through in full, since it's *why* the sweep above is written
the way it is (`native/src/acq/acq_core.c:322-410`):

- It's a **ring-buffer FIFO**, not an accumulate-then-process call. Each
    call writes as many input samples as currently fit, drains every
    complete `n = doppler_bins * code_bins`-sample frame available (one 2-D
    FFT + PN correlate + CFAR test per frame), and loops write/drain until
    the input is consumed — so one call can hand it far more than the
    ring's own capacity.
- **Leftover samples short of a full frame stay buffered across calls** —
    unless a single call already hit the **hardcoded, non-configurable
    64-result cap** (`native/src/dsss/dsss_ext_acq.c:148`, not exposed as a
    Python parameter), in which case the remaining input for that call is
    genuinely *dropped*, not buffered. Not a concern at realistic CFAR
    settings, but a real edge case worth knowing about.
- **Framing is strictly sequential and non-overlapping**: one dwell is
    always exactly one frame's worth of samples, in order, with no
    sliding/search built into the primitive. A real burst's start is
    essentially arbitrary relative to a fixed dwell grid, so
    non-overlapping dwells *will* occasionally split a preamble across two
    dwells and miss it. Overlap is the **caller's** job — this demo
    `reset()`s the ring + accumulator between dwells at a hop 1/4 of a
    dwell width (75% overlap), since `push()` has no "restart the search
    here instead" concept of its own.

## BurstDespreader — alone

Seeded from the matching *discovered* hit (Doppler bin → Hz via
`doppler_res_hz`; the absolute sample position already resolved the code
phase, so `init_chip_phase=0.0`), `set_acq` pulls the Costas/DLL loops in
across the preamble, then `steps()` despreads the frame to soft symbols:

```python
from doppler.examples.dsss_burst_pipeline_demo import demo_despreader

results = demo_despreader(rx, hits, acq, acq_code, data_code, frame_bits)
[r["esn0_db"] for r in results]  # data-aided Es/N0 (dB), per detection
```

Bit errors land near zero and the measured Es/N0 lands within a couple dB
of the configured 10 dB on every *real* burst — the tracking loop follows
the carrier and code phase continuously across all 1029 frame symbols. Any
false alarms score wildly wrong Es/N0 and near-zero lock, exactly as they
should.

### Es/N0 (dB), not `snr_est` — now a standalone `doppler.snr` module

`BurstDespreader`'s own `snr_est` isn't useful here (see
[API notes](#api-notes)), so this demo reports a **data-aided Es/N0 (dB)**
instead, from the standalone [`doppler.snr`](../api/python-snr.md) module:
strip the known modulation (`z = soft * sign`), and Es/N0 (linear) = signal
energy over complex noise variance = `a**2 / mean(|z - a|**2)` where
`a = mean(Re(z))` — the same convention `wfm_awgn_amplitude`/`add_noise()`
use elsewhere in this repo. It's scale-invariant (works regardless of
`BurstDespreader`'s internal symbol normalization) *and* polarity-invariant
(`a**2` doesn't care whether `a` is + or −), so unlike the bit-error count
it needs no resolution of the sign ambiguity. `doppler.snr` also has a
non-data-aided sibling, `snr_m2m4_db()` (moment-based/M2M4, Pauluzzi &
Beaulieu 2000 — needs no known symbols at all, for any constant-modulus
signal), both with sliding-window `_series` counterparts.

A single scalar per burst hides the interesting part, though: a sliding
51-symbol window of the same estimate, plotted against time into the
frame, is the hero image above. It shows what a table can't — tracking-loop
settling right after the preamble hand-off, and any mid-frame dip (the DLL
boundary slip noted below shows up as exactly that on burst 0).

## BurstDemod — full pipeline

`BurstDemod` is a **one-shot feedforward** design: estimate `(freq, rate)`,
dechirp once, despread, frame-sync, check the CRC-16. No tracking loop —
also seeded from the *discovered* hit, not ground truth:

```python
from doppler.examples.dsss_burst_pipeline_demo import demo_burst_demod

demod_results = demo_burst_demod(
    rx, hits, acq, acq_code, data_code, payload_bits
)
sum(1 for valid, _errs in demod_results if valid), len(demod_results)
```

Every real burst decodes; any false alarms correctly fail the CRC.

## API notes

- **`Acquisition.push()`'s buffering/framing** is a ring-buffer FIFO with
    non-overlapping frames and a hardcoded 64-result-per-call cap — see
    [How `push()` actually buffers and frames samples](#how-push-actually-buffers-and-frames-samples)
    above for the full walkthrough. The short version: leftover samples
    persist across calls (good), but a call that hits the 64-result cap
    drops its remaining input (edge case), and achieving search overlap
    across dwells is entirely the caller's responsibility (not a knob on
    the object).

- **Pfa stays correctly calibrated under a blind, overlapping-dwell sweep**
    like this demo's, validated by a large Monte-Carlo sweep
    (`dsss_acq_characterization.py`'s `measure_sweep_pfa`) across several
    overlap fractions: overlapping search does not inflate the false-alarm
    rate beyond the configured `pfa`.

- **`BurstDespreader` has no absolute phase reference.** The Costas loop
    locks to a line, not a point, so its raw hard bits can come out globally
    inverted. Resolving that sign is exactly what `BurstDemod`'s sync-word
    correlation (or a hand-rolled equivalent) is for — `BurstDespreader`
    alone cannot do it, which is why the demo above scores both polarities.

- **`BurstDespreader.bits()`/`.steps()` don't always emit exactly
    `len(x) // (sf*sps)` symbols** for an exact-multiple input. Under
    realistic noise the DLL's code-tracking jitter can slip the
    integrate-and-dump boundary by one symbol over a long (1000+ symbol)
    frame. Genuine streaming behaviour, not a bug — but downstream code must
    not assume the output length is exact.

- **`Acquisition.push()`'s `cn0_dbhz_est`** tracks true C/N0 while AWGN
    dominates the CFAR noise estimate, and saturates at the code's own
    autocorrelation-sidelobe floor once C/N0 exceeds what the code/geometry
    can resolve — a real ceiling, not a bug.

- **`BurstDespreader.snr_est` is numerically unstable once the Costas loop
    is well locked on BPSK**, and isn't dB either — an EMA of
    `Re(prompt)^2 / Im(prompt)^2`. A locked BPSK prompt has `Im -> 0`, so the
    ratio can spike to absurd values. Treat it as a rough lock-quality
    signal, not a calibrated SNR — this demo reports a proper
    [data-aided Es/N0 (dB)](#esn0-db-not-snr_est-now-a-standalone-dopplersnr-module)
    instead.

- **`BurstDemod` is one-shot feedforward** (no tracking loop): one static
    `(f0, mu)` dechirp covers the whole payload, so its chirp-rate
    refinement matters. A `PolynomialPhaseEstimator` pass over the despread
    payload (a baseline ~20x longer than the preamble) NDA-refines both the
    rate and the residual frequency, applying both corrections regardless
    of whether a nonzero rate hypothesis was requested. At this demo's
    scale (1000-symbol payload, Es/N0=10 dB) that residual-frequency
    correction is what keeps the CRC passing on every real burst.
    `BurstDespreader`'s continuous tracking loop never needed this: it
    doesn't need a good feedforward estimate, it converges to one.

## Reproduce

```sh
python -m doppler.examples.dsss_burst_pipeline_demo
```

Source: `src/doppler/examples/dsss_burst_pipeline_demo.py`. Integration
tests (skipped when the `wfmgen` CLI isn't built):
`src/doppler/dsss/tests/test_burst_pipeline_demo.py`.

## See also

- [DSSS Acquisition & Despreading](dsss-despread.md) — the shorter,
    single-burst version of the `Acquisition` -> `BurstDespreader` chain.
- [DSSS Acquisition: Pd/Pfa](dsss-acq-characterization.md) — how
    `Acquisition`'s detection performance is characterised against Es/N0.
- [wfmgen — One Engine, Every Waveform](wfmgen.md) — the CLI and Composer
    API this example's generation step exercises.
- [Python: DSSS](../api/python-dsss.md) — the full `Acquisition` /
    `BurstDespreader` / `BurstDemod` reference.
