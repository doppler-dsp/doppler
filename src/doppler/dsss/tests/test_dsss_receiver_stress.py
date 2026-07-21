"""Fast pytest twin of ``dsss_receiver_stress.py`` -- imports its helper
functions and re-runs them at a much smaller trial count (mirrors
``test_acq_characterization.py``'s relationship to
``dsss_acq_characterization.py``), so the default test run gets real
coverage of the randomized C/N0 / Doppler / sample-rate / power-level /
data-sequence axes without the full 300-trial sweep's wall-clock cost.

See the example module's docstring for the two real findings this harness
surfaced (Doppler pull-in is narrow; wfmgen's Doppler mixing has a small
long-run phase drift) -- both are reflected in this file's assertions.
"""

import math

import numpy as np

from doppler.examples.dsss_receiver_stress import (
    CHIP_RATE,
    CODE,
    DOPPLER_FRAC_OF_SPAN,
    FS_GEN,
    N_SYM,
    SF,
    SPAN_HZ,
    SYM_RATE,
    find_doppler_pullin_boundary,
    make_signal_wfmgen,
    run_trial,
    sweep_cn0_near_zero_doppler,
)

TRIALS = 12  # per bucket / per random draw -- seeded, fast


def test_geometry_and_generation_rate() -> None:
    """The link geometry and the wfmgen generation-rate LCM trick are exact."""
    assert SF == 1023 and CHIP_RATE == 3.0e6 and SYM_RATE == 2100.0
    assert math.lcm(int(CHIP_RATE), int(SYM_RATE)) == FS_GEN
    assert FS_GEN % CHIP_RATE == 0 and FS_GEN % SYM_RATE == 0
    assert SPAN_HZ == CHIP_RATE / (2 * SF)
    assert CODE.shape == (SF,)


def test_wfmgen_signal_decodes_cleanly_at_known_good_point() -> None:
    """The wfmgen-composed generation pipeline itself is correct: at the
    existing gallery demo's own operating point (CN0=97 dB-Hz,
    Doppler=50 Hz, seed=6), it must decode as cleanly as the pure-numpy
    generator the rest of this story's tests use."""
    r = run_trial(
        6, cn0_dbhz=97.0, doppler_hz=50.0, spc=2, power_scale=1.0, n_sym=N_SYM
    )
    assert r["tracking"] == 1
    assert r["ber"] < 0.01, f"expected clean decode, got BER={r['ber']}"


def test_doppler_pullin_boundary_exists_and_is_bounded() -> None:
    """The receiver's Doppler pull-in range is a real, measurable quantity
    (a known, narrow-band limitation -- see the module docstring) --
    regression-guard against it collapsing to near-zero, not a claim about
    its exact value."""
    boundary_hz = find_doppler_pullin_boundary(n_sym=N_SYM, tol_hz=20.0)
    assert boundary_hz is not None, (
        "receiver failed to decode even at Doppler=0"
    )
    assert boundary_hz > 20.0, f"pull-in boundary regressed: {boundary_hz} Hz"
    assert boundary_hz <= DOPPLER_FRAC_OF_SPAN * SPAN_HZ


def test_cn0_monotonicity_near_zero_doppler() -> None:
    """Clean-decode rate must not get worse as C/N0 improves, at a fixed,
    comfortably-inside-pull-in Doppler and nominal front-end rate/power
    (isolating the C/N0 axis from the separate Doppler/power axes)."""
    rates = sweep_cn0_near_zero_doppler(n_per_bucket=TRIALS, doppler_hz=20.0)
    assert rates == sorted(rates), (
        f"clean-decode rate should be non-decreasing with C/N0, got {rates}"
    )
    assert rates[-1] >= 0.75, (
        f"expected a high clean-decode rate in the strongest C/N0 bucket, "
        f"got {rates[-1]:.0%}"
    )


def test_random_sweep_produces_full_telemetry() -> None:
    """A small random sweep across every axis (C/N0, Doppler, sample rate,
    power level, data sequence) runs end to end and every record carries
    the full diagnostic telemetry this harness's whole point relies on."""
    rng = np.random.default_rng(1)
    records = [
        run_trial(int(s), n_sym=N_SYM)
        for s in rng.integers(0, 2**31 - 1, TRIALS)
    ]

    expected_keys = {
        "seed",
        "cn0_true_dbhz",
        "doppler_true_hz",
        "spc",
        "power_scale",
        "n_syms_recovered",
        "tracking",
        "doppler_hz",
        "cn0_dbhz_est",
        "chip_phase",
        "code_rate",
        "lock",
        "norm_freq",
        "segments",
        "sps",
        "n",
        "ber",
        "lag",
        "inverted",
        "esn0_meas_db",
    }
    for r in records:
        assert expected_keys <= r.keys()
        assert r["spc"] in (2, 3, 4, 5, 6, 8)
        assert abs(r["doppler_true_hz"]) <= DOPPLER_FRAC_OF_SPAN * SPAN_HZ

    # Sanity floor: at least some draws should land in favorable enough
    # conditions to actually lock (not every trial -- a large fraction of
    # the Doppler range is known to legitimately fail, see the pull-in
    # test above).
    assert any(r["tracking"] == 1 for r in records)


def test_make_signal_wfmgen_shape_and_dtype() -> None:
    """The three-Synth-call composition returns the expected shape/dtype."""
    x, data_bits = make_signal_wfmgen(60.0, 100.0, seed=0, n_sym=50)
    assert x.dtype == np.complex64
    assert len(x) == 50 * (FS_GEN / SYM_RATE)
    assert data_bits.shape == (50,)
    assert set(np.unique(data_bits)) <= {0, 1}
