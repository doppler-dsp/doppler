"""Streaming acquisition tests for ``doppler.dsss.Acquisition``.

Drives the engine with a realistic receive stream — silence (AWGN) of varying
length, then a burst of repeated BPSK PN segments at an unknown code phase and
carrier (Doppler) offset, then an appended DSSS payload using a *different*
data code, then trailing silence — and checks the **number of detection hits
against a derived expectation**, plus the physics auto-config math (C/N0 → snr,
the chosen coherent depth ``doppler_bins``, threshold, non-coherent looks).

The engine ingests **raw** cf32 and applies the slow-time Doppler FFT
internally, so (unlike the CorrDetector2D demo) these builders never call
``np.fft``.

Construction is physics-only: ``Acquisition`` derives the search grid from
``(reps, spc, chip_rate, cn0_dbhz, pfa, pd, doppler_uncertainty)``.  The
streaming tests pin the coherent depth to ``reps`` with a deliberately low
sizing ``cn0_dbhz`` (so ``doppler_bins == reps``); the injected burst is much
stronger than that sizing, so it clears the gate deterministically. The
resulting grid (doppler_bins 16, code_bins NX, n == N) matches the demo's fixed
grid, so the hit-count algebra below is unchanged.
Because the sizing ``cn0_dbhz`` cannot meet ``pd`` on that grid, construction
emits an under-powered ``UserWarning`` — filtered in the streaming helper.

Frame-grid geometry (the basis for the expected hit counts)
-----------------------------------------------------------
The engine frames on a fixed grid anchored at stream sample 0: frame ``g``
covers ``[g*N, (g+1)*N)``.  A frame is *full-gain* only if its whole window
lies inside the burst, so for a burst of ``R`` segments (``R*NX`` samples)
preceded by ``l_pre`` silence samples the full-gain frame count is::

    F = (l_pre + R * NX) // N - ceil(l_pre / N)

Each full frame yields one dump (one hit).  A non-aligned ``l_pre`` also shifts
the apparent code phase by ``l_pre % NX`` — the silence offset is extra
propagation delay — so the true cell is ``(CODE_PHASE + l_pre) % NX``. Boundary
frames straddling the burst edge can add up to two extra hits at strong SNR;
aligned bursts give exactly ``F``.
"""

import math
import warnings

import numpy as np
import pytest

from doppler.detection import (
    det_pd,
    det_threshold,
    det_threshold_noncoherent,
)
from doppler.dsss import Acquisition
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
CHIP_RATE = 1.0e6  # Hz; fs = CHIP_RATE * SPS = 4 MHz
REPS = NY  # 16 — pins doppler_bins to reps in the streaming tests
CN0_SIZE = 30.0  # dB-Hz; deliberately low so doppler_bins exhausts to reps
R_SEG = 6 * NY  # burst length in segments -> R_SEG*NX = 6*N samples
N_PAY_SYM = 160  # payload data symbols (one segment each)
SQRT_2_OVER_PI = math.sqrt(2.0 / math.pi)


def _cn0_for_snr(snr_amp, fs):
    """C/N0 (dB-Hz) yielding per-sample amplitude SNR ``snr_amp`` at ``fs``."""
    return 10.0 * math.log10(snr_amp**2 * fs)


def _stream_acq(**kw):
    """Construct with doppler_bins pinned to REPS (low sizing C/N0).

    The conservative sizing C/N0 can't meet pd on the reps-deep grid, so an
    "under-powered" UserWarning is expected here — the injected burst is far
    stronger.  Filtered so the streaming assertions stay clean.
    """
    with warnings.catch_warnings():
        warnings.simplefilter("ignore", UserWarning)
        a = Acquisition(
            CODE,
            reps=REPS,
            spc=SPS,
            chip_rate=CHIP_RATE,
            cn0_dbhz=CN0_SIZE,
            pfa=PFA,
            pd=PD,
            **kw,
        )
    # Geometry must match the demo's fixed grid for the hit-count algebra.
    assert (a.code_bins, a.doppler_bins) == (NX, NY)
    assert a.code_bins * a.doppler_bins == N
    return a


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


# ── Physics auto-config math ─────────────────────────────────────────────────


