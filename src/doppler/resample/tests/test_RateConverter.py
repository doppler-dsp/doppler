"""Tests for the C-backed doppler.resample.RateConverter class."""

import math

import numpy as np
import pytest

from doppler.resample import MatchedRateConverter, RateConverter, rate_convert
from doppler.wfm import rrc_h


def _dc(n: int) -> np.ndarray:
    return np.ones(n, dtype=np.complex64)


def _csin(n: int, freq: float = 0.0) -> np.ndarray:
    t = np.arange(n, dtype=np.float64)
    return np.exp(1j * 2 * math.pi * freq * t).astype(np.complex64)


# ------------------------------------------------------------------ #
# Construction / teardown                                             #
# ------------------------------------------------------------------ #


def test_create_valid():
    rc = RateConverter(0.5)
    assert rc is not None


def test_invalid_rate_raises():
    with pytest.raises((ValueError, MemoryError)):
        RateConverter(0.0)
    with pytest.raises((ValueError, MemoryError)):
        RateConverter(-1.0)


def test_destroy():
    rc = RateConverter(0.5)
    rc.destroy()
    with pytest.raises(RuntimeError, match="destroyed"):
        rc.execute(_dc(4))


def test_context_manager():
    with RateConverter(0.5) as rc:
        y = rc.execute(_dc(64))
    assert len(y) == 32


# ------------------------------------------------------------------ #
# Stage selection                                                     #
# ------------------------------------------------------------------ #


def test_stages_interpolation():
    rc = RateConverter(2.0)
    assert len(rc.stages) == 1
    assert rc.stages[0].startswith("Resampler")


def test_stages_halfband_x1():
    rc = RateConverter(0.5)
    assert rc.stages == ["HalfbandDecimator"]


def test_stages_halfband_x2():
    rc = RateConverter(0.25)
    assert rc.stages == ["HalfbandDecimator", "HalfbandDecimator"]


def test_stages_cic():
    rc = RateConverter(0.125)
    assert rc.stages == ["CIC(8)"]


def test_stages_cic_comp():
    rc = RateConverter(0.125, compensate=1)
    assert rc.stages == ["CIC(8)+FIR"]


def test_stages_cic_resampler():
    rc = RateConverter(1.0 / 12.0)
    assert len(rc.stages) == 2
    assert rc.stages[0].startswith("CIC")
    assert rc.stages[1].startswith("Resampler")


def test_stages_small_non_integer():
    rc = RateConverter(1.0 / 3.0)
    assert len(rc.stages) == 1
    assert rc.stages[0].startswith("Resampler")


# ------------------------------------------------------------------ #
# Output length                                                       #
# ------------------------------------------------------------------ #


def test_execute_output_length_hb():
    rc = RateConverter(0.5)
    assert len(rc.execute(_dc(1024))) == 512


def test_execute_output_length_cic():
    rc = RateConverter(0.125)
    assert len(rc.execute(_dc(1024))) == 128


def test_execute_dtype():
    rc = RateConverter(0.5)
    y = rc.execute(_dc(64))
    assert y.dtype == np.complex64


# ------------------------------------------------------------------ #
# Properties                                                          #
# ------------------------------------------------------------------ #


def test_rate_readable():
    rc = RateConverter(0.25)
    assert rc.rate == pytest.approx(0.25)


def test_rate_set_rebuilds_cascade():
    rc = RateConverter(0.5)
    assert rc.stages == ["HalfbandDecimator"]
    rc.rate = 0.125
    assert rc.stages == ["CIC(8)"]
    assert len(rc.execute(_dc(1024))) == 128


# ------------------------------------------------------------------ #
# Reset                                                               #
# ------------------------------------------------------------------ #


def test_reset_reproducible():
    rc = RateConverter(0.5)
    x = _csin(64, freq=0.05)
    y1 = rc.execute(x)
    rc.reset()
    y2 = rc.execute(x)
    np.testing.assert_array_equal(y1, y2)


# ------------------------------------------------------------------ #
# Functional wrapper                                                  #
# ------------------------------------------------------------------ #


def test_rate_convert_returns_array_and_rc():
    y, rc = rate_convert(_dc(256), 0.5)
    assert isinstance(y, np.ndarray)
    assert isinstance(rc, RateConverter)
    assert len(y) == 128


def test_rate_convert_reuses_rc():
    _, rc1 = rate_convert(_dc(64), 0.5)
    _, rc2 = rate_convert(_dc(64), 0.5, rc1)
    assert rc2 is rc1


