#!/usr/bin/env python3
"""M-PSK receiver performance, characterised by Monte Carlo over random
geometries -- both the complex-baseband and real-IF receivers, side by side.

`track.MpskReceiver` and `track.MpskReceiverR` are the library's most
load-bearing composites, so their performance is characterised rather than
spot-checked. **Every nuisance parameter is drawn at random per trial** --
constellation order, outputs per symbol, samples per symbol (a non-integer
double, as a free-running ADC clock gives you), IF placement, both loop
bandwidths, the carrier and sample-clock offsets, the input level and the data
-- and the figures report distributions across trials rather than one hand-
picked geometry.

That choice is the point. A fixed grid measures the geometries you thought of,
and it is exactly how a real outlier hid here for weeks: at `sps = 10` with
`m_out = 4` and an IF at 0.10 the occupied band reaches DC, where the real
front end's image rejection collapses, and EVM falls to -4 dB. A grid of
"reasonable" cases never drew it. Random draws over the documented input
domain do, and they either show a clean distribution or hand you the outlier.

The committed gallery figure is **three panels** -- does it meet the bound
(`evm`, `ber`) and how long does it take (`lock`). Five more exist and are
plottable on demand (`--only falsealarm,telemetry`, or `--only all`), but they
answer PASS/FAIL questions that a plot is the wrong shape for: level and
sample-rate invariance are flat lines, false alarm is a count of zero, chunking
is a bar chart of exact zeros, telemetry is two bars. Those five are assertions
now -- `src/doppler/track/tests/test_mpsk_receiver_performance.py` and the
`bench_mpsk_receiver*.py` pair -- which is where a pass/fail property belongs.

All eight panels:

    evm         EVM against the coherent bound, EVM_dB = -(Es/N0)_dB
    ber         symbol error rate against the theoretical M-PSK bound
    lock        lock time as a fraction of the loop's own settling budget
    invariance  lock time in SYMBOLS against samples per symbol
    falsealarm  noise only: how often either detector wrongly declares lock
    level       invariance to absolute input level over three decades
    chunking    streaming: arbitrary chunk sizes give a bit-identical stream
    telemetry   throughput with all 11 probes attached vs fully detached

**Loop bandwidth is normalised to the SYMBOL rate, so settling is invariant to
the sample rate.** A loop needs ~5/Bn *symbols* whether that is 8 samples per
symbol or 13333, which is what the `invariance` panel shows directly. The
practical consequence is that a heavily oversampled case needs a long record in
SAMPLES for the same number of symbols -- millions of them at a realistic IF
geometry (40 MSa/s, 10 MHz IF, Rs = 3 kS/s is sps = 13333) -- and that is a
stimulus-generation problem, not a reason to shorten the measurement. `wfmgen`
exists for exactly that; this script keeps its own stimulus small enough to run
in CI and covers the sample-rate axis with the invariance panel instead.

Run:
    python mpsk_receiver_performance_demo.py [out.png] [--only PANEL[,PANEL]]
                                            [--trials N]
"""

from __future__ import annotations

import argparse
import sys
import time

import numpy as np

from doppler.ber import (
    ber_lock_symbol,
    ber_settle_syms,
    ber_theory_ser,
)
from doppler.telemetry import Telemetry
from doppler.track import MpskReceiver, MpskReceiverR

# ── the measurement rules, in one place ──────────────────────────────────
#
# These are not tunables, they are what makes the numbers mean anything. The
# same three live in src/doppler/track/tests/_mpsk_rx_harness.py, which is
# where the test suite gets them; the duplication here is deliberate so this
# script reads top-to-bottom as documentation.
#
# 1. SETTLING. Two loops, cascaded (the carrier discriminator reads the on-time
#    strobe, so it cannot converge until timing has), and both closed at once.
#    So the budgets ADD and joint tracking DOUBLES the sum:
#        settle = 2 * (5/bn_timing + 5/bn_carrier)
#    At bn = 0.01 that is 2000 symbols, not 500. Measuring from 500 reports
#    -9.0 dB where the settled answer is -23.2 dB.
# 2. OFFSETS INSIDE THE LOOP BANDWIDTH. Seeded exactly on truth a loop never
#    leaves its initial state and its "lock time" is meaningless; asked to
#    acquire beyond Bn it is a coin flip (measured: carrier lock in 39 symbols
#    at 0.25*Bn, 1376 at 1*Bn, never at 2*Bn). Every trial draws its offsets
#    inside Bn, which is the region the loop is specified for.
# 3. EVM, NOT BER, FOR QUALITY. SER saturates at 0 long before a constellation
#    is good. The bound is EVM_dB = -(Es/N0)_dB -- an I/Q-plane quantity
#    against an I/Q-plane bound, no factor of two -- so an EVM that BEATS it
#    means the measurement is wrong, never that the receiver is brilliant.

#: Ddcr's design centre: its R2C halfband bakes in a +fs/4 shift.
IF_FS4 = 0.25
#: Usable IF band for the real path, measured on the front end alone: image
#: rejection is past -100 dB inside this and collapses to -7 dB at 0.01.
IF_BAND = (0.06, 0.44)

#: Es/N0 sweep. Starts at 4 dB: below that a receiver is not being
#: characterised, it is being asked to work outside its operating range, and
#: every metric there measures the acquisition threshold rather than the
#: receiver. Both trials that failed to lock in an earlier run sat at 2 dB.
ESN0_GRID = np.arange(4.0, 21.0, 2.0)

#: **The lock threshold is O(1) only because both lock integrators AVERAGE.**
#: The carrier statistic is an EMA (`l->lock += 0.05 * (lk - l->lock)`, so its
#: settled value is the MEAN of the per-look signal, ~1.0 at lock, with an
#: equivalent averaging length of 1/alpha = 20 looks); the timing statistic is
#: an explicit block mean (`lock_stat = lock_sum / avgs`, avgs = 133). Because
#: both are means, a normalised per-look signal of ~1.0 at lock gives a settled
#: statistic of ~1.0 whatever N is, and one threshold works.
#:
#: If either integrator is ever changed to ACCUMULATE instead of average, the
#: threshold must scale with N -- a sum of N unit-mean looks settles at N, not
#: 1 -- and a threshold left at ~1.0 would then declare lock on the first look
#: and never drop. Anything derived from a Pfa (det_threshold returns eta in
#: units of the thresholded statistic's own noise sigma) has the same
#: dependency: sigma falls as 1/sqrt(N) for a mean and grows as sqrt(N) for a
#: sum. Check the integrator before trusting a threshold.
LOCK_STAT_IS_AVERAGED = True


