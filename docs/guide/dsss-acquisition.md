# DSSS Burst Acquisition

`doppler.dsss.Acquisition` acquires a direct-sequence spread-spectrum burst — a run
of repeated, BPSK-modulated PN-code segments — arriving with an **unknown code
phase** and an **unknown carrier-frequency (Doppler) offset**, buried in noise.
It owns the whole receive-side acquisition pipeline:

```
raw cf32  →  ring  →  reframe (doppler_bins, code_bins)  →  slow-time Doppler FFT
          →  code correlation (corr2d)  →  CFAR gate  →  (Doppler, code-phase) hits
```

Construction is **physics-only**: you state the waveform (`code`, `chip_rate`),
the front end (`spc`), the sensitivity (`cn0_dbhz`), and a target **(Pfa, Pd)**.
The engine sizes its own search grid — coherent depth, threshold, non-coherent
looks — from the [detection theory](../api/python-detection.md) and streams
detections. You never pick a bin count or a threshold.

This is the usage walk-through. For the matched-filter surface it builds on, see
[2-D Acquisition](../gallery/detection2d.md) (`CorrDetector2D`); for what happens
*after* acquisition, see the [DSSS BurstDespreader](../api/python-dsss.md).

!!! tip "The 30-second version"

    ```python
    import numpy as np
    from doppler.dsss import Acquisition
    from doppler.wfm import PN, mls_poly

    code = PN(poly=mls_poly(5), seed=1, length=5).generate(31)  # 31-chip PN

    acq = Acquisition(
        code,                  # sf = len(code) = 31 (inferred)
        reps=16,               # up to 16 coherent code repetitions
        spc=4,                 # samples per chip (fs = chip_rate · spc)
        chip_rate=1.023e6,     # Hz
        cn0_dbhz=52,           # sensitivity (carrier-to-noise density, dB-Hz)
        pfa=1e-3, pd=0.9,      # target false-alarm / detection rates
    )
    # The engine sized the grid: doppler_bins=12, code_bins=124, res≈2750 Hz.
    # Sizing is honest: pd_predicted is the AVERAGE Pd over the straddle
    # priors (random Doppler / code phase across the grid), not the
    # on-grid best case — see acq.straddle_loss for the mean derating.
    assert acq.pd_predicted >= acq.pd          # confirm it can meet the target

    # demo capture: a real 31-chip DSSS burst in light noise, split into
    # cf32 blocks (any block size works — the engine reframes internally).
    chip = np.repeat(1 - 2.0 * (code & 1), 4)  # ±1 per chip, ×spc oversample
    capture = np.tile(chip, 36).astype(np.complex64) * 8.0
    capture += (0.1 * (np.random.standard_normal(capture.size)
                + 1j * np.random.standard_normal(capture.size))
                ).astype(np.complex64)
    iq_stream = np.array_split(capture, 8)

    for chunk in iq_stream:                          # any cf32 block size
        for dop, phase, peak, noise, stat, cn0 in acq.push(chunk):
            print(f"hit: Doppler bin {dop}, code phase {phase} samples, "
                  f"C/N0≈{cn0:.1f} dB-Hz")
    ```

______________________________________________________________________

## The acquisition problem

A spread-spectrum transmitter sends the same PN segment over and over (the
acquisition preamble). By the time it reaches you it has an unknown **code
phase** (propagation delay, a circular shift of the code) and an unknown
**carrier offset** (Doppler), and it is well below the noise floor. Acquisition
is the 2-D search that recovers both at once:

- **code phase** — *where* in the code period the signal sits (the matched-filter
    lag), and
- **Doppler bin** — *how far* the carrier has shifted.

`Acquisition` evaluates the entire (Doppler × code-phase) grid per frame and
reports the cells whose detection statistic crosses an automatically set CFAR
gate.

______________________________________________________________________

## How it works — slow-time / fast-time

Acquisition is a delay–Doppler search, and `Acquisition` factors it into two FFTs
over the repeated-segment structure. It frames the stream into a
`(doppler_bins, code_bins)` matrix where **each row is one PN segment — one code
repetition** — and there are `doppler_bins` of them:

- **fast-time** — *within* one segment (`code_bins = sf·spc` samples). A circular
    correlation against the known code → the **code-phase** axis.
