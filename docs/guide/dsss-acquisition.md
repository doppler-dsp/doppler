# DSSS Burst Acquisition

`doppler.dsss.Acquirer` acquires a direct-sequence spread-spectrum burst — a run
of repeated, BPSK-modulated PN-code segments — arriving with an **unknown code
phase** and an **unknown carrier-frequency (Doppler) offset**, buried in noise.
It owns the whole receive-side acquisition pipeline:

```
raw cf32  →  ring  →  reframe (ny, nx)  →  slow-time Doppler FFT
          →  code correlation (corr2d)  →  CFAR gate  →  (Doppler, code-phase) hits
```

You give it the PN code to search for and a target **(Pfa, Pd)**; it computes
its own detection threshold and coherent dwell from the
[detection theory](../api/python-detection.md) and streams detections.

This is the usage walk-through. For the matched-filter surface it builds on, see
[2-D Acquisition](../gallery/detection2d.md) (`Detector2D`); for what happens
*after* acquisition, see the [DSSS Despreader](../api/python-dsss.md).

!!! tip "The 30-second version"

    ```python
    import numpy as np
    from doppler.dsss import Acquirer
    from doppler.wfm import PN, mls_poly

    code = PN(poly=mls_poly(5), seed=1, length=5).generate(31)  # 31-chip PN

    acq = Acquirer(
        code, sf=31, spc=4, ny=16,   # search grid: 16 Doppler × 124 code-phase
        pfa=1e-3, pd=0.9,            # target false-alarm / detection rates
        min_snr=0.2,                # expected per-sample amplitude SNR
    )
    acq.dwell, acq.threshold        # auto-configured from (pfa, pd, min_snr)

    for chunk in iq_stream:                          # any cf32 block size
        for dop, phase, peak, noise, stat, snr in acq.push(chunk):
            print(f"hit: Doppler bin {dop}, code phase {phase} samples, "
                  f"SNR≈{snr:.2f}")
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

`Acquirer` evaluates the entire (Doppler × code-phase) grid per dwell and reports
the cells whose detection statistic crosses an automatically set CFAR gate.

______________________________________________________________________

## How it works — slow-time / fast-time

Acquisition is a delay–Doppler search, and `Acquirer` factors it into two FFTs
over the repeated-segment structure. It frames the stream into a `(ny, nx)`
matrix where each **row is one PN segment** and there are `ny` of them:

- **fast-time** — *within* one segment (`nx = sf·spc` samples). A circular
    correlation against the known code → the **code-phase** axis.
- **slow-time** — *across* the `ny` segments. An FFT along that axis resolves the
    per-segment carrier phase ramp → the **Doppler** axis.

```
                fast-time  (nx = sf·spc samples, one segment)
              ┌───────────────────────────────────────────┐
   slow-time  │ seg 0   · · · · · · · · · · · · · · · · · · │
   (ny rows = │ seg 1   · · · · · · · · · · · · · · · · · · │   FFT down ──► Doppler
    segments) │  ...                                        │   columns
              │ seg ny-1· · · · · · · · · · · · · · · · · · │
              └───────────────────────────────────────────┘
                          │ circular code correlation
                          ▼  along each row → code phase