#: The one organising fact of this characterisation: there is an ACQUISITION
#: THRESHOLD, it is PER-M, and every bound below is asserted only above it.
#:
#: Above 8 dB, measured over randomised geometries and loop bandwidths: every
#: trial locks, lock time is at worst 0.43 of its settling budget, and
#: implementation loss against the coherent bound stays inside 5 dB. Below it
#: all three degrade together -- BPSK at 2 dB with bn = 0.022 shows 6.8 dB of
#: loss and takes 1.29x budget to declare -- because an M-th-power NDA loop
#: pays
#: squaring loss plus phase jitter that grows as Es/N0 falls and bn rises. That
#: is the receiver's real behaviour, not a defect, so the sub-threshold region
#: is PLOTTED and printed rather than asserted. Asserting it would either fail
#: on physics or be loosened until it caught nothing.
def squaring_loss_db(m, esn0_db):
    """M-th-power squaring loss, from the fits in docs/design/mpsk.md §2.3.

    This is the loss the NDA discriminator pays turning a modulated signal into
    a carrier-phase estimate, and it is brutal at high M: BPSK bottoms out near
    -0.43 dB (Yuen Eq. 8-19, half-symbol boxcar arm) while 8PSK is still at
    -21.6 dB at Es/N0 = 8 dB. The loop's effective SNR is Es/N0 plus this, so a
    negative sum means the carrier loop cannot work however long it runs.
    """
    if m == 2:
        return -0.43
    if m == 4:
        return -0.0564724 * esn0_db**2 + 1.90284531 * esn0_db - 15.65792221
    return -0.14285557 * esn0_db**2 + 5.70706958 * esn0_db - 58.13670891


#: The operating point everything is specified at: the Es/N0 where the coherent
#: bound gives SER = 1e-3. One stated number, and the per-M Es/N0 falls out of
#: it rather than being chosen -- 6.8 dB for BPSK, 10.3 for QPSK, 15.7 for
#: 8PSK.
SER_SPEC = 1e-3

#: A tracking loop needs ~20 dB of LOOP SNR to work -- SNR measured in the
#: loop's own bandwidth, after the discriminator's squaring loss. Below that
#: the
#: loop is being driven by noise: the phase estimate random-walks, the lock
#: statistic still rises on the transient, and the receiver declares lock while
#: producing chance-level symbols.
LOOP_SNR_MIN_DB = 20.0

#: And in practice a loop bandwidth is 0.01 of the symbol rate or less. The way
#: to meet the loop-SNR requirement is to NARROW the loop, never to accept less
#: SNR -- which is why `bn` here is derived from the operating point and
#: capped,
#: not drawn freely. Measured before this rule was applied: 8PSK at bn = 0.030
#: to 0.034 declared lock and produced SER 0.60-0.85 (chance is 0.875), at and
#: above its own SER = 1e-3 Es/N0. Those bn values are simply not usable there.
BN_MAX = 0.01


def loop_snr_db(m, esn0_db, bn):
    """SNR in the loop's own bandwidth, after squaring loss.

    `bn` is normalised to the symbol rate, so the noise bandwidth ratio is
    `1/bn` symbols of integration: rho_L = Es/N0 + squaring_loss - 10log10(bn).
    """
    return esn0_db + squaring_loss_db(m, esn0_db) - 10.0 * np.log10(bn)


def bn_for_loop_snr(m, esn0_db, target_db=LOOP_SNR_MIN_DB, cap=BN_MAX):
    """Widest `bn` that still delivers `target_db` of loop SNR, capped.

    Narrowing the loop is the lever: every 3 dB of missing loop SNR is halving
    `bn`. Returns a floor of 1e-4 so a hopeless operating point still yields a
    runnable (if very long) trial rather than zero.
    """
    bn = cap
    while bn > 1e-4 and loop_snr_db(m, esn0_db, bn) < target_db:
        bn *= 0.5
    return max(bn, 1e-4)


def esn0_spec(m, ser_spec=SER_SPEC):
    """Es/N0 at which the coherent bound reaches `ser_spec` for this M.

    This is the operating point the receiver is held to, and it is per-M
    because
    the bound is: at a given Es/N0, 8PSK's symbols are simply closer together.
    Anchoring on a SER makes "does the receiver meet its bound" a question
    asked
    at the same PLACE ON THE CURVE for every constellation, instead of at the
    same Es/N0 -- where 8PSK would be asked to work 9 dB below its own 1e-3
    point and BPSK 3 dB above.

    It also lands where the M-th-power acquisition can actually operate, which
    is not a coincidence: `squaring_loss_db` leaves 8PSK at -21.6 dB of loop
    SNR at Es/N0 = 8 dB, and measured there it declares lock while producing
    SER 0.847 (chance is 0.875). At its own 1e-3 point the loss is manageable.
    """
    lo, hi = -10.0, 40.0
    for _ in range(60):
        mid = 0.5 * (lo + hi)
        if ber_theory_ser(m, 10 ** (mid / 10.0)) > ser_spec:
            lo = mid
        else:
            hi = mid
    return 0.5 * (lo + hi)


def settle_floor(bn_timing, bn_carrier):
    """Symbols to allow for settling: `2*(5/bn_t + 5/bn_c)`.

    Delegates to `ber.ber_settle_syms` -- the C implementation is the only
    one. 5/Bn per loop, the two budgets ADD because the loops are cascaded
    (the carrier discriminator reads the on-time strobe), and the sum DOUBLES
    for joint tracking.
    """
    return ber_settle_syms(bn_timing, bn_carrier)