@pytest.mark.parametrize(
    "cn0_dbhz, want_db", [(65.0, 1), (57.0, 2), (53.0, 4)]
)
def test_config_physics(cn0_dbhz, want_db):
    """C/N0 → snr, smallest coherent depth meeting Pd, and grid math."""
    a = Acquisition(
        CODE,
        reps=16,
        spc=SPS,
        chip_rate=CHIP_RATE,
        cn0_dbhz=cn0_dbhz,
        pfa=PFA,
        pd=PD,
    )
    # Physical read-backs.
    assert a.sf == SF and a.code_bins == NX
    assert a.fs == pytest.approx(CHIP_RATE * SPS)
    assert a.doppler_span_hz == pytest.approx(CHIP_RATE / (2 * SF))
    assert a.doppler_res_hz == pytest.approx(CHIP_RATE / (SF * a.doppler_bins))

    # The engine picks the *smallest* coherent depth meeting Pd (min latency).
    assert a.doppler_bins == want_db
    assert not a.underpowered and a.pd_predicted >= PD

    nx, db = a.code_bins, a.doppler_bins
    db * nx
    snr = math.sqrt(10.0 ** (cn0_dbhz / 10.0) / a.fs)

    # Bonferroni over the searched cells (du=0 → all db Doppler bins).
    pfa_cell = 1.0 - (1.0 - PFA) ** (1.0 / (db * nx))
    eta = det_threshold(pfa_cell)
    assert a.pfa_cell == pytest.approx(pfa_cell, rel=1e-9)
    assert a.eta == pytest.approx(eta, rel=1e-5)
    assert a.threshold == pytest.approx(eta * SQRT_2_OVER_PI, rel=1e-5)

    # Boundary: db meets Pd; db-1 (at its own threshold) does not.
    assert det_pd(snr, db * nx, eta) >= PD
    if db > 1:
        pc = 1.0 - (1.0 - PFA) ** (1.0 / ((db - 1) * nx))
        assert det_pd(snr, (db - 1) * nx, det_threshold(pc)) < PD
    assert a.pd_predicted == pytest.approx(det_pd(snr, db * nx, eta), rel=1e-5)


def test_d_selection_monotone():
    """Weaker C/N0 needs a deeper coherent grid (more reps)."""
    strong = Acquisition(
        CODE,
        reps=16,
        spc=SPS,
        chip_rate=CHIP_RATE,
        cn0_dbhz=65.0,
        pfa=PFA,
        pd=PD,
    )
    mid = Acquisition(
        CODE,
        reps=16,
        spc=SPS,
        chip_rate=CHIP_RATE,
        cn0_dbhz=53.0,
        pfa=PFA,
        pd=PD,
    )
    assert strong.doppler_bins < mid.doppler_bins
    assert not strong.underpowered and not mid.underpowered


def test_doppler_uncertainty_sharpens_gate():
    """A tighter Doppler prior → fewer cells → lower threshold, higher Pd."""
    # Both are under-powered at this weak C/N0 (the point is the *relative*
    # threshold change from the Doppler prior), so the warning is expected.
    with warnings.catch_warnings():
        warnings.simplefilter("ignore", UserWarning)
        full = Acquisition(
            CODE,
            reps=16,
            spc=SPS,
            chip_rate=CHIP_RATE,
            cn0_dbhz=44.0,
            pfa=PFA,
            pd=PD,
            doppler_uncertainty=0.0,
        )
        narrow = Acquisition(
            CODE,
            reps=16,
            spc=SPS,
            chip_rate=CHIP_RATE,
            cn0_dbhz=44.0,
            pfa=PFA,
            pd=PD,
            doppler_uncertainty=full.doppler_span_hz / 4,
        )
    assert narrow.pfa_cell > full.pfa_cell  # fewer cells → higher per-cell pfa
    assert narrow.eta < full.eta  # ... → lower amplitude threshold
    assert narrow.pd_predicted >= full.pd_predicted

    # Beyond the native span there is nothing to search → rejected.
    with pytest.raises(MemoryError):
        Acquisition(
            CODE,
            reps=4,
            spc=SPS,
            chip_rate=CHIP_RATE,
            cn0_dbhz=50.0,
            pfa=PFA,
            pd=PD,
            doppler_uncertainty=full.doppler_span_hz * 2,
        )