```

Internally the fast-time correlation is the FFT-domain
[`Corr2D`](../gallery/detection2d.md) engine against a **single-row reference**
(the code in row 0, zeros elsewhere); the slow-time FFT is applied to the data
*before* that correlation. Because the engine ingests **raw** samples and does
the slow-time FFT itself, you never pre-transform anything — just `push` IQ.

!!! note "Doppler resolution and range"

    Two independent quantities:

    - **bin spacing** = `1 / (ny·nx)` cycles/sample — set by how many segments
        `ny` you integrate (more segments → finer bins, the receiver's choice).
    - **search span** = `±1 / (2·nx)` cycles/sample — set by the segment length
        `nx`, i.e. the *waveform*, and independent of `ny`.

    With `sf=31, spc=4 → nx=124` and `ny=16`: bins are `1/1984` cycles/sample
    apart, spanning `±1/248`. Since `nx = sf·spc` and `spc ≥ 1`, the widest native
    span is `±1/(2·sf)` (at `spc=1`) — fixed by the code. To search *wider* than
    that, sweep a coarse Doppler grid in front of `Acquirer` — see
    [Widening the Doppler search](#widening-the-doppler-search).

______________________________________________________________________

## Auto-configuration from (Pfa, Pd)

You do not pick a threshold or a dwell — you state the operating point and the
engine derives them with the [detection functions](../api/python-detection.md).
Given the grid size `N = ny·nx` and the expected per-sample amplitude SNR
`min_snr`:

```
pfa_cell  = 1 - (1 - pfa)**(1/N)     # Bonferroni: N cells searched per dwell
eta       = det_threshold(pfa_cell)  # Rayleigh amplitude gate  √(-2 ln pfa_cell)
threshold = eta · √(2/π)             # eta in mean-CFAR units (E|Rayleigh| = √(π/2))
dwell     = smallest d with det_pd(min_snr, d·N, eta) ≥ pd
```

One frame coherently integrates `N` samples, so a dwell of `d` frames integrates
`d·N` samples — that is the integration length handed to `det_pd`. The results
are exposed as **read-only** properties:

```python
acq = Acquirer(code, sf=31, spc=4, ny=16, pfa=1e-3, pd=0.9, min_snr=0.2)

acq.ny, acq.nx, acq.n          # 16, 124, 1984  — the search grid
acq.pfa_cell                   # per-cell false-alarm prob (Bonferroni)
acq.eta                        # raw Rayleigh threshold
acq.threshold                  # the CFAR gate actually applied (eta·√(2/π))
acq.dwell                      # frames coherently integrated per detection
acq.pd_predicted               # Pd at min_snr and the chosen dwell
```

`min_snr` is a **per-sample amplitude** SNR (linear). Convert from a per-sample
power SNR in dB with `min_snr = 10**(snr_db/20)` — e.g. `-18 dB → 0.126`. Lower
`min_snr` ⇒ a larger `dwell` is needed to hold `pd`:

| `min_snr` | meaning (power dB) | resulting `dwell` |
| --------- | ------------------ | ----------------- |
| `0.30`    | −10.5 dB           | 1                 |
| `0.20`    | −14.0 dB           | 1                 |
| `0.09`    | −20.9 dB           | 2                 |

`noise_mode` selects the CFAR estimator (`"mean"` by default, which is what the
analytic `threshold` assumes; `"median"` is more robust but is not analytically
calibrated). `max_dwell` caps the dwell search.

______________________________________________________________________

## Streaming and reading hits

`push` accepts any-length cf32 blocks, buffers them in a ring, and emits one
detection per coherent **dump** (every `dwell` frames) whose statistic clears the
gate. Each hit is a 6-tuple:

```python
for dop, phase, peak, noise, stat, snr in acq.push(chunk):
    ...
