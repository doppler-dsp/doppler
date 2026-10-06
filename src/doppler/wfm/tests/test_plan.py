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
    STAGE_CRC16,
    Composer,
    FrameDesc,
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
    frame = FrameDesc()
    frame.add_field("sync", sync)
    frame.add_data("payload", len(pay))
    frame.add_derived("crc", 16)
    frame.add_stage_over(STAGE_CRC16, "payload", "crc")

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
            frame=frame,
            data=pay,
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


def _doppler_scene(**extra) -> Composer:
    return Composer(
        [
            Segment(
                "bpsk",
                fs=1e6,
                sps=4,
                snr=12.0,
                num_samples=2048,
                seed=3,
                carrier_hz=2.2e9,
                **extra,
            )
        ]
    )


# ── Doppler: a channel applied at render time over the cached signal ───────
#
# The cache holds each source's clean ON-time, BEFORE the channel. A channel is
# stateful and runs through the gaps (gh-409), so it cannot live in a
# per-source on-time cache; the Plan runs it at render time, through the
# composer's own renderer, over the cached signal (doppler#1109). Everything
# below is therefore asserted against compose() to the BIT, which is the whole
# contract: a Plan that differs from compose() in a way nothing downstream can
# see is worse than no Plan.

_D = {"doppler": 20.0, "carrier_hz": 2.2e9}
_K = {"fs": 1e6, "num_samples": 3000}
_GAPS = {"off_samples": 700, "delay_samples": 300, "repeats": 3}


def _assert_bits(got: np.ndarray, want: np.ndarray) -> None:
    """Bit-identical, not merely equal: ``assert_array_equal`` treats -0.0 and
    +0.0 as the same number, and a Doppler channel's ring-out is precisely
    where a filter running on zeros emits a negative zero. A Plan that summed
    into a zeroed buffer matched compose() in value and differed in bits, and
    only the C ``memcmp`` could see it."""
    assert got.shape == want.shape
    assert got.tobytes() == want.tobytes()


def _q(**kw):
    return qpsk(seed=7, sps=8, pn_length=7, **kw)


def _t(**kw):
    return tone(freq=1e5, seed=3, sps=8, **kw)


