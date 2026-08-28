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
  quantity against an I/Q-plane bound, no factor of two), so an EVM that beats
  the bound usually means the measurement is wrong rather than the receiver
  good — **but only usually, and only above ~12 dB.** `ber_evm_db` is
  SELF-referenced: it scores each symbol against the stream's own hard
  decision, so a misdecided symbol is measured against a nearer constellation
  point than the one sent and contributes too small an error vector. The
  metric therefore flatters by an amount set by the SER — measured on QPSK,
  2.45 dB at Es/N0 = 3 dB, 1.06 at 6, 0.44 at 9, 0.20 at 12, 0.11 at 15,
  0.04 at 21 (`native/tests/test_ber_core.c` pins both halves). Every EVM
  assertion here runs at 12 dB or above, where the flattery is inside the
  margin; a test at a lower operating point must widen its lower bound or it
  will read the estimator's own bias as a receiver beating physics.
* **Nothing is measured before the loops settle.** A second-order loop needs
  ~5/Bn symbols, which at the default `bn_timing = 0.01` is 500 -- longer than
  many test bursts. Measuring inside that window measures settling.
* **The alignment is DETECTED, never searched.** Group delay varies with
  pulse, front end and rate, so it is not knowable in advance -- but a
  minimum-over-lag search answers that by optimising over the answer, which
  false-passes on a lucky alignment over garbage and false-floors when the true
  lag falls outside its span. `detect_alignment` correlates a known marker and
  gates the peak on a false-alarm probability, and every metric here that needs
  to know where the stream sits goes through it.
* **Lock time comes from the receiver's own verify-counted detectors**
  (`sync.locked` / `car.locked`), not from a threshold applied to a statistic.
  Note the granularity: the timing detector averages `avgs` looks per decision
  (133 by default), so its lock time is quantised to multiples of that and
  lands on 132, 265, ... -- assert against a bound, never an exact value.
