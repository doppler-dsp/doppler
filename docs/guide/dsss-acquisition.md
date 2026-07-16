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

!!! warning "`reps` assumes a data-free coherent window"

    Deeper coherent integration (a larger `reps`/`doppler_bins`) is only safe for
    the classic **preamble** case this section describes — a data-free code
    repeated back to back. Raising it on a **continuous, data-modulated**
    signal doesn't just cost some gain, it can produce a deterministic
    **mislock** onto the wrong Doppler bin. See
    [Continuous, data-modulated signals](#continuous-data-modulated-signals-the-asynchronous-symbol-clock-case)
    below before reaching for `reps` on that kind of signal.

### Narrowing the Doppler search

If you already know the carrier offset lies within `±Δf`, pass
`doppler_uncertainty=Δf` (Hz, ≤ the native span). The engine then scans only the
Doppler bins inside that band, so the CFAR threshold pays a Bonferroni penalty
over **fewer** cells — a lower gate at the same system `pfa`, i.e. more
sensitivity for free. A value beyond the native span is rejected (`MemoryError`);
to search wider, use the coarse-mix bank below.

______________________________________________________________________

## Continuous, data-modulated signals — the asynchronous-symbol-clock case

Everything above assumes the classic **preamble** case: an isolated run of the
*same*, data-free code repeated back to back, so any coherent depth up to
`reps` sums honestly. That assumption breaks for a **continuous** carrier —
e.g. a beacon or telemetry stream where the spreading code repeats forever and
BPSK data rides on top continuously, with a symbol clock that is *not* an
integer multiple of the code-epoch clock (`chip_rate / symbol_rate` not a
whole number — the common case in real hardware, where the two clocks derive
from independent budgets). If you know your `symbol_rate`, use it to drive the
construction below instead of the default coherent-first sizing.

### Why this changes the sizing decision

A data-bit transition landing *inside* one coherent epoch splits that epoch's
contribution into two oppositely-signed halves. This does more than cost some
correlation gain at the true code phase — it can produce a genuine,
deterministic **mislock**: the code's off-peak correlation has no bound over
an unequal-length partial window the way it does over a full period, so at
some *other* candidate phase the two mis-signed partial sums can happen to add
constructively and beat the (weakened) true-phase peak. This is not a noise
event — see
[DSSS Acquisition — Continuous Async-Data Modulation](../gallery/dsss-acq-async-data.md)
for the worked proof: a bad epoch replayed with all injected noise removed
reproduces the identical mislock, bit for bit.

The failure only shows up once your coherent window spans more than a small
fraction of a symbol — which is exactly what a naive
`Acquisition(code, reps=16, ...)` call risks, since the engine's default
sizing greedily grows the coherent depth (`doppler_bins`) up to `reps` before
ever falling back to non-coherent combining (`n_noncoh`). By default, nothing
in the constructor tells it a data-modulated symbol clock is present —
`Acquisition` only ever sees a `code` and a `chip_rate`. Pass `symbol_rate` and
it does.

### The robust default: pass `symbol_rate`

Given the high-level inputs a typical caller actually has — the `code`, the
`chip_rate`, the `symbol_rate`, a Doppler uncertainty, and a `cn0_dbhz`
sensitivity — the robust construction is:

```python
chip_rate = 1.023e6            # Hz, the waveform (matches the code above)
symbol_rate = 2400.0           # Hz -- present and asynchronous to chip_rate
doppler_uncertainty = 5000.0   # Hz, your link's Doppler budget

acq = Acquisition(
    code, chip_rate=chip_rate, cn0_dbhz=52,
    spc=4, doppler_uncertainty=doppler_uncertainty,
    reps=16,             # generous coherent-depth ceiling -- headroom, not a mandate
    max_noncoh=8,        # ceiling for the joint search's non-coherent axis
    symbol_rate=symbol_rate,  # tells the sizer a data clock is present
    pfa=1e-3, pd=0.9,
)
assert acq.pd_predicted >= acq.pd    # raise max_noncoh / reps if this fails
assert acq.doppler_bins == 1         # the search still lands here on its own:
                                      # growing the coherent depth at this
                                      # margin doesn't pay for the extra
                                      # self-cancellation risk, so it doesn't
```

`symbol_rate > 0` switches `_auto_config`'s search from the single-axis
"smallest `doppler_bins` that meets `pd` alone, else escalate `n_noncoh` at
the full `reps` ceiling" logic to a **joint** search over
`doppler_bins ∈ [1, reps] × n_noncoh ∈ [1, max_noncoh]`, honestly pricing the
data-transition self-cancellation loss at every candidate grid (the
semi-analytical model in
[DSSS Acquisition — Continuous Async-Data Modulation](../gallery/dsss-acq-async-data.md#the-theory-curves-dashed)),
and picks whichever grid meets `pd` with the fewest total epochs, breaking
ties toward the smaller coherent depth. In practice this means the engine
naturally lands on a short coherent depth plus non-coherent looks exactly when
the physics calls for it — no more manually deciding to cap `reps=1` and
reasoning about *why* by hand. You still supply `max_noncoh` (and `reps`, if
you want to allow a nontrivial coherent depth at all) as ceilings; the search
picks the actual working point within them.

This is the better default for a second, independent reason beyond the
mislock: **it doesn't require phase coherency across the integration window.**
A coherent stack over several epochs (`doppler_bins > 1`) is a single-frequency
-hypothesis slow-time FFT — it implicitly assumes the carrier's phase evolves
predictably across the *entire* window. Any unmodeled dynamics inside that
window (residual acceleration, oscillator phase noise, a Doppler rate finer
than the bin grid resolves) bleeds coherent gain away as the window lengthens,
on top of whatever the data-transition mechanism costs. Non-coherent combining
is blind to phase between looks by construction, so it is immune to that
failure mode too — a real channel has these drift sources even when a
controlled simulation does not model them. The one thing you give up is the
classic non-coherent combining loss (roughly 1–3 dB versus the same total
energy combined ideally coherently, for small `N`) — a fair price for
robustness on a continuous, data-modulated link.

!!! danger "`doppler_resolution`/`doppler_rate` can defeat this protection"

    `Acquisition` also has `doppler_resolution` (floor a minimum Doppler-bin
    resolution) and `doppler_rate` (cap the coherent depth for Doppler-rate
    smearing) knobs. Both exist to serve a genuine, separate need — getting a
    finer Doppler estimate handed to a downstream tracking loop — but
    `doppler_resolution` works by **forcing `doppler_bins` up**, overriding the
    exact protection this section just described: even with `symbol_rate` set,
    a large enough `doppler_resolution` floor can push the coherent depth well
    past what the joint search would have picked on its own (the
    `acq.doppler_bins == 1` guarantee above no longer holds). Confirmed
    directly on this project's own continuous receiver: forcing
    `doppler_bins` up via `doppler_resolution` to shrink a downstream
    carrier-loop's pull-in range caused *frequent, gross* mislocks (the wrong
    Doppler bin winning outright, not just reduced accuracy) — the data
    modulation's own baseband spectrum, sampled at close to one sample per
    symbol, is broadband enough to alias real energy across the *entire*
    Doppler-bin axis once the coherent window spans more than a handful of
    symbols. Treat `doppler_resolution` as unsafe to raise on a continuous,
    data-modulated signal until it's paired with a resolution mechanism that
    doesn't grow real coherent depth (zero-padding the Doppler FFT, not yet
    shipped) — raising `reps`/`doppler_resolution` together is not a
    substitute.

### When coherent-only is still fine

If there is no `symbol_rate` to speak of — a genuine data-free preamble, the
classic case the rest of this guide describes — none of this applies: leave
`symbol_rate` at its default `0.0` (no known data-modulation clock) and get
the simpler coherent-first sizing (`reps` grown before `max_noncoh` engages),
which pays no combining-loss penalty. Pass `symbol_rate` **only** when you
know the code carries continuous data modulation during acquisition itself.

### Advanced: pinning the grid directly

Both searches above are convenience layers over the same underlying knob:
`doppler_bins` and `n_noncoh`. If you already know the grid you want — from a
prior characterization run, or because you want to A/B two specific
configurations without reconstructing the object — `configure_search_raw`
pins it directly, bypassing both searches:

```python
acq.configure_search_raw(doppler_bins=1, n_noncoh=8)
```

It resizes every buffer/plan the grid touches (the slow-time FFT, the code
correlator, every per-frame scratch buffer) and re-derives the threshold
ladder for that exact grid, clearing any in-flight accumulation — call it
between `push()` calls, never a substitute for one. Bounds are still
enforced (`doppler_bins ∈ [1, reps]`, `n_noncoh ∈ [1, max_noncoh]`); an
out-of-range pin raises `ValueError` and leaves the engine at its prior grid.

!!! warning "Pinning a large `doppler_bins` bypasses the mislock protection too"

    `configure_search_raw` is a direct pin — it doesn't know or care whether
    your signal is continuous and data-modulated. Pinning a large
    `doppler_bins` on one reintroduces exactly the mislock risk described in
    [Continuous, data-modulated signals](#continuous-data-modulated-signals-the-asynchronous-symbol-clock-case)
    above, with none of the joint search's Pd-honest pricing to warn you.
    Only pin a coherent depth beyond a handful of epochs when you know the
    window is genuinely data-free (a preamble) for its whole span.

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
- [Gallery: DSSS Acquisition — Continuous Async-Data Modulation](../gallery/dsss-acq-async-data.md) —
    the worked proof behind the `reps=1`/`max_noncoh` recommendation above
- [Design: pure-functional acquisition kernel](../design/acq-fn.md) — the
    elastic `(ddc_fn, acq_fn)` pipeline behind `Acquisition`, for pod fan-out
    and checkpoint/resume
