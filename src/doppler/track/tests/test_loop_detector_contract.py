"""The loop-bandwidth contract, held where each loop meets it (doppler#3229).

A loop's ``bn`` is its closed loop's noise bandwidth only if the rest of the
loop has unit gain (the contract stated on ``LoopFilter``): the error detector
reads slope 1 per unit of the filter's error input, and the actuator applies
the control with unit gain, once per update. This matrix holds that in three
parts.

1. The primitives. ``LoopFilter`` and ``StateFilter``, closed around a unit
   gain loop, deliver the bandwidth they state.
2. The detectors. Each error detector that feeds a loop filter reads unit
   slope at its lock point, open loop and noise free, at every signal level;
   how the slope falls with SNR per symbol is held as a statement, not
   hidden.
3. The composition. One closed-loop row per loop object: white phase noise of
   known variance ``s**2`` per update at the input, the variance of the loop's
   own phase estimate against ``2 * bn * t * s**2``. It checks that 1 and 2
   were composed right (units, update period), and is not where the
   contract is held.

Discovery is from the manifests: every object whose constructor takes a loop
bandwidth (``bn``, ``bn_*`` or ``loop_bw``) is in ``COVERED`` or ``GAPS``, or
the test fails, so a new loop with no row cannot land silently. A row whose
object does not meet the contract is ``xfail(strict=True)`` with the issue
that records it: when the object is fixed the row passes, strict xfail turns
that into a failure, and the marker must be deleted (the list only shrinks).
``GAPS`` is the other shrink-only list: loop objects whose rows are not built
yet, each with its reason.

The numbers behind the rows came from this matrix's own probes on main:
``Costas`` and ``CarrierMpsk`` read 1.000 per radian at every level;
``CarrierNda`` reads 2.000 (doppler#3224); the closed loops read 0.99 to 1.02
and 1.00 times ``2 * bn * t * s**2``, and ``CarrierNda``'s, whose loop divides
the raw error by that slope, inside ``BAND``.
"""

from __future__ import annotations

import re
import sys

import numpy as np
import pytest

from doppler.agc import AGC
from doppler.mpsk import mpsk_map
from doppler.tests._repo import repo_root
from doppler.track import (
    CarrierMpsk,
    CarrierNda,
    Costas,
    Dll,
    LoopFilter,
    RateSync,
    StateFilter,
)
from doppler.wfm import PN, mls_poly, rrc_h

if sys.version_info >= (3, 11):
    import tomllib
else:  # pragma: no cover
    import tomli as tomllib

ROOT = repo_root(__file__)

#: Objects with a row below.
COVERED = {
    "loop_filter",
    "state_filter",
    "costas",
    "carrier_mpsk",
    "carrier_nda",
    "dll",
    "agc",
    "ratesync",
    "mpsk_receiver",
}

#: Rows of a covered object that are not built: (object, row) -> the issue.
#: Shrink-only, like GAPS.
ROW_GAPS = {
    ("ratesync", "closed loop"): "doppler#3246",
    ("mpsk_receiver", "closed loops"): "doppler#3247",
}

#: Objects whose constructor takes `bn_fll` (a first-order frequency-assist
#: loop whose bandwidth is mu / 4): each needs a row, and has an issue.
FLL_GAPS = {
    "costas": "doppler#3244",
    "carrier_mpsk": "doppler#3245",
    "despreader": "doppler#3242",
}

#: Loop objects with no row yet: object -> why and the issue. Shrink-only:
#: delete an entry when its row lands; an entry for an object that is no
#: longer a loop fails.
GAPS = {
    "symsync": (
        "its detector is not normalised by its own slope, so its bandwidth "
        "runs 0.54x to 1.99x of bn across roll-off (doppler#3238); the row "
        "needs the loop's matched-cascade harness, which that issue's "
        "measurement used"
    ),
    "burst_despreader": (
        "an envelope code detector and a Costas-form carrier detector with "
        "no detector read on the Python face (doppler#3241)"
    ),
    "despreader": (
        "composes Dll and Costas, so its detector rows are theirs; its "
        "composition row is not built (doppler#3242)"
    ),
    "state_filter_kalman": (
        "drives StateFilter with its own detector's error; the filter's row "
        "is held, the gain schedule's is not (doppler#3243)"
    ),
}