def test_underpowered_warns():
    """An infeasible operating point still builds, but warns + self-flags."""
    with pytest.warns(UserWarning, match="under-powered"):
        a = Acquisition(
            CODE,
            reps=2,
            spc=SPS,
            chip_rate=CHIP_RATE,
            cn0_dbhz=20.0,
            pfa=PFA,
            pd=PD,
        )
    assert a.underpowered and a.pd_predicted < a.pd

    # A comfortably powered build emits no warning.
    with warnings.catch_warnings():
        warnings.simplefilter("error")
        b = Acquisition(
            CODE,
            reps=16,
            spc=SPS,
            chip_rate=CHIP_RATE,
            cn0_dbhz=65.0,
            pfa=PFA,
            pd=PD,
        )
    assert not b.underpowered


# ── Streaming hit counts ─────────────────────────────────────────────────────


@pytest.mark.parametrize("l_pre", [0, N, 2 * N])
def test_stream_aligned_hits(l_pre):
    """Frame-aligned silence: exactly F hits at the unshifted code phase."""
    rng = np.random.default_rng(1234)
    a = _stream_acq()
    stream = _stream(rng, l_pre=l_pre, sigma=_sigma(SNR_DB), with_payload=True)
    true_n, fa_n = _split(_collect(a, rng, stream), CODE_PHASE)
    assert true_n == _full_frames(l_pre)  # aligned -> exact
    assert fa_n <= 2  # payload cross-correlation may leak a little


@pytest.mark.parametrize("l_pre", [250, 661, 1000])
def test_stream_misaligned_hits(l_pre):
    """Non-aligned silence: F..F+2 hits, code phase shifted by l_pre % NX."""
    rng = np.random.default_rng(1234)
    a = _stream_acq()
    cp_expected = (CODE_PHASE + l_pre) % NX
    stream = _stream(rng, l_pre=l_pre, sigma=_sigma(SNR_DB), with_payload=True)
    true_n, fa_n = _split(_collect(a, rng, stream), cp_expected)
    f = _full_frames(l_pre)
    assert f <= true_n <= f + 2
    assert fa_n <= 2


def test_payload_decorrelates():
    """The distinct-code payload does not masquerade as the preamble burst:
    the burst is still detected (== F) and the payload adds no flood of hits at
    the true cell.
    """
    rng = np.random.default_rng(7)
    a = _stream_acq()
    l_pre = N
    with_pay = _stream(
        rng, l_pre=l_pre, sigma=_sigma(SNR_DB), with_payload=True
    )
    true_with, fa_with = _split(_collect(a, rng, with_pay), CODE_PHASE)
    assert true_with == _full_frames(l_pre)  # payload doesn't add true hits
    assert fa_with <= 2  # nor a flood of false ones


def test_state_roundtrip_resume():
    """The serializable (elastic / pure-transducer) face: serialize
    mid-stream, restore into a *fresh* engine, and resume — the concatenated
    detections must match an uninterrupted run (the pod-handoff guarantee).
    Mirrors the C ``acq_run`` round-trip; compares detection cells (Doppler
    bin, code phase), which are exact across the split.
    """
    rng = np.random.default_rng(2024)
    stream = _stream(rng, l_pre=N, sigma=_sigma(SNR_DB), with_payload=True)
    cut = len(stream) // 2

    ref = _stream_acq().push(stream)  # uninterrupted reference

    e1 = _stream_acq()  # split: e1 pushes [:cut], hands its state to e2
    hb = e1.push(stream[:cut])
    blob = e1.get_state()
    assert isinstance(blob, bytes) and len(blob) == e1.state_bytes()
    e2 = _stream_acq()
    e2.set_state(blob)
    hb = hb + e2.push(stream[cut:])

    def cells(hs):
        return [(h[0], h[1]) for h in hs]  # (doppler_bin, code_phase)

    assert cells(hb) == cells(ref)

    with pytest.raises(ValueError):  # size mismatch
        e2.set_state(b"\x00")
    with pytest.raises(TypeError):  # not bytes
        e2.set_state(42)


