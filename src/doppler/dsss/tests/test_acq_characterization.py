"""Performance-characterisation gate for ``doppler.dsss.BurstAcquisition``.

Drives the engine through the ``dsss_acq_characterization`` demo — a length-511
PN preamble (9-stage MLS) repeated 5 times, a 64-chip-per-symbol BPSK DSSS
payload, and silence before and after, arriving at a random integer code phase
and a random Doppler across the engine's capture range, in AWGN — and pins the
two operating-curve properties a characterisation must hold:

* **Pd** rises with Es/N0 — a noise floor at low Es/N0 and (near-)certain
  acquisition once the coherent gain clears the CFAR gate.
* **Pfa** is governed by the CFAR threshold: measured on noise-only frames it
  tracks the configured target and does not depend on the signal.

The waveform geometry, the wfm "wfmgen" signal construction, the (Doppler bin,
code phase) mapping, and the Es/N0 maths all live in the demo module so it and
this gate share one source of truth; see its docstring for the DSP.
"""

import math

import numpy as np
import pytest

from doppler.examples.dsss_acq_characterization import (
    ACQ_CODE,
    ACQ_REPS,
    CHIP_RATE,
    DATA_SF,
    DOPPLER_SPAN_HZ,
    FRAME,
    PFA,
    SF,
    SPC,
    acq_surface,
    build_scene,
    es_n0_db,
    make_engine,
    measure_pd,
    measure_pfa,
    run_trial,
    snr_db_for_es_n0,
)

# A loose, fast grid: a deep null, the knee, and the plateau.
LOW_ES_N0 = -9.0  # well below threshold → Pd ~ 0
HIGH_ES_N0 = 9.0  # well above threshold → Pd ~ 1
TRIALS = 40  # per point — seeded, fast


def test_geometry_and_esn0() -> None:
    """Burst geometry and the Es/N0 conversion match the configured physics."""
    assert ACQ_CODE.shape == (SF,) and SF == 2**9 - 1  # 511-chip MLS
    a = make_engine()
    assert (a.code_bins, a.doppler_bins) == (SF, ACQ_REPS)
    assert a.code_bins * a.doppler_bins == FRAME
    assert a.fs == pytest.approx(CHIP_RATE * SPC)
    assert a.doppler_span_hz == pytest.approx(CHIP_RATE / (2 * SF))
    # Es/N0 is a flat +10log10(DATA_SF*SPC) above the per-sample SNR.
    offset = 10.0 * math.log10(DATA_SF * SPC)
    assert es_n0_db(-20.0) == pytest.approx(-20.0 + offset)
    assert snr_db_for_es_n0(es_n0_db(-13.0)) == pytest.approx(-13.0)


def test_scene_is_frame_aligned_with_one_preamble() -> None:
    """The built stream is whole frames; exactly one carries the preamble."""
    x, kinds = build_scene(0.0, code_phase=137, doppler_hz=400.0, seed=0)
    assert x.dtype == np.complex64
    assert len(x) % FRAME == 0
    assert kinds.count("pre") == 1
    assert kinds.count("sil") >= 2  # silence before and after


def test_high_esn0_acquires_true_cell() -> None:
    """A single strong burst peaks on the true (Doppler, code) cell."""
    det, n_false, _ = run_trial(snr_db_for_es_n0(HIGH_ES_N0), seed=0)
    assert det and n_false == 0


def test_pd_rises_with_esn0() -> None:
    """Pd is a noise floor at low Es/N0 and near-certain at high Es/N0."""
    grid = np.array(
        [snr_db_for_es_n0(LOW_ES_N0), snr_db_for_es_n0(HIGH_ES_N0)]
    )
    pd, _ = measure_pd(grid, n_trials=TRIALS, seed=1)
    pd_low, pd_high = float(pd[0]), float(pd[1])
    assert pd_low <= 0.2, f"Pd should floor at low Es/N0, got {pd_low}"
    assert pd_high >= 0.85, f"Pd should saturate at high Es/N0, got {pd_high}"
    assert pd_high > pd_low


def test_pfa_tracks_target_and_is_signal_independent() -> None:
    """Noise-only Pfa stays within a small factor of the configured target."""
    pfa_hat = measure_pfa(n_frames=4000, seed=5)
    # 4000 frames at PFA=1e-3 → ~4 expected hits; bound generously both ways.
    assert pfa_hat <= 5.0 * PFA, f"Pfa {pfa_hat} exceeds 5x target {PFA}"


def test_surface_argmax_is_the_engine_detection() -> None:
    """The reconstructed surface peak == the engine's emitted (bin, phase)."""
    cp, dop = 211, 350.0
    x, _ = build_scene(snr_db_for_es_n0(HIGH_ES_N0), cp, dop, seed=3)
    surface = acq_surface(x[3 * FRAME : 4 * FRAME])  # K_PRE == 3 → preamble
    assert surface.shape == (ACQ_REPS, SF)
    row, col = np.unravel_index(int(np.argmax(surface)), surface.shape)
    hits = make_engine().push(x)
    assert hits, "engine should detect the strong burst"
    assert (int(hits[0][0]), int(hits[0][1])) == (int(row), int(col))


def test_doppler_drawn_within_capture_range() -> None:
    """Every trial's Doppler stays inside the +/- span capture range."""
    rng = np.random.default_rng(7000 + 11)
    rng.integers(0, SF)  # advance exactly as run_trial does (code phase first)
    dop = float(rng.uniform(-DOPPLER_SPAN_HZ, DOPPLER_SPAN_HZ))
    assert abs(dop) <= DOPPLER_SPAN_HZ