def _discover() -> set[str]:
    """Objects whose constructor takes a loop bandwidth."""
    found = set()
    for f in sorted((ROOT / "objects").glob("*.toml")):
        for obj, table in tomllib.loads(f.read_text(encoding="utf-8")).items():
            if not isinstance(table, dict):
                continue
            names = [p.get("name", "") for p in table.get("init_params") or []]
            if any(re.fullmatch(r"bn(_[a-z]+)?|loop_bw", n) for n in names):
                found.add(obj)
    return found


def test_every_loop_object_has_a_row_or_a_declared_gap():
    found = _discover()
    assert found - COVERED - set(GAPS) == set(), (
        "a loop object with no row: add one, or declare the gap with its "
        f"reason: {sorted(found - COVERED - set(GAPS))}"
    )
    assert COVERED - found == set(), sorted(COVERED - found)
    assert set(GAPS) - found == set(), sorted(set(GAPS) - found)
    assert COVERED & set(GAPS) == set()


def _discover_fll() -> set[str]:
    found = set()
    for f in sorted((ROOT / "objects").glob("*.toml")):
        for obj, table in tomllib.loads(f.read_text(encoding="utf-8")).items():
            if not isinstance(table, dict):
                continue
            names = [p.get("name", "") for p in table.get("init_params") or []]
            if "bn_fll" in names:
                found.add(obj)
    return found


def test_every_frequency_assist_has_a_declared_first_order_gap():
    """``bn_fll`` is a first-order loop's bandwidth (``k_fll = 4 * bn_fll``);
    none has its impulse-response row yet, so each is declared."""
    assert _discover_fll() == set(FLL_GAPS), (
        sorted(_discover_fll()),
        sorted(FLL_GAPS),
    )


def test_a_row_gap_names_a_covered_object():
    assert {o for o, _ in ROW_GAPS} <= COVERED


# ── 1. The primitives ────────────────────────────────────────────────────


def _unit_gain_bandwidth(step, n=60000):
    """Noise bandwidth of the loop ``error = in - out; out += step(error)``,
    cycles per update, from the impulse response (sum h**2 / 2)."""
    out = 0.0
    h = np.empty(n)
    for i in range(n):
        out += step((1.0 if i == 0 else 0.0) - out)
        h[i] = out
    return float(np.sum(h * h) / 2.0)


@pytest.mark.parametrize("bn", [0.002, 0.01])
def test_loop_filter_delivers_bn_in_a_unit_gain_loop(bn):
    """The promise, with the stated excess: the delivered bandwidth is
    wide by about 16 zeta^2 / (4 zeta^2 + 1)^2 * bn (loop_filter_core.h)."""
    got = _unit_gain_bandwidth(LoopFilter(bn=bn, zeta=0.707, t=1.0).step)
    z2 = 0.707**2
    want = bn * (1 + 16 * z2 / (4 * z2 + 1) ** 2 * bn)
    assert got == pytest.approx(want, rel=0.003), (bn, got, want)


@pytest.mark.parametrize("bn", [0.002, 0.01])
def test_state_filter_delivers_bn_exactly_in_a_unit_gain_loop(bn):
    """Its gains are solved so the exact discrete bandwidth is bn * t."""
    got = _unit_gain_bandwidth(StateFilter(bn=bn, t=1.0).step)
    assert got == pytest.approx(bn, rel=0.003), (bn, got)


# ── 2. The detectors: unit slope at lock, at every level ─────────────────

LEVELS = (0.1, 1.0, 10.0)
H = 1e-3


def _slope(error, **kw):
    return (error(H, **kw) - error(-H, **kw)) / (2 * H)


def _costas_error(phi, *, a=1.0):
    c = Costas(bn=1e-6, zeta=0.707, init_norm_freq=0.0, tsamps=1, bn_fll=0.0)
    c.steps(np.array([a * np.exp(1j * phi)], np.complex64))
    return c.last_error


