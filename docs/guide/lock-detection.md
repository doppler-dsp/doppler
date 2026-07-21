# Lock Detection Across `doppler.track`

Every continuous tracking loop in this codebase needs to answer one
question on every decision: *am I still locked?* This guide is the
map across all of them — which loops carry a lock detector, how each
one is sized, and which of two config entry points to reach for.

## Two classes of lock detection

This codebase has exactly two designs, and they don't mix:

- **Continuous, verify-counted** — a loop that runs indefinitely
    (code, carrier, or timing tracking) steps a statistic every look
    and feeds it to the shared primitive
    [`lockdet_core.h`](../api/python-detection.md#lock-verification):
    level hysteresis (a declare/drop threshold pair) plus time
    hysteresis (`n_up`/`n_down` consecutive looks). See
    [Lock Detection: Verify Counts](../gallery/lockdet.md) for how
    and why the verify counts work.
- **One-shot, statistical** — `BurstDespreader` tests a whole burst
    at once (`det_threshold_f`, an F-test over the burst's energy),
    because a burst has no "next look" to accumulate across. It is
    **not** a gap to retrofit with a verify count; there is nothing
    to verify against.

Everything below is the first class. `BurstDespreader`'s `lock_stat`
/ `stat_n` stay a deliberately different shape — see
[`docs/api/python-dsss.md`](../api/python-dsss.md).

## The consistency table

| Object                                        | Statistic                                                                                        | Config method                                                                                                                        | Decision                           |
| --------------------------------------------- | ------------------------------------------------------------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------------ | ---------------------------------- |
| [`Dll`](../gallery/dll.md)                    | CFAR ratio `R = sqrt(2·Σ\|P\|²/E‖O‖²)` over N looks                                              | `configure_lock(pfa, n_looks, ref_snr_db=0.0)` (derived) / `configure_lock_raw(...)` (raw escape hatch)                              | `.locked` / `.lock_stat`           |
| [`Costas`](../gallery/costas.md)              | `\|Re P\|/\|P\|` EMA                                                                             | `configure_lock(up_thresh, down_thresh, n_up, n_down)` (raw)                                                                         | `.locked` / `.lock_metric`         |
| [`SymbolSync`](../gallery/symsync.md)         | Gardner eye-opening ratio, block-averaged (see [design note](../design/timing_lock_detector.md)) | `configure_lock(rolloff, esno_min_db, pfa, pd)` (derived) / `configure_lock_raw(...)`                                                | `.locked` / `.lock_stat`           |
| [`CarrierNda`](../gallery/carrier-nda.md)     | M-th-power arm ratio EMA                                                                         | `configure_lock(up_thresh, down_thresh, n_up, n_down)` (raw)                                                                         | `.locked` / `.lock`                |
| [`MpskReceiver`](../gallery/mpsk-receiver.md) | carrier lock EMA (acquisition ↔ tracking handover)                                               | `configure_lock(up_thresh, down_thresh, n_up, n_down)` (raw)                                                                         | `.tracking`                        |
| `Despreader`                                  | forwards to its embedded `Dll` (code) and `Costas` (carrier)                                     | `configure_code_lock(pfa, n_looks, ref_snr_db=0.0)` (derived) / `configure_carrier_lock(up_thresh, down_thresh, n_up, n_down)` (raw) | `.code_locked` / `.carrier_locked` |

Every row got here the same way: an embedded `lockdet_core.h`
instance, a `configure_lock`-family setter, and a `.locked`-family
getter that reads the verify-counted decision, not the raw statistic.

## Which config method do I call?

Two families, and the difference is *whether a closed-form
`(pfa, pd)` derivation exists for that loop's statistic*:

- **Derived (`pfa`-style)** — `Dll`, `SymbolSync`, and
    `Despreader.configure_code_lock` (which forwards to `Dll`'s). Both
    statistics have a documented closed-form sizing: `Dll`'s CFAR ratio
    from `det_threshold_noncoherent`, `SymbolSync`'s eye-opening ratio
    from the Gaussian sizing in
    [`docs/design/timing_lock_detector.md`](../design/timing_lock_detector.md).
    State the operating point you want (`pfa`, `pd` or `n_looks`); the
    object works out the threshold and averaging depth itself.
- **Raw geometry** — `Costas`, `CarrierNda`, `MpskReceiver`, and
    `Despreader.configure_carrier_lock` (which forwards to `Costas`'s).
    Their statistics (`|Re P|/|P|`, the M-th-power arm ratio) don't
    have a documented closed-form `(pfa, pd)` sizing yet, so you state
    the `lockdet_core.h` geometry directly: `up_thresh`, `down_thresh`,
    `n_up`, `n_down`. Every "raw" object also exposes this as the
    escape hatch under its derived sibling (`Dll.configure_lock_raw`,
    `SymbolSync.configure_lock_raw`) for a caller that wants to size
    the geometry independently instead of trusting the closed form.

```python
import numpy as np

from doppler.dsss import Despreader
from doppler.track import CarrierNda, SymbolSync

rng = np.random.default_rng(0)
noise = (
    rng.standard_normal(4000) + 1j * rng.standard_normal(4000)
).astype(np.complex64)

# Derived: state the target operating point, the object sizes itself.
ss = SymbolSync(sps=4, bn=0.01, zeta=0.707)
ss.configure_lock(rolloff=0.35, esno_min_db=10.0, pfa=1e-3, pd=0.9)

d = Despreader(code=np.zeros(31, dtype=np.uint8), sps=2)
d.configure_code_lock(pfa=1e-3, n_looks=20)

# Raw geometry: state the lockdet_core.h threshold/verify-count rule
# directly (see the note on n_up below before copying these numbers).
car = CarrierNda(bn=0.01, sps=8, n=4, m=4)
car.configure_lock(up_thresh=0.5, down_thresh=0.4, n_up=64, n_down=32)

ss.steps(noise)
car.steps(noise)
d.steps(noise)
assert ss.locked is False
assert car.locked is False
assert d.code_locked is False
```

## A magic number is not the same as a correctly-sized one

Two findings from building this consistency pass are worth carrying
forward as standing principles, not just historical notes:

**Verify-count independence can silently fail on a fast, correlated
statistic.** `n_up` consecutive above-threshold looks compounding to
`pfa^n_up` (the whole reason a verify count buys cheap false-alarm
suppression — see [Lock Detection: Verify Counts](../gallery/lockdet.md))
*assumes independent looks*. `CarrierNda`'s lock statistic is a fast
EMA, so consecutive samples are highly autocorrelated — borrowing
`MpskReceiver`'s own already-shipped `n_up=8` for the *same*
statistic gave a real 13% false-lock rate under direct Monte Carlo
against noise-only input. `n_up=64` was the smallest verify count
that reliably eliminated false locks over 300 trials (see
`carrier_nda_core.c`'s `CARRIER_NDA_LOCK_DEFAULT_*` comment for the
trial data). The same root cause blocked two earlier `SymbolSync`
statistic designs before the shipped one. **The lesson: before
reusing a verify count across two different statistics, check
whether the new statistic is actually looking at independent
samples — a fast loop-filter EMA usually isn't, even if the
underlying physical quantity is.**

**A constant that lands safe by accident is not the same as one
that's sized correctly.** `SymbolSync`'s `avgs` sizing formula
originally carried a bare, uncalibrated `8` in a variance's place.
It happened to be *safe* (both `pfa` and `pd` targets were met, with
margin) — but only because `8` was roughly 6× larger than the
statistic's real measured variance, not because it was derived from
anything. Replacing it required two steps, not one: checking whether
`8` had a legitimate theoretical basis before assuming a straight
substitution (it didn't — see
[the design note](../design/timing_lock_detector.md#sizing-pfa-pd-avgs-threshold)),
and then *empirically verifying* the replacement rather than trusting
the algebra alone — a naive "just use the measured variance" swap
turned out to undersize `avgs` and blow past the `pfa` target by
~13×, because the formula's use of `erfcinv` instead of the standard
`Q⁻¹` silently drops a factor of 2. **The lesson: a magic number that
happens to work is a debugging lead, not a fix — replace it with
something named and measured, and confirm the replacement empirically
before shipping it, even when the algebra looks right.** Full
derivation and validation numbers in
[the design note](../design/timing_lock_detector.md).

## See also

- [Lock Detection: Verify Counts](../gallery/lockdet.md) — the shared
    `lockdet_core.h` mechanics: level + time hysteresis, and why
    consecutive-look compounding buys cheap false-alarm suppression.
- [Timing Lock Detector design note](../design/timing_lock_detector.md) —
    the full `SymbolSync` statistic derivation and empirical validation.
- [Many Emitters, One Consumer](../gallery/telemetry-fanin.md) — reading
    `.lock` / `.locked` traces off the shared telemetry bus.
- A full receiver chain with all three loops' `.locked` traces on one
    timeline: [Full-Chain Lock-Up](../gallery/receiver-lock.md).
