# DSSS Primary Use Cases for Code Acquisition Design

Two operating regimes drive the acquisition design. Both are **wide-Doppler**
(the uncertainty far exceeds one code-period's resolution), but they sit at
opposite ends of the speed-vs-sensitivity trade, so they pick **different**
methods. The design goal for each is the **fastest method that still acquires**
at the target `(Pfa, Pd)`.

______________________________________________________________________

## The method palette

Every acquisition method is built from two Doppler primitives — both already
shipped in doppler — optionally wrapped in a coarse mixer/tiling layer and a
non-coherent layer:

| Method                                              | doppler API                                         | Doppler res / span                    | Integration            | Use when                                                 |
| --------------------------------------------------- | --------------------------------------------------- | ------------------------------------- | ---------------------- | -------------------------------------------------------- |
| **Column-FFT** (slow-time)                          | `dsss.BurstAcquisition`                              | `1/(ny·nx)` / `±1/(2nx)`              | ny epochs **coherent** | many coherent reps; need the gain                        |
| **2-D roll** (2-D code FFT × signal FFT → 2-D IFFT) | `spectral.Corr2D` / `CorrDetector2D` (manual composition); natively via `dsss.BurstAcquisition`'s wideband fallback when `doppler_uncertainty` exceeds the native span, or unconditionally via `dsss.Acquisition` (continuous mode always uses this mechanism) | `1/nx` / `±1/2` | **1 epoch** | wide Δf, few reps, enough SNR — whole grid in one 2-D op |
| **Mixer bank**                                      | caller loop ([guide](../guide/dsss-acquisition.md)) | tiles either, `1/(2nx)` step          | —                      | widen Δf at a fine resolution; linear cost               |
| **Non-coherent**                                    | auto-selected `n_noncoh` (both classes, read-only)  | sensitivity past the coherent ceiling | `N_nc` looks           | data-bit / burst-limited `M_coh`                         |

The two primitives are **exact duals**: the roll's *resolution* (`1/nx`) equals
the column-FFT's *span* (`±1/(2nx)`). Roll = **coarse + wide** from one epoch;
column-FFT = **fine + narrow** from `ny` coherent epochs. Mixing the input is the
continuous-frequency dual of rolling `conj(FFT(code))` by integer bins.

______________________________________________________________________

## Choosing the method — three knobs

1. **Doppler uncertainty `Δf`** vs the native span `±1/(2nx)` (`= ±1/(2·T_epoch)`).
    Narrow → one column-FFT covers it. Wide → tile with a mixer bank **or** sweep
    all bins at once with a 2-D roll.
1. **Coherent epochs `M_coh`** — how many epochs you can integrate *coherently*,
    capped by the burst length (UC2) or the data-bit period (UC1). Large → the
    column-FFT's fine resolution + `10·log10(ny·N)` gain is worth its narrow span.
    Small (≈1) → the column-FFT is wasted; the roll gives the whole grid for the
    cost of one epoch.
1. **Sensitivity (`cn0_dbhz`)** — can one epoch acquire, or do you need the gain?
    High C/N0 → roll (fast). Low C/N0 → coherent gain (column-FFT) and/or
    **non-coherent** looks (`n_noncoh`, auto-selected) when the coherent
    ceiling is the data bit.

**Decision table** (`Δf` vs native span × `M_coh`):

|               | `M_coh` small (≈1)                                       | `M_coh` large                                                                                    |
| ------------- | -------------------------------------------------------- | ------------------------------------------------------------------------------------------------ |
| **Δf narrow** | `BurstAcquisition`, `reps`=few                           | `BurstAcquisition` alone (fine + gain)                                                           |
| **Δf wide**   | **2-D roll** (one op, all bins) + non-coherent over reps | 2-D-roll *coarse* **or** mixer-tile, then column-FFT *fine*; pick by channel-count vs one 2-D op |

The cost asymmetry that decides the wide-Δf row: a 2-D roll sweeps all
`~Δf/(1/T_epoch)` Doppler bins from **one** epoch, whereas the fine mixer bank
runs that many channels, each a `Corr2D` tile **integrating `ny` epochs and
resolving `ny`× finer bins**. So the bank costs roughly `ny`× more *per Doppler
window* — and measurably more once the finer bin count is included: **≈40×** on a
`16×2046` (`ny=16`) grid, **≈10×** on a `5×4094` (`ny=5`) grid — the factor
tracks `ny` (`bench_widedoppler.py`). The roll pays for it with one epoch's
gain and `1/nx` (coarse) resolution. So: **roll when SNR lets one epoch acquire;
column-FFT/mixer when you must buy the coherent gain.**

!!! warning "`M_coh` large is only safe over a genuinely code-only window"

    "Capped by ... the data-bit period" above is not a diminishing-returns
    cutoff you can push past for a bit more gain — it's a hard requirement.
    `M_coh` large is only the right column-FFT answer when those `M_coh`
    epochs are genuinely data-free for their whole span (a preamble, or the
    periodic code-only pilot epochs in UC1 below). Coherently integrating
    across real data transitions doesn't just cost gain smoothly: the data's
    own baseband spectrum aliases across the *entire* Doppler-bin axis once
    the window spans more than a handful of symbols, and a real,
    deterministic mislock (wrong bin wins outright) results — confirmed on
    this project's own continuous receiver. See
    [Continuous, data-modulated signals](../guide/dsss-acquisition.md#continuous-data-modulated-signals-the-asynchronous-symbol-clock-case)
    and
    [DSSS Acquisition — Continuous Async-Data Modulation](../gallery/dsss-acq-async-data.md).
    On a data-bearing stretch, `M_coh` is effectively 1 regardless of what
    the decision table above implies — reach for `Acquisition` (continuous)
    instead of `BurstAcquisition`, not a larger `reps` on the same class. The
    continuous class's `n_noncoh` auto-selects the non-coherent looks that
    close the resulting Pd gap; there's no manual choice to make.

______________________________________________________________________

## GPS-like always on 1023-chip Gold code

- Chip rate < 10 MHz
- Doppler < 25 ppm moves
- Symbol rate kHz range
- Data BPSK asynchronous with code
- Periodic multi-epoch, code-only transmissions
- Plenty of time to acquire and lock

**Regime** (illustrative: L1 C/A, `Rc = 1.023 MHz`, `L = 1023`, `spc = 2`):
`nx = 2046`, `T_epoch = 1 ms`, native span **±500 Hz** (one slow-time bin =
1 kHz). 25 ppm of 1.575 GHz ⇒ Doppler **±39 kHz** — **~79×** the native span,
so decidedly wide. Asynchronous kHz-rate data ⇒ `M_coh ≈ 1` on data-bearing
stretches; the **periodic code-only epochs** are the coherent windows
(`M_coh = pilot epochs`).

**Approach — sensitivity-driven (time is plentiful):**

- **Coherent inner:** run `BurstAcquisition` over the code-only pilot windows
    for the fine `1/(ny·T_epoch)` Doppler and full coherent gain (safe here
    because those windows are genuinely data-free).
- **Non-coherent across:** `Acquisition` (continuous)'s auto-selected
    `n_noncoh` (**P1**) accumulates looks across the always-on, data-bearing
    stream — the data-bit ceiling makes this the real sensitivity engine;
    "plenty of time" means you can spend many looks, up to the internal
    256-look safety valve.
- **Wide Δf:** a coarse layer in front — either a **mixer bank** (~158 channels
    at 50% overlap for \<1 dB edge loss) or a **2-D roll** (~79 Doppler bins in one
    2-D op per epoch) — then the column-FFT refines each coarse bin.

Because sensitivity, not latency, is the constraint, the column-FFT + P1 stack is
the core; the coarse layer just covers the 25 ppm span.

______________________________________________________________________

## Burst Transmission

- Chip rate in MHz, symbol rate in kHz to MHz
- Doppler < 25 ppm moves
- 5+ long (rel. to data code) acquisition code repetitions (no data)
- Payload follows with shorter data code
- Data synchronous with symbols

**Regime** (illustrative: `Rc = 5 MHz`, long acq code `L = 2047`, `spc = 2`,
carrier 2.4 GHz): `nx = 4094`, `T_epoch ≈ 0.41 ms`, native span **±1.2 kHz**
(2.44 kHz window). 25 ppm ⇒ Doppler **±60 kHz** — **~49×** the native span, wide.
The unmodulated preamble gives a **clean coherent `M_coh = R ≥ 5`**, and the
burst is **latency-bound**: acquire within the ~5-epoch preamble (~2 ms) before
the payload.

**Approach — speed-driven (latency-bound, wide Δf, few reps):**

- **2-D roll is the fit:** one 2-D op per epoch sweeps **all ~49 Doppler bins**
    at the coarse `1/nx` (2.44 kHz) resolution — **≈10× cheaper** on this `5×4094`
    grid (`bench_widedoppler.py`) than running ~98 fine mixer channels each
    integrating 5 epochs, which a 2-ms latency budget cannot afford serially.
- **Refine only if needed:** if the burst despreader's pull-in can't swallow a
    2.44 kHz residual, column-FFT over the 5 reps **within** the winning coarse
    bin (one channel) for the fine `1/(5·T_epoch)` ≈ 490 Hz Doppler.
- **Hand off:** the `(Doppler bin, code phase)` seeds the shipped
    [`BurstDespreader`](../api/python-dsss.md) on the shorter data code for the
    synchronous payload.

Here latency and the wide span dominate; the roll's one-epoch coarse sweep beats
the coherent bank.

______________________________________________________________________

## Why these are the *fastest* — cost model

For a Doppler span `Δf` requiring `C = Δf / (1/T_epoch)` native windows:

- **Mixer + column-FFT:** `C` channels, each a `Corr2D(ny, nx)` tile — `C·ny`
    **fine** bins, full `10·log10(ny·N)` coherent gain, but a 2-D FFT over `ny`
    epochs per channel.
- **2-D roll:** `C` **coarse** bins from **one** epoch — one forward `FFT_nx`
    shared across `C` inverse `FFT_nx` — one epoch's gain, `1/nx` resolution.

Measured (`bench_widedoppler.py`): the fine bank is **≈40×** the roll on the GPS
`16×2046` grid (`C=79`) and **≈10×** on the burst `5×4094` grid (`C=49`) — it
pays for `ny`× finer bins *and* `ny` epochs of integration, so the gap grows with
`ny`. So the
choice is gain vs speed at fixed span: spend the compute (and `ny` epochs of
latency) to buy `10·log10(ny)` of coherent gain (UC1, sensitivity-bound), or take
the one-epoch roll when the SNR already supports a single-epoch detection (UC2,
latency-bound). The non-coherent layer (P1) extends UC1's reach when the coherent
ceiling is the data bit. See [Benchmarking](benchmarking.md).

______________________________________________________________________

## See also

- [DSSS acquisition architecture + roadmap](../design/dsss-acquisition.md) — the
    CAF framing, the trade space, and the phased plan (P1 non-coherent shipped; P2
    sub-block `K` is the wide-Doppler *widener* that cuts the channel count).
- [DSSS Burst Acquisition guide](../guide/dsss-acquisition.md) — the mixer-bank
    loop and worked ±100 kHz example.
- [corr2d interpolated inverse](../design/corr2d-interpolated-inverse.md) — the
    code-phase axis (independent of the Doppler-span choice here).
