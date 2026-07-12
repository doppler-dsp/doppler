# SymbolSync Timing Lock Detector

`track.SymbolSync` (Gardner/DTTL symbol-timing recovery) had no lock concept
at all until this design landed — the last stage before bit decisions in a
tracking chain, with zero trust signal. The statistic and sizing formula
below were supplied by a doppler user from their own operational experience,
not derived from a paper this codebase carries; this page records the
formula as given, what it means, and the empirical validation that confirms
it actually hits its claimed operating point.

## The statistic

```text
lock_signal = 2 * (
    (symbol.real**2 - mid_symbol.real**2)
    + (symbol.imag**2 - mid_symbol.imag**2)
) / (
    (symbol.real**2 + mid_symbol.real**2)
    + (symbol.imag**2 + mid_symbol.imag**2)
)
```

A Gardner-style eye-opening ratio: `symbol` is the on-time interpolant (the
Farrow-evaluated sample at the recovered symbol strobe) and `mid_symbol` is
the mid-symbol (transition-gate) interpolant already formed by the Gardner
TED. At correct timing, a Nyquist (ISI-free) pulse's on-time sample depends
only on the current symbol, so it sits at the eye's peak while the
mid-symbol sample sits closer to a zero crossing — `lock_signal` is positive
and grows with pulse rolloff and Es/N0. Under noise or wrong timing it
hovers near zero.

An earlier design attempt in this codebase used the algebraically
equivalent ratio `q = symbol_pwr / (symbol_pwr + mid_pwr)` (`lock_signal = 4q - 2`) and appeared to *invert sign* between raised-cosine and rectangular
pulses under a first Monte Carlo pass — that turned out to be a test-harness
bug (a signal generator silently truncating the last ~30 symbols of every
"clean" trial, not a real property of the statistic). Once fixed, both
pulse shapes gave the same, correctly-signed result.

## Sizing: (pfa, pd) → (avgs, threshold)

```text
mean_lock_detect = (0.6 * rolloff + 0.26) * (
    1 - exp(-0.275 * 10 ** (esno_min / 10))
)

avgs = 2 * var * ((erfcinv(2 * pfa) - erfcinv(2 * pd)) / mean_lock_detect) ** 2
threshold = erfcinv(2 * pfa) * mean_lock_detect / (
    erfcinv(2 * pfa) - erfcinv(2 * pd)
)
```

`lock_signal` is non-coherently block-averaged over `avgs` looks before each
decision (a tumbling window, mirroring `Dll`'s CFAR pattern — a sliding
window would break the verify-count independence assumption the same way it
would for the DLL). `mean_lock_detect` estimates the per-look mean from the
matched-filter rolloff and the minimum operating Es/N0; the classic Gaussian
test-statistic sizing (`N = variance * ((Q⁻¹(pfa) - Q⁻¹(pd)) / mean)²`,
`threshold = Q⁻¹(pfa) * mean / (Q⁻¹(pfa) - Q⁻¹(pd))`) gives `avgs` and the
declare threshold. `erfcinv` is used directly (not the `√2·erfcinv(2p) = Q⁻¹(p)` conversion a Gaussian Q-function derivation would normally use) —
implemented literally as given, with one correction: the source formula's
`avgs` used a bare `8` in `var`'s place, an uncalibrated placeholder rather
than a derived constant. `var` is now `SYMSYNC_LOCK_STAT_VARIANCE`, the real
per-look variance of `lock_signal` under noise-only input, measured directly
(5,000,000-sample Monte Carlo: mean ≈0, variance ≈1.343) rather than assumed.
The leading `2` is *not* part of that variance — because `erfcinv` is used
directly instead of `Q⁻¹`, the √2 factors cancel in `threshold` (identical
either way) but not in `avgs`, which needs an explicit factor of 2 to match
the classic `N = variance * ((Q⁻¹(pfa) - Q⁻¹(pd)) / mean)²` derivation once
rewritten in terms of the `erfcinv`-based denominator. Both hypotheses were
tried empirically before picking one (see the validation section below): the
measured variance alone, without the factor of 2, undersizes `avgs` and
blows past the pfa target by ~13×. No down-threshold or verify-count
derivation is implied by the source formula, so those default to the same
shape `Dll.configure_lock` uses (see
[`native/src/symsync/symsync_core.c`](https://github.com/doppler-dsp/doppler/blob/main/native/src/symsync/symsync_core.c)'s
`SYMSYNC_LOCK_STAT_VARIANCE` comment for the exact defaults and full
derivation).

No standard C library function computes `erfcinv`; `symsync_core.c` carries
a private Winitzki-initial-guess-plus-Newton-refinement implementation
(verified to machine precision against `erfc`), kept local to that file
pending a second consumer.

## Empirical validation — does it actually hit (pfa, pd)?

Since the formula wasn't derived here, it was validated by direct Monte
Carlo against the real object rather than trusted on faith
(`native/validation/symsync_lock.c`, gated in CI via `ctest -R validate_symsync_lock`):

| Check                 | Method                                                                           | Nominal target             | Measured                         |
| --------------------- | -------------------------------------------------------------------------------- | -------------------------- | -------------------------------- |
| False-alarm rate      | 500,000 independent noise-only `avgs`-symbol blocks                              | pfa = 1e-3 (~500 expected) | **429** false declares (8.58e-4) |
| Detection probability | 2,000 independent raised-cosine BPSK blocks at exactly the `esno_min` design SNR | pd = 0.9                   | **1.0000** true declares         |

At the default operating point (rolloff=0.35, esno_min=10dB, pfa=1e-3,
pd=0.9) this gives `avgs=133`, `threshold=0.311` (`threshold` is
`var`-independent, so unchanged from the original placeholder). Both targets
land correctly sized rather than accidentally oversized: `avgs` shrank from
395 (under the original bare `8`) to 133 — about 3×, not the ~6× a naive
"replace 8 with the measured 1.343" substitution would give. That naive
substitution was tried first and rejected: it undersizes `avgs` (drops the
implicit factor of 2 from the `erfcinv`-vs-`Q⁻¹` convention) and blows past
the pfa target by ~13× (empirical 1.31e-2 against a 1e-3 nominal) —
confirming the factor-of-2 correction above is required, not optional.

## Usage

```python
from doppler.track import SymbolSync

ss = SymbolSync(sps=4, bn=0.01, zeta=0.707)
ss.configure_lock(rolloff=0.35, esno_min_db=10.0, pfa=1e-3, pd=0.9)
assert ss.locked is False  # no signal yet

# ... ss.steps(x) on a real oversampled baseband block ...
# ss.locked / ss.lock_stat report the verify-counted decision and the
# last block-averaged lock_signal.

# The raw escape hatch for direct control of avgs/thresholds/verify counts:
ss.configure_lock_raw(
    avgs=200, up_thresh=0.3, down_thresh=0.25, n_up=1, n_down=8
)
```

See also: [Lock Detection: Verify Counts](../gallery/lockdet.md) for the
shared `lockdet_core.h` primitive every continuous tracking loop in this
codebase uses, and [`Dll`](../gallery/dll.md)'s CFAR lock detector for the
closed-form-derived counterpart this design mirrors in shape.
