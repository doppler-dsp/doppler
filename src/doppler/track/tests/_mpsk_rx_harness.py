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

import math

import numpy as np

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

    Three factors, and skipping any of them produces a confident wrong number:

    1. **5/Bn per loop.** The standard second-order-loop rule of thumb.
    2. **The two budgets ADD, they do not max.** The loops are CASCADED: the
       carrier discriminator reads the on-time strobe, which is not a
       constellation point until the timing loop has converged, so the carrier
       cannot begin settling until timing has finished. `5/bn_timing` THEN
       `5/bn_carrier`.
    3. **Double it for JOINT tracking.** Both loops are closed at once, so each
       sees the other's transient as a disturbance rather than converging into
       a quiet channel. The sequential sum is a floor on the joint case, not an
       estimate of it.

    At the defaults: `2 * (500 + 500) = 2000` symbols. The measured lock times
    sit inside that envelope -- at `sps=10, m_out=4` timing declares at 1063
    and
    the carrier at 832 -- which is the check that the budget is not fantasy.
    Taking `max(5/bn)` instead gives 500 and reads -9.0 dB where the settled
    answer is -23.2 dB.

    Burst length follows from this: a test needs `settle_floor(...)` plus a few
    hundred symbols of window, so widening the loops is the way to test high
    oversampling without a multi-million-sample stimulus.
    """
    return int(2.0 * (5.0 / bn_timing + 5.0 / bn_carrier))


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
    names = tlm.probe_names()
    probes = {
        n.removeprefix("rx."): rec[rec["probe"] == pid]["value"].astype(float)
        for n, pid in names.items()
    }
    rx.set_telemetry(None, "rx")
    assert tlm.dropped == 0, "telemetry ring overran; the series are truncated"
    return y, probes


def lock_symbol(flag, sustain=200, min_frac=0.9):
    """Symbol index from which a verify-counted flag is SUSTAINED.

    "Sustained" is `sustain` consecutive symbols high, and at least `min_frac`
    of everything after that point high too. Both halves are load-bearing: the
    run rejects a single lucky decision, and the fraction rejects a detector
    that declares early and then spends the burst flapping.

    An earlier version dated the lock by the FINAL contiguous run of ones,
    which is right with no noise -- a dropout there is a real defect -- and
    badly wrong with it. A verify-counted detector legitimately dips under
    AWGN: at Es/N0 = 12 dB the carrier flag reads high 81-90% of the burst with
    the tail above 90%, and one late dip made this report 2286 instead of 415,
    which then left no measurement window and looked like a receiver that never
    locked.

    Returns `None` when no such point exists, which is the honest answer for
    "never locked" and forces the caller to handle it rather than measuring a
    transient.
    """
    ones = flag > 0.5
    if ones.size == 0 or not ones.any():
        return None
    # cumulative sum lets both conditions be checked in one pass
    csum = np.concatenate(([0], np.cumsum(ones)))
    n = ones.size
    for i in range(n):
        end = min(i + sustain, n)
        if csum[end] - csum[i] < end - i:  # a zero inside the run
            continue
        if n - i < sustain:  # not enough burst left to judge
            return None
        if (csum[n] - csum[i]) / (n - i) >= min_frac:
            return int(i)
    return None


def settle_from(probes, floor=SETTLE_SYMS):
    """Where the measurement window may start, from the receiver's own locks.

    The analytic budget (`settle_floor`) and the reported locks are both
    fallible in the same direction, so take whichever is later. The budget can
    be optimistic when a geometry converges slowly for reasons bandwidth does
    not capture -- at `sps = 10, m_out = 4` the timing detector does not
    declare
    until symbol 1063 -- and the locks can be optimistic because a detector
    declares on a statistic that crossed a threshold, not on a settled loop.
    `max(budget, timing lock, carrier lock)`: whatever settles last decides.

    **With `acq_to_track` on, the handover is what settles last.** It fires ON
    carrier lock plus a warmup, so it is strictly after everything else this
    function looks at, and the decision-directed loop then has its own
    transient -- the shared loop filter carries the frequency estimate across,
    so it is shorter than a cold start, but not zero. Measured on 8PSK at its
    SER = 1e-3 anchor, complex path: the handover fires at symbol 2525 against
    a 2000-symbol budget, and measuring from 2000 reads SER 5.9x the coherent
    bound where the settled answer is **1.7x** -- nearly every error in that
    window is pre-handover. So a handover adds `floor` again on top of its own
    instant.

    Returns `None` when either loop is not locked at the end of the burst;
    there is no valid steady-state window in that case and the caller must say
    so rather than quietly measuring the transient.
    """
    t = lock_symbol(probes["sync.locked"])
    c = lock_symbol(probes["car.locked"])
    if t is None or c is None:
        return None
    out = max(floor, t, c)
    # `tracking` stays 0 when acq_to_track is off, so this contributes nothing
    # for a pure-NDA receiver.
    h = lock_symbol(probes["tracking"]) if "tracking" in probes else None
    if h is not None:
        out = max(out, h + floor)
    return out


def coherent_errors(y, idx, m, settle, lag_span=40):
    """Coherent symbol errors over the settled window: `(errors, symbols)`.

    Returns COUNTS, not a rate, because the sensible way to measure a symbol
    error rate is to run until a fixed number of ERRORS and let the symbol
    count be the random variable (inverse binomial sampling) -- and a rate
    alone cannot be accumulated across bursts. See `ser_confidence()`.

    De-rotates by the estimated M-th-power phase first, which removes the
    arbitrary phase the loop settled at; the residual ambiguity is then exactly
    the `m` discrete rotations, searched here along with the lag. Skipping that
    de-rotation is not a small error: QPSK lands on its decision boundaries and
    reads ~0.5 regardless of how clean the constellation is.

    Returns `None` if the window is too short to judge.
    """
    z = y[settle:]
    if z.size < 500:
        return None
    z = z * np.exp(-1j * np.angle(np.mean(z.astype(np.complex128) ** m)) / m)
    step = 2.0 * np.pi / m
    dec = np.round(np.angle(z) / step).astype(int) % m
    best = None
    for lag in range(-lag_span, lag_span + 1):
        b = np.arange(settle, settle + z.size) + lag
        if b.min() < 0 or b.max() >= idx.size:
            continue
        for rot in range(m):
            e = int(np.count_nonzero(((dec - idx[b] - rot) % m) != 0))
            if best is None or e < best:
                best = e
    return (best, z.size) if best is not None else None


def ser_confidence(errors, symbols, z=1.96):
    """`(p_hat, lo, hi)` for a run stopped on an ERROR count.

    Under inverse binomial sampling -- fix the number of errors `r`, let the
    number of symbols `N` be what falls out -- `N` is negative-binomially
    distributed, not the errors. Two consequences the fixed-`N` habit misses:

    * the naive `r/N` is **biased**; the unbiased estimator is `(r-1)/(N-1)`;
    * the relative standard error is `1/sqrt(r)` and depends ONLY on the error
      count, which is exactly why stopping on errors gives a consistent
      measurement while stopping on symbols does not. At a target SER of 1e-3,
      a 20 000-symbol burst yields ~20 errors and ~22% relative error -- big
      enough to look like real seed-to-seed variation in the receiver, which is
      how it was first misread here.

    The interval is the large-`r` log-normal form `p_hat * exp(+-z/sqrt(r))`,
    accurate for `r >= 100`. For small `r` use the exact Poisson/Gamma relation
    instead: `p` in `[chi2_{a/2}(2r) / 2N, chi2_{1-a/2}(2r) / 2N]`.
    """
    if errors < 2 or symbols < 2:
        return float("nan"), 0.0, float("inf")
    p = (errors - 1) / (symbols - 1)
    rel = 1.0 / math.sqrt(errors)
    return p, p * math.exp(-z * rel), p * math.exp(z * rel)


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
