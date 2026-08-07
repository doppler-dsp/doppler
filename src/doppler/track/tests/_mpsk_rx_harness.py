"""Shared measurement harness for the two M-PSK receivers.

Both `MpskReceiver` (complex baseband) and `MpskReceiverR` (real IF) recover
symbols from the same signal model, so the thing worth testing is the pair
against each other on ONE stimulus, measured the same way. This module owns
that stimulus and the three measurements; the test modules own the thresholds.

Why the harness is shared rather than duplicated per test: every trap in these
measurements has already been paid for once, and a second copy re-earns them.
In particular

* **EVM is the quality metric, not BER.** BER/SER saturates at 0 long before
  the constellation is good, so it cannot distinguish a receiver on the bound
  from one 8 dB off it. The convention is `EVM_dB = -(Es/N0)_dB` (an I/Q-plane
  quantity against an I/Q-plane bound, no factor of two), so an EVM that BEATS
  the bound means the measurement is wrong, never that the receiver is good.
* **Nothing is measured before the loops settle.** A second-order loop needs
  ~5/Bn symbols, which at the default `bn_timing = 0.01` is 500 -- longer than
  many test bursts. Measuring inside that window measures settling.
* **The lag search must be generous.** Group delay varies with pulse, front end
  and rate; a window that clips it reports chance SER on a perfect decode.
  `symbol_metrics` searches +-200 and returns the winning lag so a saturated
  search is visible to the caller.
* **Lock time comes from the receiver's own verify-counted detectors**
  (`sync.locked` / `car.locked`), not from a threshold applied to a statistic.
  Note the granularity: the timing detector averages `avgs` looks per decision
  (133 by default), so its lock time is quantised to multiples of that and
  lands on 132, 265, ... -- assert against a bound, never an exact value.
"""

from __future__ import annotations

import numpy as np

from doppler.ber import (
    BerMeter,
    ber_evm_scatter_floor_db,
    ber_lock_symbol,
    ber_settle_from,
    ber_settle_syms,
)
from doppler.telemetry import Telemetry
from doppler.track import MpskReceiver, MpskReceiverR

#: Ddcr's design centre. The R2C halfband bakes in a +fs/4 shift, and its
#: image rejection is >100 dB across roughly 0.06..0.44 but collapses at the
#: band edges (-7 dB at 0.01), so a real-IF signal belongs near fs/4. This is
#: also the realistic case: 40 MSa/s with a 10 MHz IF is exactly fs/4.
IF_FS4 = 0.25

#: Both loop bandwidths, stated explicitly rather than left to the constructor
#: defaults, because the settling floor below is derived from them and a silent
#: default change must move the floor with it.
BN_TIMING = 0.01
BN_CARRIER = 0.01


def settle_floor(bn_timing=BN_TIMING, bn_carrier=BN_CARRIER):
    """Symbols to allow for settling before any metric is meaningful.

    `2 * (5/bn_timing + 5/bn_carrier)`. Three factors, and skipping any of
    them produces a confident wrong number: 5/Bn per loop is the standard
    second-order settling time (in SYMBOLS, since both `bn` are symbol-rate
    normalised); the two budgets ADD because the loops are CASCADED (the
    carrier discriminator reads the on-time strobe, so it cannot converge
    until timing has); and the sum DOUBLES for joint tracking, where each loop
    sees the other's transient as a disturbance.

    Delegates to `ber.ber_settle_syms` -- the C implementation is the only
    one. At the defaults this is 2000 symbols; taking `max(5/bn)` instead
    gives 500 and reads -9.0 dB EVM where the settled answer is -23.2 dB.
    """
    return ber_settle_syms(bn_timing, bn_carrier)


#: A configuration-only meter used purely as the entry point to the exact
#: confidence interval; it accumulates nothing itself.
_CI_METER = BerMeter(m=2, target_errors=1, conf=0.99)

#: The default-bandwidth budget, for tests that do not retune the loops.
SETTLE_SYMS = settle_floor()

#: Amplitude, comfortably inside the cascade's +-1.0 CIC input bound.
AMPL = 0.4