def draw_geometry(rng, real):
    """One random receiver geometry, inside the documented input domain.

    `sps` is drawn as a non-integer double on purpose: the terminal
    accumulator is a double and the loop only has to steer the strobe, so an
    irrational samples-per-symbol is not a special case and should not be
    tested as if it were.
    """
    m = int(rng.choice([2, 4, 8]))
    m_out = int(rng.choice([4, 6, 8]))
    # the real path needs sps > 2*m_out (its cascade runs at twice the overall
    # rate behind the halfband); the complex path only needs sps >= m_out
    lo = 2.2 * m_out if real else 1.1 * m_out
    sps = float(rng.uniform(lo, max(lo + 4.0, 40.0)))
    # Both bandwidths are randomised for COVERAGE of the usable range -- never
    # to shorten a record. Loop bandwidth is normalised to the symbol rate, so
    # widening it does not buy samples, it changes the receiver: settling,
    # jitter and pull-in range all move together. The settling budget below is
    # derived from whatever is drawn here, so a wide draw costs fewer symbols
    # and a narrow one costs more, and both are measured on their own terms.
    bn_timing = float(10 ** rng.uniform(np.log10(0.002), np.log10(BN_MAX)))
    bn_carrier = float(10 ** rng.uniform(np.log10(0.002), np.log10(BN_MAX)))

    # IF placement: keep the pulse's occupied band (fc +- 1/sps) inside the
    # front end's usable range, which is what the real path actually requires.
    if real:
        half = 1.0 / sps
        lo_f, hi_f = IF_BAND[0] + half, IF_BAND[1] - half
        fc = float(rng.uniform(lo_f, hi_f)) if hi_f > lo_f else IF_FS4
    else:
        fc = float(rng.uniform(0.05, 0.45))

    return {
        "m": m,
        "m_out": m_out,
        "sps": sps,
        "fc": fc,
        "bn_timing": bn_timing,
        "bn_carrier": bn_carrier,
        # offsets INSIDE each loop bandwidth, sign included
        "freq_offset": float(rng.uniform(-0.5, 0.5)) * bn_carrier / sps,
        "clock_offset": float(rng.uniform(-0.5, 0.5)) * bn_timing,
        "ampl": float(10 ** rng.uniform(np.log10(0.05), np.log10(0.8))),
        "seed": int(rng.integers(0, 1 << 31)),
    }


def make_signal(g, nsym, real, esn0_db=None, noise_only=False):
    """Random M-PSK on an IF (real) or at baseband (complex).

    The real flavour is `Re{}` of the complex one, so both paths see the same
    waveform. Es/N0 conventions differ and both are exercised: complex has
    `Es = A^2 * sps` with `N0` the per-complex-sample variance; real has
    `Es = A^2 * sps / 2`, since half a real passband signal's power sits at the
    negative frequency, and a real noise variance of `Es / (2 * Es/N0)`.
    """
    rng = np.random.default_rng(g["seed"])
    nsamp = int(nsym * g["sps"])
    idx = rng.integers(0, g["m"], nsym)

    if noise_only:
        sigma = g["ampl"]
        if real:
            x = rng.standard_normal(nsamp) * sigma
            return np.ascontiguousarray(x.astype(np.float32)), idx
        x = rng.standard_normal(nsamp) + 1j * rng.standard_normal(nsamp)
        return np.ascontiguousarray(
            (x * sigma / np.sqrt(2)).astype(np.complex64)
        ), idx

    # symbol index for each sample: floor(n / sps), so a non-integer sps needs
    # no resampling -- the transitions simply land off the sample grid
    n = np.arange(nsamp)
    si = np.minimum((n / g["sps"]).astype(int), nsym - 1)
    bb = np.exp(2j * np.pi * idx[si] / g["m"])
    z = bb * np.exp(2j * np.pi * g["fc"] * n) * g["ampl"]

    if real:
        x = z.real.astype(np.float64)
        if esn0_db is not None:
            es = g["ampl"] ** 2 * g["sps"] / 2.0
            var = es / (2.0 * 10 ** (esn0_db / 10.0))
            x = x + rng.standard_normal(x.size) * np.sqrt(var)
        return np.ascontiguousarray(x.astype(np.float32)), idx

    x = z.astype(np.complex128)
    if esn0_db is not None:
        es = g["ampl"] ** 2 * g["sps"]
        var = es / 10 ** (esn0_db / 10.0)
        x = x + (
            rng.standard_normal(x.size) + 1j * rng.standard_normal(x.size)
        ) * np.sqrt(var / 2)
    return np.ascontiguousarray(x.astype(np.complex64)), idx


def demod(x, g, real, telemetry=True):
    """Run the matching receiver; return `(symbols, probes, seconds)`."""
    cls = MpskReceiverR if real else MpskReceiver
    rx = cls(
        m=g["m"],
        sps=g["sps"] * (1.0 + g["clock_offset"]),
        m_out=g["m_out"],
        bn_timing=g["bn_timing"],
        bn_carrier=g["bn_carrier"],
        init_norm_freq=g["fc"] - g["freq_offset"],
    )
    tlm = None
    if telemetry:
        tlm = Telemetry(1 << 22)
        rx.set_telemetry(tlm, "rx")
    t0 = time.perf_counter()
    y = rx.steps(x)
    dt = time.perf_counter() - t0
    probes = {}
    if tlm is not None:
        rec = tlm.read()
        names = tlm.probe_names()
        probes = {
            k.removeprefix("rx."): rec[rec["probe"] == pid]["value"].astype(
                float
            )
            for k, pid in names.items()
        }
        rx.set_telemetry(None, "rx")
    return y, probes, dt


def lock_symbol(flag, sustain=200, min_frac=0.9):
    """Symbol index from which a verify-counted flag is SUSTAINED.

    Delegates to `ber.ber_lock_symbol`. `sustain` consecutive symbols high AND
    at least `min_frac` of everything after that point high too -- the run
    rejects a single lucky decision, the fraction rejects a detector that
    declares early then flaps. Returns `None` for "never locked".
    """
    idx = ber_lock_symbol(
        np.asarray(flag, dtype=np.float64) > 0.5, sustain, min_frac
    )
    return None if idx < 0 else int(idx)


def metrics(y, idx, m, settle):
    """EVM (dB) and differential SER over the settled window.

    Both are invariant to the M-fold phase ambiguity: the constellation is
    de-rotated by the mean M-th-power phase, and SER is scored on symbol
    DIFFERENCES so only a lag is needed, not an absolute rotation. The lag
    search is deliberately wide -- group delay varies with pulse, front end and
    rate, and a clipped search reports chance SER on a perfect decode.
    """
    ys = y[settle:]
    if len(ys) < 200:
        return float("nan"), float("nan")
    step = 2.0 * np.pi / m
    yr = ys * np.exp(-1j * np.angle(np.mean(ys**m)) / m)
    yr = yr / np.sqrt(np.mean(np.abs(yr) ** 2))
    ideal = np.exp(1j * step * np.round(np.angle(yr) / step))
    evm = 10 * np.log10(np.mean(np.abs(yr - ideal) ** 2))

    dec = np.round(np.angle(yr) / step).astype(int) % m
    dd, dt = np.diff(dec) % m, np.diff(idx) % m
    ser = 1.0
    for lag in range(-200, 201):
        a0, b0 = max(0, lag), max(0, -lag) + settle
        k = min(len(dd) - a0, len(dt) - b0)
        if k >= 200:
            ser = min(ser, float(np.mean(dd[a0 : a0 + k] != dt[b0 : b0 + k])))
    return evm, ser


