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
import time
import warnings

import numpy as np
import pytest

from doppler.detection import (
    det_pd,
    det_threshold,
    det_threshold_noncoherent,
    marcum_q,
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


# Expected depths are sized at the AVERAGE Pd over the straddle priors
# (Doppler scalloping, intra-segment rotation, code sample offset), so
# the engine buys enough coherent depth to meet pd in operation — not on
# the grid's best case, and not at the mean amplitude (which Jensen
# makes optimistic).
@pytest.mark.parametrize(
    "cn0_dbhz, want_db", [(65.0, 1), (57.0, 4), (53.0, 9)]
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

    # Boundary/minimality through the engine itself: capping reps at
    # db - 1 forces the next-smaller grid, which cannot meet pd (the
    # sizing quadrature lives in C; replicating it here would just be a
    # copy). Sanity-bracket pd_predicted instead of recomputing it: it is
    # the straddle-AVERAGED Pd, so it sits at or above pd and strictly
    # below the on-grid best case.
    assert PD <= a.pd_predicted < det_pd(snr, db * nx, eta)
    assert 0.0 < a.straddle_loss < 1.0
    if db > 1:
        with warnings.catch_warnings():
            warnings.simplefilter("ignore", UserWarning)
            smaller = Acquisition(
                CODE,
                reps=db - 1,
                spc=SPS,
                chip_rate=CHIP_RATE,
                cn0_dbhz=cn0_dbhz,
                pfa=PFA,
                pd=PD,
            )
        assert smaller.underpowered
        assert smaller.pd_predicted < PD


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
# 46 dB-Hz: the weakest point where nc <= 4 non-coherent looks meet
# pd = 0.9 at the straddle-AVERAGED Pd (45 dB-Hz needs nc = 5; the old
# mean-amplitude sizing called 45/nc=4 powered when its true average Pd
# was 0.849 — the honest criterion moved the boundary, not the physics).
NC_CN0 = 46.0
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


# ── Symbol-rate-aware sizing (continuous, data-modulated signals) ───────────
#
# See docs/gallery/dsss-acq-async-data.md: a data-bit transition landing
# mid-coherent-epoch splits the coherent sum into two oppositely-signed
# partial segments, a self-cancellation loss the physics above (Doppler
# scalloping / intra-segment rotation / code-phase straddle) knows nothing
# about. ``symbol_rate`` gives the engine the missing piece (the data clock)
# so it can jointly search (doppler_bins, n_noncoh) pricing that loss
# honestly, instead of the old "set reps=1 by hand" workaround.


def test_symbol_rate_default_disabled():
    """symbol_rate defaults to 0 -- the legacy single-axis search, byte
    -identical to every pre-existing test in this file (none of which pass
    symbol_rate)."""
    a = Acquisition(
        CODE,
        reps=16,
        spc=SPS,
        chip_rate=CHIP_RATE,
        cn0_dbhz=65.0,
        pfa=PFA,
        pd=PD,
    )
    assert a.symbol_rate == 0.0
    assert a.epochs_per_symbol == 0.0
    assert a.doppler_bins == 1  # matches test_config_physics's cn0=65 case


def test_symbol_rate_joint_search_avoids_pure_max_depth():
    """At a weak, data-modulated margin, the joint search should not simply
    grow doppler_bins to the reps ceiling with n_noncoh == 1 (the old
    single-axis search's behavior) -- self-cancellation makes a large
    coherent-only depth unable to reach pd, so a real look count should
    show up instead, at a bounded coherent depth."""
    symbol_rate = CHIP_RATE / SF / 1.4  # epochs_per_symbol == 1.4
    a = Acquisition(
        CODE,
        reps=16,
        spc=SPS,
        chip_rate=CHIP_RATE,
        cn0_dbhz=46.0,
        pfa=PFA,
        pd=PD,
        max_noncoh=16,
        symbol_rate=symbol_rate,
    )
    assert a.epochs_per_symbol == pytest.approx(1.4)
    assert not a.underpowered
    assert a.n_noncoh > 1
    assert a.doppler_bins < 16  # didn't just exhaust to the ceiling
    assert a.doppler_bins * a.n_noncoh <= 16 * 16


# ``doppler_resolution`` trades the joint search's fewest-total-epochs
# guarantee for a resolution guarantee: it floors doppler_bins at
# ceil(chip_rate/(sf*doppler_resolution)) and takes the first grid (starting
# from that floor) meeting pd, rather than exhaustively searching for the
# global minimum-total grid. This also fixes a real scaling problem: the
# unfloored joint search's own cost is O(reps^3) (every D up to reps pays the
# O(D^2) DFT inside the data-modulation Pd model), so naively raising reps to
# reach a fine resolution made construction take minutes; anchoring the
# search at the floor keeps it fast regardless of reps.


def test_doppler_resolution_default_disabled():
    """doppler_resolution defaults to 0 -- the exhaustive fewest-total-epochs
    search, byte-identical to every other symbol_rate>0 test in this file
    (none of which pass doppler_resolution)."""
    symbol_rate = CHIP_RATE / SF / 1.4
    a = Acquisition(
        CODE,
        reps=16,
        spc=SPS,
        chip_rate=CHIP_RATE,
        cn0_dbhz=46.0,
        pfa=PFA,
        pd=PD,
        max_noncoh=16,
        symbol_rate=symbol_rate,
    )
    assert a.doppler_resolution == 0.0
    assert a.n_noncoh > 1
    assert a.doppler_bins < 16


def test_doppler_resolution_floors_coherent_depth():
    """A resolution finer than the unfloored search would ever pick (it
    favors doppler_bins=1 whenever that meets pd) floors doppler_bins at
    ceil(chip_rate/(sf*doppler_resolution)), and the achieved doppler_res_hz
    meets the target."""
    symbol_rate = CHIP_RATE / SF / 1.4
    res_hz = 100.0
    reps = 400  # comfortably past the floor this res_hz implies
    floor_d = math.ceil(CHIP_RATE / (SF * res_hz))

    a = Acquisition(
        CODE,
        reps=reps,
        spc=SPS,
        chip_rate=CHIP_RATE,
        cn0_dbhz=55.0,
        pfa=PFA,
        pd=PD,
        max_noncoh=8,
        symbol_rate=symbol_rate,
        doppler_resolution=res_hz,
    )
    assert a.doppler_resolution == res_hz
    assert a.doppler_bins >= floor_d
    assert a.doppler_res_hz <= res_hz
    assert not a.underpowered


def test_doppler_resolution_clips_to_reps():
    """A resolution finer than reps can ever deliver clips the floor at reps
    itself, rather than rejecting the request."""
    symbol_rate = CHIP_RATE / SF / 1.4
    a = Acquisition(
        CODE,
        reps=8,
        spc=SPS,
        chip_rate=CHIP_RATE,
        cn0_dbhz=55.0,
        pfa=PFA,
        pd=PD,
        max_noncoh=8,
        symbol_rate=symbol_rate,
        doppler_resolution=1.0,  # would need doppler_bins in the thousands
    )
    assert a.doppler_bins == 8


def test_doppler_resolution_rejects_negative():
    with pytest.raises(MemoryError):
        Acquisition(
            CODE,
            reps=16,
            spc=SPS,
            chip_rate=CHIP_RATE,
            cn0_dbhz=55.0,
            pfa=PFA,
            pd=PD,
            symbol_rate=CHIP_RATE / SF / 1.4,
            doppler_resolution=-1.0,
        )


def test_doppler_resolution_construction_stays_fast_at_large_reps():
    """The actual point of the fix: construction must stay fast even at a
    reps large enough that the unfloored sweep would be O(reps^3) -- this
    measured minutes before the floor-anchored early exit (see
    native/tests/test_acq_core.c's C-level version of this same guard for
    the extrapolated number); a generous ceiling leaves wide margin while
    still catching a regression back to the full sweep."""
    symbol_rate = CHIP_RATE / SF / 1.4
    t0 = time.perf_counter()
    a = Acquisition(
        CODE,
        reps=100,
        spc=SPS,
        chip_rate=CHIP_RATE,
        cn0_dbhz=55.0,
        pfa=PFA,
        pd=PD,
        max_noncoh=8,
        symbol_rate=symbol_rate,
        doppler_resolution=20.0,
    )
    dt = time.perf_counter() - t0
    assert dt < 10.0
    assert a.doppler_bins <= 100


# ``doppler_rate`` caps the coherent-depth ceiling from the opposite
# direction to ``doppler_resolution``'s floor: past
# doppler_bins >= chip_rate/(sf*sqrt(doppler_rate)), in-window Doppler drift
# would smear the FFT peak across a resolution bin.


def test_doppler_rate_default_disabled():
    symbol_rate = CHIP_RATE / SF / 1.4
    a = Acquisition(
        CODE,
        reps=16,
        spc=SPS,
        chip_rate=CHIP_RATE,
        cn0_dbhz=55.0,
        pfa=PFA,
        pd=PD,
        max_noncoh=8,
        symbol_rate=symbol_rate,
    )
    assert a.doppler_rate == 0.0


def test_doppler_rate_rejects_negative():
    with pytest.raises(MemoryError):
        Acquisition(
            CODE,
            reps=16,
            spc=SPS,
            chip_rate=CHIP_RATE,
            cn0_dbhz=55.0,
            pfa=PFA,
            pd=PD,
            symbol_rate=CHIP_RATE / SF / 1.4,
            doppler_rate=-1.0,
        )


def test_doppler_rate_caps_unfloored_search():
    """A tight doppler_rate caps the unfloored search's own range, even
    though reps alone would let it search much deeper."""
    symbol_rate = CHIP_RATE / SF / 1.4
    rate_hz = 1.0e6
    rate_ceiling = math.floor(CHIP_RATE / (SF * math.sqrt(rate_hz)))
    assert 1 <= rate_ceiling < 400

    a = Acquisition(
        CODE,
        reps=400,
        spc=SPS,
        chip_rate=CHIP_RATE,
        cn0_dbhz=55.0,
        pfa=PFA,
        pd=PD,
        max_noncoh=8,
        symbol_rate=symbol_rate,
        doppler_rate=rate_hz,
    )
    assert a.doppler_bins <= rate_ceiling


def test_doppler_rate_caps_resolution_floor():
    """A tight doppler_rate also wins over a caller-requested
    doppler_resolution that alone would ask for a much deeper coherent
    depth."""
    symbol_rate = CHIP_RATE / SF / 1.4
    rate_hz = 1.0e6
    rate_ceiling = math.floor(CHIP_RATE / (SF * math.sqrt(rate_hz)))

    a = Acquisition(
        CODE,
        reps=400,
        spc=SPS,
        chip_rate=CHIP_RATE,
        cn0_dbhz=55.0,
        pfa=PFA,
        pd=PD,
        max_noncoh=8,
        symbol_rate=symbol_rate,
        doppler_resolution=100.0,  # alone would ask for doppler_bins ~323
        doppler_rate=rate_hz,
    )
    assert a.doppler_bins <= rate_ceiling


def _window_epoch_segments(phi0, depth, epochs_per_symbol):
    """Segment a ``depth``-epoch window starting at symbol-phase ``phi0``
    into per-epoch ``[(length, symbol_id), ...]`` lists -- mirrors
    ``_dm_segment_window`` in native/src/acq/acq_core.c (and the earlier
    ``_window_epoch_segments`` in dsss_acq_async_data_demo.py it was ported
    from)."""
    segs_per_epoch = []
    phase = phi0
    symbol_id = 0
    for _ in range(depth):
        segs = []
        remaining = 1.0
        while remaining > 1e-9:
            to_boundary = epochs_per_symbol - phase
            take = min(to_boundary, remaining)
            segs.append((take, symbol_id))
            remaining -= take
            phase += take
            if phase >= epochs_per_symbol - 1e-9:
                phase = 0.0
                symbol_id += 1
        segs_per_epoch.append(segs)
    return segs_per_epoch, symbol_id + 1


def _group_alpha(v):
    d = len(v)
    if d == 1:
        return abs(v[0])
    return float(np.abs(np.fft.fft(v)).max() / d)


def _data_mod_pd_reference(
    doppler_bins, n_noncoh, epochs_per_symbol, snr, cb, eta, n_phi=8
):
    """Independent Python reimplementation of ``_data_mod_pd``
    (native/src/acq/acq_core.c) for cross-validating the C sizing engine's
    data-modulation Pd model -- the same semi-analytical model (window
    -phase quadrature x exact sign enumeration through det_pd/marcum_q),
    deliberately written from scratch here rather than calling into the C
    engine, so a shared bug in the C port wouldn't silently agree with
    itself. Callers must keep every window within the exact-enumeration
    regime (no Monte-Carlo fallback on either side -- the two PRNGs
    wouldn't agree), which the parametrized (doppler_bins, n_noncoh, 1.4)
    cases below satisfy by construction."""
    depth = doppler_bins * n_noncoh
    pd_acc = 0.0
    for p in range(n_phi):
        phi0 = (p + 0.5) / n_phi * epochs_per_symbol
        segs_per_epoch, n_symbols = _window_epoch_segments(
            phi0, depth, epochs_per_symbol
        )
        assert n_symbols <= 6  # exact-enumeration regime, see docstring
        n_combos = 1 << n_symbols
        pd_sum = 0.0
        for bits in range(n_combos):
            signs = np.array(
                [1.0 if (bits >> i) & 1 else -1.0 for i in range(n_symbols)]
            )
            v = np.array(
                [
                    sum(signs[sid] * length for length, sid in segs)
                    for segs in segs_per_epoch
                ]
            )
            if n_noncoh <= 1:
                alpha = _group_alpha(v)
                pd_sum += det_pd(snr * alpha, doppler_bins * cb, eta)
            else:
                lam = 0.0
                for g in range(n_noncoh):
                    alpha = _group_alpha(
                        v[g * doppler_bins : (g + 1) * doppler_bins]
                    )
                    amp = math.sqrt(2.0 * doppler_bins * cb) * snr * alpha
                    lam += amp * amp
                pd_sum += marcum_q(n_noncoh, math.sqrt(lam), eta)
        pd_acc += pd_sum / n_combos
    return pd_acc / n_phi


@pytest.mark.parametrize(
    "doppler_bins, n_noncoh",
    [(1, 1), (3, 1), (1, 4), (2, 3)],  # last two exercise D>1 AND nc>1
)
def test_data_mod_pd_matches_reference(doppler_bins, n_noncoh):
    """pd_predicted on a symbol_rate>0 engine, pinned via
    configure_search_raw to a specific (doppler_bins, n_noncoh), matches
    an independent Python reimplementation of the same model -- including
    the D>1 && n_noncoh>1 general case the gallery script's own
    _theoretical_pd (which this ports from) was validated only in the
    doppler_bins==1 or n_noncoh==1 special cases."""
    a = Acquisition(
        CODE,
        reps=8,
        spc=SPS,
        chip_rate=CHIP_RATE,
        cn0_dbhz=50.0,
        pfa=PFA,
        pd=PD,
        max_noncoh=8,
        symbol_rate=CHIP_RATE / SF / 1.4,
    )
    assert a.epochs_per_symbol == pytest.approx(1.4)
    a.configure_search_raw(doppler_bins, n_noncoh)
    assert (a.doppler_bins, a.n_noncoh) == (doppler_bins, n_noncoh)

    snr = math.sqrt(10.0 ** (a.cn0_dbhz / 10.0) / a.fs)
    eta = a.eta_nc if n_noncoh > 1 else a.eta
    expected = _data_mod_pd_reference(
        doppler_bins, n_noncoh, a.epochs_per_symbol, snr, a.code_bins, eta
    )
    assert a.pd_predicted == pytest.approx(expected, rel=1e-6, abs=1e-9)


def test_configure_search_raw_bounds():
    """Out-of-range pins raise ValueError and leave the engine untouched at
    its prior grid; a valid pin changes the grid and re-derives the
    threshold ladder."""
    a = Acquisition(
        CODE,
        reps=8,
        spc=SPS,
        chip_rate=CHIP_RATE,
        cn0_dbhz=65.0,
        pfa=PFA,
        pd=PD,
        max_noncoh=4,
    )
    orig_db, orig_nc = a.doppler_bins, a.n_noncoh

    for db, nc in [(0, 1), (9, 1), (1, 0), (1, 5)]:
        with pytest.raises(ValueError):
            a.configure_search_raw(db, nc)
    assert (a.doppler_bins, a.n_noncoh) == (orig_db, orig_nc)

    a.configure_search_raw(3, 2)
    assert (a.doppler_bins, a.n_noncoh) == (3, 2)
    assert a.eta_nc > 0.0 and a.threshold == 0.0


def test_configure_search_raw_detects():
    """A pinned grid actually detects a noise-free burst at that geometry
    -- configure_search_raw doesn't just relabel fields, it rebuilds the
    correlator/FFT/buffers to match."""
    a = Acquisition(
        CODE,
        reps=8,
        spc=SPS,
        chip_rate=CHIP_RATE,
        cn0_dbhz=65.0,
        pfa=PFA,
        pd=PD,
        max_noncoh=4,
    )
    a.configure_search_raw(4, 2)
    assert (a.doppler_bins, a.n_noncoh) == (4, 2)

    s0 = np.repeat(np.where(CODE & 1, -1.0, 1.0), SPS)
    burst = np.tile(s0, 2 * a.doppler_bins).astype(np.complex64)  # 2 decisions
    hits = a.push(burst)
    assert len(hits) == 1
    dop, col, *_ = hits[0]
    assert dop == 0 and col == 0