def make_signal(sps, nsym, *, real, m=4, fc=IF_FS4, esn0_db=None, seed=3):
    """One M-PSK stimulus, as either a real IF or complex baseband.

    Rectangular symbols at `sps` samples each on a carrier at `fc`
    cycles/sample. The real flavour is `Re{}` of the complex one, which is what
    a single-ended ADC hands you, so the two paths are compared on the SAME
    waveform rather than on two similar ones.

    `esn0_db` adds AWGN at that symbol-energy-to-noise-density ratio. The two
    conventions differ and both are needed:

    * complex baseband -- `Es = A^2 * sps`, and `N0` is the variance per
      complex sample, split evenly between I and Q.
    * real IF -- `Es = A^2 * sps / 2`, because half the power of a real
      passband signal sits at the negative frequency, and the real noise
      variance is `Es / (2 * Es/N0)`.

    Both were validated by measurement, not derivation: at `Es/N0 = 12 dB`
    both paths measure EVM within 0.5 dB of the -12 dB bound at every
    oversampling ratio tested. A wrong convention here shows up immediately as
    one path apparently beating the bound.

    Returns
    -------
    x : ndarray
        float32 (real) or complex64 (complex) samples.
    idx : ndarray
        The transmitted symbol indices, for `symbol_metrics`.
    """
    rng = np.random.default_rng(seed)
    idx = rng.integers(0, m, nsym)
    bb = np.repeat(np.exp(2j * np.pi * idx / m), int(sps))
    z = bb * np.exp(2j * np.pi * fc * np.arange(bb.size)) * AMPL

    if real:
        x = z.real.astype(np.float64)
        if esn0_db is not None:
            es = AMPL * AMPL * sps / 2.0
            var = es / (2.0 * 10 ** (esn0_db / 10.0))
            x = x + rng.standard_normal(x.size) * np.sqrt(var)
        return np.ascontiguousarray(x.astype(np.float32)), idx

    x = z.astype(np.complex128)
    if esn0_db is not None:
        es = AMPL * AMPL * sps
        var = es / 10 ** (esn0_db / 10.0)
        x = x + (
            rng.standard_normal(x.size) + 1j * rng.standard_normal(x.size)
        ) * np.sqrt(var / 2)
    return np.ascontiguousarray(x.astype(np.complex64)), idx


def evm_scatter_floor_db(m):
    """EVM (dB) of an M-PSK constellation at a UNIFORMLY RANDOM rotation.

    -1.4 dB at BPSK, -7.0 at QPSK, -12.9 at 8PSK. **Any fixed EVM threshold
    must be stated against this, never against 0 dB** -- "scattered reads
    ~0 dB" is the BPSK limit only, and at 8PSK a stream with no carrier
    recovery reads the same -12.9 dB a healthy 13 dB link does.

    Delegates to `ber.ber_evm_scatter_floor_db`. Not to be confused with the
    NOISE floor -(Es/N0).
    """
    return ber_evm_scatter_floor_db(m)


def freq_offset_inside_bw(bn, sps, frac=0.5):
    """A carrier offset guaranteed INSIDE the loop bandwidth, in cycles/sample.

    This is the only kind of offset a lock-time assertion may use. `bn_carrier`
    is normalised to the SYMBOL rate, so a loop bandwidth of `bn` is `bn * Rs`,
    which at `sps` samples per symbol is `bn / sps` cycles per sample. `frac`
    scales inside that: 0.5 sits at half the loop bandwidth.

    Testing OUTSIDE the loop bandwidth is a coin flip -- acquisition beyond
    `Bn`
    depends on where the transient happens to push the integrator, so a pass
    means the dice fell well and a failure means nothing was broken. Neither
    outcome is a test. Pull-in range beyond `Bn` is what `nda_tap` and a coarse
    frequency estimate are for; if it needs measuring, measure it as a
    characterisation sweep with a reported success fraction, never as a
    pass/fail assertion.
    """
    return frac * bn / sps


def clock_offset_inside_bw(bn, frac=0.5):
    """A fractional sample-clock error inside the timing loop's bandwidth.

    Applied by telling the receiver a nominal `sps` that differs from the
    stimulus by this fraction, which is exactly what a free-running ADC clock
    looks like. `bn_timing` is also symbol-rate normalised, so the offset is
    dimensionless in symbols per symbol and needs no `sps` scaling.
    """
    return frac * bn


