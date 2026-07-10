# A 5-Burst DSSS Link — wfmgen's Three Faces, the Full Receiver Chain

Two things in one example: exercise every way `wfmgen` can produce a
waveform against the *same* declarative scene, then run the resulting
capture through all three DSSS receiver objects — `Acquisition`,
`BurstDespreader`, `BurstDemod` — each demonstrated on its own before
they're chained. The goal driving the geometry choices below wasn't a clean
demo; it was finding rough edges and real failure modes in the API, which
it did (see [Rough edges found](#rough-edges-found)).

## The scenario

5 bursts, each `[ 5x512-chip unmodulated preamble | 1000-symbol BPSK payload spread 50 chips/symbol ]`, payload Es/N0 = 10 dB, separated by a
variable (burst-to-burst distinct) inter-burst gap. The payload frame is
`sync word | 1000 payload bits | CRC-16`, spread by a *second*, short code
distinct from the preamble code — the same frame layout
[`BurstDemod`'s own test suite](../api/python-dsss.md) uses, just longer.

## Generating the capture — three faces, one scene

`wfmgen` is one C engine reachable three ways: the CLI binary, the
`Composer.from_file` Python binding loading the identical JSON scene, and
the `Composer`/`Segment` object API built with no JSON at all. All three
should produce byte-identical samples for the same scene — and here that's
not just documentation, it's an assertion:

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

## Acquisition — alone

A fresh `Acquisition` instance per burst, fed exactly that burst's preamble
window (`5 x 512 x 4 = 10240` samples — one full coherent frame):

```python
from doppler.examples.dsss_burst_pipeline_demo import demo_acquisition

hits = demo_acquisition(rx, starts, acq_code)
all(hit is not None for hit in hits)
```

Every burst is detected at Doppler bin 0 (no Doppler was injected in this
scene) with a test statistic well clear of the CFAR threshold — the coherent
gain from stacking 5 x 512 = 2560 chips is large even though the payload
itself sits at a modest 10 dB Es/N0.

## BurstDespreader — alone

Seeded from the matching acquisition hit (Doppler bin -> Hz via
`doppler_res_hz`, code phase in samples -> chips), `set_acq` pulls the
Costas/DLL loops in across the preamble, then `bits()` despreads the frame:

```python
from doppler.examples.dsss_burst_pipeline_demo import demo_despreader

results = demo_despreader(
    rx, starts, hits, acq_code, data_code, frame_bits
)
[r[0] for r in results]  # bit errors per burst, best of two polarities
```

Bit errors land near zero on every burst — the tracking loop follows the
carrier and code phase continuously across all 1029 frame symbols, so a
10 dB Es/N0 payload after 50x spreading gain is comfortable.

## BurstDemod — full pipeline

`BurstDemod` is a **one-shot feedforward** design: estimate `(freq, rate)`,
dechirp once, despread, frame-sync, check the CRC-16. No tracking loop:

```python
from doppler.examples.dsss_burst_pipeline_demo import demo_burst_demod

demod_results = demo_burst_demod(
    rx, starts, acq_code, data_code, payload_bits
)
sum(1 for valid, _errs in demod_results if valid), len(demod_results)
```

All 5 bursts decode. That wasn't true when this example was first written —
at this scale it failed the CRC on more than half the bursts, which led to a
real bug fix in the C core. See below.

## Rough edges found

Finding these was the point of building this example, not an
inconvenience:

- **`wfmgen`'s `snr_mode="esno"`/`"auto"`** treats the modulated *symbol* of
    a `type="bits"` segment as one output chip, not the outer DSSS data
    symbol. Hitting a target *data-symbol* Es/N0 needs a hand conversion
    (`snr_db_fs = esn0_db - 10*log10(sf*sps)`) plus an explicit
    `snr_mode="fs"`. There's no "Es/N0 for a spread symbol" knob.

- **`type="pn"` codes are capped to Mersenne periods** (`2**n - 1` chips) —
    an exact chip count like 512 or 50 needs a hand-built `bits` pattern
    instead of the built-in PN generator.

- **No "N discrete bursts, jittered spacing" primitive** — build it as N
    segments with distinct (or ranged, `[lo, hi]`) `off_samples` per segment.

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

- **Neither DSSS `snr_est` field is actually in dB.** `Acquisition.push()`'s
    6th tuple element is documented as "estimated *per-sample amplitude*
    SNR" (`acq_core.h`) — a linear ratio, `test_stat / sqrt(2*pi) /   sqrt(2*n)`. It's *supposed* to look small and flat (~0.2 here) even when
    `test_stat` is large and healthy (~24): it backs the coherent-integration
    gain back out of `test_stat` to recover the raw per-sample SNR, a
    different, correctly-related quantity — not a broken one. First draft of
    this demo printed it as `snr(dB)` and that was simply wrong.

- **`BurstDespreader.snr_est` is numerically unstable once the Costas loop
    is well locked on BPSK**, and isn't dB either — an EMA of
    `Re(prompt)^2 / Im(prompt)^2`. A locked BPSK prompt has `Im -> 0`, so the
    ratio can spike to absurd values — this demo observed everything from
    single digits up to `6.9e6` across otherwise-healthy bursts. Treat it as
    a rough lock-quality signal, not a calibrated SNR. Neither field is
    suffixed `_db` even though `BurstDemod.est_snr_db` is — a naming
    inconsistency worth fixing upstream so "not dB" is visible from the name.

- **Found and fixed a real bug in `BurstDemod`**
    (`native/src/burst_demod/burst_demod_core.c`). At this demo's original
    scale (1000-symbol payload, 5x512-chip preamble, Es/N0=10dB) it failed
    the CRC on more than half of runs. Raising `est_segments` from 10 to 200
    barely moved the pass rate (1-2/10 either way) — a real clue, since
    `est_segments` only changes the *time-sampling grid* within the fixed
    preamble span, not the span itself.

    The actual bug: `burst_demod_demod()` already squared the despread
    payload symbols and ran `PolynomialPhaseEstimator` over them — a
    baseline ~20x longer than the preamble — to NDA-refine the chirp rate
    `mu`, but **discarded the frequency term the same estimate call also
    returns**, and gated the whole refinement behind `max_rate > 0.0`,
    skipping it entirely for the Doppler-only (`max_rate=0`) case this demo
    uses. The fix applies the discarded frequency correction the same way
    the rate correction already was (halved for the BPSK squaring — safe
    here because the preamble estimate already pins the residual to a small
    fraction of a cycle per symbol, nowhere near the squaring's half-cycle
    ambiguity zone) and stops gating the block on `max_rate` (a
    Doppler-only `ppe_create(nsym, 0.0)` naturally returns `rate_norm=0`,
    so the rate term is already a no-op when `max_rate=0`).

    Verified: 10/10 valid in an isolated numpy repro (residual frequency
    error dropped from a few-to-17 Hz down to sub-Hz) and 5/5 in this
    demo's actual wfmgen capture, with the existing 89-test
    `test_burst_demod.py` / `test_realtime_file_demod.py` / `test_ppe.py`
    suites still green. `BurstDespreader`'s continuous tracking loop never
    had this ceiling — it doesn't need a good feedforward estimate, it
    converges to one.

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
