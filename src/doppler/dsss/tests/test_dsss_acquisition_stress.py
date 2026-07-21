"""Fast pytest twin of ``dsss_acquisition_stress.py`` -- imports its helper
functions and re-runs them at a much smaller trial count (mirrors
``test_dsss_receiver_stress.py``'s relationship to
``dsss_receiver_stress.py``), so the default test run gets real coverage of
the isolation question without the full-sweep script's wall-clock cost.

See the example module's docstring for the confirmed finding this harness
answers: `Acquisition`'s own search is essentially exact across its whole
configured range (not the source of the composed `DsssReceiver`'s ~263 Hz
Doppler pull-in boundary), but its Doppler *estimate* is frequently
degenerate (a single bin spanning the whole native span) by design, which
is why the downstream carrier loop inherits the narrowing. Both halves of
that finding are reflected in this file's assertions.
"""

import numpy as np

from doppler.examples.dsss_acquisition_stress import (
    _CODE_PHASE_REF,
    CHIP_RATE,
    CODE,
    DOPPLER_FRAC_OF_SPAN,
    DOPPLER_UNCERTAINTY,
    SF,
    SPAN_HZ,
    SPC_CHOICES,
    measure_doppler_handoff_quality,
    run_trial,
    sweep_cn0_calibration,
    sweep_doppler_acquisition_range,
)

TRIALS = 10  # per bucket / per random draw -- seeded, fast


def test_geometry_matches_dsss_receiver_stress() -> None:
    """Reuses dsss_receiver_stress's own link geometry verbatim."""
    assert SF == 1023 and CHIP_RATE == 3.0e6
    assert SPAN_HZ == CHIP_RATE / (2 * SF)
    assert DOPPLER_UNCERTAINTY == 0.95 * SPAN_HZ
    assert CODE.shape == (SF,)


def test_code_phase_reference_is_stable_per_spc() -> None:
    """The empirically-measured code-phase reference is computed once per
    spc at import time and is a valid column index for every spc tested."""
    assert set(_CODE_PHASE_REF) == set(SPC_CHOICES)
    for spc, col in _CODE_PHASE_REF.items():
        assert 0 <= col < SF * spc


def test_strong_signal_acquires_on_true_cell() -> None:
    """A single strong, near-zero-Doppler burst must land exactly on the
    true (Doppler bin, code phase) cell -- the basic sanity floor every
    other assertion in this file builds on."""
    r = run_trial(6, cn0_dbhz=70.0, doppler_hz=20.0, spc=2, power_scale=1.0)
    assert r["detected"]
    assert r["on_true_cell"], r
    assert r["code_phase_err_chips"] == 0.0


def test_doppler_search_covers_full_configured_range() -> None:
    """Acquisition's own search must stay accurate across its WHOLE
    configured Doppler range, not just near zero -- this is the isolation
    result: if this holds while dsss_receiver_stress's composed-receiver
    boundary sits at ~263 Hz, the narrowing lives downstream, not here."""
    rates = sweep_doppler_acquisition_range(n_per_bucket=TRIALS)
    assert all(r >= 0.7 for r in rates), (
        f"Acquisition's on-true-cell rate dropped within its own "
        f"configured search range: {rates}"
    )


def test_cn0_calibration_is_monotonic_and_well_predicted() -> None:
    """Detection accuracy must not get worse as C/N0 improves, and the
    engine's own predicted Pd should track a high empirical rate in the
    strongest bucket (the "measure performance against setting" check)."""
    rates, predicted = sweep_cn0_calibration(n_per_bucket=TRIALS)
    assert rates == sorted(rates), (
        f"on-true-cell rate should be non-decreasing with C/N0, got {rates}"
    )
    assert rates[-1] >= 0.9, (
        f"expected near-certain acquisition, got {rates[-1]:.0%}"
    )
    assert predicted[-1] >= 0.9


def test_doppler_handoff_degrades_to_single_bin_by_design() -> None:
    """Confirms the actual root cause behind the composed receiver's
    narrow pull-in: at a C/N0 well above the sizing threshold,
    `doppler_bins` collapses to 1 (zero frequency resolution across the
    whole native span) -- Acquisition still finds the right code phase,
    but its Doppler estimate is no longer meaningful, and a downstream
    carrier loop has to close that gap on its own."""
    rng = np.random.default_rng(2)
    records = [
        run_trial(int(s), cn0_dbhz=65.0, spc=2)
        for s in rng.integers(0, 2**31 - 1, TRIALS)
    ]
    assert all(r["on_true_cell"] for r in records)
    assert all(r["doppler_bins"] == 1 for r in records)

    handoff = measure_doppler_handoff_quality(records)
    assert handoff["n"] == TRIALS
    assert handoff["frac_doppler_bins_eq_1"] == 1.0


def test_random_sweep_produces_full_telemetry() -> None:
    """A small random sweep across every axis (C/N0, Doppler, sample rate,
    power level, data sequence) runs end to end and every record carries
    the full diagnostic telemetry this harness relies on."""
    rng = np.random.default_rng(1)
    records = [run_trial(int(s)) for s in rng.integers(0, 2**31 - 1, TRIALS)]

    expected_keys = {
        "seed",
        "cn0_true_dbhz",
        "doppler_true_hz",
        "spc",
        "power_scale",
        "doppler_bins",
        "n_noncoh",
        "doppler_res_hz",
        "pd_predicted",
        "straddle_loss",
        "underpowered",
        "threshold",
        "pfa_cell",
        "detected",
        "frames_to_detect",
        "doppler_bin",
        "doppler_hz_est",
        "doppler_err_hz",
        "code_phase",
        "code_phase_err_chips",
        "cn0_dbhz_est",
        "cn0_err_db",
        "on_true_cell",
    }
    for r in records:
        assert expected_keys <= r.keys()
        assert r["spc"] in SPC_CHOICES
        assert abs(r["doppler_true_hz"]) <= DOPPLER_FRAC_OF_SPAN * SPAN_HZ

    # At this C/N0 range (40-80 dB-Hz), acquisition should succeed on
    # essentially every draw -- unlike the composed receiver's BER, which
    # legitimately collapses over most of the Doppler range once handed to
    # the narrower downstream carrier loop.
    assert all(r["detected"] for r in records)