def demod(
    x,
    *,
    real,
    sps,
    m_out,
    m=4,
    fc=IF_FS4,
    bn_timing=BN_TIMING,
    bn_carrier=BN_CARRIER,
    freq_offset=0.0,
    clock_offset=0.0,
    **kw,
):
    """Run the matching receiver over `x` with telemetry attached.

    Both bandwidths are passed explicitly so the caller's settling budget and
    the receiver's actual loops cannot drift apart -- pair every call with
    `settle_floor(bn_timing, bn_carrier)`.

    `freq_offset` (cycles/sample) is subtracted from the seeded
    `init_norm_freq`,
    so the carrier loop must acquire it. **Pass a non-zero value whenever the
    test says anything about the carrier loop**: seeded exactly on truth the
    loop
    has nothing to do, never leaves its initial state, and any conclusion drawn
    about its lock time or pull-in is void. Use `freq_offset_inside_bw`.

    `clock_offset` (dimensionless) scales the nominal `sps` the receiver is
    told, so the timing loop must absorb it. Use `clock_offset_inside_bw`.

    Returns `(symbols, probes)` where `probes` maps a probe suffix to its
    per-symbol series -- `sync.locked`, `car.locked`, `sync.mu`, and the rest
    of the eleven the receivers publish.
    """
    tlm = Telemetry(1 << 22)
    cls = MpskReceiverR if real else MpskReceiver
    rx = cls(
        m=m,
        sps=float(sps) * (1.0 + clock_offset),
        m_out=m_out,
        bn_timing=bn_timing,
        bn_carrier=bn_carrier,
        init_norm_freq=fc - freq_offset,
        **kw,
    )
    rx.set_telemetry(tlm, "rx")
    y = rx.steps(x)
    rec = tlm.read()
    names = tlm.probe_names
    probes = {
        n.removeprefix("rx."): rec[rec["probe"] == pid]["value"].astype(float)
        for n, pid in names.items()
    }
    rx.set_telemetry(None, "rx")
    assert tlm.dropped == 0, "telemetry ring overran; the series are truncated"
    return y, probes


def lock_symbol(flag, sustain=200, min_frac=0.9):
    """Symbol index from which a verify-counted flag is SUSTAINED.

    `sustain` consecutive symbols high, and at least `min_frac` of everything
    after that point high too. Both halves are load-bearing: the run rejects a
    single lucky decision, the fraction rejects a detector that declares early
    then flaps. Dating the lock by the FINAL contiguous run is right with no
    noise and badly wrong with it -- one late dip once moved a reported lock
    from 415 to 2286 and left no measurement window at all.

    Delegates to `ber.ber_lock_symbol`. Returns `None` for "never locked",
    which forces the caller to handle it rather than measure a transient.
    """
    idx = ber_lock_symbol(
        np.asarray(flag, dtype=np.float64) > 0.5, sustain, min_frac
    )
    return None if idx < 0 else int(idx)


def settle_from(probes, floor=SETTLE_SYMS):
    """Where the measurement window may start, from the receiver's own locks.

    `max(budget, timing lock, carrier lock, handover + budget)` -- the
    analytic budget and the reported locks are both fallible in the SAME
    direction, so whichever settles last decides. **With `acq_to_track` on the
    handover is what settles last**: it fires on carrier lock plus a warmup,
    strictly after everything else, and the decision-directed loop then has
    its own transient, so it contributes its instant PLUS the budget again.
    Measured on 8PSK at its SER=1e-3 anchor: handover at symbol 2525 against a
    2000-symbol budget, SER 5.9x the coherent bound from 2000 versus 1.7x from
    4525.

    Delegates the policy to `ber.ber_settle_from`. Returns `None` when either
    loop is not locked at the end of the burst -- there is no valid
    steady-state window then, and the caller must say so.
    """
    t = lock_symbol(probes["sync.locked"])
    c = lock_symbol(probes["car.locked"])
    if t is None or c is None:
        return None
    h = lock_symbol(probes["tracking"]) if "tracking" in probes else None
    return int(ber_settle_from(floor, t, c, -1 if h is None else h))