```

| field         | meaning                                               |
| ------------- | ----------------------------------------------------- |
| `doppler_bin` | peak row — slow-time Doppler bin (`0 … ny-1`)         |
| `code_phase`  | peak column — integer-sample code phase (`0 … nx-1`)  |
| `peak_mag`    | peak correlation magnitude over the surface           |
| `noise_est`   | CFAR noise estimate                                   |
| `test_stat`   | `peak_mag / noise_est` (compared against `threshold`) |
| `snr_est`     | estimated per-sample amplitude SNR of the burst       |

Map the integer bins back to physical units:

```python
def doppler_cps(dop, ny, nx):
    """Doppler bin → cycles/sample (folds the upper half to negative)."""
    k = (dop + ny // 2) % ny - ny // 2
    return k / (ny * nx)

delay_chips = phase / acq.spc          # code phase in chips
```

`reset()` drains the ring and the coherent accumulator (use it between
independent captures).

!!! note "Code phase tracks the stream offset"

    The code phase is measured against the frame grid, which is anchored at sample
    0\. Inserting `Δ` extra samples of lead-in (silence) before the burst shifts
    every reported `code_phase` by `Δ mod nx` — that offset *is* extra
    propagation delay. Frame the same burst at a different stream position and the
    Doppler bin is unchanged but the code phase rotates accordingly.

______________________________________________________________________

## How many hits to expect

Acquisition fires once per dwell while the burst fills the search window. The
engine frames on a fixed grid anchored at stream sample 0, so a frame produces
full processing gain only when its whole `N`-sample window lies inside the burst.
For a burst of `R` segments (`R·nx` samples) after `L` lead-in samples, the count
of full-gain frames is

```
F = (L + R·nx) // N  -  ceil(L / N)
```

With `dwell == 1` each full frame yields one hit. A **frame-aligned** lead-in
(`L` a multiple of `N`) gives exactly `F` hits at one cell; a non-aligned lead-in
gives `F` to `F+2` (boundary frames that straddle the burst edge may also fire)
and rotates the code phase by `L mod nx`. With `dwell == d`, hits arrive once per
`d` fully-covered frames. A payload that follows the preamble on a **different**
code decorrelates from the matched filter and does not produce burst-cell hits.

______________________________________________________________________

## Choosing parameters

Some parameters describe the **transmitted waveform** — the receiver must match
them, they are not knobs:

- **`code`, `sf`** — the PN sequence and its length in chips. Fixed by the
    transmitter (`len(code) == sf`).
- **`spc`** — **samples per chip** (chip-rate oversampling; *not* samples per
    *symbol* — that is `sps`) = your `sample_rate / chip_rate`. The chip rate is
    the transmitter's, so you only move `spc` by resampling the front end. It sets
    the segment length `nx = sf·spc` and therefore the Doppler **span**
    `±1/(2·nx)` — at `spc=1` that bottoms out at `±1/(2·sf)`, the widest *native*
    search. Going wider needs a coarse spectral-roll pre-search, not a tweak here.

The genuine receiver / operator knobs:

| Goal                                   | Lever                                                                       |
| -------------------------------------- | --------------------------------------------------------------------------- |
| Tighter false-alarm rate               | smaller **`pfa`** (raises `threshold`)                                      |
| Hold `pd` at lower SNR                 | smaller **`min_snr`** (raises `dwell`)                                      |
| Finer Doppler resolution + more gain   | integrate more segments — larger **`ny`**, up to the K present in the burst |
| Lower latency / shorter required burst | fewer segments — smaller **`ny`** (coarser Doppler, less gain)              |
| Robust noise estimate (uncalibrated)   | **`noise_mode="median"`**                                                   |
| Bound the coherent integration         | **`max_dwell`**                                                             |

`ny` changes Doppler **resolution** (`1/(ny·nx)`), not span — the span is fixed
by the waveform above.

______________________________________________________________________

## Widening the Doppler search

The native search spans only `±1/(2·nx)` — one slow-time Nyquist, set by the code
period. When the true Doppler exceeds that, tile the wider range with a sequence
of **coarse Doppler hypotheses**: mix the raw stream down by each `f_coarse` and
run `Acquirer` on the result. The engine's fine FFT then resolves the residual
within `±1/(2·nx)`, and the absolute Doppler is `f_coarse +` the fine bin.

```python
import numpy as np

fs = 2e6                                   # = spc · chip_rate
coarse = np.arange(-100e3, 100e3, 500.0)   # coarse grid (Hz) — see step rule below
bank = [Acquirer(code, sf=1000, spc=2, ny=10, pfa=1e-3, pd=0.9, min_snr=0.3)
        for _ in coarse]                   # one engine per channel (own integration state)

n0 = 0
for chunk in iq_stream:                                  # any cf32 block
    n = n0 + np.arange(len(chunk))
    for f_coarse, acq in zip(coarse, bank):
        mixed = (chunk * np.exp(-2j * np.pi * f_coarse / fs * n)).astype(np.complex64)
        for dop, phase, *_rest, snr in acq.push(mixed):
            doppler_hz = f_coarse + doppler_cps(dop, acq.ny, acq.nx) * fs
            print(f"hit: {doppler_hz:+.0f} Hz, code phase {phase}")
    n0 += len(chunk)
```

- **Coarse step.** Within-segment carrier rotation (`residual · code-period`
    cycles) sinc-rolls the code correlation, so keep the residual under ~0.25 cycle
    for \<1 dB loss. Stepping by the **full** native window (`1/nx`) abuts the tiles
    but leaves a `±1/(2·nx)` residual at each edge — 0.5 cycle, ~4 dB down. Halving
    the step to `1/(2·nx)` (50% overlap) drops the residual to `±1/(4·nx)`
    (0.25 cycle, \<1 dB) at twice the channel count.
- **Relation to the roll method.** Mixing the input is the continuous-frequency
    dual of rolling `conj(FFT(code))` by integer bins (each bin = one native
    window, `1/nx`); mixing just lets you choose a finer, half-window step.
- **Cost** scales linearly with the number of coarse channels; the inner fine
    search is unchanged.

### Worked example — 1 Mcps, length-1000 code, ±100 kHz

| quantity                 | value                                   |
| ------------------------ | --------------------------------------- |
| chip rate `Rc`           | 1 Mcps                                  |
| code length `L` (= `sf`) | 1000 chips                              |
| code period              | `L/Rc` = 1 ms                           |
| segments `ny`            | 10 → 10 ms coherent integration         |
| samples/chip `spc`       | 2 → `fs` = 2 Msps, `nx = sf·spc` = 2000 |

**Fine (native) search** — the Doppler figures depend only on the waveform, not
`spc` (it cancels):

- resolution = `1 / (ny · code-period)` = `1/10 ms` = **100 Hz** ( = `fs/(ny·nx)` )
- span = `±1 / (2 · code-period)` = **±500 Hz** (10 bins; = `±fs/(2·nx)`)
- code-phase bins = `nx` = **2000** (half-chip, because `spc = 2`)

**Reaching ±100 kHz** — that is 200× the native ±500 Hz window, so sweep a coarse
grid (`200 kHz` total to cover):

- abutting tiles: step = native window = `Rc/L` = **1 kHz** → `200 kHz / 1 kHz` =
    **200 channels**, but the `±500 Hz` edge residual costs ~4 dB.
- low-loss (50% overlap): step = `Rc/(2L)` = `fs/(2·nx)` = **500 Hz** →
    `200 kHz / 500 Hz` = **400 channels**, residual `±250 Hz` (≤ 0.25 cycle, \<1 dB)
    — this is the grid in the snippet above.
- each channel searches a `10 × 2000` (Doppler × code-phase) surface at the native
    **100 Hz** resolution, with full 10 ms coherent gain.

So acquisition is **200–400 fine searches**, one per coarse mix — `Acquirer` runs
the inner search and CFAR; your loop sweeps `f_coarse`. Halving the requirement
(e.g. ±50 kHz) halves the channel count; a shorter code (smaller `L`) widens the
native window and cuts the coarse sweep proportionally.

______________________________________________________________________

## The DSSS receive chain

`Acquirer` is the front of a two-stage receiver: **acquire**, then **track**.
Once it reports a `(Doppler bin, code phase)`, hand the coarse estimate to the
[`Despreader`](../api/python-dsss.md), which closes a DLL + Costas loop to track
code phase and carrier and recover the payload bits. Both live in
`doppler.dsss`:

```python
from doppler.dsss import Acquirer, Despreader
```

______________________________________________________________________

## See also

- [Python: DSSS API](../api/python-dsss.md) — full `Acquirer` + `Despreader` reference
- [Python: Detection Statistics](../api/python-detection.md) — `det_threshold` / `det_pd` / `det_dwell`
- [Gallery: 2-D Acquisition](../gallery/detection2d.md) — the `Detector2D` matched-filter surface
- [Gallery: DSSS Acquisition & Despreading](../gallery/dsss-despread.md) — end-to-end demo