def test_empirical_pfa():
    """Noise-only stream respects the auto-configured false-alarm budget."""
    rng = np.random.default_rng(99)
    a = _stream_acq()
    frames = 400
    noise = _awgn(rng, frames * N, 1.0).astype(np.complex64)
    hits = _collect(a, rng, noise)
    pfa_emp = len(hits) / frames
    assert pfa_emp < 5e-2  # loose: validates the analytic mean-mode gate


# ── Non-coherent integration ─────────────────────────────────────────────────


def test_config_noncoherent_autosplit():
    """A C/N0 unreachable by coherent reps alone auto-splits into looks."""
    pfa_cell = 1.0 - (1.0 - PFA) ** (1.0 / N)
    eta = det_threshold(pfa_cell)
    cn0 = 42.0  # too weak for 16 coherent reps to reach Pd
    snr = math.sqrt(10.0 ** (cn0 / 10.0) / (CHIP_RATE * SPS))
    a = Acquisition(
        CODE,
        reps=16,
        spc=SPS,
        chip_rate=CHIP_RATE,
        cn0_dbhz=cn0,
        pfa=PFA,
        pd=PD,
        max_noncoh=64,
    )
    assert a.doppler_bins == 16  # coherent grown to the reps ceiling
    assert det_pd(snr, N, eta) < PD  # coherent-only falls short
    assert a.n_noncoh > 1  # ... then non-coherent looks
    # threshold is the order-N_nc Marcum null, not the coherent Rayleigh gate
    assert a.eta_nc == pytest.approx(
        det_threshold_noncoherent(a.pfa_cell, a.n_noncoh), rel=1e-5
    )
    assert a.pd_predicted >= PD  # the split now meets the Pd target
    assert not a.underpowered


def test_config_strong_stays_coherent():
    """A strong target needs no looks: pure-coherent path (N_nc == 1)."""
    a = Acquisition(
        CODE,
        reps=16,
        spc=SPS,
        chip_rate=CHIP_RATE,
        cn0_dbhz=65.0,
        pfa=PFA,
        pd=PD,
        max_noncoh=16,
    )
    assert a.n_noncoh == 1
    assert a.eta_nc == 0.0  # non-coherent gate unused
    assert a.threshold == pytest.approx(a.eta * SQRT_2_OVER_PI, rel=1e-5)


# Sizing that yields doppler_bins == NY with a small auto-split look count
# (n_noncoh == 2), so the burst's frames complete several non-coherent dumps.
NC_CN0 = 45.0
NC_MAX_NONCOH = 4


def _noncoherent_acq():
    a = Acquisition(
        CODE,
        reps=REPS,
        spc=SPS,
        chip_rate=CHIP_RATE,
        cn0_dbhz=NC_CN0,
        pfa=PFA,
        pd=PD,
        max_noncoh=NC_MAX_NONCOH,
    )
    assert a.doppler_bins == NY and a.n_noncoh > 1 and not a.underpowered
    return a


def test_noncoherent_detects_burst():
    """The non-coherent path accumulates looks and fires at the true cell."""
    rng = np.random.default_rng(7)
    a = _noncoherent_acq()
    stream = _stream(rng, l_pre=0, sigma=_sigma(SNR_DB), with_payload=False)
    true_n, fa_n = _split(_collect(a, rng, stream), CODE_PHASE)
    assert true_n >= 1  # the burst is found through the order-N_nc gate
    assert fa_n <= 1  # at most a single boundary straddle


def test_noncoherent_empirical_pfa():
    """Noise-only through the N_nc>1 gate stays within the Pfa budget."""
    rng = np.random.default_rng(99)
    a = _noncoherent_acq()
    nnc = a.n_noncoh
    # ~200 non-coherent dumps of pure noise; expected FA ~ 200 * PFA = 0.2.
    noise = _awgn(rng, nnc * 200 * N, _sigma(SNR_DB))
    fa = len(_collect(a, rng, noise.astype(np.complex64)))
    assert fa <= 4  # generous bound on a ~0.2-expectation Poisson