#: Scene shapes, each one a way the cache used to be wrong. The first four are
#: the measurements doppler#1109 was filed against: the gap carries the
#: burst's ring-out, a leading delay advances the geometry, and (bundled) the
#: noise sits INSIDE the channel, which the old clean-cache-plus-noise model
#: missed by 1.73 on a unit-power signal.
_DOPPLER_SCENES = {
    "clean_gaps": lambda: Segment.sum(_t(**_D), **_GAPS, **_K),
    "bundled_noisy": lambda: Segment.sum(_q(snr=12.0, **_D), **_GAPS, **_K),
    "shared_noise_doppler_on_anchor": lambda: Segment.sum(
        _q(snr=12.0, **_D), _t(), **_GAPS, **_K
    ),
    "shared_noise_doppler_on_clean": lambda: Segment.sum(
        _q(snr=12.0), _t(**_D), **_GAPS, **_K
    ),
    "two_doppler_sources": lambda: Segment.sum(
        _q(snr=12.0, **_D),
        _t(doppler=-9.0, carrier_hz=2.2e9),
        **_GAPS,
        **_K,
    ),
    "persist": lambda: Segment.sum(
        _t(doppler_rate=300.0, carrier_hz=2.2e9, doppler_lifetime="persist"),
        off_samples=700,
        delay_samples=300,
        repeats=4,
        **_K,
    ),
    "persist_bundled": lambda: Segment.sum(
        _q(
            snr=12.0,
            doppler_rate=300.0,
            carrier_hz=2.2e9,
            doppler_lifetime="persist",
        ),
        **_GAPS,
        **_K,
    ),
    # Two PERSIST sources over 4096+ samples: the instances are chained by the
    # channels, so the render walks them in order and fans out ACROSS the
    # sources instead, each with its own slot.
    "persist_two_sources": lambda: Segment.sum(
        _t(doppler_rate=300.0, carrier_hz=2.2e9, doppler_lifetime="persist"),
        _q(doppler=-7.0, carrier_hz=2.2e9, doppler_lifetime="persist"),
        fs=1e6,
        num_samples=6000,
        off_samples=700,
        delay_samples=300,
        repeats=3,
    ),
    # The branch #1863 just broke, in its noisiest form: a BUNDLED source,
    # whose AWGN sits inside the channel, with the gaps never pulled.
    "bundled_gap_noise_off": lambda: Segment.sum(
        _q(snr=12.0, **_D),
        gap_noise="off",
        off_samples=700,
        delay_samples=300,
        repeats=3,
        **_K,
    ),
    # A drawn Doppler no channel can be built for (at or below -1e6 ppm: a
    # time base that stops or runs backwards). compose() fails the WHOLE
    # instance -- delay + off of silence, no ON region -- and the Plan has to
    # reach the same verdict from the same draws, not serve the clean signal.
    "unbuildable_doppler": lambda: Segment.sum(
        _t(doppler=-2.0e6, carrier_hz=2.2e9), **_GAPS, **_K
    ),
    # Some instances build and some do not: the failed ones are SHORTER, so
    # every later instance moves, which the parallel render must also know.
    "ranged_doppler_some_unbuildable": lambda: Segment.sum(
        _t(doppler=(-1.5e6, 5.0), carrier_hz=2.2e9),
        off_samples=700,
        delay_samples=300,
        repeats=12,
        **_K,
    ),
    "persist_some_unbuildable": lambda: Segment.sum(
        _t(
            doppler=(-1.5e6, 5.0),
            carrier_hz=2.2e9,
            doppler_lifetime="persist",
        ),
        off_samples=700,
        delay_samples=300,
        repeats=12,
        **_K,
    ),
    "gap_noise_off": lambda: Segment.sum(
        _t(**_D),
        gap_noise="off",
        off_samples=700,
        delay_samples=300,
        repeats=2,
        **_K,
    ),
    "gap_noise_off_shared": lambda: Segment.sum(
        _q(snr=12.0, **_D),
        _t(),
        gap_noise="off",
        off_samples=700,
        delay_samples=300,
        repeats=2,
        **_K,
    ),
    "ranged_doppler": lambda: Segment.sum(
        _t(doppler=(2.0, 9.0), carrier_hz=2.2e9), **_GAPS, **_K
    ),
    "ranged_doppler_rate": lambda: Segment.sum(
        _t(doppler_rate=(100.0, 500.0), carrier_hz=2.2e9), **_GAPS, **_K
    ),
}


@pytest.mark.parametrize("name", sorted(_DOPPLER_SCENES))
def test_doppler_scene_is_bit_identical_to_compose(name: str) -> None:
    scene = Composer(_DOPPLER_SCENES[name]())
    plan = prepare(scene)
    ref = scene.compose()
    # len() is a worst-case CAPACITY: a scene whose instances fail is shorter.
    # The drawn length is the array's, which is what has to match.
    assert len(plan) >= len(ref)
    _assert_bits(plan.render(), ref)


def test_persist_is_per_segment_and_source_not_per_scene() -> None:
    """A PERSIST channel is one pass across a segment's repeats, keyed by
    (segment, source). Two segments therefore carry two independent passes,
    and the Plan has to key them the same way compose() does."""
    kw = {"doppler_rate": 300.0, "carrier_hz": 2.2e9}
    scene = Composer(
        [
            Segment.sum(
                _t(doppler_lifetime="persist", **kw),
                off_samples=400,
                repeats=3,
                **_K,
            ),
            Segment.sum(
                _t(doppler_lifetime="persist", **kw),
                off_samples=400,
                repeats=2,
                **_K,
            ),
        ]
    )
    _assert_bits(prepare(scene).render(), scene.compose())


def test_doppler_burst_gap_is_ring_out_through_the_plan() -> None:
    """The Plan inherits the doppler#1858 behaviour rather than restating it:
    a clean burst's gap is silent beyond the ring-out."""
    scene = Composer(
        Segment.sum(_t(**_D), fs=1e6, num_samples=1000, off_samples=3000)
    )
    a = np.abs(prepare(scene).render())
    assert a[100:900].mean() > 0.9
    assert a[1000 + 64 :].max() == 0.0


def test_doppler_gain_axis_matches_compose() -> None:
    # The channel is linear, so a gain is applied AFTER it, as compose()
    # does. Level on the Doppler tone; the noise floor (the qpsk anchor) is
    # untouched.
    def scene(level: float) -> Composer:
        return Composer(
            Segment.sum(_q(snr=12.0), _t(level=level, **_D), **_GAPS, **_K)
        )

    plan = prepare(scene(0.0))
    _assert_bits(plan.render(gains=[0.0, -6.0]), scene(-6.0).compose())


