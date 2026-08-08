# Python Error-Rate Measurement API

The `doppler.ber` module is the **instrument** for an error rate. An error rate
is a measurement, and like any measurement it can be confidently wrong — this
module owns the three things that make it defensible, so no caller has to
re-derive them:

1. **Is the window SETTLED?** — `ber_settle_syms` and
    `ber_settle_from`.
1. **Have enough ERRORS been counted?** — `BerMeter` stops on
    an error target, not a symbol count.
1. **Does the answer make sense?** — `ber_evm_db`,
    `ber_evm_scatter_floor_db` and
    `ber_theory_ser` cross-check it against measurements
    that cannot fail the same way.

See the worked example in the gallery:
[Measuring an Error Rate, Defensibly](../gallery/ber-awgn.md).

## Stop on errors, not on symbols

Under **inverse binomial sampling** — fix the number of errors `r`, let the
symbol count `N` be the random variable — the relative standard error is
`1/sqrt(r)`, a function of the error count **alone**. The error target *is* the
precision, chosen up front and independent of the rate about to be measured.

Stop on a fixed symbol count instead and precision depends on the very quantity
under test, degrading silently as the rate falls: 20 000 symbols at a SER of
1e-3 yields ~20 errors and ~22% relative error, which reads as real
seed-to-seed variation in a receiver and is not.

```pycon
>>> from doppler.ber import BerMeter, ber_theory_ser, ber_esn0_db_for_ser
>>> round(ber_esn0_db_for_ser(4, 1e-3), 2)      # QPSK's SER=1e-3 anchor
10.35
>>> round(ber_theory_ser(2, 10 ** 0.96), 7)     # BPSK at 9.6 dB
9.7e-06

```

## The alignment is detected, never searched

`BerMeter.align()` correlates a **known marker** — a sync word, one period of
a spreading code, or (in a simulation, where truth exists) a stretch of the
truth sequence — and gates the correlation peak on a false-alarm probability.

It deliberately does **not** search for the lag and rotation that minimise the
error count. That search is an optimisation over the answer rather than a
measurement of the receiver, and it fails both ways: a wide search on a short
window finds a lucky low-error alignment on garbage, and a narrow one misses the
true alignment on a healthy stream and reports chance. `align()` returns `False`
when the marker cannot identify an alignment, and the marker's own symbols are
excluded from scoring so they cannot also flatter the rate.

## The interval is exact — assert on `lo`

`ser()`, `ber()` and `interval()` return a `BerInterval` record with fields
`p_hat`, `lo`, `hi`, `rel`, `conf`, `errors`, `symbols`. The limits are the
**exact** Gamma/chi-square interval for inverse binomial sampling, not a normal
approximation, so they stay honest down to a single error; the quantiles come
from [`detection.det_threshold_noncoherent`](python-detection.md), doppler's own
inverse regularized incomplete gamma, rather than a second numeric kernel.

Two things the fixed-`N` habit gets wrong and this does not: the naive `r/N` is
**biased** (the unbiased estimator is `(r-1)/(N-1)`), and the interval is
asymmetric. **Compare the interval's lower limit `lo` against a spec, never
`p_hat`** — that is the form counting noise cannot flake.

## The EVM floor is M-dependent

`ber_evm_scatter_floor_db` gives what a
*completely destroyed* constellation reads: `2 - 2 sin(pi/M)/(pi/M)`, i.e.
**−1.4 dB at BPSK, −7.0 at QPSK, −12.9 at 8PSK**. The familiar "a scattered
constellation reads about 0 dB" is the BPSK limit only — at 8PSK a stream with
no carrier recovery at all reads the same −12.9 dB a perfectly healthy 13 dB
link does. **State any fixed EVM threshold against this floor, never against
zero.** Not to be confused with the *noise* floor `-(Es/N0)`.

```pycon
>>> from doppler.ber import ber_evm_scatter_floor_db
>>> [round(ber_evm_scatter_floor_db(m), 2) for m in (2, 4, 8)]
[-1.39, -7.0, -12.92]

```

______________________________________________________________________

::: doppler.ber.BerMeter

::: doppler.ber.ber_theory_ser

::: doppler.ber.ber_theory_ber

::: doppler.ber.ber_esn0_db_for_ser

::: doppler.ber.ber_evm_db

::: doppler.ber.ber_evm_scatter_floor_db

::: doppler.ber.ber_settle_syms

::: doppler.ber.ber_settle_from

::: doppler.ber.ber_lock_symbol

## Related pages

<!-- related-pages:start -->

**Gallery** — [Measuring an Error Rate, Defensibly](../gallery/ber-awgn.md), [Gallery](../gallery/index.md)
**Design** — [RateSync Timing Recovery](../design/ratesync-timing.md)

<!-- related-pages:end -->