@pytest.mark.parametrize("a", LEVELS)
def test_costas_detector_reads_unit_slope_at_every_level(a):
    assert _slope(_costas_error, a=a) == pytest.approx(1.0, abs=1e-3)


def _mpsk_error(phi, *, m, a):
    pt = mpsk_map(np.array([0], np.uint8), m)[0]
    c = CarrierMpsk(
        bn=1e-6, zeta=0.707, init_norm_freq=0.0, tsamps=1, bn_fll=0.0, m=m
    )
    c.steps(np.array([a * pt * np.exp(1j * phi)], np.complex64))
    return c.last_error


@pytest.mark.parametrize("a", LEVELS)
@pytest.mark.parametrize("m", [2, 4, 8])
def test_carrier_mpsk_detector_reads_unit_slope_at_every_level(m, a):
    assert _slope(_mpsk_error, m=m, a=a) == pytest.approx(1.0, abs=1e-3)


_SPS, _N = 8, 4
_ARM = _SPS // _N


def _nda_error(phi, *, m, a):
    c = CarrierNda(
        bn=1e-9, zeta=0.707, init_norm_freq=0.0, sps=_SPS, n=_N, m=m
    )
    c.steps(np.full(_ARM, a * np.exp(1j * phi), np.complex64))
    return c.last_error


@pytest.mark.xfail(
    raises=AssertionError,
    strict=True,
    reason="doppler#3224: the detector's raw error has slope 2 at every M "
    "and level (test_theory_carrier_nda.py and carrier_nda_core.h pin 2); "
    "the loop divides it by CARRIER_NDA_DISC_SLOPE, so the closed-loop row "
    "passes and this one does not flip",
)
@pytest.mark.parametrize("a", LEVELS)
@pytest.mark.parametrize("m", [2, 4, 8])
def test_carrier_nda_detector_reads_unit_slope_at_every_level(m, a):
    assert _slope(_nda_error, m=m, a=a) == pytest.approx(1.0, abs=1e-3)


# The slope rides on SNR per symbol: the statement, held at two points.

_RNG = np.random.default_rng(7)


def _noisy_slope(make, snr_db, trials=2500, phi=0.04):
    sig = 10 ** (-snr_db / 20)
    acc = 0.0
    for sgn in (1, -1):
        s = 0.0
        for _ in range(trials):
            nz = (_RNG.standard_normal() + 1j * _RNG.standard_normal()) * (
                sig / np.sqrt(2)
            )
            s += make(sgn * phi, nz)
        acc += sgn * s / trials
    return acc / (2 * phi)


def _costas_noisy(phi, nz):
    c = Costas(bn=1e-6, zeta=0.707, init_norm_freq=0.0, tsamps=1, bn_fll=0.0)
    sym = _RNG.choice([-1.0, 1.0])
    c.steps(np.array([sym * np.exp(1j * phi) + nz], np.complex64))
    return c.last_error


def test_costas_slope_falls_with_snr_per_decision():
    """Unit at high SNR; low at 0 dB, where decisions are wrong."""
    assert _noisy_slope(_costas_noisy, 30.0) == pytest.approx(1.0, abs=0.03)
    assert _noisy_slope(_costas_noisy, 0.0) < 0.8


# ── 3. The composition: injected phase noise, closed loop ────────────────

S = 0.02  # rms phase noise per update, radians: small against the S-curve
P = 100000