def test_doppler_snr_axis_matches_compose() -> None:
    # Bundled: the AWGN sits inside the channel, so an SNR override has to move
    # the noise that is FED to it, not one added afterwards.
    def scene(snr: float) -> Composer:
        return Composer(Segment.sum(_q(snr=snr, **_D), **_GAPS, **_K))

    plan = prepare(scene(12.0))
    _assert_bits(
        plan.render(snr=6.0, seed=plan.anchor_seed), scene(6.0).compose()
    )


def test_ranged_doppler_redraws_per_seed_and_matches_compose() -> None:
    """The Monte-Carlo shape #1109 names: a ranged Doppler, a seed per trial.
    Each seed must be exactly the scene composed with that seed -- the draw is
    the same number, not merely a similar one."""

    def scene(seed: int) -> Composer:
        return Composer(
            Segment.sum(
                tone(
                    freq=1e5,
                    seed=seed,
                    sps=8,
                    doppler=(2.0, 9.0),
                    carrier_hz=2.2e9,
                ),
                **_GAPS,
                **_K,
            )
        )

    plan = prepare(scene(3))
    r1, r2 = plan.render(seed=101), plan.render(seed=202)
    _assert_bits(r1, scene(101).compose())
    _assert_bits(r2, scene(202).compose())
    assert not np.array_equal(r1, r2)


def test_doppler_plan_survives_save_and_restore() -> None:
    """The cache is the signal BEFORE the channel, so a saved Plan restores
    from it and still renders the channel exactly."""
    scene = Composer(_DOPPLER_SCENES["bundled_noisy"]())
    plan = prepare(scene)
    restored = PlanFromBlob(plan.save())
    _assert_bits(restored.render(), scene.compose())


def test_doppler_phase_override_rotates_the_signal() -> None:
    """A phase override is applied to the signal BEFORE the channel (the
    channel is linear, so it commutes). Identity is exact; a real rotation
    changes the output without moving the gap's ring-out in time."""
    plan = prepare(Composer(_DOPPLER_SCENES["clean_gaps"]()))
    base = plan.render()
    np.testing.assert_array_equal(plan.render(phases=[0.0]), base)
    rot = plan.render(phases=[np.pi / 2])
    assert not np.array_equal(rot, base)
    np.testing.assert_allclose(np.abs(rot), np.abs(base), atol=1e-5)


def test_doppler_gain_axis_on_a_bundled_source_matches_compose() -> None:
    """A bundled source's gain multiplies signal AND noise together, after the
    channel (the noise is inside it), as compose() scales the whole stream."""

    def scene(level: float) -> Composer:
        return Composer(
            Segment.sum(_q(snr=12.0, level=level, **_D), **_GAPS, **_K)
        )

    plan = prepare(scene(0.0))
    _assert_bits(plan.render(gains=[-6.0]), scene(-6.0).compose())


def test_doppler_phase_axis_on_a_bundled_source() -> None:
    """Identity is exact; a real rotation changes the render. (Phase is a
    Plan-only axis, so there is no compose() to compare a rotation to.)"""
    plan = prepare(Composer(_DOPPLER_SCENES["bundled_noisy"]()))
    base = plan.render()
    _assert_bits(plan.render(phases=[0.0]), base)
    assert not np.array_equal(plan.render(phases=[1.0]), base)


def test_doppler_enable_axis() -> None:
    """Dropping a Doppler source removes it: all-on is the baseline exactly,
    and all-but-the-Doppler-tone is the same as that tone at a vanishing
    level (its channel is not run at all)."""
    plan = prepare(
        Composer(_DOPPLER_SCENES["shared_noise_doppler_on_clean"]())
    )
    base = plan.render()
    _assert_bits(plan.render(enable=[True, True]), base)
    dropped = plan.render(enable=[True, False])
    assert not np.array_equal(dropped, base)
    np.testing.assert_allclose(
        dropped, plan.render(gains=[0.0, -300.0]), atol=1e-6
    )


