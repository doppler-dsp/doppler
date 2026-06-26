"""Streaming acquisition tests for ``doppler.dsss.Acquirer``.

Drives the engine with a realistic receive stream — silence (AWGN) of varying
length, then a burst of repeated BPSK PN segments at an unknown code phase and
carrier (Doppler) offset, then an appended DSSS payload using a *different*
data code, then trailing silence — and checks the **number of detection hits
against a derived expectation**, plus the auto-configured threshold/dwell math.

The engine ingests **raw** cf32 and applies the slow-time Doppler FFT
internally, so (unlike the Detector2D demo) these builders never call
``np.fft``.

Frame-grid geometry (the basis for the expected hit counts)
-----------------------------------------------------------
The engine frames on a fixed grid anchored at stream sample 0: frame ``g``
covers ``[g*N, (g+1)*N)``.  A frame is *full-gain* only if its whole window
lies inside the burst, so for a burst of ``R`` segments (``R*NX`` samples)
preceded by ``l_pre`` silence samples the full-gain frame count is::

    F = (l_pre + R * NX) // N - ceil(l_pre / N)

With ``dwell=1`` each full frame yields one dump (one hit).  A non-aligned
``l_pre`` also shifts the apparent code phase by ``l_pre % NX`` — the silence
offset is extra propagation delay — so the true cell is
``(CODE_PHASE + l_pre) % NX``.  Boundary frames straddling the burst edge can
add up to two extra hits at strong SNR; aligned bursts give exactly ``F``.
"""

import math

import numpy as np
import pytest

from doppler.detection import det_pd, det_threshold
from doppler.dsss import Acquirer
from doppler.examples.detector2d_acq_demo import (
    NX,
    NY,
    SF,
    SPS,
    N,
    carrier_for_bin,
    make_code,
)
from doppler.wfm import PN, dsss_spread, mls_poly

# ── Scenario constants ───────────────────────────────────────────────────────

CODE = make_code()  # preamble PN (length-5 MLS, seed 1)
DATA_CODE = PN(poly=mls_poly(5), seed=2, length=5).generate(SF)  # distinct
U_TRUE = 3  # Doppler bin; f = U_TRUE/N is on-bin (f*N integer)
CODE_PHASE = 37  # integer-sample code delay
SNR_DB = -18.0  # per-sample power SNR; full frames fire reliably (Pd~1)
PFA = 1e-3
PD = 0.9
MIN_SNR_D1 = 0.2  # -> dwell == 1
MIN_SNR_D2 = 0.09  # -> dwell == 2
R_SEG = 6 * NY  # burst length in segments -> R_SEG*NX = 6*N samples
N_PAY_SYM = 160  # payload data symbols (one segment each)
SQRT_2_OVER_PI = math.sqrt(2.0 / math.pi)


def _sigma(snr_db):
    """Per-component noise scale for unit (chip) signal power."""
    return 10.0 ** (-snr_db / 20.0)


def _awgn(rng, n, sigma):
    return (sigma / math.sqrt(2.0)) * (
        rng.standard_normal(n) + 1j * rng.standard_normal(n)
    )


def _carrier(n):
    return np.exp(2j * np.pi * carrier_for_bin(U_TRUE) * np.arange(n))


def _burst():
    """R_SEG repeated oversampled BPSK PN segments, rolled + carrier."""
    s0 = np.repeat(np.where(CODE & 1, -1.0, 1.0), SPS)
    s0d = np.roll(s0, CODE_PHASE)  # circular delay (matches corr2d wrap)
    seg = np.tile(s0d, R_SEG)
    return (seg * _carrier(len(seg))).astype(np.complex64)


def _payload(rng):
    """DSSS payload over the *distinct* DATA_CODE — decorrelates from CODE."""
    bits = rng.integers(0, 2, N_PAY_SYM)
    syms = np.where(bits & 1, -1.0, 1.0).astype(np.complex64)
    chips = dsss_spread(syms, DATA_CODE, SF)
    pay = np.repeat(chips, SPS)
    return (pay * _carrier(len(pay))).astype(np.complex64)


def _stream(rng, *, l_pre, sigma, with_payload):
    parts = [
        _awgn(rng, l_pre, sigma),
        _burst() + _awgn(rng, R_SEG * NX, sigma),
    ]
    if with_payload:
        pay = _payload(rng)
        parts.append(pay + _awgn(rng, len(pay), sigma))
    parts.append(_awgn(rng, N, sigma))
    return np.concatenate(parts).astype(np.complex64)


def _collect(a, rng, stream):
    """Push the whole stream in random chunk sizes (exercises the ring)."""
    hits = []
    i = 0
    while i < len(stream):
        c = int(rng.integers(200, 900))
        hits.extend(a.push(stream[i : i + c]))
        i += c
    return hits


def _split(hits, cp_expected):
    """Partition hits into true-cell vs false-alarm by location (±1 cell)."""
    true_n = fa_n = 0
    for dop, col, _peak, _noise, _stat, _snr in hits:
        d_ok = min((dop - U_TRUE) % NY, (U_TRUE - dop) % NY) == 0
        c_ok = min((col - cp_expected) % NX, (cp_expected - col) % NX) <= 1
        if d_ok and c_ok:
            true_n += 1
        else:
            fa_n += 1
    return true_n, fa_n


