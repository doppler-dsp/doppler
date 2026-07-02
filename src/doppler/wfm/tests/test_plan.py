"""Tests for the wfm ``Plan`` stimulus engine (component cache).

The contract is bit-exactness against a full compose: the baseline
``Plan.render()`` reproduces ``Composer.compose()`` to the bit, and each
overridable axis matches a full compose of the equivalently-modified scene. The
C core proves this in ``test_wfm_plan.c``; here we prove the generated binding
and the Python wrapper preserve it, plus the wrapper ergonomics (sweep /
monte_carlo / context manager) and the scope rejects.
"""

from __future__ import annotations

import numpy as np
import pytest

from doppler.wfm import Composer, Plan, Segment, prepare, qpsk, tone


def _scene(qpsk_snr: float = 12.0, tone_level: float = 0.0) -> Composer:
    """A separable 2-source scene: a qpsk anchor (SNR) + a clean tone."""
    return Composer(
        Segment.sum(
            qpsk(snr=qpsk_snr, seed=7, sps=8, pn_length=7),
            tone(freq=1e5, seed=3, sps=8, level=tone_level),
            fs=1e6,
            num_samples=4096,
        )
    )


def test_baseline_render_is_bit_identical_to_compose() -> None:
    scene = _scene()
    plan = prepare(scene)
    assert len(plan) == 4096
    assert plan.n_sources == 2
    assert plan.anchor_seed == 7
    np.testing.assert_array_equal(plan.render(), scene.compose())


def test_render_no_overrides_equals_empty_and_none_paths() -> None:
    plan = prepare(_scene())
    base = plan.render()
    # the wrapper sends "{}" for no overrides; the raw handle also accepts NULL
    np.testing.assert_array_equal(base, plan._h.render("{}"))
    np.testing.assert_array_equal(base, plan._h.render(""))


def test_snr_axis_matches_full_compose() -> None:
    plan = prepare(_scene(qpsk_snr=12.0))
    ref = _scene(qpsk_snr=6.0).compose()
    # at(snr, anchor_seed) and render(snr=) both reproduce compose @ that SNR
    np.testing.assert_array_equal(plan.at(6.0, plan.anchor_seed), ref)
    np.testing.assert_array_equal(
        plan.render(snr=6.0, seed=plan.anchor_seed), ref
    )
    # at()'s default seed is the anchor seed
    np.testing.assert_array_equal(plan.at(6.0), plan.at(6.0, plan.anchor_seed))


def test_gain_axis_on_non_anchor_matches_compose() -> None:
    # moving the clean (non-anchor) tone leaves the noise floor in place
    plan = prepare(_scene())
    got = plan.render(gains=[0.0, -6.0])
    ref = _scene(tone_level=-6.0).compose()
    np.testing.assert_array_equal(got, ref)


def test_phase_identity_and_transform() -> None:
    plan = prepare(_scene())
    base = plan.render()
    np.testing.assert_array_equal(plan.render(phases=[0.0, 0.0]), base)
    assert not np.array_equal(plan.render(phases=[1.5, 0.0]), base)


def test_enable_all_on_is_baseline_all_off_drops_signal() -> None:
    plan = prepare(_scene())
    base = plan.render()
    np.testing.assert_array_equal(plan.render(enable=[True, True]), base)
    assert not np.array_equal(plan.render(enable=[False, False]), base)


def test_render_is_deterministic() -> None:
    plan = prepare(_scene())
    np.testing.assert_array_equal(plan.at(6.0, 42), plan.at(6.0, 42))


def test_monte_carlo_seeds_draw_independent_noise() -> None:
    plan = prepare(_scene())
    draws = list(plan.monte_carlo(6.0, 5, seed0=100))
    assert len(draws) == 5
    # every realization is distinct (only the noise differs)
    assert len({d.tobytes() for d in draws}) == 5


def test_sweep_yields_snr_labelled_arrays() -> None:
    plan = prepare(_scene())
    out = dict(plan.sweep([0.0, 6.0, 12.0]))
    assert set(out) == {0.0, 6.0, 12.0}
    assert all(
        v.shape == (4096,) and v.dtype == np.complex64 for v in out.values()
    )
    # a held seed isolates the SNR axis: at the base SNR it equals the baseline
    np.testing.assert_array_equal(out[12.0], plan.render())


def test_construct_from_json_string() -> None:
    scene = _scene()
    ref = scene.compose()
    np.testing.assert_array_equal(Plan(scene.to_json()).render(), ref)
    np.testing.assert_array_equal(
        Plan(scene.to_json().encode()).render(),
        ref,  # bytes too
    )


def test_context_manager_closes() -> None:
    scene = _scene()
    with prepare(scene) as plan:
        np.testing.assert_array_equal(plan.render(), scene.compose())


def test_rejects_bundled_single_noisy_source() -> None:
    # one source carrying SNR → its RNG is fused into the signal, not separable
    bundled = Composer(
        Segment.sum(qpsk(snr=12.0, seed=7), fs=1e6, num_samples=1024)
    )
    with pytest.raises(ValueError):
        prepare(bundled)


def test_rejects_ranged_scene() -> None:
    # a swept (ranged) parameter draws per-epoch → ambiguous for a static Plan
    ranged = Composer(
        Segment.sum(
            qpsk(snr=[6.0, 12.0], seed=7),
            tone(freq=1e5, seed=3),
            fs=1e6,
            num_samples=1024,
        )
    )
    with pytest.raises(ValueError):
        prepare(ranged)