def test_seed_override_sets_every_sources_draw_seed() -> None:
    """A seed override replaces EVERY source's seed for the Doppler draw, as it
    does for the gap draw, so two ranged-Doppler sources stay independent
    through the source index in the draw key rather than through their own
    seeds. Tones, because their signal does not depend on the seed: the cache
    is still the right signal for the recomposed scene."""

    def scene(seed_a: int, seed_b: int) -> Composer:
        return Composer(
            Segment.sum(
                tone(
                    freq=1e5,
                    seed=seed_a,
                    sps=8,
                    doppler=(2.0, 9.0),
                    carrier_hz=2.2e9,
                ),
                tone(
                    freq=-1e5,
                    seed=seed_b,
                    sps=8,
                    doppler=(-8.0, -1.0),
                    carrier_hz=2.2e9,
                ),
                **_GAPS,
                **_K,
            )
        )

    plan = prepare(scene(3, 5))
    got = plan.render(seed=101)
    _assert_bits(got, scene(101, 101).compose())
    assert got.tobytes() != scene(101, 5).compose().tobytes()


def test_background_source_with_doppler_is_still_refused() -> None:
    """The one carve-out. The background fold sums sources into a single
    composite before the render, and a channel is per source: it cannot be
    applied to a sum. Refused rather than cached wrong."""
    scene = Composer(
        Segment.sum(
            _q(level=-24.0, background=True, **_D),
            _q(snr=12.0),
            **_K,
        )
    )
    with pytest.raises(ValueError, match="doppler"):
        prepare(scene)


def test_doppler_free_scene_still_prepares() -> None:
    # The reject above must be about the CHANNEL, not about the keys being
    # present: zero doppler AND zero doppler_rate builds no channel, so a
    # declared lifetime and a declared carrier describe nothing and cannot
    # cost a scene its plan.
    plan = prepare(_doppler_scene(doppler_lifetime="persist", doppler=0.0))
    assert len(plan) == 2048
    np.testing.assert_array_equal(
        plan.render(),
        _doppler_scene(doppler_lifetime="persist", doppler=0.0).compose(),
    )


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


# ── background=True: a static source prefix folds into one cache slot ──


def _field_scene(
    n_bg: int, *, background: bool, snr: float = 12.0
) -> Composer:
    """`n_bg` static users (flagged or not) + a wanted anchor + a jammer.

    The background sources lead the segment, which puts the SNR-carrying
    anchor at index `n_bg` -- deliberately not sources[0].
    """
    field = [
        qpsk(
            seed=500 + k,
            sps=8,
            pn_length=7,
            freq=-2e5 + 1e4 * k,
            level=-24.0,
            background=background,
        )
        for k in range(n_bg)
    ]
    return Composer(
        Segment.sum(
            *field,
            qpsk(snr=snr, seed=7, sps=8, pn_length=7),
            tone(freq=2.2e5, seed=3, sps=8, level=-9.0),
            fs=1e6,
            num_samples=4096,
        )
    )


def test_background_prefix_folds_to_one_slot() -> None:
    """The population collapses to a single overridable cache entry."""
    folded = prepare(_field_scene(8, background=True))
    per_source = prepare(_field_scene(8, background=False))
    assert folded.n_sources == 3  # background + anchor + tone
    assert per_source.n_sources == 10


def test_background_fold_is_bit_identical_to_compose() -> None:
    """The whole point: folding changes the cache, never the samples."""
    scene = _field_scene(8, background=True)
    folded = prepare(scene)
    np.testing.assert_array_equal(folded.render(), scene.compose())
    # ...and identical to the same scene cached one buffer per source.
    per_source = prepare(_field_scene(8, background=False))
    np.testing.assert_array_equal(folded.render(), per_source.render())


def test_background_slot_scales_the_whole_field() -> None:
    """gains[0] trims the composite as a unit; the others keep their own."""
    plan = prepare(_field_scene(8, background=True))
    base = plan.render()
    off = plan.render(enable=[False, True, True])
    trimmed = plan.render(gains=[-6.0, 0.0, -9.0])

    # Removing the field must change the output but leave the anchor's own
    # contribution, so the result is neither unchanged nor empty.
    assert not np.array_equal(base, off)
    assert np.any(off != 0)

    # The field's own contribution scales by the trim: powers of independent
    # sources add, so subtracting the field-off render isolates it.
    p_base = float(np.mean(np.abs(base - off) ** 2))
    p_trim = float(np.mean(np.abs(trimmed - off) ** 2))
    scale_db = 10.0 * np.log10(p_base / p_trim)
    assert 5.5 < scale_db < 6.5