def _phase_ratio(make, arm, a, bn, seed):
    """Var of the loop's phase estimate over 2 * bn * t * s**2, white noise
    per update (t = ``arm`` samples), estimate = input phase - residual."""
    rng = np.random.default_rng(seed)
    phi = rng.normal(0, S, P)
    y = np.asarray(
        make().steps(np.repeat(a * np.exp(1j * phi), arm).astype(np.complex64))
    )
    n = len(y) // arm
    resid = np.angle((y[: n * arm].reshape(-1, arm) * np.conj(a)).mean(axis=1))
    theta = (phi[:n] - resid)[n // 10 :]
    return float(theta.var() / (2 * bn * arm * S * S))


def _mean_ratio(*args):
    return float(np.mean([_phase_ratio(*args, seed=sd) for sd in range(3)]))


BN = 0.005
BAND = (0.93, 1.07)


def test_costas_closed_loop_reads_the_stated_bandwidth():
    r = _mean_ratio(
        lambda: Costas(
            bn=BN, zeta=0.707, init_norm_freq=0.0, tsamps=1, bn_fll=0.0
        ),
        1,
        1.0,
        BN,
    )
    assert BAND[0] < r < BAND[1], r


@pytest.mark.parametrize("m", [2, 4, 8])
def test_carrier_mpsk_closed_loop_reads_the_stated_bandwidth(m):
    pt = mpsk_map(np.array([0], np.uint8), m)[0]
    r = _mean_ratio(
        lambda: CarrierMpsk(
            bn=BN, zeta=0.707, init_norm_freq=0.0, tsamps=1, bn_fll=0.0, m=m
        ),
        1,
        pt,
        BN,
    )
    assert BAND[0] < r < BAND[1], (m, r)


@pytest.mark.parametrize("m", [2, 4, 8])
def test_carrier_nda_closed_loop_reads_the_stated_bandwidth(m):
    """What the reading should be is not fitted. The detector's raw error
    has slope 2 (the open-loop row above), and the loop divides it by
    ``CARRIER_NDA_DISC_SLOPE`` before the filter (``carrier_nda_loop_error``,
    doppler#3224), so the loop gain is K = 1 and by the contract
    (``LoopFilter``) the bandwidth is ``bn``. The row divides the variance
    by ``2 * bn * t * s^2`` with t = the arm length, 2 samples, the loop's
    update period, so it reads 1 plus the small discrete excess the filter
    header states (about ``16*zeta^2/(4*zeta^2+1)^2 * bn*t`` = 0.9% at
    bn*t = 0.01). Before the division the same row read 1.70, the contract's
    5/3 at K = 2 plus that excess."""
    r = _mean_ratio(
        lambda: CarrierNda(
            bn=BN, zeta=0.707, init_norm_freq=0.0, sps=_SPS, n=_N, m=m
        ),
        _ARM,
        1.0,
        BN,
    )
    assert BAND[0] < r < BAND[1], (m, r)


# The DLL: a code loop, timing noise in chips in place of phase noise.

_N_PN = 7
_L = (1 << _N_PN) - 1
_CODE = np.asarray(
    PN(poly=mls_poly(_N_PN), seed=1, length=_N_PN).generate(_L)
).astype(np.uint8)
_CS = np.where(_CODE & 1, -1.0, 1.0)
_PRE = np.concatenate([[0.0], np.cumsum(_CS)])


def _chip_integral(x):
    """Integral of the periodic chip sequence from 0 to x chips."""
    k = np.floor(x).astype(np.int64)
    return (k // _L) * _PRE[_L] + _PRE[k % _L] + _CS[k % _L] * (x - k)


def _dll_ratio(bn, segments, sps=4, spacing=0.5, s=0.02, periods=8000):
    """var(code phase) / (2 * bn * s**2): the code delayed per period by
    white noise of s chips, exact fractional delay by integrating each chip's
    overlap with each sample."""
    rng = np.random.default_rng(11)
    d = Dll(_CODE, sps, 0.0, bn, 0.707, spacing, segments)
    n = np.arange(_L * sps)
    taus = rng.normal(0, s, periods)
    est = np.empty(periods)
    for k in range(periods):
        lo, hi = n / sps - taus[k], (n + 1) / sps - taus[k]
        d.steps(
            (sps * (_chip_integral(hi) - _chip_integral(lo))).astype(
                np.complex64
            )
        )
        est[k] = ((d.code_phase + _L / 2) % _L) - _L / 2
    return float(est[periods // 10 :].var() / (2 * bn * s * s))


def test_dll_slope_is_level_free_and_has_the_documented_sign():
    """Noise free, the discriminator divides by the prompt power, so the
    slope does not depend on level; it is restoring (negative)."""
    sps = 8
    slopes = []
    for a in LEVELS:
        sig = a * np.tile(np.repeat(_CS, sps).astype(np.complex64), 3)
        e = []
        for tau in (0.01, -0.01):
            # the code delayed by tau chips, replica at 0: init_chip is the
            # replica's offset, so a delay of tau is an init_chip of -tau
            d = Dll(_CODE, sps, -tau, 1e-7, 0.707, 0.5)
            d.steps(sig)
            e.append(d.last_error)
        slopes.append((e[0] - e[1]) / 0.02)
    assert slopes[0] == pytest.approx(slopes[1], rel=1e-3)
    assert slopes[1] == pytest.approx(slopes[2], rel=1e-3)
    assert slopes[1] > 0  # e > 0 for a delay of +tau with replica offset


@pytest.mark.xfail(
    raises=AssertionError,
    strict=True,
    reason="doppler#3234: at segments = 1 the code loop reads about 50x "
    "its stated bn (127 chips, sps 4)",
)
def test_dll_closed_loop_reads_the_stated_bandwidth_at_one_segment():
    r = _dll_ratio(0.002, segments=1)
    assert BAND[0] < r < BAND[1], r


@pytest.mark.xfail(
    raises=AssertionError,
    strict=True,
    reason="doppler#3234: segmented, the reading is 3 to 4x bn at the "
    "receivers' non-integer samples per chip and intermittent at integer "
    "sps (127 chips, sps 4 here); not settled, held loosely",
)
def test_dll_closed_loop_reads_the_stated_bandwidth_when_segmented():
    r = _dll_ratio(0.002, segments=4)
    assert BAND[0] < r < BAND[1], r


# ── The first-order loop: the AGC ────────────────────────────────────────


@pytest.mark.parametrize("alpha", [0.05, 1.0])
def test_agc_first_order_bandwidth_is_loop_bw(alpha):
    """``gain_db += 4 * loop_bw * error``: a first-order loop, so mu / 4 is
    its noise bandwidth. A 0.5 dB step in the input level, the impulse
    response of the normalised step response, sum h**2 / 2."""
    bw, step_db, n = 0.0025, 0.5, 40000
    a = AGC(ref_db=0.0, loop_bw=bw, alpha=alpha)
    a.steps(np.ones(4000, np.complex64))
    amp = 10 ** (step_db / 20)
    g = np.empty(n)
    for k in range(n):
        a.step(complex(amp, 0))
        g[k] = a.gain_db
    y = np.concatenate([[0.0], -g / step_db])
    got = float(np.sum(np.diff(y) ** 2) / 2)
    assert got == pytest.approx(bw, rel=0.05), (alpha, got)


# ── RateSync: the timing loop's detector ─────────────────────────────────
#
# The method is ``native/validation/ratesync_scurve.c`` phase 3's: the slope
# at the STABLE zero (the eye centre, half a symbol from the stream's origin
# through this cascade), by a central difference of half-step 1/32 symbol
# with the same symbol sequence driving both offsets (so the data-dependent
# part of the S-curve, the transition density, cancels in the difference),
# averaged over ten independent sequences of 4000 symbols, with the standard
# error of the mean beside it. A single sequence is a draw: one seed reads
# 0.89 where the ten-seed mean reads 0.98.

_SPAN = 8
_SPS = 4
_NSYM = 4000
_SEEDS = tuple(7 + 1000 * i for i in range(10))
_DTAU = 0.03125


def _rrc_stream(syms, tau, beta, amp):
    """RRC-shaped BPSK at 4 samples per symbol, timing offset ``tau``
    symbols, symbol amplitude ``amp``."""
    taps = rrc_h(
        (np.arange(-_SPAN * _SPS, _SPAN * _SPS + 1) / _SPS) - tau, beta
    )
    up = np.zeros(len(syms) * _SPS)
    up[::_SPS] = syms
    return (amp * np.convolve(up, taps)).astype(np.complex64)


def _ratesync_error(ted, beta, amp, tau, syms):
    """Mean ``timing_error`` after the first 300 symbols, read every 4
    samples, loop frozen (bn = 1e-9) so the strobe stays where it started."""
    rs = RateSync(
        sps=4.0, pulse="rrc", beta=beta, span=_SPAN, m=2, bn=1e-9, ted=ted
    )
    x = _rrc_stream(syms, tau, beta, amp)
    acc = []
    for k in range(0, len(x) - 4, 4):
        rs.steps(x[k : k + 4])
        if k > 300 * _SPS:
            acc.append(rs.timing_error)
    return float(np.mean(acc))


def _ratesync_slope(ted, beta, amp, seeds=_SEEDS, zero=0.5):
    """(mean, standard error) over ``seeds`` of the paired slope magnitude
    per symbol at the stable zero."""
    v = []
    for seed in seeds:
        syms = np.where(
            np.random.default_rng(seed).integers(0, 2, _NSYM) > 0, 1.0, -1.0
        )
        e = [
            _ratesync_error(ted, beta, amp, zero + s, syms)
            for s in (_DTAU, -_DTAU)
        ]
        v.append(-(e[0] - e[1]) / (2 * _DTAU))
    v = np.asarray(v)
    return float(v.mean()), float(v.std(ddof=1) / np.sqrt(len(v)))


@pytest.mark.parametrize("beta", [0.1, 0.35, 0.9])
@pytest.mark.parametrize("ted", ["gardner", "dttl"])
def test_ratesync_detector_reads_unit_slope_at_unit_amplitude(ted, beta):
    """The TED's raw output is divided by its own slope against the pulse
    (``ted_scale``), so at unit symbol amplitude the loop sees slope 1.

    Measured here (mean +- standard error over ten sequences): Gardner
    0.90 +- 0.03 at beta 0.1, 0.98 +- 0.01 at 0.35, 0.99 +- 0.005 at 0.9;
    DTTL 0.986 +- 0.004 at every roll-off. The validator reads Gardner
    0.9447 at beta 0.1, where the pulse's tails are truncated hardest (by
    this harness and by ``dp_symsync_ted_slope`` alike), and DTTL 0.9998 to
    1.0013. So the tolerance is the validator's own, set from measurement:
    0.05 everywhere, except Gardner at beta 0.1, which is held to the
    validator's documented shortfall (0.85 to 1.05) and not hidden."""
    mean, se = _ratesync_slope(ted, beta, 1.0)
    assert se < 0.03, (ted, beta, se)  # the tolerance is not inside the noise
    low = 0.85 if (ted, beta) == ("gardner", 0.1) else 0.95
    assert low < mean < 1.05, (ted, beta, mean, se)


@pytest.mark.parametrize("amp", [0.25, 4.0])
@pytest.mark.parametrize(
    ("ted", "power"), [("gardner", 2), ("dttl", 1)], ids=["gardner", "dttl"]
)
def test_ratesync_slope_rides_on_level(ted, power, amp):
    """The statement, as numbers: the slope goes as A^2 (Gardner) or A
    (DTTL), so the timing bandwidth is the stated one only at unit symbol
    amplitude. A quarter-amplitude stream runs a Gardner loop at 1/16 of
    ``bn`` (test_ratesync.py records the instance); the AGC in front is the
    caller's, and MpskReceiver has one."""
    seeds = _SEEDS[:4]
    ref, _ = _ratesync_slope(ted, 0.35, 1.0, seeds)
    got, _ = _ratesync_slope(ted, 0.35, amp, seeds)
    assert got / ref == pytest.approx(amp**power, rel=0.05), (ted, amp)


# ── MpskReceiver: the detectors are those functions ──────────────────────


def test_mpsk_receiver_calls_the_detectors_the_matrix_holds():
    """The receiver has no detector of its own: its carrier error is
    ``carrier_nda_disc`` (CarrierNda's rows above) and its timing is
    ``ratesync_loop_take_output`` (RateSync's). If it grows another, the
    matrix has a detector it does not hold."""
    text = (
        ROOT / "native/inc/doppler/mpsk_receiver/mpsk_rx_loops.h"
    ).read_text(encoding="utf-8")
    assert "carrier_nda_disc (z, l->m, &pe, &lk)" in text
    assert "ratesync_loop_take_output (&l->timing, y, &on, ted)" in text
    others = re.findall(
        r"\b(dp_costas_[a-z_]+|dp_dll_[a-z_]+|dp_symsync_[a-z_]+)\s*\(", text
    )
    assert not others, others