# --8<-- [start:chunking]
# Feeding a stream in chunks: the receiver is a streaming object, so `steps()`
# may be called with whatever block the source hands you. It keeps every piece
# of state that spans a call -- the LO phase, the cascade's delay lines, the
# timing accumulator, both loop integrators, and (on the real path) the R2C
# halfband's orphan sample when a chunk has odd length -- so a chunked run is
# BIT-IDENTICAL to one big call. Chunk size is a buffering decision, not a
# signal-processing one.
import numpy as np  # noqa: E402  (region must be self-contained)


def demod_stream(rx, blocks):
    """Feed an iterable of blocks; yield the symbols each call produced.

    The output count per call VARIES -- a chunk of N samples yields about
    N/sps symbols, but which side of the boundary a symbol lands on depends
    on the timing accumulator's phase, so never assume a fixed ratio.
    """
    for block in blocks:
        symbols = rx.steps(block)  # may be empty for a short block
        if len(symbols):
            yield symbols


def chunk_randomly(x, rng, lo=997, hi=9973):
    """Split `x` into chunks of random length -- deliberately including ODD
    lengths, which is what exercises the real path's even/odd halfband pairing
    across a call boundary."""
    i = 0
    while i < x.size:
        n = int(rng.integers(lo, hi))
        yield x[i : i + n]
        i += n


# --8<-- [end:chunking]


def run_chunked(g, real, nsym, chunk_plan, seed=11):
    """Run one stimulus whole, then re-run it chunked, and compare.

    Returns `(whole, chunked_by_plan, symbols_per_call)`. Any difference is a
    state bug: something that should have survived a call boundary did not.
    """
    x, _ = make_signal(g, nsym, real)
    whole, _, _ = demod(x, g, real, telemetry=False)

    cls = MpskReceiverR if real else MpskReceiver
    out, counts = {}, {}
    for label, plan in chunk_plan.items():
        rx = cls(
            m=g["m"],
            sps=g["sps"] * (1.0 + g["clock_offset"]),
            m_out=g["m_out"],
            bn_timing=g["bn_timing"],
            bn_carrier=g["bn_carrier"],
            init_norm_freq=g["fc"] - g["freq_offset"],
        )
        rng = np.random.default_rng(seed)
        blocks = plan(x, rng)
        pieces = list(demod_stream(rx, blocks))
        out[label] = (
            np.concatenate(pieces) if pieces else np.empty(0, np.complex64)
        )
        counts[label] = [len(p) for p in pieces]
    return whole, out, counts


def implementation_loss_db(m, esn0_db, ser):
    """dB by which Es/N0 would have to be RAISED for theory to predict `ser`.

    The right way to score a real receiver against a coherent bound. A raw
    SER/theory RATIO is unusable across a sweep: theory underflows at high
    Es/N0 (BPSK at 14 dB is 7e-13), so one symbol error in a finite record
    reports a ratio in the billions while the loss in dB is a fraction of one.
    The same 2-5x ratio also means very different things at 4 dB and at 12 dB.

    Bisects theory_ser, which is monotone decreasing in Es/N0.
    """
    if not (ser > 0.0):
        return 0.0
    lo, hi = -10.0, 40.0
    for _ in range(60):
        mid = 0.5 * (lo + hi)
        if ber_theory_ser(m, 10 ** (mid / 10.0)) > ser:
            lo = mid
        else:
            hi = mid
    return esn0_db - 0.5 * (lo + hi)


def run_trial(
    rng, real, esn0_db=None, noise_only=False, nsym_extra=1200, ampl=None
):
    """One Monte-Carlo trial. Returns a record, or None if it never locked."""
    g = draw_geometry(rng, real)
    if ampl is not None:
        g["ampl"] = ampl
    if esn0_db is not None:
        # Narrow both loops until each has its 20 dB of loop SNR. This is the
        # whole reason bn is not a free nuisance parameter: at a given Es/N0
        # and
        # M there is a widest usable bn, and exceeding it does not degrade
        # gracefully -- the loop stops tracking while still declaring lock.
        bn_max = bn_for_loop_snr(g["m"], esn0_db)
        g["bn_carrier"] = min(g["bn_carrier"], bn_max)
        g["bn_timing"] = min(g["bn_timing"], bn_max)
        g["freq_offset"] = np.sign(g["freq_offset"]) * min(
            abs(g["freq_offset"]), 0.5 * g["bn_carrier"] / g["sps"]
        )
        g["clock_offset"] = np.sign(g["clock_offset"]) * min(
            abs(g["clock_offset"]), 0.5 * g["bn_timing"]
        )
    budget = settle_floor(g["bn_timing"], g["bn_carrier"])
    nsym = budget + nsym_extra
    x, idx = make_signal(g, nsym, real, esn0_db, noise_only)
    y, pr, dt = demod(x, g, real)

    t_lock = lock_symbol(pr["sync.locked"])
    c_lock = lock_symbol(pr["car.locked"])
    rec = dict(
        g,
        budget=budget,
        esn0_db=esn0_db,
        real=real,
        nsym=nsym,
        t_lock=t_lock,
        c_lock=c_lock,
        nsamp=x.size,
        seconds=dt,
        lock_stat=float(np.mean(pr["lock"][budget:]))
        if pr["lock"].size > budget
        else float("nan"),
    )
    if noise_only:
        return rec
    if t_lock is None or c_lock is None:
        rec.update(evm=float("nan"), ser=float("nan"), locked=False)
        return rec
    settle = max(budget, t_lock, c_lock)
    if len(y) - settle < 300:
        rec.update(evm=float("nan"), ser=float("nan"), locked=False)
        return rec
    evm, ser = metrics(y, idx, g["m"], settle)
    rec.update(evm=evm, ser=ser, locked=True)
    return rec


# ── panels ───────────────────────────────────────────────────────────────