# ------------------------------------------------------------------ #
# Signal quality                                                      #
# ------------------------------------------------------------------ #


@pytest.mark.parametrize("rate", [0.5, 0.25, 0.125, 0.0625, 0.1, 2.0])
def test_dc_gain(rate):
    rc = RateConverter(rate)
    n_in = 4096 if rate <= 1.0 else 256
    y = rc.execute(_dc(n_in))
    settled = y[max(1, len(y) // 10) :]
    amp = float(np.mean(np.abs(settled)))
    assert 0.5 < amp < 2.0, f"rate={rate}: DC amplitude {amp:.3f}"


def test_alias_rejection_cic():
    rc = RateConverter(0.125)
    n = 8192
    x = _csin(n, freq=0.45)
    y = rc.execute(x)
    settled = y[len(y) // 4 :]
    in_power = float(np.mean(np.abs(_csin(len(settled), freq=0.45)) ** 2))
    out_power = float(np.mean(np.abs(settled) ** 2))
    rejection_db = 10 * math.log10(in_power / (out_power + 1e-30))
    assert rejection_db >= 20.0, f"alias rejection {rejection_db:.1f} dB"


def test_compensate_reduces_passband_droop():
    rate = 0.0625
    rc_plain = RateConverter(rate, compensate=0)
    rc_comp = RateConverter(rate, compensate=1)
    n = 8192
    f_in = 0.1 * rate
    x = _csin(n, freq=f_in)
    y_plain = rc_plain.execute(x)
    y_comp = rc_comp.execute(x)
    skip = len(y_plain) // 4
    amp_plain = float(np.mean(np.abs(y_plain[skip:])))
    amp_comp = float(np.mean(np.abs(y_comp[skip:])))
    assert abs(amp_comp - 1.0) <= abs(amp_plain - 1.0) + 0.02, (
        f"compensate=1 not flatter: plain={amp_plain:.4f} comp={amp_comp:.4f}"
    )


def test_rateconverter_state_roundtrip_resume():
    """Serializable (elastic) face: serialize the cascade mid-stream, restore
    into a fresh RateConverter at the same rate, and resume — the continuation
    matches an uninterrupted run bit-for-bit; a wrong-size or clobbered blob is
    rejected."""
    rng = np.random.default_rng(11)
    x = (rng.standard_normal(3000) + 1j * rng.standard_normal(3000)).astype(
        np.complex64
    )
    cut = 1100

    ref = RateConverter(0.5)
    ref.execute(x[:cut])
    tail = np.array(ref.execute(x[cut:]))  # copy: execute returns a view

    a = RateConverter(0.5)
    a.execute(x[:cut])
    blob = a.get_state()
    assert isinstance(blob, bytes) and len(blob) == a.state_bytes()

    b = RateConverter(0.5)
    b.set_state(blob)
    assert np.array_equal(np.array(b.execute(x[cut:])), tail)

    with pytest.raises(ValueError):
        b.set_state(blob[:-1])
    with pytest.raises(ValueError):
        b.set_state(bytes([blob[0] ^ 0xFF]) + blob[1:])


def test_execute_out_writes_into_callers_buffer():
    rc = RateConverter(0.5)
    x = _dc(256)
    out = np.zeros(
        max(rc.execute_max_out(), int(len(x) * 0.5) + 4), dtype=np.complex64
    )
    y = rc.execute(x, out=out)
    assert np.shares_memory(y, out)
    assert len(y) == 128


def test_execute_out_undersized_raises():
    rc = RateConverter(0.5)
    with pytest.raises(ValueError):
        rc.execute(_dc(256), out=np.zeros(1, dtype=np.complex64))


def test_execute_out_unconvertible_raises():
    rc = RateConverter(0.5)
    with pytest.raises((TypeError, ValueError)):
        rc.execute(_dc(256), out="not an array")


def test_execute_max_out_after_destroy_raises():
    rc = RateConverter(0.5)
    rc.destroy()
    with pytest.raises(RuntimeError, match="destroyed"):
        rc.execute_max_out()


def test_execute_returned_view_survives_rate_change():
    # gh-219: set_rate must retire, not free, the execute buffer -- a
    # previously returned view must stay valid (not silently corrupted by
    # a reused allocation) after a rate change invalidates the buffer.
    rc = RateConverter(0.5)
    y1 = rc.execute(_dc(64))  # view into the (about-to-be-invalidated) buffer
    snapshot = np.array(y1)  # copy for comparison, taken while still valid
    rc.rate = 0.25
    # allocator pressure: if the old buffer were freed (not retired), one of
    # these allocations is likely to reuse that memory and corrupt y1.
    junk = [bytearray(256) for _ in range(64)]
    y2 = rc.execute(_dc(64))
    del junk
    assert np.array_equal(y1, snapshot)
    assert len(y2) == 16


# ------------------------------------------------------------------ #
# Matched-filter terminal bank (pulse=)                               #
# ------------------------------------------------------------------ #

_MF_BETA = 0.35
_MF_SPAN = 8
_MF_NSYM = 500


def _rrc_bpsk(sps: float, phi: float, seed: int = 7) -> tuple:
    """RRC-shaped BPSK plus the symbols that made it.

    Amplitude stays well inside ``cic_core``'s Q15 full scale: a CIC stage
    quantizes at its boundary, so overdriving it clips and every measured
    EVM collapses for reasons that have nothing to do with the filter.
    """
    syms = np.where(
        np.random.default_rng(seed).integers(0, 2, _MF_NSYM) > 0, 1.0, -1.0
    )
    n = int(_MF_NSYM * sps) + 64
    idx = np.arange(n, dtype=np.float64)
    x = np.zeros(n, dtype=np.float64)
    for k, a in enumerate(syms):
        t = (idx - (k + _MF_SPAN) * sps) / sps - phi
        near = np.abs(t) <= _MF_SPAN
        x[near] += a * rrc_h(t[near], _MF_BETA)
    return (0.25 * x).astype(np.complex64), syms


def _best_evm_db(y: np.ndarray, syms: np.ndarray) -> float:
    """Min EVM over strobe alignment.

    Open loop the cascade's strobe phase is arbitrary, so minimising over
    alignment isolates the matched filter from the timing loop that does not
    exist yet (that is Layer 2's job, ``track.RateSync``).
    """
    best = np.inf
    ref = syms[40 : _MF_NSYM - 40]
    for par in (0, 1):
        for lag in range(140):
            idx = lag + par + 2 * np.arange(40, _MF_NSYM - 40)
            if idx[-1] >= len(y):
                break
            got = y[idx]
            g = float(np.dot(ref, got.real) / len(ref))
            if g == 0.0:
                continue
            err = got - g * ref
            best = min(
                best,
                float(np.linalg.norm(err) / (abs(g) * math.sqrt(len(ref)))),
            )
    return 20 * math.log10(best)


def _sweep_evm_db(sps: float, compensate: int) -> float:
    return min(
        _best_evm_db(
            MatchedRateConverter(
                rate=2 / sps,
                compensate=compensate,
                pulse="rrc",
                beta=_MF_BETA,
                span=_MF_SPAN,
                pulse_sps=2.0,
            ).execute(x),
            syms,
        )
        for x, syms in (_rrc_bpsk(sps, j / 8.0) for j in range(8))
    )


def test_pulse_forces_a_steerable_terminal_stage():
    # The plain planner drops the fractional stage when the integer stages
    # already land the rate -- leaving nothing for a timing loop to steer.
    assert RateConverter(rate=2 / 64).stages == ["CIC(32)"]
    assert MatchedRateConverter(rate=2 / 64, pulse="rrc").stages == [
        "CIC(32)",
        "Resampler(1,rrc)",
    ]
    # ...and the label names the pulse, so the matched filter is visibly IN
    # the cascade rather than a stage still to be appended.
    for sps in (4, 8, 16, 17.333333333, 64, 256):
        assert (
            MatchedRateConverter(rate=2 / sps, pulse="rrc")
            .stages[-1]
            .endswith(",rrc)")
        )
    assert (
        MatchedRateConverter(rate=2 / 17.3, pulse="iandd")
        .stages[-1]
        .endswith(",iandd)")
    )


@pytest.mark.parametrize("pulse", ["sinc", "RRC", ""])
def test_unknown_pulse_raises(pulse):
    with pytest.raises(ValueError, match="pulse must be"):
        MatchedRateConverter(rate=0.5, pulse=pulse)


@pytest.mark.parametrize(
    "kw",
    [
        {"beta": 2.0},
        {"beta": -0.1},
        {"span": 0},
        {"pulse_sps": 0.0},
        {"num_phases": 1000},  # not a power of two
        {"num_phases": 1},
    ],
)
def test_invalid_matched_params_are_rejected(kw):
    """Every out-of-range parameter is refused with the parent's ValueError.

    A flavor's constructor has strictly MORE parameters than its parent's, so
    strictly more ways to fail on a bad argument — and it now reports them the
    same way. Until jm 0.33.13 a view inherited none of its parent's
    ``create_error`` translation and these surfaced as a blanket MemoryError
    (doppler-filed, jm gh-580).
    """
    with pytest.raises(ValueError):
        MatchedRateConverter(rate=0.5, pulse="rrc", **kw)


def test_plain_ctor_still_translates_its_errors():
    """The parent keeps its create_error, so the gap is the flavor's only."""
    with pytest.raises(ValueError):
        RateConverter(rate=-1.0)


def test_droop_compensation_folds_into_the_bank():
    # No extra stage, and no "+FIR" on the CIC label: the compensator is a
    # per-arm convolution on the terminal bank's own tap grid.
    off = MatchedRateConverter(
        rate=2 / 17.333333333, pulse="rrc", compensate=0
    )
    on = MatchedRateConverter(rate=2 / 17.333333333, pulse="rrc", compensate=1)
    assert len(on.stages) == len(off.stages)
    assert "FIR" not in on.stages[0]
    # And it is worth ~28 dB on a CIC cascade -- not a refinement.
    assert _sweep_evm_db(17.333333333, 1) < -45.0
    assert (
        _sweep_evm_db(17.333333333, 1) < _sweep_evm_db(17.333333333, 0) - 20.0
    )


def test_matched_cascade_recovers_symbols_at_arbitrary_rate():
    # sps is not an integer and not a ratio of small integers: the cascade
    # decimates by 8 in the CIC and takes the 0.923 remainder on the bank.
    assert _sweep_evm_db(17.333333333, 1) < -45.0
    # A halfband cascade has no quantizing stage, so it shows what the bank
    # itself is worth.
    assert _sweep_evm_db(4.0, 0) < -55.0


def test_bank_is_sized_by_the_post_decimation_rate():
    # Matched-filtering at the INPUT rate would cost taps proportional to the
    # input samples per symbol (thousands at sps=256, tens of MB of bank).
    # Sized by the terminal stage's rate it is constant in the input rate.
    # compensate=0 on both so the comparison isolates the sizing: the droop
    # fold adds its taps only on a plan that HAS a CIC (2/256 does, 2/4 does
    # not), which would otherwise show up as a difference here.
    arms4, n4 = MatchedRateConverter(
        rate=2 / 4, pulse="rrc", compensate=0
    ).bank_shape
    arms256, n256 = MatchedRateConverter(
        rate=2 / 256, pulse="rrc", compensate=0
    ).bank_shape
    assert (arms4, n4) == (arms256, n256) == (1024, n256)
    assert n256 < 4 * _MF_SPAN * 2 + 8
    # The fold costs a handful of taps per arm and no extra stage.
    folded = MatchedRateConverter(rate=2 / 256, pulse="rrc").bank_shape[1]
    assert n256 < folded < n256 + 16
    # The rectangle is one symbol wide whatever span says -- smaller still.
    assert (
        MatchedRateConverter(
            rate=2 / 256, pulse="iandd", compensate=0
        ).bank_shape[1]
        < n256
    )
    # An integer-only cascade has no bank at all.
    assert RateConverter(rate=2 / 64).bank_shape == []


@pytest.mark.parametrize("sps", [4.0, 17.333333333, 64.0])
def test_execute_ctrl_push_matches_block_and_execute(sps):
    # The per-input streaming form is the only one a closed loop can use (it
    # computes each correction FROM the outputs already emitted), so it has to
    # agree with the block form -- otherwise open and closed loop would be
    # running different filters.
    x, _ = _rrc_bpsk(sps, 0.3)
    kw = {"rate": 2 / sps, "compensate": 1, "pulse": "rrc"}
    block = np.array(MatchedRateConverter(**kw).execute_ctrl(x, 0.0))
    rc = MatchedRateConverter(**kw)
    push = np.concatenate(
        [rc.execute_ctrl_push(complex(v), 0.0) for v in x]
        + [np.empty(0, np.complex64)]
    )
    assert np.array_equal(block, push)
    # execute() on a matched cascade must be the SAME algorithm: the pulse
    # bank is laid out for the unified accumulator, while resamp's decimating
    # path is transposed-form and indexes arms the other way.
    assert np.array_equal(
        block, np.array(MatchedRateConverter(**kw).execute(x))
    )


def test_matched_state_round_trips_mid_stream():
    x, _ = _rrc_bpsk(17.333333333, 0.2)
    kw = {"rate": 2 / 17.333333333, "compensate": 1, "pulse": "rrc"}
    a, b = MatchedRateConverter(**kw), MatchedRateConverter(**kw)
    half = len(x) // 2
    a.execute(x[:half])
    b.set_state(a.get_state())
    assert np.array_equal(
        np.array(a.execute(x[half:])), np.array(b.execute(x[half:]))
    )


def test_set_rate_keeps_the_pulse():
    # The pulse is configuration, not part of the plan, so re-planning must
    # keep it -- including the always-append rule and the fold.
    rc = MatchedRateConverter(rate=2 / 17.333333333, compensate=1, pulse="rrc")
    rc.rate = 2 / 64
    assert rc.stages == ["CIC(32)", "Resampler(1,rrc)"]
    assert "FIR" not in rc.stages[0]


# ------------------------------------------------------------------ #
# Input amplitude bound (inherited from a planned CIC stage)          #
# ------------------------------------------------------------------ #


@pytest.mark.parametrize(
    "rate,has_cic",
    [
        (0.5, False),  # HalfbandDecimator
        (0.25, False),  # two halfbands
        (1 / 3, False),  # plain Resampler
        (2.0, False),  # interpolating Resampler
        (0.125, True),  # CIC(8)
        (0.1, True),  # CIC(8) + Resampler(0.8)
        (1 / 64, True),  # CIC(64)
    ],
)
def test_input_bound_applies_exactly_when_a_cic_is_planned(rate, has_cic):
    # `stages` is the documented way to tell whether this cascade is
    # scale-free, so the docs' rule has to be the code's rule.
    assert any("CIC" in s for s in RateConverter(rate=rate).stages) is has_cic

    # Drive it 4x past the bound and see whether the gain holds.
    y = RateConverter(rate=rate).execute(
        np.full(8192, 8.0, dtype=np.complex64)
    )
    gain = float(y[-1].real) / 8.0
    if has_cic:
        # Clipped at the boundary, which is CIC_PAPR_HEADROOM (2.0, 6 dB
        # above unity) rather than 1.0 -- that headroom is the whole point
        # of the encode scale being 32768/CIC_PAPR_HEADROOM.
        assert float(y[-1].real) == pytest.approx(2.0, rel=1e-2)
        assert gain == pytest.approx(0.25, rel=1e-2)
    else:
        assert gain == pytest.approx(1.0, rel=1e-2)


def test_input_bound_is_silent_not_an_error():
    # No exception and no NaN -- the failure mode this documents is that the
    # output looks entirely plausible.
    y = RateConverter(rate=0.1).execute(
        np.full(8192, 50.0, dtype=np.complex64)
    )
    assert np.all(np.isfinite(y))
    # Saturates at the CIC_PAPR_HEADROOM bound (2.0), not at unity.
    assert abs(float(y[-1].real) - 2.0) < 0.01


def test_clipped_reports_the_bound_the_cascade_hides():
    # RateConverter plans a CIC without saying so, so it has to surface the
    # bound that CIC brings with it.
    rc = RateConverter(rate=0.1)
    assert any("CIC" in s for s in rc.stages)
    assert rc.clipped is False
    rc.execute(np.full(4096, 0.5, dtype=np.complex64))
    assert rc.clipped is False
    rc.execute(np.full(4096, 4.0, dtype=np.complex64))
    assert rc.clipped is True
    rc.execute(np.full(4096, 0.5, dtype=np.complex64))
    assert rc.clipped is True  # sticky
    rc.reset()
    assert rc.clipped is False


def test_clipped_is_false_for_a_scale_free_plan():
    # No CIC, no bound -- and False is the honest answer, not a stub.
    rc = RateConverter(rate=0.5)
    assert not any("CIC" in s for s in rc.stages)
    rc.execute(np.full(4096, 1000.0, dtype=np.complex64))
    assert rc.clipped is False


def test_matched_cascade_surfaces_clipping_too():
    # The measured -25 dB EVM detour that motivated all of this: an
    # overdriven matched cascade now says so.
    rc = MatchedRateConverter(rate=2 / 17.333333333, compensate=1, pulse="rrc")
    x, _ = _rrc_bpsk(17.333333333, 0.0)
    rc.execute(x)
    assert rc.clipped is False
    rc.execute((x * 8).astype(np.complex64))
    assert rc.clipped is True