def coherent_errors(y, idx, m, settle, lag_span=40):
    """Coherent symbol errors over the settled window: `(errors, symbols)`.

    Returns COUNTS, not a rate, because the sensible way to measure an error
    rate is to run until a fixed number of ERRORS and let the symbol count be
    the random variable -- and a rate alone cannot be accumulated across
    bursts. See `ser_confidence()`.

    **The alignment is DETECTED, not searched.** This used to take the minimum
    error count over a lag and rotation search, which is an optimisation over
    the answer rather than a measurement: it can find a lucky alignment on
    garbage, and it can miss the true one on a healthy stream and report
    chance. `BerMeter.align()` correlates a known marker -- here a stretch of
    the truth sequence, taken from the FRONT of the window so the symbols that
    fix the alignment are disjoint from the ones scored -- and gates the peak
    on a false-alarm probability.

    Returns `None` if the window is too short to judge, or if no alignment
    could be detected in it.
    """
    if y.size - settle < 500:
        return None
    rx = np.ascontiguousarray(y, dtype=np.complex64)
    truth = np.ascontiguousarray(idx, dtype=np.uint8)
    # `rx` is NOT sliced: the meter's convention is rx[i] <-> truth[i + lag],
    # so slicing off `settle` from one side only would make the true lag
    # `settle` itself -- thousands of symbols outside any sane search.
    t0 = settle + lag_span
    n_marker = 256
    if t0 + n_marker >= truth.size:
        return None
    meter = BerMeter(m=m, target_errors=10**9)
    meter.set_truth(truth)
    if not meter.align(rx, t0=t0, n_marker=n_marker, lag_span=lag_span):
        return None
    # Score from just past the marker: the symbols that fixed the alignment
    # must not also be scored, or they flatter the rate.
    lo = t0 + n_marker - meter.lag
    meter.score(rx, lo=lo, hi=rx.size)
    return (int(meter.errors), int(meter.symbols)) if meter.symbols else None


def ser_confidence(errors, symbols, z=1.96):
    """`(p_hat, lo, hi)` for a run stopped on an ERROR count.

    Under inverse binomial sampling -- fix the errors `r`, let the trials `N`
    be what falls out -- `N` is negative-binomially distributed, not the
    errors. Two consequences the fixed-`N` habit misses: the naive `r/N` is
    **biased** (unbiased is `(r-1)/(N-1)`), and the relative standard error is
    `1/sqrt(r)`, depending ONLY on the error count. That is exactly why
    stopping on errors gives a consistent measurement and stopping on symbols
    does not: 20 000 symbols at SER 1e-3 yields ~20 errors and ~22% relative
    error, which reads as real seed-to-seed variation in the receiver.

    Delegates to `BerMeter.interval()`, which is the EXACT Gamma/chi-square
    interval at every error count including 1 -- no normal approximation, so
    it stays honest where a Wald interval is worst.

    `z` is accepted for backward compatibility and IGNORED: the C side is
    parameterised by a confidence level (0.99 here), and the exact interval is
    not symmetric, so a two-sided z-score has nothing to multiply.
    """
    del z
    if errors < 2 or symbols < 2:
        return float("nan"), 0.0, float("inf")
    r = _CI_METER.interval(errors, symbols)
    return r.p_hat, r.lo, r.hi


def symbol_metrics(y, idx, m=4, settle=SETTLE_SYMS):
    """EVM (dB) and DIFFERENTIAL SER over the SETTLED part of the burst.

    Both are M-fold-ambiguity-invariant: the constellation is de-rotated by the
    mean M-th-power phase before EVM, and the SER is computed on symbol
    DIFFERENCES so it needs only a lag, not an absolute rotation.

    **Do not compare this SER against a coherent bound.** A differential
    decision fails when either of its two symbols is wrong, so at high SNR one
    symbol error produces TWO differential errors and this reads ~2x a coherent
    SER -- measured 1.88 to 2.11 across orders and both paths. Comparing it to
    `theory_ser()` cost a whole session's worth of imagined implementation loss
    (2-4.75x the bound, "fixed" by loosening a tolerance to 10) where the
    coherent measurement is 1.2-2.4x, i.e. 0.3-1.0 dB. Use `coherent_errors()`
    when the reference is a coherent curve.

    Returns `(evm_db, ser, lag)`. `lag` is the winning offset; if it sits at
    either end of the +-200 search the search saturated and the SER is not
    trustworthy.
    """
    ys = y[settle:]
    if len(ys) < 200:
        return float("nan"), float("nan"), None
    step = 2.0 * np.pi / m
    yr = ys * np.exp(-1j * np.angle(np.mean(ys**m)) / m)
    yr = yr / np.sqrt(np.mean(np.abs(yr) ** 2))
    ideal = np.exp(1j * step * np.round(np.angle(yr) / step))
    evm = 10 * np.log10(np.mean(np.abs(yr - ideal) ** 2))

    dec = np.round(np.angle(yr) / step).astype(int) % m
    dd, dt = np.diff(dec) % m, np.diff(idx) % m
    ser, best = 1.0, None
    for lag in range(-200, 201):
        a0, b0 = max(0, lag), max(0, -lag) + settle
        k = min(len(dd) - a0, len(dt) - b0)
        if k >= 200:
            s = float(np.mean(dd[a0 : a0 + k] != dt[b0 : b0 + k]))
            if s < ser:
                ser, best = s, lag
    return evm, ser, best
