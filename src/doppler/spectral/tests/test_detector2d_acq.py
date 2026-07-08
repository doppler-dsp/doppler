"""DSSS burst acquisition gate for ``doppler.spectral.CorrDetector2D``.

Exercises the detector as a real spread-spectrum acquisition engine: a burst of
repeated PN-sequence segments, BPSK-modulated and oversampled, arriving with a
random integer code phase and a random carrier-frequency offset (Doppler) in
AWGN.  Verifies, vs SNR, that the detector both *fires* and lands its peak on
the correct ``(Doppler bin, code phase)`` cell, plus a noise-only Pfa check.

The signal model, single-row reference, slow-time Doppler-FFT framing, and the
``(Doppler bin, code col)`` mapping all live in the demo module so the demo and
this gate share one source of truth; see its docstring for the DSP.
"""

import math

import numpy as np
import pytest

from doppler.examples.detector2d_acq_demo import (
    NX,
    NY,
    PFA_SYS,
    build_acq_frame,
    build_ref,
    carrier_for_bin,
    doppler_bins,
    make_code,
    make_detector,
)

# Validated low-loss SNR grid (per-sample power dB): a clean S-curve from a
# noise floor at -42 dB to Pd = 1 at -15 dB after the ~33 dB coherent gain.
SNR_GRID = [-42.0, -30.0, -24.0, -21.0, -18.0, -15.0]
TRIALS = 60  # per SNR point — loose, seeded, fast


def _noise_frame(rng):
    """One pre-FFT'd noise-only frame (unit per-sample complex noise power)."""
    x = (
        rng.standard_normal((NY, NX)) + 1j * rng.standard_normal((NY, NX))
    ).astype(np.complex64) / math.sqrt(2.0)
    return np.fft.fft(x, axis=0).astype(np.complex64)


def _calibrate_theta(det, rng, n_trials=400):
    """Empirical gate: the ``1 - PFA_SYS`` quantile of noise-only test_stat."""
    stats = np.empty(n_trials)
    for i in range(n_trials):
        det.reset()
        (res,) = det.push(_noise_frame(rng))
        stats[i] = res[4]
    return float(np.quantile(stats, 1.0 - PFA_SYS))


@pytest.fixture(scope="module")
def sweep():
    """Run the full seeded SNR sweep once; reused by the assertions below."""
    rng = np.random.default_rng(1234)
    code = make_code()
    det = make_detector(build_ref(code))
    theta = _calibrate_theta(det, rng)
    bins = doppler_bins()

    pd = np.zeros(len(SNR_GRID))
    loc = np.full(len(SNR_GRID), np.nan)
    for k, snr_db in enumerate(SNR_GRID):
        n_det = n_loc = 0
        for _ in range(TRIALS):
            u = int(bins[rng.integers(len(bins))])
            cp = int(rng.integers(NX))
            y, dbin, col = build_acq_frame(
                rng,
                chips01=code,
                code_phase=cp,
                carrier_f=carrier_for_bin(u),
                snr_db=snr_db,
            )
            det.reset()
            (res,) = det.push(y)
            row, pcol, _, _, stat = res
            if stat > theta:
                n_det += 1
                d_ok = min((row - dbin) % NY, (dbin - row) % NY) <= 1
                if d_ok and abs(pcol - col) <= 1:
                    n_loc += 1
        pd[k] = n_det / TRIALS
        loc[k] = (n_loc / n_det) if n_det else np.nan

    return {"theta": theta, "pd": pd, "loc": loc, "det": det}


def test_pd_monotonic_in_snr(sweep):
    """Detection probability rises with SNR (small slack for MC noise)."""
    pd = sweep["pd"]
    assert np.all(np.diff(pd) >= -0.10), f"non-monotonic Pd: {pd}"


def test_high_snr_detects(sweep):
    """At the top SNR the burst is essentially always detected."""
    assert sweep["pd"][-1] >= 0.90, f"Pd[-1] = {sweep['pd'][-1]}"


def test_low_snr_floor(sweep):
    """At the bottom SNR detection collapses toward the Pfa floor."""
    assert sweep["pd"][0] <= 0.20, f"Pd[0] = {sweep['pd'][0]}"


def test_full_2d_localization_at_high_snr(sweep):
    """Above the knee, detections land on the true (Doppler, code) cell."""
    loc = sweep["loc"]
    for k in (-1, -2):  # top two SNR points
        assert loc[k] >= 0.95, f"localization {loc[k]} at SNR_GRID[{k}]"


def test_noise_only_pfa(sweep):
    """Independent noise-only trials respect the calibrated Pfa gate."""
    rng = np.random.default_rng(99)
    det = sweep["det"]
    theta = sweep["theta"]
    n = 300
    fa = 0
    for _ in range(n):
        det.reset()
        (res,) = det.push(_noise_frame(rng))
        fa += res[4] > theta
    pfa = fa / n
    assert pfa < 0.05, f"empirical Pfa = {pfa} (target {PFA_SYS})"


def test_deterministic_localization():
    """Hard regression guard for the pre-FFT + single-row-ref construction:
    a clean high-SNR burst at a fixed (Doppler, code phase) must localize
    exactly.  If the slow-time ``FFT(x, axis=0)`` is ever dropped, the row
    axis goes flat and this fails.
    """
    rng = np.random.default_rng(7)
    code = make_code()
    det = make_detector(build_ref(code))
    y, dbin, col = build_acq_frame(
        rng,
        chips01=code,
        code_phase=83,
        carrier_f=carrier_for_bin(3),
        snr_db=-12.0,  # well above the knee
    )
    det.reset()
    results = det.push(y)
    assert len(results) == 1  # dwell=1 → exactly one dump per frame
    row, pcol, peak, noise, stat = results[0]
    assert (row, pcol) == (dbin, col)
    assert math.isfinite(peak) and math.isfinite(noise) and math.isfinite(stat)
    assert stat > 1.0