def test_background_must_be_a_contiguous_prefix() -> None:
    """A background source behind a foreground one is refused, not degraded.

    The composite sums from zero, so it only reproduces the composer's running
    accumulator bit-for-bit while nothing precedes it.
    """
    scene = Composer(
        Segment.sum(
            qpsk(seed=501, sps=8, pn_length=7, level=-20.0, background=True),
            qpsk(snr=12.0, seed=7, sps=8, pn_length=7),
            qpsk(seed=502, sps=8, pn_length=7, level=-20.0, background=True),
            fs=1e6,
            num_samples=1024,
        )
    )
    with pytest.raises(ValueError):
        prepare(scene)


def test_background_on_a_bundled_source_is_a_no_op() -> None:
    """A lone real-SNR source has nothing to fold, and its baked-in noise
    amplitude rides on the base gain the fold would overwrite."""
    scene = Composer(
        Segment.sum(
            qpsk(
                snr=9.0,
                seed=7,
                sps=8,
                pn_length=7,
                level=-3.0,
                background=True,
            ),
            fs=1e6,
            num_samples=1024,
        )
    )
    plan = prepare(scene)
    assert plan.n_sources == 1
    np.testing.assert_array_equal(plan.render(), scene.compose())


def test_background_survives_save_restore() -> None:
    """A folded cache round-trips as-is -- the blob holds the composite."""
    scene = _field_scene(6, background=True)
    plan = prepare(scene)
    restored = PlanFromBlob(plan.save())
    assert restored.n_sources == plan.n_sources == 3
    np.testing.assert_array_equal(restored.render(), plan.render())


def test_ranged_gap_draw_length_varies_and_never_exceeds_len() -> None:
    """`len()` is a CAPACITY; the drawn length is a property of the draw.

    Certified in `tests/validation/wfm_plan` (§2.4). For a ranged-gap
    scene every seed redraws the gaps, so the materialized length moves
    and a caller must read it off the returned array -- `len(plan)` is
    the worst case, every gap at its `hi` bound.

    Pinned here because the Python docstring currently promises the
    opposite ("length ``len()``"), which costs a Monte-Carlo caller both
    rectangular idioms; that is doppler#1128. When it is fixed, this test
    is what makes the change deliberate rather than silent.
    """
    plan = prepare(
        Composer(
            Segment.sum(
                qpsk(snr=10.0, seed=33, sps=8, pn_length=7),
                fs=1e6,
                num_samples=256,
                off_samples=(32, 512),
                delay_samples=(16, 256),
                repeats=3,
            )
        )
    )
    cap = len(plan)
    lengths = {int(plan.render(seed=s).shape[0]) for s in (1, 2, 3, 7, 11)}

    assert len(lengths) > 1, "a ranged gap must redraw the length per seed"
    assert max(lengths) <= cap, "no draw may exceed the worst-case capacity"
    # The precondition that keeps the assertion above from being vacuous:
    # a capacity every draw happened to saturate would satisfy it too.
    assert min(lengths) < cap, "len() is a worst case, not the drawn length"


def test_fixed_gap_scene_draws_one_rectangular_length() -> None:
    """The other side of doppler#1128: with no ranged gap, stacking is safe.

    This is what makes the rule usable rather than a warning -- the
    rectangular Monte-Carlo idiom is correct exactly when the scene
    declares no ranged gap, and that boundary is the thing a caller needs.
    """
    plan = prepare(
        Composer(
            Segment.sum(
                qpsk(snr=10.0, seed=33, sps=8, pn_length=7),
                fs=1e6,
                num_samples=256,
                off_samples=128,
                repeats=3,
            )
        )
    )
    draws = list(plan.monte_carlo(6.0, 4, seed0=1))
    assert len({int(d.shape[0]) for d in draws}) == 1
    assert np.array(draws).shape == (4, len(plan))


def _header_why_no_noise() -> str:
    """DP_WFM_PLAN_WHY_NO_NOISE, read from wfm_plan.h: the one sentence."""
    import re

    from doppler.tests._repo import repo_root

    text = (
        repo_root(__file__)
        / "native"
        / "inc"
        / "doppler"
        / "wfm"
        / "wfm_plan.h"
    ).read_text(encoding="utf-8")
    body = text.split("#define DP_WFM_PLAN_WHY_NO_NOISE", 1)[1]
    body = body.split("\n\n", 1)[0]
    return "".join(re.findall(r'"((?:[^"\\]|\\.)*)"', body))