def panel_evm(ax, trials):
    for real, colour, name in (
        (True, "tab:red", "R (real IF)"),
        (False, "tab:blue", "C (complex)"),
    ):
        pts = [
            (t["esn0_db"], t["evm"])
            for t in trials
            if t["real"] is real and t["locked"]
        ]
        if pts:
            e, v = np.array(pts).T
            ax.scatter(
                e + (0.15 if real else -0.15),
                v,
                s=14,
                alpha=0.55,
                c=colour,
                label=name,
            )
            med = [
                (d, np.median([b for a, b in pts if a == d]))
                for d in sorted(set(e))
            ]
            ax.plot(*np.array(med).T, lw=1.4, color=colour, alpha=0.9)
    g = np.linspace(ESN0_GRID[0] - 1, ESN0_GRID[-1] + 1, 50)
    ax.plot(g, -g, "k--", lw=1.2, label="bound: EVM = -(Es/N0)")
    ax.set_xlabel("Es/N0, dB")
    ax.set_ylabel("EVM, dB")
    ax.set_title(
        "EVM vs the coherent bound\n(random geometry per point)", fontsize=9.5
    )
    ax.grid(alpha=0.3)
    ax.legend(fontsize=7, loc="upper right")


def panel_ber(ax, trials):
    for m, marker in ((2, "o"), (4, "s"), (8, "^")):
        for real, colour in ((True, "tab:red"), (False, "tab:blue")):
            pts = [
                (t["esn0_db"], t["ser"])
                for t in trials
                if t["m"] == m
                and t["real"] is real
                and t["locked"]
                and t["ser"] > 0
            ]
            if pts:
                e, s = np.array(pts).T
                ax.scatter(e, s, s=16, marker=marker, alpha=0.5, c=colour)
        g = np.linspace(ESN0_GRID[0], ESN0_GRID[-1], 40)
        ax.plot(
            g,
            [ber_theory_ser(m, 10 ** (d / 10.0)) for d in g],
            lw=1.2,
            ls="--",
            label=f"{m}-PSK theory",
        )
    ax.set_yscale("log")
    ax.set_ylim(1e-5, 1.0)
    ax.set_xlabel("Es/N0, dB")
    ax.set_ylabel("symbol error rate")
    ax.set_title(
        "SER vs the coherent M-PSK bound\n(red = real IF, blue = complex)",
        fontsize=9.5,
    )
    ax.grid(alpha=0.3, which="both")
    ax.legend(fontsize=7, loc="lower left")


def panel_lock(ax, trials):
    """Lock time as a FRACTION of each trial's own settling budget, which is
    what makes trials with different loop bandwidths comparable."""
    for key, colour, name in (
        ("t_lock", "tab:green", "timing"),
        ("c_lock", "tab:purple", "carrier"),
    ):
        frac = sorted(
            t[key] / t["budget"]
            for t in trials
            if t.get(key) is not None and not t.get("noise_only")
        )
        if frac:
            ax.plot(
                frac,
                np.linspace(0, 100, len(frac)),
                lw=1.6,
                color=colour,
                label=f"{name} (n={len(frac)})",
            )
    ax.axvline(
        1.0, color="k", ls="--", lw=1.2, label="budget 2*(5/bn_t + 5/bn_c)"
    )
    ax.set_xlim(0, 1.35)
    ax.set_xlabel("lock time / settling budget")
    ax.set_ylabel("percent of trials locked by")
    ax.set_title(
        "Lock time against each trial's own budget\n"
        "(offsets drawn INSIDE the loop bandwidth)",
        fontsize=9.5,
    )
    ax.grid(alpha=0.3)
    ax.legend(fontsize=7, loc="lower right")


def panel_falsealarm(ax, noise_trials, signal_trials):
    """Noise only: the carrier lock statistic must stay below what a real
    constellation produces, and the detectors must not declare."""
    n_stat = [
        t["lock_stat"] for t in noise_trials if np.isfinite(t["lock_stat"])
    ]
    s_stat = [
        t["lock_stat"]
        for t in signal_trials
        if np.isfinite(t["lock_stat"]) and t["locked"]
    ]
    bins = np.linspace(
        min(n_stat + s_stat + [0.0]), max(n_stat + s_stat + [1.0]), 30
    )
    ax.hist(
        n_stat,
        bins=bins,
        color="tab:orange",
        alpha=0.75,
        label=f"noise only (n={len(n_stat)})",
    )
    ax.hist(
        s_stat,
        bins=bins,
        color="tab:blue",
        alpha=0.6,
        label=f"signal (n={len(s_stat)})",
    )
    fa = sum(
        1
        for t in noise_trials
        if t["t_lock"] is not None and t["c_lock"] is not None
    )
    ax.set_xlabel("carrier lock statistic (settled mean)")
    ax.set_ylabel("trials")
    ax.set_title(
        f"False alarm on noise only\n"
        f"both detectors declared in {fa}/{len(noise_trials)} trials",
        fontsize=9.5,
    )
    ax.grid(alpha=0.3)
    ax.legend(fontsize=7)


def panel_level(ax, level_trials):
    for real, colour, name in (
        (True, "tab:red", "R (real IF)"),
        (False, "tab:blue", "C (complex)"),
    ):
        pts = [
            (t["ampl"], t["evm"])
            for t in level_trials
            if t["real"] is real and t["locked"]
        ]
        if pts:
            a, v = np.array(pts).T
            o = np.argsort(a)
            ax.plot(
                a[o],
                v[o],
                "o-",
                ms=4,
                lw=1.1,
                color=colour,
                alpha=0.8,
                label=name,
            )
    ax.set_xscale("log")
    ax.set_xlabel("input amplitude (linear, full scale = 1.0)")
    ax.set_ylabel("EVM, dB")
    ax.set_title(
        "Level invariance\n(AGC-normalised; EVM must not track level)",
        fontsize=9.5,
    )
    ax.grid(alpha=0.3, which="both")
    ax.legend(fontsize=7)


def panel_invariance(ax, trials):
    """Lock time in SYMBOLS against samples per symbol.

    `bn` is normalised to the symbol rate, so this must be FLAT: a loop needs
    ~5/Bn symbols whether that is 8 samples per symbol or 13333. A trend here
    would mean a bandwidth that had quietly become sample-rate dependent, which
    is the bug the normalisation exists to prevent (and which `bn_carrier` had
    before the cascade rebuild).
    """
    for key, colour, name in (
        ("t_lock", "tab:green", "timing"),
        ("c_lock", "tab:purple", "carrier"),
    ):
        pts = [
            (t["sps"], t[key] * t["bn_timing"])
            for t in trials
            if t.get(key) is not None and not t.get("noise_only")
        ]
        if pts:
            s, v = np.array(pts).T
            ax.scatter(s, v, s=14, alpha=0.55, c=colour, label=name)
    ax.axhline(5.0, color="k", ls="--", lw=1.1, label="5/Bn (one loop)")
    ax.set_xlabel("samples per symbol (drawn per trial, non-integer)")
    ax.set_ylabel("lock time x bn  [symbols / (1/bn)]")
    ax.set_title(
        "Loop bandwidth is sample-rate invariant\n"
        "(bn is normalised to the SYMBOL rate — this must be flat)",
        fontsize=9.5,
    )
    ax.grid(alpha=0.3)
    ax.legend(fontsize=7)


