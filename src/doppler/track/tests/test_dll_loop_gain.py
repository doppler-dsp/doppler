"""The Dll's loop gain: the integral path is a unit-gain actuator (#3234).

``loop_filter_core.h``'s contract is a unit-gain actuator: one unit of the
filter's output moves the code phase one chip per update, the integral term
as the proportional one. The coherent full-epoch loop applied the integrator
as a code-rate ratio, so a unit moved the phase ``sf`` chips: an integral path
``sf`` times the contract's, and the loop ran about ``sf / 3`` times wider
than its ``bn`` (11 at 31 chips, 54 at 127, unstable at 1023).

With the path at unit gain the loop sits near nominal and the closed-loop
ratio reads the detector's slope ``Kd`` alone: ``(4 zeta^2 Kd + 1) /
(4 zeta^2 + 1)`` times ``bn`` (the contract, ``LoopFilter``), independent of
the code length. ``Kd`` is read from the object, open loop and noise free, in
the same configuration, and the prediction is written from it, not fitted.
The slope itself is not unit (1.31 per chip at spacing 0.5 and 4 samples per
chip: the replica's edge is linear over half a chip); that is the next
change, and this one does not touch it.
"""

from __future__ import annotations

import numpy as np
import pytest

from doppler.track import Dll
from doppler.wfm import PN, mls_poly

ZETA = 0.707
FOUR_Z2 = 4 * ZETA**2
SPS = 4
S = 0.02  # chips rms, white per period
#: A tolerance stated from the measurement: four seeds of 20,000 to 60,000
#: periods read the ratio to a sd of 0.03 to 0.06 per seed, and the filter's
#: own discrete excess (one update of delay) is about 5% at this bandwidth.
TOL = 0.12


def _code(n: int) -> np.ndarray:
    return np.asarray(
        PN(poly=mls_poly(n), seed=1, length=n).generate((1 << n) - 1)
    ).astype(np.uint8)


def _slope(n: int, spacing: float) -> float:
    """|de/dtau| per chip at lock, open loop, noise free (frozen loop)."""
    c = _code(n)
    cs = np.where(c & 1, -1.0, 1.0)
    sig = np.tile(np.repeat(cs, SPS).astype(np.complex64), 3)
    e = []
    for tau in (0.01, -0.01):  # the replica offset is minus the delay
        d = Dll(c, SPS, -tau, 1e-9, ZETA, spacing)
        d.steps(sig)
        e.append(d.last_error)
    return abs((e[0] - e[1]) / 0.02)


def _ratio(
    n: int, bn: float, spacing: float, periods: int, seed: int
) -> float:
    """var(code phase) / (2 * bn * s^2) under white per-period delay noise
    of S chips (exact fractional delay: each chip integrated over each
    sample), one ``steps`` per period."""
    c = _code(n)
    length = len(c)
    cs = np.where(c & 1, -1.0, 1.0)
    pre = np.concatenate([[0.0], np.cumsum(cs)])

    def chips_to(x: np.ndarray) -> np.ndarray:  # integral of the code
        k = np.floor(x).astype(np.int64)
        return (
            (k // length) * pre[length]
            + pre[k % length]
            + cs[k % length] * (x - k)
        )

    rng = np.random.default_rng(seed)
    d = Dll(c, SPS, 0.0, bn, ZETA, spacing, 1)
    m = np.arange(length * SPS)
    taus = rng.normal(0, S, periods)
    est = np.empty(periods)
    for k in range(periods):
        lo, hi = m / SPS - taus[k], (m + 1) / SPS - taus[k]
        d.steps((SPS * (chips_to(hi) - chips_to(lo))).astype(np.complex64))
        est[k] = ((d.code_phase + length / 2) % length) - length / 2
    return float(est[periods // 10 :].var() / (2 * bn * S * S))


def _mean_ratio(n: int, bn: float, spacing: float, periods: int) -> float:
    return float(
        np.mean([_ratio(n, bn, spacing, periods, sd) for sd in range(4)])
    )


def _predicted(n: int, spacing: float) -> float:
    return (FOUR_Z2 * _slope(n, spacing) + 1) / (FOUR_Z2 + 1)


@pytest.mark.parametrize(("n", "periods"), [(5, 40000), (7, 20000)])
def test_the_loop_reads_the_slope_alone_at_every_code_length(n, periods):
    """At spacing 0.5: 1.22 at 31 chips and 1.29 at 127 (4 seeds), against
    the prediction from the measured slope (1.21); before the fix 11 and 54."""
    got = _mean_ratio(n, 0.002, 0.5, periods)
    want = _predicted(n, 0.5)
    assert got == pytest.approx(want, rel=TOL), (n, got, want)


def test_the_ratio_does_not_follow_the_code_length():
    short = _mean_ratio(5, 0.002, 0.5, 40000)
    long = _mean_ratio(7, 0.002, 0.5, 20000)
    # 4.7 before the fix (11.4 and 54), the length's own ratio
    assert long / short == pytest.approx(1.0, abs=0.2), (short, long)


def test_the_default_bn_is_stable_at_a_thousand_chips():
    """The default (0.01) at 1023 chips was unstable (the integral path's
    gain of sf put wn near 0.2 per period with one update of delay): no
    caller could have used it. Now it reads the slope alone."""
    got = _ratio(10, 0.01, 0.5, 8000, 1)
    want = _predicted(10, 0.5)
    assert got == pytest.approx(want, rel=0.25), (got, want)


@pytest.mark.xfail(
    raises=AssertionError,
    strict=True,
    reason="doppler#3234: at spacing 0.25 the open-loop slope reads 1.97 per "
    "chip at every offset out to 0.12 chip, and the loop reads 1.0 where "
    "that predicts 1.65: the effective gain is about half the slope's, "
    "unexplained",
)
def test_the_loop_reads_the_slope_alone_at_spacing_a_quarter():
    got = _mean_ratio(7, 0.002, 0.25, 20000)
    want = _predicted(7, 0.25)
    assert got == pytest.approx(want, rel=TOL), (got, want)
