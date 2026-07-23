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

from doppler.wfm import (
    Composer,
    Plan,
    PlanFromBlob,
    PlanFromFile,
    Segment,
    prepare,
    qpsk,
    tone,
)


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


def test_save_restore_round_trips_bit_exact(tmp_path) -> None:
    """save()/PlanFromBlob and dump()/PlanFromFile reconstruct a bit-exact Plan
    from the cached buffers — no re-run of prepare()'s DSP."""
    plan = prepare(_scene())
    base = plan.render()

    blob = plan.save()
    assert isinstance(blob, bytes) and len(blob) > 0
    restored = PlanFromBlob(blob)
    np.testing.assert_array_equal(restored.render(), base)
    # a variation also matches through the restored cache
    np.testing.assert_array_equal(restored.at(6.0, 1000), plan.at(6.0, 1000))
    assert restored.n_sources == plan.n_sources
    assert len(restored) == len(plan)

    p = tmp_path / "plan.bin"
    plan.dump(p)
    np.testing.assert_array_equal(PlanFromFile(p).render(), base)


def test_restore_rejects_a_corrupt_blob() -> None:
    """A truncated or wrong-magic blob is rejected, not reinterpreted."""
    blob = prepare(_scene()).save()
    with pytest.raises((ValueError, RuntimeError)):
        PlanFromBlob(blob[:16])  # truncated
    with pytest.raises((ValueError, RuntimeError)):
        PlanFromBlob(b"\x00" * len(blob))  # wrong magic


def test_dump_to_unwritable_path_raises_oserror(tmp_path) -> None:
    """dump() surfaces a failed write as OSError (the int->raise binding)."""
    plan = prepare(_scene())
    with pytest.raises(OSError):
        plan.dump(tmp_path / "no_such_dir" / "plan.bin")


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


def test_accepts_bundled_single_noisy_source() -> None:
    # A lone source carrying its own SNR is now separable: its AWGN is
    # reconstructed via a per-instance noise synth rather than an external
    # multiply (BUNDLED mode), matching a full compose bit-for-bit.
    scene = Composer(
        Segment.sum(
            qpsk(snr=12.0, seed=7, sps=8, pn_length=7),
            fs=1e6,
            num_samples=1024,
        )
    )
    plan = prepare(scene)
    assert plan.n_sources == 1
    np.testing.assert_array_equal(plan.render(), scene.compose())

    ref = Composer(
        Segment.sum(
            qpsk(snr=9.0, seed=7, sps=8, pn_length=7), fs=1e6, num_samples=1024
        )
    ).compose()
    np.testing.assert_array_equal(plan.render(snr=9.0), ref)


def test_accepts_bundled_dsss_source_with_owned_arrays() -> None:
    # A bundled dsss source carries acq_code/data_code/sync/payload as
    # owned arrays -- exercises the deep-copy path (not just the scalar
    # fields) that a plain qpsk/tone bundled source above never touches.
    rng = np.random.default_rng(0)
    acq = rng.integers(0, 2, 64, dtype=np.uint8)
    dat = rng.integers(0, 2, 13, dtype=np.uint8)
    pay = rng.integers(0, 2, 40, dtype=np.uint8)
    sync = np.array([1, 1, 1, 1, 1, 0, 0, 1, 1, 0, 1, 0, 1], dtype=np.uint8)

    def _seg(snr: float) -> Segment:
        return Segment(
            type="dsss",
            fs=4e6,
            sps=4,
            seed=1,
            snr=snr,
            snr_mode="esno",
            acq_code=acq,
            acq_reps=4,
            data_code=dat,
            sync=sync,
            payload=pay,
        )

    scene = Composer(_seg(10.0))
    plan = prepare(scene)
    assert plan.n_sources == 1
    np.testing.assert_array_equal(plan.render(), scene.compose())

    ref = Composer(_seg(6.0)).compose()
    np.testing.assert_array_equal(plan.render(snr=6.0), ref)


def test_rejects_ranged_scene() -> None:
    # a swept (ranged) per-source parameter draws per-epoch → ambiguous for
    # a static Plan's signal cache (still out of scope, unlike ranged gaps)
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


def test_rejects_ranged_num_samples() -> None:
    # a ranged on-time would invalidate the fixed-length signal cache
    ranged = Composer(
        Segment.sum(
            qpsk(snr=12.0, seed=7),
            fs=1e6,
            num_samples=(1024, 2048),
        )
    )
    with pytest.raises(ValueError):
        prepare(ranged)


def test_multi_segment_plan_matches_compose() -> None:
    segs = [
        Segment.sum(
            qpsk(snr=12.0, seed=7, sps=8, pn_length=7),
            fs=1e6,
            num_samples=1024,
            off_samples=200,
        ),
        Segment.sum(
            tone(freq=2e5, seed=11, level=-3.0), fs=1e6, num_samples=512
        ),
    ]
    scene = Composer(segs)
    plan = prepare(scene)
    assert len(plan) == 1024 + 200 + 512
    assert plan.n_sources == 2
    np.testing.assert_array_equal(plan.render(), scene.compose())


def test_repeats_plan_matches_compose() -> None:
    scene = Composer(
        Segment.sum(
            qpsk(snr=10.0, seed=21, sps=8, pn_length=7),
            fs=1e6,
            num_samples=256,
            off_samples=64,
            repeats=4,
        )
    )
    plan = prepare(scene)
    assert len(plan) == 4 * (256 + 64)
    np.testing.assert_array_equal(plan.render(), scene.compose())
    # per-instance AWGN differs: the 4 (equal-length) bursts are not
    # identical to each other.
    rendered = plan.render()
    burst0 = rendered[:256]
    burst1 = rendered[256 + 64 : 256 + 64 + 256]
    assert not np.array_equal(burst0, burst1)


def test_ranged_gap_redraws_per_seed() -> None:
    scene = Composer(
        Segment.sum(
            qpsk(snr=10.0, seed=33, sps=8, pn_length=7),
            fs=1e6,
            num_samples=256,
            off_samples=(32, 256),
            repeats=3,
        )
    )
    plan = prepare(scene)
    # baseline (no seed override) reproduces a full compose bit-for-bit --
    # the segment's own draw seed (epoch 0), same as the no-Plan path.
    np.testing.assert_array_equal(plan.render(), scene.compose())

    r1 = plan.render(seed=101)
    r2 = plan.render(seed=202)
    assert len(r1) != len(r2) or not np.array_equal(r1, r2)