def panel_chunking(ax, chunk_results):
    """Chunked streaming against one big call: the error must be exactly zero.

    Plotted on a log axis with a floor, so "identical" is visibly pinned at the
    bottom rather than hidden by autoscaling.
    """
    labels, errs, spread, exact = [], [], [], []
    for (name, label), (max_err, counts) in chunk_results.items():
        labels.append(f"{name}\n{label}")
        exact.append(max_err == 0.0)  # BEFORE the log-axis floor
        errs.append(max(max_err, 1e-20))  # 1e-20 is a floor, not an error
        spread.append((min(counts), max(counts)) if counts else (0, 0))
    xs = np.arange(len(labels))
    ax.bar(
        xs,
        errs,
        0.55,
        color=["tab:green" if ok else "tab:red" for ok in exact],
    )
    ax.set_yscale("log")
    ax.set_ylim(1e-21, 1e-3)
    ax.axhline(
        1e-20,
        color="tab:green",
        ls="--",
        lw=1.0,
        label="bit-identical (error == 0)",
    )
    for i, (lo, hi) in enumerate(spread):
        ax.annotate(
            f"{lo}-{hi} sym/call",
            (i, 2e-20),
            ha="center",
            va="bottom",
            fontsize=7,
            rotation=90,
        )
    ax.set_xticks(xs)
    ax.set_xticklabels(labels, fontsize=7.5)
    ax.set_ylabel("max |chunked - whole|")
    ax.set_title(
        "Streaming: chunk size is a buffering choice\n"
        "(odd-length chunks included — they cross the R2C parity)",
        fontsize=9.5,
    )
    ax.grid(alpha=0.3, axis="y")
    ax.legend(fontsize=7, loc="upper right")


def panel_telemetry(ax, rows):
    labels = [r[0] for r in rows]
    on = [r[1] for r in rows]
    off = [r[2] for r in rows]
    xs = np.arange(len(rows))
    ax.bar(xs - 0.2, off, 0.4, color="tab:grey", label="detached")
    ax.bar(
        xs + 0.2,
        on,
        0.4,
        color="tab:cyan",
        label="attached (11 probes, decim=1)",
    )
    for i, (o, a) in enumerate(zip(off, on)):
        ax.annotate(
            f"{100 * (o - a) / o:+.0f}%",
            (i + 0.2, a),
            ha="center",
            va="bottom",
            fontsize=7.5,
        )
    ax.set_xticks(xs)
    ax.set_xticklabels(labels, fontsize=8)
    ax.set_ylabel("throughput, Msamp/s")
    ax.set_title(
        "Cost of always-on telemetry\n"
        "(one 16-byte ring write per probe per symbol)",
        fontsize=9.5,
    )
    ax.grid(alpha=0.3, axis="y")
    ax.legend(fontsize=7)