def _full_frames(l_pre):
    """Full-gain burst frames F on the sample-0-anchored frame grid."""
    return (l_pre + R_SEG * NX) // N - math.ceil(l_pre / N)


# ── Auto-config math ─────────────────────────────────────────────────────────


@pytest.mark.parametrize(
    "min_snr, want_dwell",
    [(MIN_SNR_D1, 1), (MIN_SNR_D2, 2), (0.3, 1)],
)
def test_config_math(min_snr, want_dwell):
    a = Acquirer(CODE, sf=SF, spc=SPS, ny=NY, pfa=PFA, pd=PD, min_snr=min_snr)
    assert (a.n, a.nx, a.ny) == (N, NX, NY)

    pfa_cell = 1.0 - (1.0 - PFA) ** (1.0 / N)
    eta = det_threshold(pfa_cell)
    assert a.pfa_cell == pytest.approx(pfa_cell, rel=1e-9)
    assert a.eta == pytest.approx(eta, rel=1e-5)
    assert a.threshold == pytest.approx(eta * SQRT_2_OVER_PI, rel=1e-5)

    # dwell is the smallest frame count meeting Pd at min_snr.
    assert a.dwell == want_dwell
    assert det_pd(min_snr, a.dwell * N, eta) >= PD
    if a.dwell > 1:
        assert det_pd(min_snr, (a.dwell - 1) * N, eta) < PD
    assert a.pd_predicted == pytest.approx(
        det_pd(min_snr, a.dwell * N, eta), rel=1e-5
    )


# ── Streaming hit counts ─────────────────────────────────────────────────────


@pytest.mark.parametrize("l_pre", [0, N, 2 * N])
def test_stream_aligned_hits(l_pre):
    """Frame-aligned silence: exactly F hits at the unshifted code phase."""
    rng = np.random.default_rng(1234)
    a = Acquirer(
        CODE, sf=SF, spc=SPS, ny=NY, pfa=PFA, pd=PD, min_snr=MIN_SNR_D1
    )
    assert a.dwell == 1
    stream = _stream(rng, l_pre=l_pre, sigma=_sigma(SNR_DB), with_payload=True)
    true_n, fa_n = _split(_collect(a, rng, stream), CODE_PHASE)
    assert true_n == _full_frames(l_pre)  # aligned -> exact
    assert fa_n <= 2  # payload cross-correlation may leak a little


@pytest.mark.parametrize("l_pre", [250, 661, 1000])
def test_stream_misaligned_hits(l_pre):
    """Non-aligned silence: F..F+2 hits, code phase shifted by l_pre % NX."""
    rng = np.random.default_rng(1234)
    a = Acquirer(
        CODE, sf=SF, spc=SPS, ny=NY, pfa=PFA, pd=PD, min_snr=MIN_SNR_D1
    )
    cp_expected = (CODE_PHASE + l_pre) % NX
    stream = _stream(rng, l_pre=l_pre, sigma=_sigma(SNR_DB), with_payload=True)
    true_n, fa_n = _split(_collect(a, rng, stream), cp_expected)
    f = _full_frames(l_pre)
    assert f <= true_n <= f + 2
    assert fa_n <= 2


def test_stream_dwell2_hits():
    """dwell=2, dump-aligned silence: exactly 3 coherent-dump hits."""
    rng = np.random.default_rng(1234)
    a = Acquirer(
        CODE, sf=SF, spc=SPS, ny=NY, pfa=PFA, pd=PD, min_snr=MIN_SNR_D2
    )
    assert a.dwell == 2
    # l_pre = 2N = one full dump of silence; burst = 6N = 3 clean dumps.
    stream = _stream(rng, l_pre=2 * N, sigma=_sigma(SNR_DB), with_payload=True)
    true_n, fa_n = _split(_collect(a, rng, stream), CODE_PHASE)
    assert true_n == 3
    assert fa_n <= 2


def test_payload_decorrelates():
    """The distinct-code payload does not masquerade as the preamble burst:
    the burst is still detected (>= F) and the payload adds no flood of hits at
    the true cell.
    """
    rng = np.random.default_rng(7)
    a = Acquirer(
        CODE, sf=SF, spc=SPS, ny=NY, pfa=PFA, pd=PD, min_snr=MIN_SNR_D1
    )
    l_pre = N
    with_pay = _stream(
        rng, l_pre=l_pre, sigma=_sigma(SNR_DB), with_payload=True
    )
    true_with, fa_with = _split(_collect(a, rng, with_pay), CODE_PHASE)
    assert true_with == _full_frames(l_pre)  # payload doesn't add true hits
    assert fa_with <= 2  # nor a flood of false ones


def test_empirical_pfa():
    """Noise-only stream respects the auto-configured false-alarm budget."""
    rng = np.random.default_rng(99)
    a = Acquirer(
        CODE, sf=SF, spc=SPS, ny=NY, pfa=PFA, pd=PD, min_snr=MIN_SNR_D1
    )
    frames = 400
    noise = _awgn(rng, frames * N, 1.0).astype(np.complex64)
    hits = _collect(a, rng, noise)
    pfa_emp = len(hits) / frames
    assert pfa_emp < 5e-2  # loose: validates the analytic mean-mode gate