def _clean_plan() -> Plan:
    """A scene whose every source is clean: no noise floor anywhere."""
    return prepare(Composer(type="tone", fs=1e6, num_samples=256))


def test_a_clean_scene_refuses_an_snr_naming_the_fix() -> None:
    # #1695: at(snr) on a scene with no noise returned the clean signal at
    # EVERY snr, so a BER sweep over it read a perfect receiver. It is
    # refused, and the reason is the C header's sentence, word for word.
    why = _header_why_no_noise()
    assert why.startswith("this scene carries no noise")
    plan = _clean_plan()
    for draw in (
        lambda: plan.at(6.0),
        lambda: plan.at(6.0, seed=3),
        lambda: plan.render(snr=6.0),
        lambda: next(plan.sweep([0.0, 6.0])),
        lambda: next(plan.monte_carlo(6.0, 2)),
    ):
        with pytest.raises(ValueError) as exc:
            draw()
        assert str(exc.value).startswith(why), str(exc.value)


def test_a_clean_scene_still_renders_what_it_can() -> None:
    # The baseline and a seed (which redraws a ranged gap) mean something on
    # a clean scene, so they are not refused.
    plan = _clean_plan()
    clean = Composer(type="tone", fs=1e6, num_samples=256).compose()
    assert np.array_equal(plan.render(), clean)
    assert plan.render(seed=5).shape == clean.shape


def test_a_noisy_scene_moves_its_floor() -> None:
    # The refusal's edge: one noisy source is enough, and at() honours snr.
    plan = prepare(Composer(type="tone", fs=1e6, num_samples=256, snr=10.0))
    lo, hi = plan.at(0.0, seed=1), plan.at(30.0, seed=1)
    assert np.mean(np.abs(lo - hi) ** 2) > 1e-3


# ── #1619 F6a: a FINITE data source through Plan ─────────────────────────────


def _data_scene(snr: float) -> Composer:
    """One bpsk source whose payload is drawn from a 40-bit data source,
    16 bits a frame with a CRC: its length is the frames', derived."""
    bits = np.array([1, 0, 1, 1, 0, 0, 1, 0] * 5, np.uint8)
    desc = FrameDesc()
    desc.add_data("payload", 16)
    desc.add_derived("crc", 16)
    desc.add_stage_over(STAGE_CRC16, "payload", "crc")
    return Composer(
        [
            Segment(
                type="bpsk",
                fs=1e6,
                sps=4,
                snr=snr,
                seed=11,
                data=bits,
                frame=desc,
                fill=np.array([0, 1], np.uint8),
            )
        ]
    )


def test_a_finite_data_source_plans_like_compose() -> None:
    """Plan accepts a finite data source, and at(snr, anchor_seed) is the
    compose at that SNR byte for byte. The noisy case is the BUNDLED path,
    whose noise copy must not keep the data source's borrowed pointers
    (wfm_plan.c drop_borrowed): they point into the composer plan_build
    destroys, and a later render would read freed memory."""
    plan = prepare(_data_scene(snr=12.0))
    ref = _data_scene(snr=6.0).compose()
    assert ref.size == 3 * (16 + 16) * 4  # 3 frames, derived
    np.testing.assert_array_equal(plan.at(6.0, plan.anchor_seed), ref)
    np.testing.assert_array_equal(plan.at(6.0), plan.at(6.0, plan.anchor_seed))


def test_a_continuous_dsss_data_source_plans_like_compose() -> None:
    """Continuous dsss over a finite data source: Plan derives the same
    run and renders the compose byte for byte (doppler#1719)."""
    code = np.array([1, 1, 0, 1, 0], np.uint8)
    bits = np.array([1, 0, 1, 1, 0, 0, 1, 0] * 3, np.uint8)

    def scene(snr: float) -> Composer:
        return Composer(
            [
                Segment(
                    type="dsss",
                    fs=1e6,
                    sps=2,
                    snr=snr,
                    seed=5,
                    data_code=code,
                    symbol_rate=1e6 / 2 / 3.7,
                    data=bits,
                )
            ]
        )

    plan = prepare(scene(12.0))
    ref = scene(6.0).compose()
    np.testing.assert_array_equal(plan.at(6.0, plan.anchor_seed), ref)