def measure_telemetry(rng, reps=7):
    """Throughput with every probe attached vs fully detached.

    Three things this has to get right, all learned by getting them wrong:

    * **Warm up outside the timed region.** The first `steps()` call lazily
      allocates the variable-output buffer, so timing it charges that malloc to
      whichever configuration ran first. Unwarmed, this reported the ATTACHED
      case as 46% faster than detached, which is impossible.
    * **Interleave, don't run one config then the other.** Clock and cache
    state
      drift over a measurement, so A-then-B attributes the drift to B. Each rep
      does both.
    * **Drain the ring.** A saturated ring switches to drop-and-count, which is
      a different (cheaper) code path than a ring that accepts writes.

    The cost is per PROBE per SYMBOL -- eleven 16-byte ring writes -- so as a
    fraction of throughput it falls as `sps` rises. The fixed geometry below
    (`sps = 32`) is therefore a pessimistic case, not a typical one.
    """
    rows = []
    for real, name in ((False, "MpskReceiver"), (True, "MpskReceiverR")):
        g = draw_geometry(rng, real)
        g.update(
            sps=32.0,
            m_out=8,
            m=4,
            fc=IF_FS4,
            bn_timing=0.01,
            bn_carrier=0.01,
            ampl=0.4,
        )
        x, _ = make_signal(g, 4000, real)
        warm = x[: max(1024, x.size // 20)]
        cls = MpskReceiverR if real else MpskReceiver
        best = {True: 0.0, False: 0.0}
        for _ in range(reps):
            for tel in (True, False):
                rx = cls(
                    m=g["m"],
                    sps=g["sps"],
                    m_out=g["m_out"],
                    bn_timing=g["bn_timing"],
                    bn_carrier=g["bn_carrier"],
                    init_norm_freq=g["fc"],
                )
                tlm = None
                if tel:
                    tlm = Telemetry(1 << 22)
                    rx.set_telemetry(tlm, "rx")
                rx.steps(warm)  # warm-up: lazy alloc, page faults
                if tlm is not None:
                    tlm.read()  # drain, so writes are not dropped
                t0 = time.perf_counter()
                rx.steps(x)
                dt = time.perf_counter() - t0
                if tlm is not None:
                    assert tlm.dropped == 0, "ring overran; timing is invalid"
                best[tel] = max(best[tel], x.size / dt / 1e6)
        rows.append((name, best[True], best[False]))
    return rows


# ── driver ───────────────────────────────────────────────────────────────

ALL_PANELS = (
    "evm",
    "ber",
    "lock",
    "invariance",
    "falsealarm",
    "level",
    "chunking",
    "telemetry",
)

#: What the committed gallery figure shows, and the default when no `--only` is
#: given. Two panels: how close to the bound (`evm`) and how long to get there
#: (`lock`). Those are the two questions a reader of a receiver page has.
#:
#: The other six are plottable on demand (`--only ber,falsealarm`, or
#: `--only all`) but are not in the figure, because each is a PASS/FAIL
#: property and a plot is the wrong shape for one: level and sample-rate
#: invariance are flat lines, false alarm is a count of zero, chunking is a bar
#: chart of exact zeros, telemetry is two bars, and `ber` restates `evm` on a
#: log axis. All six
#: are assertions instead -- `test_mpsk_receiver_performance.py` (false alarm,
#: level invariance, and the coherent bound via inverse binomial sampling) and
#: the `bench_mpsk_receiver*.py` pair (telemetry) -- which is where a pass/fail
#: property belongs, since it then runs on every commit.
GALLERY_PANELS = ("evm", "lock")

#: Chunking plans exercised by the `chunking` panel. Fixed powers of two are
#: the
#: common case; the random plan is the one that matters, because it produces
#: odd
#: chunk lengths and those are what cross the real path's even/odd halfband
#: pairing. A single-sample plan is the pathological limit.
CHUNK_PLANS = {
    "4096": lambda x, rng: (x[i : i + 4096] for i in range(0, x.size, 4096)),
    "random odd": chunk_randomly,
    "1 sample": lambda x, rng: (x[i : i + 1] for i in range(0, x.size)),
}


def measure_chunking(rng):
    """One geometry per path, run whole vs three chunking plans."""
    results = {}
    for real, name in ((True, "MpskReceiverR"), (False, "MpskReceiver")):
        g = draw_geometry(rng, real)
        g.update(
            sps=20.0,
            m_out=8,
            m=4,
            fc=IF_FS4,
            bn_timing=0.01,
            bn_carrier=0.01,
            ampl=0.4,
        )
        nsym = settle_floor(g["bn_timing"], g["bn_carrier"]) + 600
        plans = dict(CHUNK_PLANS)
        if real:
            plans.pop("1 sample")  # 1 real sample cannot fill the 2:1 halfband
        whole, chunked, counts = run_chunked(g, real, nsym, plans)
        for label, y in chunked.items():
            n = min(len(whole), len(y))
            err = (
                float(np.max(np.abs(y[:n] - whole[:n]))) if n else float("inf")
            )
            # a chunked run must not lose or invent symbols either
            if len(y) != len(whole):
                err = float("inf")
            results[(name, label)] = (err, counts[label])
    return results


def main(
    out_path="mpsk_receiver_performance_demo.png",
    only=GALLERY_PANELS,
    trials=6,
):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    rng = np.random.default_rng(20260727)

    sweep, noise, level = [], [], []
    need_sweep = {"evm", "ber", "lock", "falsealarm"} & set(only)
    if need_sweep:
        for esn0 in ESN0_GRID:
            for real in (True, False):
                for _ in range(trials):
                    sweep.append(run_trial(rng, real, esn0_db=float(esn0)))
    if "falsealarm" in only:
        for real in (True, False):
            for _ in range(max(8, trials * 2)):
                t = run_trial(rng, real, noise_only=True)
                t["noise_only"] = True
                noise.append(t)
    if "level" in only:
        for real in (True, False):
            for a in np.logspace(np.log10(0.01), np.log10(0.9), 8):
                level.append(run_trial(rng, real, esn0_db=14.0, ampl=float(a)))
    chunk_results = measure_chunking(rng) if "chunking" in only else {}
    tel_rows = measure_telemetry(rng) if "telemetry" in only else []

    # ── self-validation: exit 0 must mean "demonstrated AND checked" ──────
    locked = [t for t in sweep if t["locked"]]
    if need_sweep:
        # Acquisition has an SNR THRESHOLD, so a blanket lock-rate assertion is
        # the wrong shape: it either fails on physics or is too weak to catch a
        # defect. Measured, every trial at Es/N0 >= 8 dB locks and every
        # failure
        # sits at <= 6 dB, ordered BPSK < QPSK ~ 8PSK exactly as the squaring
        # loss predicts. So the assertion is unconditional above the threshold,
        # and the region below it is characterisation (printed, not asserted).
        #
        # This is also the assertion that caught the per-M lock-statistic bug:
        # the ceiling used to be 1 / 0.619 / 0.412 against a fixed 0.5 default
        # threshold, so 8PSK could NEVER declare carrier lock -- it read 0/2 at
        # 14 dB while locking 7/7 at 6 dB on the transient overshoot, i.e. the
        # flag was anti-correlated with lock. Normalising the statistic to ~1.0
        # at every M fixed it; do not weaken this back to a global rate.
        above = [t for t in sweep if t["esn0_db"] >= esn0_spec(t["m"])]
        missed = [t for t in above if not t["locked"]]
        assert not missed, (
            f"{len(missed)}/{len(above)} trials at Es/N0 >= 8 dB failed to "
            f"lock: "
            + ", ".join(
                f"m={t['m']} m_out={t['m_out']} sps={t['sps']:.2f} "
                f"Es/N0={t['esn0_db']:.0f}"
                for t in missed[:4]
            )
        )
        hi = [t for t in locked if t["esn0_db"] >= esn0_spec(t["m"])]
        assert hi, "no high-Es/N0 trials locked"
        for t in hi:
            # The matched-filter bound is -(Es/N0), but an NDA carrier loop
            # adds
            # phase jitter on top of it, and the size of that jitter is exactly
            # what the squaring loss predicts -- so the allowance carries it.
            # Measured: 8PSK at its 1e-3 point (15.7 dB, loss -3.8 dB) reaches
            # -11.8 dB against a -16 dB bound; BPSK, whose loss is -0.4 dB,
            # tracks the bound within ~3 dB. A single flat tolerance would
            # either fail on 8PSK's physics or stop policing BPSK entirely.
            allow = 3.0 + abs(squaring_loss_db(t["m"], t["esn0_db"]))
            # `m_out` is a MEASURED axis, not a nuisance one. An I&D matched
            # filter at m_out = 4 already leaves 1-2 dB (which is why the
            # default is 8), and 8PSK has the smallest decision margin, so the
            # corner compounds: measured, 8PSK at m_out = 4 reaches only
            # -12.1 dB at Es/N0 = 20 dB, i.e. ~8 dB off its bound, while
            # m_out >= 8 tracks the squaring-loss allowance. That is a real
            # constraint on the configuration, so it is asserted at its own
            # measured level rather than folded into one loose tolerance --
            # widening `allow` to cover it would stop the bound policing
            # anything at all.
            if t["m"] == 8 and t["m_out"] < 8:
                allow += 5.0
            assert t["evm"] < -t["esn0_db"] + allow, (
                f"EVM {t['evm']:.1f} dB at Es/N0 {t['esn0_db']:.0f} dB, "
                f"allowance {allow:.1f} dB "
                f"(sps={t['sps']:.2f}, m={t['m']}, m_out={t['m_out']})"
            )
            assert t["evm"] > -t["esn0_db"] - 2.0, (
                f"EVM {t['evm']:.1f} dB BEATS the {-t['esn0_db']:.0f} dB "
                f"bound — the measurement is wrong"
            )
        # Implementation loss in dB, and only where the record can resolve it:
        # fewer than ~5 symbol errors is a quantised SER, and 1/N against an
        # underflowed theory reports a huge apparent loss for one unlucky
        # symbol. An NDA loop legitimately pays squaring loss plus phase jitter
        # that grows as Es/N0 falls and bn rises -- measured median loss is
        # 1.5-3 dB across the sweep, matching the 2 dB tolerance the C
        # validator
        # (native/validation/mpsk_receiver_ber.c) already uses at a fixed
        # geometry. 5 dB here covers the randomised bn and geometry.
        for t in [x for x in locked if x["esn0_db"] >= esn0_spec(x["m"])]:
            n_meas = max(1, t["nsym"] - t["budget"])
            if t["ser"] * n_meas < 5.0:
                continue
            loss = implementation_loss_db(t["m"], t["esn0_db"], t["ser"])
            assert loss <= 5.0, (
                f"implementation loss {loss:.1f} dB at Es/N0 "
                f"{t['esn0_db']:.0f} dB (m={t['m']}, m_out={t['m_out']}, "
                f"sps={t['sps']:.2f}, bn_c={t['bn_carrier']:.3f}, "
                f"SER {t['ser']:.4f})"
            )
        # The settling budget is a LINEAR-settling estimate, so it is a valid
        # bound only above the acquisition threshold. Near threshold
        # acquisition
        # time grows past it: measured, one BPSK trial at Es/N0 = 2 dB took
        # 1.29x
        # budget, while at >= 8 dB the worst case is 0.43x (carrier) and 0.35x
        # (timing) -- better than two-to-one margin. Asserting the budget
        # across
        # all Es/N0 would fail on that threshold behaviour, which is physics.
        for t in [x for x in locked if x["esn0_db"] >= esn0_spec(x["m"])]:
            assert t["t_lock"] <= t["budget"], (
                f"timing lock {t['t_lock']} > budget {t['budget']} at "
                f"Es/N0 {t['esn0_db']:.0f} dB"
            )
            assert t["c_lock"] <= t["budget"], (
                f"carrier lock {t['c_lock']} > budget {t['budget']} at "
                f"Es/N0 {t['esn0_db']:.0f} dB"
            )
    if noise:
        both = [
            t
            for t in noise
            if t["t_lock"] is not None and t["c_lock"] is not None
        ]
        assert not both, f"{len(both)}/{len(noise)} noise-only trials declared"
    if level:
        for real in (True, False):
            v = [t["evm"] for t in level if t["real"] is real and t["locked"]]
            assert len(v) >= 6, f"level sweep lost trials (real={real})"
            assert max(v) - min(v) < 6.0, (
                f"EVM spread {max(v) - min(v):.1f} dB across three decades of "
                f"input level — the AGC is not normalising"
            )
    for (name, label), (err, counts) in chunk_results.items():
        assert err == 0.0, (
            f"{name} chunked '{label}': max |chunked - whole| = {err:.3e} — "
            f"state that must survive a call boundary did not"
        )
        assert counts, f"{name} chunked '{label}': produced no symbols"
    for name, on, off in tel_rows:
        assert on > 0.35 * off, (
            f"{name}: telemetry costs {100 * (off - on) / off:.0f}% "
            f"({on:.0f} vs {off:.0f} Msamp/s)"
        )

    # ── figure ───────────────────────────────────────────────────────────
    panels = [p for p in ALL_PANELS if p in only]
    ncol = 3 if len(panels) > 2 else len(panels)
    nrow = int(np.ceil(len(panels) / ncol))
    fig, axes = plt.subplots(
        nrow, ncol, figsize=(6.0 * ncol, 4.6 * nrow), squeeze=False
    )
    fig.suptitle(
        "M-PSK receiver performance — random geometry per trial.  "
        "Blue = complex baseband, red = real IF.",
        fontsize=10,
    )

    for ax, name in zip(np.ravel(axes), panels):
        if name == "evm":
            panel_evm(ax, sweep)
        elif name == "ber":
            panel_ber(ax, sweep)
        elif name == "lock":
            panel_lock(ax, sweep)
        elif name == "falsealarm":
            panel_falsealarm(ax, noise, sweep)
        elif name == "invariance":
            panel_invariance(ax, sweep)
        elif name == "level":
            panel_level(ax, level)
        elif name == "chunking":
            panel_chunking(ax, chunk_results)
        elif name == "telemetry":
            panel_telemetry(ax, tel_rows)
    for ax in np.ravel(axes)[len(panels) :]:
        ax.axis("off")

    fig.tight_layout(rect=(0, 0, 1, 0.955))
    fig.savefig(out_path, dpi=115)
    print(f"wrote {out_path}")

    if locked:
        print(
            f"\n{len(locked)}/{len(sweep)} random trials locked; "
            f"median EVM margin to the bound "
            f"{np.median([t['evm'] + t['esn0_db'] for t in locked]):+.1f} dB"
        )
    for name, on, off in tel_rows:
        print(
            f"{name}: {off:.0f} Msamp/s detached, {on:.0f} attached "
            f"({100 * (off - on) / off:+.0f}%)"
        )


if __name__ == "__main__":
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "out", nargs="?", default="mpsk_receiver_performance_demo.png"
    )
    ap.add_argument(
        "--only",
        default=",".join(GALLERY_PANELS),
        help=(
            f"comma-separated subset of {','.join(ALL_PANELS)} "
            f"(default {','.join(GALLERY_PANELS)}; 'all' for every panel)"
        ),
    )
    ap.add_argument(
        "--trials", type=int, default=6, help="trials per (Es/N0, path) cell"
    )
    a = ap.parse_args()
    sel = tuple(p.strip() for p in a.only.split(",") if p.strip())
    if sel == ("all",):
        sel = ALL_PANELS
    bad = set(sel) - set(ALL_PANELS)
    if bad:
        sys.exit(f"unknown panel(s): {', '.join(sorted(bad))}")
    main(a.out, only=sel, trials=a.trials)