- **slow-time** — *across* the `doppler_bins` segments. An FFT along that axis
    resolves the per-segment carrier phase ramp → the **Doppler** axis.

```
            fast-time  (code_bins = sf·spc samples, one segment)
          ┌─────────────────────────────────────────────┐
slow-time │ rep 0   · · · · · · · · · · · · · · · · · · │
(rows =   │ rep 1   · · · · · · · · · · · · · · · · · · │   FFT down ──► Doppler
 code     │  ...                                        │   columns
 reps)    │ rep d-1 · · · · · · · · · · · · · · · · · · │
          └─────────────────────────────────────────────┘
                      │ circular code correlation
                      ▼  along each row → code phase
```

The coherent depth `doppler_bins` is **chosen by the engine** — the smallest
number of repetitions in `[1, reps]` that meets `pd` at your `cn0_dbhz` (more on
that below). Internally the fast-time correlation is the FFT-domain
[`Corr2D`](../gallery/detection2d.md) engine against a **single-row reference**
(the code in row 0, zeros elsewhere); the slow-time FFT is applied to the data
*before* that correlation. Because the engine ingests **raw** samples and does
the slow-time FFT itself, you never pre-transform anything — just `push` IQ.

!!! note "Doppler resolution and range (in Hz)"

    Two independent quantities, both reported as read-only properties:

    - **resolution** `doppler_res_hz = chip_rate / (sf · doppler_bins)` — set by
        how many repetitions the engine integrates (deeper → finer bins).
    - **span** `doppler_span_hz = ±chip_rate / (2·sf)` — the slow-time Nyquist,
        set by the *code period* (`sf` chips) alone, independent of `spc` and
        `doppler_bins`.

    With `sf=31`, `chip_rate=1.023 MHz`, `doppler_bins=12`: bins are ≈2750 Hz
    apart, spanning ±16.5 kHz. To search *wider* than the native span, sweep a
    coarse Doppler grid in front of `Acquisition` — see
    [Widening the Doppler search](#widening-the-doppler-search).

______________________________________________________________________

## Auto-configuration from the physics

You do not pick a threshold, a coherent depth, or a bin count — you state the
operating point and the engine derives the grid with the
[detection functions](../api/python-detection.md).

First it converts your sensitivity to the per-sample amplitude SNR the detection
math uses (noise power = `N0·fs` over the sampled bandwidth):

```
fs   = chip_rate · spc
snr  = sqrt( 10**(cn0_dbhz/10) / fs )      # per-sample amplitude SNR
```

Then it picks the **smallest** coherent depth `D ∈ [1, reps]` whose `D·code_bins`
coherent samples meet `pd` — least latency for a strong signal, full `reps` for a
weak one:

```
for D in 1 .. reps:
    cells     = searched_bins(D) · code_bins      # Bonferroni population
    pfa_cell  = 1 - (1 - pfa)**(1/cells)
    eta       = det_threshold(pfa_cell)           # √(-2 ln pfa_cell)
    if mean_pd(snr, D, eta) ≥ pd: break  # Pd AVERAGED over the straddle
                                         # priors (quadrature), not on-grid
doppler_bins = D
threshold    = eta · √(2/π)                        # eta in mean-CFAR units
```

The chosen grid is exposed as **read-only** properties:

```python
acq = Acquisition(code, reps=16, spc=4, chip_rate=1.023e6, cn0_dbhz=52,
                  pfa=1e-3, pd=0.9)

acq.doppler_bins, acq.code_bins   # 12, 124   — the grid the engine chose
acq.doppler_span_hz, acq.doppler_res_hz   # ±16500 Hz, 2750 Hz
acq.fs                            # 4.092e6   — chip_rate · spc
acq.pfa_cell                      # per-cell false-alarm prob (Bonferroni)
acq.eta                           # raw Rayleigh threshold √(-2 ln pfa_cell)
acq.threshold                     # the CFAR gate actually applied (eta·√(2/π))
acq.n_noncoh                      # non-coherent looks (1 = pure coherent)
acq.pd_predicted, acq.underpowered   # achieved Pd, and whether it fell short
```

### Check `underpowered` after construction

When the operating point is infeasible — even `reps` coherent repetitions plus
non-coherent looks (up to `max_noncoh`) cannot reach `pd` — auto-config does
**not** raise; it builds a best-effort grid with `pd_predicted < pd`, sets
`acq.underpowered = True`, and emits a `UserWarning`. Guard against shipping an
under-powered acquirer:

```python
assert acq.pd_predicted >= acq.pd, f"under-powered: {acq.pd_predicted:.2f}"
```

The levers that close a shortfall are a higher `cn0_dbhz` (if the signal really
is stronger), more `reps`, a tighter `doppler_uncertainty`, or `max_noncoh > 1`
to engage non-coherent integration.

`cn0_dbhz` is the universal sensitivity spec — carrier-to-noise **density** in
dB-Hz, independent of sample rate. A stronger `cn0_dbhz` lets the engine reach
`pd` with fewer repetitions (lower latency, coarser Doppler):

Depths are sized at the **average Pd over the straddle priors** (random
Doppler and code phase across the grid — see `acq.straddle_loss` for the
mean amplitude derating), so the engine buys enough integration to meet
`pd` in operation, not just on-grid:

| `cn0_dbhz` | chosen `doppler_bins` (reps=16) | `pd_predicted` |
| ---------- | ------------------------------- | -------------- |
| 56         | 5                               | 0.94           |
| 54         | 7                               | 0.91           |
| 52         | 12                              | 0.92           |

`noise_mode` selects the CFAR estimator (`"mean"` by default, which is what the
analytic `threshold` assumes; `"median"` is more robust but is not analytically
calibrated).

______________________________________________________________________

## Streaming and reading hits

`push` accepts any-length cf32 blocks, buffers them in a ring, and emits one
detection per coherent **frame** (or, on the non-coherent path, per `n_noncoh`
accumulated frames) whose statistic clears the gate. Each hit is a 6-tuple:

```python
for dop, phase, peak, noise, stat, cn0 in acq.push(chunk):
    ...
```

| field          | meaning                                                              |
| -------------- | -------------------------------------------------------------------- |
| `doppler_bin`  | peak row — slow-time Doppler bin (`0 … doppler_bins-1`)              |
| `code_phase`   | peak column — integer-sample code phase (`0 … code_bins-1`)          |
| `peak_mag`     | peak correlation magnitude over the surface                          |
| `noise_est`    | CFAR noise estimate                                                  |
| `test_stat`    | `peak_mag / noise_est` (compared against `threshold`)                |
| `cn0_dbhz_est` | estimated carrier-to-noise density (dB-Hz), comparable to `cn0_dbhz` |

Map the integer bins back to physical units:

```python
def doppler_hz(dop, acq):
    """Doppler bin → Hz (folds the upper half to negative)."""
    k = (dop + acq.doppler_bins // 2) % acq.doppler_bins - acq.doppler_bins // 2
    return k * acq.doppler_res_hz

delay_chips = phase / acq.spc          # code phase in chips
```

`reset()` drains the ring and the coherent accumulator (use it between
independent captures).

!!! note "Code phase tracks the stream offset"

    The code phase is measured against the frame grid, which is anchored at sample
    0\. Inserting `Δ` extra samples of lead-in (silence) before the burst shifts
    every reported `code_phase` by `Δ mod code_bins` — that offset *is* extra
    propagation delay. Frame the same burst at a different stream position and the
    Doppler bin is unchanged but the code phase rotates accordingly.

______________________________________________________________________

## How many hits to expect

Acquisition fires once per coherent frame while the burst fills the search
window. The engine frames on a fixed grid anchored at stream sample 0, so a frame
produces full processing gain only when its whole `n = doppler_bins·code_bins`
window lies inside the burst. For a burst of `R` segments (`R·code_bins` samples)
after `L` lead-in samples, the count of full-gain frames is

```
F = (L + R·code_bins) // n  -  ceil(L / n)
```

On the pure-coherent path each full frame yields one hit. A **frame-aligned**
lead-in (`L` a multiple of `n`) gives exactly `F` hits at one cell; a non-aligned
lead-in gives `F` to `F+2` (boundary frames that straddle the burst edge may also
fire) and rotates the code phase by `L mod code_bins`. On the non-coherent path
(`n_noncoh > 1`) a hit arrives once per `n_noncoh` accumulated frames. A payload
that follows the preamble on a **different** code decorrelates from the matched
filter and does not produce burst-cell hits.

______________________________________________________________________

## Choosing parameters

### The minimum you must supply

Construction is physics-only, and only three arguments have no meaningful
default. The smallest robust call is:

```python
acq = Acquisition(
    code,                 # the PN replica; sf = len(code) is inferred
    chip_rate=1.023e6,    # waveform chip rate (Hz)
    cn0_dbhz=61,          # your link-budget sensitivity (dB-Hz)
)
assert acq.pd_predicted >= acq.pd   # confirm the search can meet the target
```

The tiers:

- **Required — no meaningful default:** `code` (the PN replica), `chip_rate` (the
    waveform), and `cn0_dbhz` (the sensitivity; its placeholder default sizes a
    toy grid, so set it to your real link budget).
- **Set for your front end:** `spc` (samples/chip = `sample_rate / chip_rate`;
    default 4) and `reps` (how many coherent code repetitions you can afford —
    the coherence ceiling and your latency budget; default 1).
- **Safe defaults — leave unless you have a reason:** `pfa=1e-3`, `pd=0.9`,
    `noise_mode="mean"`, `doppler_uncertainty=0` (full native span).
- **Weak-signal opt-in:** raise `max_noncoh` above `1` to let auto-config add
    non-coherent looks once the coherent ceiling (`reps`) is exhausted.

`sf` is **not** a parameter — it is inferred from `len(code)`, so the engine and
your replica can never disagree.

### Waveform vs. operator knobs

Some inputs describe the **transmitted waveform** — the receiver must match them,
they are not knobs:

- **`code`** — the PN sequence; its length *is* `sf` (the spreading factor).
- **`chip_rate`** — the transmitter's chip rate (Hz). With `spc` it sets the
    sample rate `fs = chip_rate·spc` and the Doppler **span** `±chip_rate/(2·sf)`.
- **`spc`** — **samples per chip** (chip-rate oversampling; *not* samples per
    *symbol* — that is `sps`) = your `sample_rate / chip_rate`. You only move it by
    resampling the front end.

The genuine receiver / operator knobs:

| Goal                                 | Lever                                                        |
| ------------------------------------ | ------------------------------------------------------------ |
| Tighter false-alarm rate             | smaller **`pfa`** (raises `threshold`)                       |
| Hold `pd` at lower C/N0              | larger **`reps`** (deeper coherent integration)              |
| More sensitive within a known offset | tighter **`doppler_uncertainty`** (fewer cells → lower gate) |
| Robust noise estimate (uncalibrated) | **`noise_mode="median"`**                                    |
| Reach below the coherent ceiling     | **`max_noncoh > 1`** (non-coherent looks)                    |

`reps` sets the coherent ceiling; the engine uses the *smallest* depth that meets
`pd`, so raising `reps` only helps weak signals (a strong one still resolves in a
few repetitions).

### Narrowing the Doppler search

If you already know the carrier offset lies within `±Δf`, pass
`doppler_uncertainty=Δf` (Hz, ≤ the native span). The engine then scans only the
Doppler bins inside that band, so the CFAR threshold pays a Bonferroni penalty
over **fewer** cells — a lower gate at the same system `pfa`, i.e. more
sensitivity for free. A value beyond the native span is rejected (`MemoryError`);
to search wider, use the coarse-mix bank below.

______________________________________________________________________

## Widening the Doppler search

The native search spans only `±chip_rate/(2·sf)` — one slow-time Nyquist, set by
the code period. When the true Doppler exceeds that, tile the wider range with a
sequence of **coarse Doppler hypotheses**: mix the raw stream down by each
`f_coarse` and run `Acquisition` on the result. The engine's fine FFT then
resolves the residual within the native span, and the absolute Doppler is
`f_coarse +` the fine bin.

```python
import numpy as np

chip_rate = 1.0e6
fs = chip_rate * 2                         # spc = 2
coarse = np.arange(-100e3, 100e3, 500.0)   # coarse grid (Hz) — see step rule below
bank = [Acquisition(code, reps=10, spc=2, chip_rate=chip_rate, cn0_dbhz=50,
                    pfa=1e-3, pd=0.9)
        for _ in coarse]                   # one engine per channel (own state)

n0 = 0
for chunk in iq_stream:                                  # any cf32 block
    n = n0 + np.arange(len(chunk))
    for f_coarse, acq in zip(coarse, bank):
        mixed = (chunk * np.exp(-2j * np.pi * f_coarse / fs * n)).astype(np.complex64)
        for dop, phase, *_rest, cn0 in acq.push(mixed):
            k = (dop + acq.doppler_bins // 2) % acq.doppler_bins \
                - acq.doppler_bins // 2
            doppler_hz = f_coarse + k * acq.doppler_res_hz
            print(f"hit: {doppler_hz:+.0f} Hz, code phase {phase}")
    n0 += len(chunk)
```

- **Coarse step.** Within-segment carrier rotation (`residual · code-period`
    cycles) sinc-rolls the code correlation, so keep the residual under ~0.25 cycle
    for \<1 dB loss. Stepping by the **full** native window (`chip_rate/sf`) abuts
    the tiles but leaves a half-window residual at each edge — 0.5 cycle, ~4 dB
    down. Halving the step (50% overlap) drops the residual to 0.25 cycle (\<1 dB)
    at twice the channel count.
- **Relation to the roll method.** Mixing the input is the continuous-frequency
    dual of rolling `conj(FFT(code))` by integer bins (each bin = one native
    window); mixing just lets you choose a finer, half-window step.
- **Cost** scales linearly with the number of coarse channels; the inner fine
    search is unchanged.

### Worked example — 1 Mcps, length-1000 code, ±100 kHz

| quantity              | value                                          |
| --------------------- | ---------------------------------------------- |
| chip rate `chip_rate` | 1 Mcps                                         |
| code length `sf`      | 1000 chips                                     |
| code period           | `sf/chip_rate` = 1 ms                          |
| repetitions `reps`    | 10 → up to 10 ms coherent integration          |
| samples/chip `spc`    | 2 → `fs` = 2 Msps, `code_bins = sf·spc` = 2000 |

**Fine (native) search** — the Doppler figures depend only on the waveform, not
`spc` (it cancels):

- resolution = `chip_rate / (sf · doppler_bins)` = `1/(10 ms)` = **100 Hz** at the
    full `doppler_bins = 10`
- span = `±chip_rate / (2·sf)` = **±500 Hz** (10 bins)
- code-phase bins = `code_bins` = **2000** (half-chip, because `spc = 2`)

**Reaching ±100 kHz** — that is 200× the native ±500 Hz window, so sweep a coarse
grid (`200 kHz` total to cover):

- abutting tiles: step = native window = `chip_rate/sf` = **1 kHz** →
    `200 kHz / 1 kHz` = **200 channels**, but the `±500 Hz` edge residual costs
    ~4 dB.
- low-loss (50% overlap): step = `chip_rate/(2·sf)` = **500 Hz** →
    `200 kHz / 500 Hz` = **400 channels**, residual `±250 Hz` (≤ 0.25 cycle, \<1 dB)
    — this is the grid in the snippet above.
- each channel searches a `10 × 2000` (Doppler × code-phase) surface at the native
    **100 Hz** resolution, with full 10 ms coherent gain.

So acquisition is **200–400 fine searches**, one per coarse mix — `Acquisition` runs
the inner search and CFAR; your loop sweeps `f_coarse`. Halving the requirement
(e.g. ±50 kHz) halves the channel count; a shorter code (smaller `sf`) widens the
native window and cuts the coarse sweep proportionally.

______________________________________________________________________

## The DSSS receive chain

`Acquisition` is the front of a two-stage receiver: **acquire**, then **track**.
Once it reports a `(Doppler bin, code phase)`, hand the coarse estimate to the
[`BurstDespreader`](../api/python-dsss.md), which closes a DLL + Costas loop to
track code phase and carrier and recover the payload bits. Both live in
`doppler.dsss`:

```python
from doppler.dsss import Acquisition, BurstDespreader
```

______________________________________________________________________

## See also

- [Python: DSSS API](../api/python-dsss.md) — full `Acquisition` + `BurstDespreader` reference
- [Python: Detection Statistics](../api/python-detection.md) — `det_threshold` / `det_pd` / `det_dwell`
- [Gallery: 2-D Acquisition](../gallery/detection2d.md) — the `CorrDetector2D` matched-filter surface
- [Gallery: DSSS Acquisition & Despreading](../gallery/dsss-despread.md) — end-to-end demo
- [Design: pure-functional acquisition kernel](../design/acq-fn.md) — the
    elastic `(ddc_fn, acq_fn)` pipeline behind `Acquisition`, for pod fan-out
    and checkpoint/resume