"""

from __future__ import annotations

from typing import NamedTuple

import numpy as np

from doppler.ber import (
    BerMeter,
    ber_evm_db,
    ber_evm_scatter_floor_db,
    ber_lock_symbol,
    ber_settle_from,
    ber_settle_syms,
)
from doppler.snr import snr_m2m4_db
from doppler.telemetry import Telemetry
from doppler.track import MpskReceiver, MpskReceiverR
from doppler.wfm import Synth

#: Ddcr's design centre, and the placement the real path exists to serve. An
#: R2C halfband is the cheapest real-to-complex converter there is, it bakes in
#: the fs/4 shift for free (the rotation is 1, j, -1, -j: sign flips and rail
#: swaps) and it decimates by two in the same pass -- all three are the same
#: fact, and all three are a statement about fs/4. It is also the realistic
#: case: 40 MSa/s with a 10 MHz IF is exactly fs/4.
#:
#: Off-centre is a TOLERANCE, not a band. The halfband's image rejection does
#: collapse at the edges (-6.5 dB at 0.01, -13.7 at 0.02, past -60 dB across
#: the middle, symmetric about fs/4), but what that costs a signal is set by
#: whether its OCCUPIED band overruns DC or Nyquist, not by where its centre
#: sits: `1/sps < fc < 0.5 - 1/sps`. See docs/design/mpsk.md section 1.3.
IF_FS4 = 0.25

#: Both loop bandwidths, stated explicitly rather than left to the constructor
#: defaults, because the settling floor below is derived from them and a silent
#: default change must move the floor with it.
BN_TIMING = 0.01
BN_CARRIER = 0.01

#: The constellation order both `make_signal` and `demod` fall back to, named
#: once so a caller can seed a carrier offset without restating it.
#:
#: `freq_offset_inside_bw` needs `m` -- the carrier discriminator is an M-th
#: power, so the acquisition bound is `bn_carrier / m` -- and a call site that
#: wrote its own `4` beside a `demod()` taking the default would go silently
#: wrong the day either moved. Two literals for one fact is exactly the
#: expressible-but-unstated shape the offset seeding rule exists to remove.
DEFAULT_M = 4


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

#: What to add to a requested Es/N0 before handing it to the COMPLEX generator
#: when the stimulus is going to be projected onto its real part.
#:
#: `10*log10(2)`, and it is a change of convention, not of noise. Taking `Re{}`
#: halves the signal energy (half the power of a real passband signal sits at
#: the negative frequency, so `Es = A^2*sps/2`) and halves the noise variance
#: with it (`Re{}` of circular complex AWGN of variance `N` is real Gaussian of
#: variance `N/2`). Es/N0 would therefore come out unchanged -- but the real
#: path's convention counts the real noise against the HALVED Es, i.e.
#: `var = Es/(2*Es/N0)`, which is 3 dB less noise than a literal projection.
#: Asking the complex generator for 3 dB more Es/N0 delivers exactly that
#: variance, so the real stimulus is bit-for-bit the same waveform, the same
#: noise realisation scaled by `1/sqrt(2)`, and the same numbers the
#: hand-rolled generator produced (measured agreement: within 0.06 dB of the
#: old path's noise variance at every `(m, sps, Es/N0)` tried).
REAL_ESNO_OFFSET_DB = 10.0 * np.log10(2.0)


def agc(x):
    """Normalise to unit AVERAGE POWER, which is what an AGC does.

    An AGC is a receiver component and it belongs up the chain, not inside a
    timing detector — the TED normalises by its own slope and nothing else
    (docs/design/mpsk.md 5.1), so the level it sees is whatever arrives. This
    is where that level is set, once, for every stimulus in this harness.

    **Average power, not peak and not symbol amplitude.** On the noiseless
    rectangular stream those coincide — unit average power IS unit symbol
    amplitude — but under noise they do not, and average power is the one an
    AGC can actually measure. The consequence is physical rather than a
    defect: at a fixed Es/N0 the per-sample noise grows with oversampling, so
    the signal's share of a unit-power composite shrinks and the loop gain
    drops with it. That is what a real receiver experiences at low
    per-sample SNR.

    **Peaks are allowed to clip.** High PAPR is inevitable on some signals,
    and a converter with a bounded input will occasionally saturate on it;
    designing the level around the worst peak instead of the average would
    throw away the range the signal actually uses. The CIC budgets headroom
    for the typical case (docs/design/cic.md) and takes the rare peak.
    """
    p = float(np.mean(np.abs(x) ** 2))
    return x if p <= 0.0 else x / np.sqrt(p)


def make_signal(
    sps, nsym, *, real, m=DEFAULT_M, fc=IF_FS4, esn0_db=None, seed=3
):
    """One M-PSK stimulus, as either a real IF or complex baseband.

    **The waveform comes from the library generator, not from here.**
    `wfm.Synth(type="symbols")` holds each constellation point for `sps`
    samples, mixes it with the `lo` carrier at `fc` cycles/sample and adds
    `awgn` at the requested Es/N0 -- the same three components the receiver
    under test is built against, and the same ones `wfmgen` ships to users. All
    this function still owns is the truth sequence, the M-PSK mapping that
    defines it, and the level convention below. A private numpy oversample +
    carrier + noise variance was what stood here; it is the one measurement in
    this harness that was a genuine duplicate rather than a delegation, and a
    stimulus nobody else runs is a stimulus nobody else checks.

    The symbol indices stay local because they are TRUTH, not signal: `idx` is
    what `symbol_metrics` and `coherent_errors` score against, and drawing it
    from an RNG the harness controls keeps "what was sent" independent of the
    generator that sends it. The constellation is `exp(2j*pi*k/m)` -- the
    definition of M-PSK rather than a mapping choice, and deliberately NOT
    `wfm.qpsk_map`, whose Gray labelling would put a different symbol under
    index `k` and silently redefine the truth these metrics compare against.

    **Unit-modulus symbols are load-bearing.** `Synth` references its SNR to a
    signal of unit power (`wfm_awgn_amplitude(snr, 1.0)` in effect), so scaling
    the constellation would scale the delivered Es/N0 with it and nothing would
    complain. Level is set once, downstream, by `agc()`.

    `esn0_db` is the symbol-energy-to-noise-density ratio, `snr_mode="esno"` on
    the generator: `Es = A^2*sps` spread over `sps` samples, `N0` the variance
    per complex sample. That claim is now checked rather than assumed --
    `test_wfm_synth.py` reads it back with `snr.snr_data_aided_db` at the
    matched-filter output and finds it within 0.04 dB across `sps` 1..16 and
    Es/N0 0..20 dB. The real flavour is `Re{}` of that complex waveform, which
    is what a single-ended ADC hands you, generated 3 dB hotter for the reason
    in `REAL_ESNO_OFFSET_DB` -- so the two paths are compared on the SAME
    waveform and the same noise realisation, not on two similar ones.

    Returns
    -------
    x : ndarray
        float32 (real) or complex64 (complex) samples.
    idx : ndarray
        The transmitted symbol indices, for `symbol_metrics`.
    """
    rng = np.random.default_rng(seed)
    idx = rng.integers(0, m, nsym)
    snr_db = 100.0  # >= WFM_SYNTH_SNR_CLEAN: no AWGN generated at all
    if esn0_db is not None:
        snr_db = esn0_db + (REAL_ESNO_OFFSET_DB if real else 0.0)
    src = Synth(
        type="symbols",
        symbols=np.exp(2j * np.pi * idx / m).astype(np.complex64),
        sps=int(sps),
        fs=1.0,  # so `freq` is read as cycles/sample
        freq=fc,
        snr=snr_db,
        snr_mode="esno",
        seed=seed,
    )
    z = src.steps(int(sps) * nsym)

    if real:
        x = agc(z.real.astype(np.float64))
        return np.ascontiguousarray(x.astype(np.float32)), idx
    x = agc(z.astype(np.complex128))
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


def freq_offset_inside_bw(bn_carrier, m, frac=0.5):
    """A carrier offset inside the loop's acquisition bound, cycles/SYMBOL.

    The only kind of offset a lock-time assertion may seed. Returned in the
    same units the loop bandwidth is stated in, because that is what makes the
    two comparable at a glance: `bn_carrier` is normalised to the symbol rate,
    so the bound is `bn_carrier / m` cycles per symbol and this returns a
    fraction of it. **There is no `sps` here.** Converting to cycles per sample
    happens once, at the constructor that wants it -- `demod` does it, and
    doing it anywhere else is the sps-sized error `dp_rx_mpsk.h` warns about.

    **The `m` is the part that was missing, and it hid a factor of m.** The
    NDA discriminator is an M-th power, so it sees `m` times the offset; the
    bound is `bn_carrier / m`, not `bn_carrier` (docs/design/mpsk.md §12
    section 4.4). A helper without it returns the same number at every order
    while asking a 4x harder question at 8PSK than at BPSK.

    `frac` is the fraction of the bound: 1.0 seeds exactly at
    `bn_carrier / m`, and the default 0.5 at half of it. **Tests seed at or
    under the bound**, and both ends of that matter -- seeded on truth the
    loop never leaves its initial state and measures nothing, seeded past the
    bound it measures the dice.

    **The envelope is measured, not quoted here.**
    `doppler.track.tests.characterization.pull_in` sweeps it — success
    fraction against multiples of this bound, across every order and two
    oversampling ratios — and `make characterize` re-derives it. As it stands
    the carrier loop is reliable out to 4x the bound (3x at 8PSK) and dead by
    5x, so seeding AT the bound keeps a 3-4x margin. Those figures live in
    that sweep's output rather than in this docstring because a number
    nothing re-runs is prose, and two findings were once filed against the
    receiver on the strength of one (doppler#843, doppler#849).

    Acquisition beyond the bound depends on where the transient happens to
    push the integrator, so a pass means the dice fell well and a failure
    means nothing was broken -- neither is a test. Pull-in range beyond it is
    what a wider loop and a coarse frequency estimate are for; if it needs
    measuring, measure it as a characterisation sweep with a reported success
    fraction, never as a pass/fail assertion.
    """
    return frac * bn_carrier / m


def clock_offset_inside_bw(bn_timing, frac=0.5):
    """A fractional sample-clock error inside the timing loop's bandwidth.

    Applied by telling the receiver a nominal `sps` that differs from the
    stimulus by this fraction, which is what a free-running ADC clock looks
    like. `bn_timing` is symbol-rate normalised, so the error is already
    dimensionless in symbols per symbol -- no `sps`, and no `m` either.

    **The absent `m` is measured rather than assumed**: the timing
    discriminator is not an M-th power, and the same sweep that establishes
    the carrier envelope establishes this one
    (`doppler.track.tests.characterization.pull_in`). It is the tighter of
    the two — reliable to about 1.6-1.8x its bound and dead by 2.5x, against
    the carrier's 3-4x — which is why the two are stated separately rather
    than sharing one fraction.
    """
    return frac * bn_timing


def demod(
    x,
    *,
    real,
    sps,
    m_out,
    m=DEFAULT_M,
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

    `freq_offset` (cycles per SYMBOL) is subtracted from the seeded
    `init_norm_freq`,
    so the carrier loop must acquire it. **Pass a non-zero value whenever the
    test says anything about the carrier loop**: seeded exactly on truth the
    loop
    has nothing to do, never leaves its initial state, and any conclusion drawn
    about its lock time or pull-in is void. Use `freq_offset_inside_bw`.

    `clock_offset` (dimensionless) scales the nominal `sps` the receiver is
    told, so the timing loop must absorb it. Use `clock_offset_inside_bw`.

    **No output-rate check lives here, and that is a finding rather than an
    omission.** A receiver emits one symbol per symbol period, so the count is
    `len(x)/sps` -- not `m_out` times it (the terminal cascade rate), and not
    half of it (a real front end's internal decimation counted twice). Getting
    that wrong does not raise anywhere by itself: the symbols are real symbols,
    they are simply not the sequence the truth array describes. The instinct is
    to add a count invariant here; measured, `BerMeter.align()` already refuses
    every one of those cases downstream, because a stream at the wrong rate
    cannot correlate against the truth -- half rate reads -2.5 dB of margin,
    double rate reads -inf, and `m_out` outputs mistaken for symbols reads
    -5.6 dB, against +10.5 dB for the healthy run. A second gate with its own
    tolerance would be a second convention for a question `ber` already
    answers. See `detect_alignment`.

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
        # THE conversion, and the only one: `freq_offset` and both loop
        # bandwidths are symbol-rate normalised, while `init_norm_freq` is
        # cycles per SAMPLE. Dividing here means no caller has to hold `sps`
        # to state a frequency -- doing it at the call site instead is the
        # sps-sized error `dp_rx_mpsk.h` records.
        init_norm_freq=fc - freq_offset / float(sps),
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

    `max(budget, timing lock, carrier lock)` -- the analytic budget and the
    reported locks are both fallible in the SAME direction, so whichever
    settles last decides.

    There was a fourth term until doppler#877. A receiver that handed the
    carrier from an NDA discriminator to a decision-directed one settled last
    of all, contributing its instant PLUS the budget again (measured on 8PSK
    at its SER=1e-3 anchor: handover at symbol 2525 against a 2000-symbol
    budget, and 5.9x the coherent bound if the window started at 2000 rather
    than 4525). No receiver here hands over any more, and the `tracking`
    probe it was read from no longer exists.

    Delegates the policy to `ber.ber_settle_from`. Returns `None` when either
    loop is not locked at the end of the burst -- there is no valid
    steady-state window then, and the caller must say so.
    """
    t = lock_symbol(probes["sync.locked"])
    c = lock_symbol(probes["car.locked"])
    if t is None or c is None:
        return None
    return int(ber_settle_from(floor, t, c))


def rate_settled(probes, tol=1e-4, hold=500):
    """First symbol after which `sync.rate` stops moving, or None.

    `settle_from()` is `max(budget, timing lock, carrier lock)`, and every
    term in it is a PHASE settling statement. None of them covers the time to
    slew out a CLOCK (rate) offset, which is a different and much slower
    thing -- measured on BPSK at sps 32.95, with the seeded clock error at
    half the timing bandwidth (`clock_offset_inside_bw`, 2.655e-3 here):

        bn_timing = 0.0064   5/Bn = 781 symbols
        rate error at 781    2.64e-3   (the initial error was 2.655e-3)
        rate error < 1e-4    at 9900   -- 12.7x the budget

    With no clock offset the same loop is settled on time, so this is
    specific to slewing a rate error rather than the loop being sluggish.

    Why it matters to a TRUTH-REFERENCED measurement, which is the only kind
    that sees it: while the rate is wrong the receiver emits symbols at
    slightly the wrong cadence, so the rx-to-truth lag MOVES. `BerMeter`
    detects one lag and scores with it verbatim, so a window opened before
    the rate settles is scored against the right truth at its start and the
    wrong truth after -- measured, lag 18 becoming lag 19, and an SER of
    0.483 on a receiver whose real SER over the settled window is 0. It reads
    exactly like a false lock and is not one (doppler#1060).

    `tol` is fractional rate error and `hold` how long it must stay inside
    it, because the estimate is noisy and one excursion is not a relapse.
    Returns None when it never settles within the record.
    """
    r = probes.get("sync.rate")
    if r is None or r.size < hold:
        return None
    final = float(np.median(r[-hold:]))
    if final == 0.0:
        return None
    err = np.abs(r - final) / abs(final)
    for i in range(0, err.size - hold, hold // 5 or 1):
        if err[i:].max() < tol:
            return int(i)
    return None


def detect_alignment(y, idx, m, settle, lag_span=40, n_marker=256):
    """The one alignment recipe: a `BerMeter` with a DETECTED lag, or `None`.

    Every number in this harness that needs to know which transmitted symbol a
    received one corresponds to comes through here, so there is exactly one
    answer to "where does the stream sit" per measurement rather than one per
    metric. `None` means no alignment was detected and nothing may be scored --
    the second refusal point.

    **Detected, not searched.** `BerMeter.align()` correlates a known marker --
    a stretch of the truth sequence, taken from the FRONT of the settled window
    so the symbols that fix the alignment can be kept out of what is scored --
    and gates the peak on a false-alarm probability, returning `align_ok`:
    detected, unambiguous (>= 3 dB over the runner-up) and unsaturated. A
    minimum-over-lag search is an optimisation over the answer instead: it
    false-passes on a lucky alignment over garbage, and false-floors when the
    true lag falls outside its span, and in both cases it returns a number.

    `rx` is NOT sliced. The meter's convention is `rx[i] <-> truth[i + lag]`,
    so slicing `settle` off one side only would make the true lag `settle`
    itself -- thousands of symbols outside any sane search.

    **It refuses a wrong-rate stream too**, which is why no separate
    output-rate invariant exists here (see `demod`): a stream carrying half,
    double or `m_out` times the symbols it should cannot correlate against the
    truth, and reports -2.5, -inf and -5.6 dB of margin respectively where a
    healthy run reports +10.5 dB.
    """
    t0 = settle + lag_span
    if t0 + n_marker >= idx.size:
        return None
    rx = np.ascontiguousarray(y, dtype=np.complex64)
    truth = np.ascontiguousarray(idx, dtype=np.uint8)
    meter = BerMeter(m=m, target_errors=10**9)
    meter.set_truth(truth)
    if not meter.align(rx, t0=t0, n_marker=n_marker, lag_span=lag_span):
        return None
    return meter


def coherent_errors(y, idx, m, settle, lag_span=40):
    """Coherent symbol errors over the settled window: `(errors, symbols)`.

    Returns COUNTS, not a rate, because the sensible way to measure an error
    rate is to run until a fixed number of ERRORS and let the symbol count be
    the random variable -- and a rate alone cannot be accumulated across
    bursts. See `ser_confidence()`.

    The alignment comes from `detect_alignment` -- detected, never searched --
    and this returns `None` rather than a number when it is unavailable.

    Returns `None` if the window is too short to judge, or if no alignment
    could be detected in it.
    """
    if y.size - settle < 500:
        return None
    meter = detect_alignment(y, idx, m, settle, lag_span=lag_span)
    if meter is None:
        return None
    # The marker symbols are held out by `score()` itself -- they are known, so
    # scoring them would flatter the rate with symbols that had no chance of
    # being wrong, and the count lands in `meter.skipped`. This used to compute
    # a `lo` past the marker by hand, which is the same guarantee written a
    # second time in a second convention: it assumed one occurrence at a lag
    # sign the meter defines, and it silently dropped every symbol between the
    # window start and the marker's end instead of just the marker's own.
    rx = np.ascontiguousarray(y, dtype=np.complex64)
    meter.score(rx, lo=settle, hi=rx.size)
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


class SymbolMetrics(NamedTuple):
    """What one settled burst measures, reported as a set rather than singly.

    Goal 4 of docs/design/rx-test.md: BER, EVM and M2M4 fail DIFFERENTLY, so
    the disagreement between them is the diagnostic and reporting one alone is
    what makes a false lock invisible. Named fields rather than a tuple so a
    caller has to say which number it means -- and so adding the fourth (FER,
    once the frame layer lands) does not silently renumber anything.
    """

    evm_db: float
    ser: float
    lag: int
    m2m4_db: float


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

    **The lag is DETECTED, and this is the second refusal point.** It used to
    be the argmin of a +-200 lag search, with the winning lag returned so the
    caller could notice a saturated search for itself -- two of seven call
    sites did, by hand, with a literal `abs(lag) < 190`. That is the refusal
    goal 1 asks for, implemented five times in five places and therefore not
    implemented. `detect_alignment()` now supplies the lag, gated on
    `align_ok`, and this RAISES rather than returning a number when no
    alignment is available. The detected lag agrees with the old search's
    (opposite sign, `rx[i] <-> truth[i + lag]`) in every configuration these
    tests run, so the numbers are unchanged where the search was right; where
    it was wrong, there is now no number.

    **All three numbers come back together**, because they fail differently
    and the disagreement is the diagnostic. Under a stable false lock at
    `df = k*Rs/M` the constellation is stationary, so the two truth-free
    metrics read almost exactly what a healthy link reads -- measured at
    sps=16, QPSK, Es/N0 15 dB: EVM -12.52 dB and M2M4 12.93 dB against -13.57
    and 13.77 for the real thing, while the receiver reports LOCKED. Only the
    truth-referenced measurement sees it, and here it refuses outright. Report
    any one of these alone and that failure is invisible.

    `evm_db` and `m2m4_db` are the library's own estimators over the SAME
    window the SER is scored on -- `ber.ber_evm_db` and `snr.snr_m2m4_db`,
    both pinned by known answer and sabotage in `native/tests/`. The EVM used
    to be recomputed here in numpy; it agreed with `ber_evm_db` to four
    decimals, which is what a duplicate looks like right up until one of them
    changes.

    Returns a `SymbolMetrics`; `lag` is in the meter's convention.
    """
    ys = y[settle:]
    if len(ys) < 200:
        raise AssertionError(
            f"only {len(ys)} symbols after settling at {settle} of "
            f"{len(y)}: too short to measure, so nothing is reported"
        )
    rx = np.ascontiguousarray(y, dtype=np.complex64)
    evm = float(ber_evm_db(rx, settle, len(y), m))
    m2m4 = float(snr_m2m4_db(np.ascontiguousarray(ys, dtype=np.complex64)))

    step = 2.0 * np.pi / m
    yr = ys * np.exp(-1j * np.angle(np.mean(ys**m)) / m)
    yr = yr / np.sqrt(np.mean(np.abs(yr) ** 2))

    meter = detect_alignment(y, idx, m, settle)
    if meter is None:
        raise AssertionError(
            f"no alignment detected over {len(ys)} settled symbols. The "
            f"truth-free pair reads EVM {evm:.1f} dB and M2M4 {m2m4:.1f} dB "
            f"(scatter floor {evm_scatter_floor_db(m):.1f} dB) -- if those "
            f"look healthy, this is the false-lock signature, not a broken "
            f"demodulator. There is no lag at which an SER means anything, "
            f"so none is reported"
        )
    lag = int(meter.lag)

    # Differences, so no absolute rotation is needed -- only the lag. The
    # decisions are taken on the settled slice, whose element k is y[settle+k]
    # and therefore truth[settle+k+lag].
    dec = np.round(np.angle(yr) / step).astype(int) % m
    dd, dt = np.diff(dec) % m, np.diff(idx) % m
    base = settle + lag
    a0 = max(0, -base)
    b0 = max(0, base)
    k = min(len(dd) - a0, len(dt) - b0)
    if k < 200:
        raise AssertionError(
            f"only {k} symbols overlap at the detected lag {lag}: too few to "
            f"report an SER"
        )
    ser = float(np.mean(dd[a0 : a0 + k] != dt[b0 : b0 + k]))
    return SymbolMetrics(evm, ser, lag, m2m4)
