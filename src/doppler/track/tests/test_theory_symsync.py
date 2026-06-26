"""Theoretical-correctness tests for the SymbolSync (Gardner) timing loop.

Validates the Gardner timing-error detector:
  * the open-loop S-curve matches a semi-analytic Gardner reference; it is
    one period per symbol with two zeros a half apart (stable + unstable
    null);
  * the timing-error variance is a data-pattern self-noise floor (present at
    infinite SNR) plus an AWGN term that grows as 1/SNR (+ 1/SNR²).
"""

import numpy as np

from doppler.track import SymbolSync

SPS = 8
BETA = 0.5


def _rc(t):
    t = np.asarray(t, float)
    s = np.sinc(t / SPS)
    d = 1 - (2 * BETA * t / SPS) ** 2
    c = np.cos(np.pi * BETA * t / SPS)
    with np.errstate(divide="ignore", invalid="ignore"):
        s = s * np.where(np.abs(d) < 1e-8, np.pi / 4, c / d)
    return s


def _train(a, offset):
    n = len(a) * SPS
    s = np.zeros(n)
    span = 8 * SPS
    for k, ak in enumerate(a):
        c = k * SPS + offset
        idx = np.arange(max(0, int(c - span)), min(n, int(c + span)))
        s[idx] += ak * _rc(idx - c)
    return s


def _measured_scurve(offsets, nsym=600, seed=1):
    a = np.random.default_rng(seed).integers(0, 2, nsym) * 2 - 1
    out = np.empty(len(offsets))
    for i, off in enumerate(offsets):
        ss = SymbolSync(sps=SPS, bn=1e-7, zeta=0.707, order="cubic")
        x = _train(a, off).astype(np.complex64)
        e = []
        for j in range(0, len(x), SPS):
            ss.steps(x[j : j + SPS])
            e.append(ss.timing_error)
        out[i] = np.mean(e[len(e) // 2 :])
    # median-filter the lone group-delay artifact
    return np.array(
        [
            np.median(np.take(out, range(i - 1, i + 2), mode="wrap"))
            for i in range(len(out))
        ]
    )


def _reference_scurve(offsets, nsym=600, seed=1):
    a = np.random.default_rng(seed).integers(0, 2, nsym) * 2 - 1
    n = nsym * SPS
    grid = np.arange(n)
    out = np.empty(len(offsets))
    for i, off in enumerate(offsets):
        s = _train(a, off)
        ks = np.arange(2, nsym - 1)
        on = np.interp(ks * SPS, grid, s)
        onm1 = np.interp((ks - 1) * SPS, grid, s)
        mid = np.interp(ks * SPS - SPS // 2, grid, s)
        out[i] = np.mean(mid * (on - onm1))
    return out


def test_scurve_matches_gardner_reference_shape():
    offs = np.linspace(0, SPS, 64, endpoint=False)
    meas = _measured_scurve(offs)
    ref = _reference_scurve(offs)
    meas /= np.max(np.abs(meas))
    ref /= np.max(np.abs(ref))
    # best circular alignment (absorb the constant group-delay offset)
    best = max(
        np.corrcoef(meas, np.roll(ref, s))[0, 1] for s in range(len(ref))
    )
    assert best > 0.99


def test_scurve_has_two_zeros_half_symbol_apart():
    offs = np.linspace(0, SPS, 128, endpoint=False)
    meas = _measured_scurve(offs, nsym=600)
    # smooth the narrow group-delay artifact before locating zeros
    k = np.ones(5) / 5
    meas = np.convolve(np.r_[meas[-2:], meas, meas[:2]], k, "valid")
    raw = offs[np.where(np.diff(np.sign(meas)) != 0)[0]] / SPS
    # merge crossings within 0.1 symbol (artifact wiggle) into clusters
    clusters = []
    for c in raw:
        if not clusters or c - clusters[-1] > 0.1:
            clusters.append(c)
    assert len(clusters) == 2
    assert abs((clusters[1] - clusters[0]) - 0.5) < 0.07


def _ted_var(snr_db, nsym=3000, seed=3):
    a = np.random.default_rng(seed).integers(0, 2, nsym) * 2 - 1
    x = _train(a, 0.0)
    if snr_db is not None:
        rng = np.random.default_rng(seed + 1)
        p = np.sqrt(np.mean(np.abs(x) ** 2))
        std = np.sqrt(10 ** (-snr_db / 10)) * p
        x = (
            x
            + rng.normal(0, std / np.sqrt(2), len(x))
            + 1j * rng.normal(0, std / np.sqrt(2), len(x))
        )
    ss = SymbolSync(sps=SPS, bn=0.01, zeta=0.707, order="cubic")
    x = x.astype(np.complex64)
    e = []
    for j in range(0, len(x), SPS):
        ss.steps(x[j : j + SPS])
        e.append(ss.timing_error)
    return np.var(e[len(e) // 2 :])


def test_self_noise_floor_exists():
    # the Gardner TED has a data-pattern self-noise floor at infinite SNR
    floor = _ted_var(None)
    assert 0.0 < floor < 0.2


def test_variance_grows_as_snr_drops_and_floor_plus_awgn_fits():
    snr_db = np.array([20, 15, 12, 9, 6, 3])
    var = np.array([_ted_var(s) for s in snr_db])
    floor = _ted_var(None)
    assert np.all(np.diff(var) > 0)  # monotonic decrease with SNR
    assert np.all(var > floor)
    snr = 10 ** (snr_db / 10)
    A = np.vstack([1 / snr, 1 / snr**2]).T
    ab, *_ = np.linalg.lstsq(A, var - floor, rcond=None)
    model = floor + A @ ab
    assert np.max(np.abs(model - var) / var) < 0.1  # floor + AWGN model fits
