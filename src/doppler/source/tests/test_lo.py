"""Tests for the LO Python extension.

Mirrors test_lo_core.c: lifecycle, DC tone, quarter-rate IQ,
phase continuity, ctrl-port FM shift, LUT accuracy, property accessors.
"""

import numpy as np
import pytest

import doppler.measure
from doppler.source import LO

TOL = 1e-3  # half a LUT bin at 2^16 resolution (~4.8e-5); 1e-3 is generous


# ── Lifecycle ────────────────────────────────────────────────────────


def test_create_defaults():
    obj = LO()
    assert obj is not None


def test_create_param():
    obj = LO(norm_freq=0.25)
    assert obj is not None


def test_context_manager():
    with LO(0.0):
        pass


def test_destroy():
    obj = LO(0.0)
    obj.destroy()


# ── DC tone ──────────────────────────────────────────────────────────


def test_dc_tone():
    """norm_freq = 0 → phase_inc = 0 → all outputs are 1 + 0j."""
    lo = LO(0.0)
    out = lo.steps(8)
    assert out.dtype == np.complex64
    np.testing.assert_allclose(
        out.real, np.ones(8, dtype=np.float32), atol=TOL
    )
    np.testing.assert_allclose(
        out.imag, np.zeros(8, dtype=np.float32), atol=TOL
    )


# ── Quarter-rate IQ ──────────────────────────────────────────────────


def test_quarter_rate_iq():
    """norm_freq = 0.25 → phase_inc = 0x40000000.

    Phasor emitted before increment, LUT maps:
      phase=0x00000000 → cos=1, sin=0  →  1 + 0j
      phase=0x40000000 → cos=0, sin=1  →  0 + 1j
      phase=0x80000000 → cos=-1, sin=0 → -1 + 0j
      phase=0xC0000000 → cos=0, sin=-1 →  0 - 1j
    """
    lo = LO(0.25)
    out = lo.steps(8)
    expected = np.array(
        [1 + 0j, 0 + 1j, -1 + 0j, 0 - 1j] * 2, dtype=np.complex64
    )
    np.testing.assert_allclose(out.real, expected.real, atol=TOL)
    np.testing.assert_allclose(out.imag, expected.imag, atol=TOL)


# ── Phase continuity ─────────────────────────────────────────────────


def test_phase_continuity():
    """Two consecutive blocks must match one block of double the length."""
    a = LO(0.1)
    b = LO(0.1)
    ref = a.steps(16).copy()
    blk0 = b.steps(8).copy()
    blk1 = b.steps(8).copy()
    np.testing.assert_allclose(blk0, ref[:8], atol=TOL)
    np.testing.assert_allclose(blk1, ref[8:], atol=TOL)


# ── ctrl-port FM shift ───────────────────────────────────────────────


def test_steps_ctrl_constant_shift():
    """Constant ctrl = 0.25 with base norm_freq=0 equals LO at 0.25."""
    lo_ctrl = LO(0.0)
    lo_ref = LO(0.25)
    ctrl = np.full(8, 0.25, dtype=np.float64)
    out_ctrl = lo_ctrl.steps_ctrl(ctrl)
    out_ref = lo_ref.steps(8)
    np.testing.assert_allclose(out_ctrl.real, out_ref.real, atol=TOL)
    np.testing.assert_allclose(out_ctrl.imag, out_ref.imag, atol=TOL)


def test_steps_ctrl_no_base_mutation():
    """steps_ctrl must not modify the base norm_freq."""
    lo = LO(0.0)
    ctrl = np.full(8, 0.25, dtype=np.float64)
    lo.steps_ctrl(ctrl)
    assert lo.norm_freq == 0.0


def test_steps_ctrl_output_length():
    """Output length equals len(ctrl)."""
    lo = LO(0.1)
    ctrl = np.zeros(16, dtype=np.float64)
    out = lo.steps_ctrl(ctrl)
    assert len(out) == 16
    assert out.dtype == np.complex64


# ── LUT accuracy ─────────────────────────────────────────────────────


def test_unit_magnitude():
    """Every phasor must lie on the unit circle: |out[k]|² ≈ 1."""
    lo = LO(0.1)
    out = lo.steps(64)
    mag2 = np.abs(out) ** 2
    np.testing.assert_allclose(
        mag2, np.ones(64, dtype=np.float32), atol=2 * TOL
    )


def test_quarter_rate_quadrature():
    """At quarter-rate, out[1] should equal j * out[0]."""
    lo = LO(0.25)
    out = lo.steps(2)
    np.testing.assert_allclose(
        [out[1].real, out[1].imag],
        [-out[0].imag, out[0].real],
        atol=TOL,
    )


# ── Spurious content ──────────────────────────────────────────────────
#
# lo_core.h guarantees SFDR >= 90 dBc at ANY frequency. That is a bound,
# not the typical ~96 dBc, and the two differ because the spur level is
# set by the LOW 16 bits of phase_inc -- the remainder the LUT index
# throws away -- rather than by the frequency. The header used to state
# ~96 dBc unqualified; these two tests are why it no longer does, and
# what stops it drifting back.
#
# Full characterisation, including the sweep these two points were
# chosen from: src/doppler/source/tests/validation/lo/results.md.

SFDR_BOUND_DBC = 90.0

# phase_inc = (carrier << 16) | remainder. The carrier keeps the tone
# clear of DC and Nyquist (both are excluded from a spur search, so a
# tone parked on either measures nonsense rather than the LO); the
# remainder is what actually sets the spur.
_SFDR_CARRIER = 12345


def _sfdr_dbc(remainder, n=1 << 16):
    """SFDR of the LO at a chosen phase_inc remainder, measured."""
    tone = doppler.measure.ToneMeasure(n=n, fs=1.0, dynamic_range_db=150.0)
    phase_inc = (_SFDR_CARRIER << 16) | remainder
    return tone.analyze_complex(LO(phase_inc / 2.0**32).steps(n)).sfdr_dbc


def test_sfdr_worst_case_meets_the_documented_bound():
    """The WORST remainder still clears 90 dBc.

    Half a LUT bin makes the truncation error alternate with period 2,
    which concentrates every bit of it into a single spur -- the
    classical 6.02*B - 3.92 = 92.40 dBc bound for a B-bit phase index,
    and the worst any frequency can do.
    """
    worst = _sfdr_dbc(0x8000)
    assert worst == pytest.approx(92.40, abs=0.2), (
        f"the worst-case spur moved: {worst:.2f} dBc, expected the "
        f"6.02*16 - 3.92 = 92.40 dBc phase-truncation bound"
    )
    assert worst >= SFDR_BOUND_DBC


def test_sfdr_typical_and_spur_free_cases():
    """The other two regimes, so the bound is not the whole story.

    A generic remainder gives the familiar ~96 dBc; a remainder of zero
    is no truncation at all and is limited only by float32.
    """
    typical = _sfdr_dbc(0x4D2F)
    assert typical == pytest.approx(96.33, abs=0.3)
    assert typical >= SFDR_BOUND_DBC

    # A whole number of LUT bins indexes the table exactly.
    assert _sfdr_dbc(0x0000) > 140.0


# ── Property accessors ────────────────────────────────────────────────


def test_property_norm_freq():
    lo = LO(0.25)
    assert lo.norm_freq == 0.25


def test_property_phase_inc():
    lo = LO(0.25)
    assert lo.phase_inc == 0x40000000


def test_property_phase_get_set():
    lo = LO(0.25)
    assert lo.phase == 0
    lo.phase = 0x80000000
    assert lo.phase == 0x80000000


def test_set_norm_freq():
    lo = LO(0.25)
    lo.norm_freq = 0.5
    assert lo.phase_inc == 0x80000000
    assert lo.phase == 0  # phase unchanged


def test_reset_zeroes_phase():
    lo = LO(0.5)
    _ = lo.steps(3)  # advance phase
    lo.reset()
    assert lo.phase == 0
    assert lo.phase_inc == 0x80000000  # norm_freq unchanged


# ── Large single-call output (#116 regression) ───────────────────────


def test_steps_large_n_no_overflow():
    """steps(n) for n past the internal default cap (65536) must not overflow
    the reuse buffer — it sizes to n. Regression for #116 (segfault at
    large n).
    """
    n = 393_216  # 96 * 4096; > LO_MAX_OUT, the issue's crash size
    y = LO(norm_freq=0.1).steps(n)
    assert y.shape == (n,)
    assert np.allclose(np.abs(y), 1.0, atol=TOL)  # unit-magnitude phasor


def test_steps_large_matches_chunked():
    """A single large steps() equals the same span pulled in chunks (the buffer
    grows but the phasor sequence is unchanged)."""
    n = 200_000
    big = LO(norm_freq=0.1).steps(n)
    lo = LO(norm_freq=0.1)
    chunked = np.concatenate([lo.steps(50_000), lo.steps(n - 50_000)])
    assert np.array_equal(big, chunked)


def test_steps_grows_then_reuses():
    """The buffer grows on a bigger call and is reused (no re-overflow) on a
    later smaller one."""
    lo = LO(norm_freq=0.1)
    assert lo.steps(100_000).shape == (100_000,)  # grow past the cap
    assert lo.steps(1024).shape == (1024,)  # smaller: reuse, no overflow
    assert lo.steps(300_000).shape == (300_000,)  # grow again


def test_steps_ctrl_large_n():
    """steps_ctrl sizes its buffer to the control-array length, not a fixed
    cap."""
    n = 200_000
    ctrl = np.zeros(n, dtype=np.float64)
    y = LO(norm_freq=0.1).steps_ctrl(ctrl)
    assert y.shape == (n,)
    assert np.allclose(np.abs(y), 1.0, atol=TOL)


def test_steps_ctrl_out_writes_into_callers_buffer():
    lo = LO(norm_freq=0.1)
    ctrl = np.zeros(64, dtype=np.float64)
    out = np.zeros(max(lo.steps_ctrl_max_out(), len(ctrl)), dtype=np.complex64)
    y = lo.steps_ctrl(ctrl, out=out)
    assert np.shares_memory(y, out)


def test_steps_ctrl_out_undersized_raises():
    lo = LO(norm_freq=0.1)
    with pytest.raises(ValueError):
        lo.steps_ctrl(
            np.zeros(64, dtype=np.float32), out=np.zeros(1, dtype=np.complex64)
        )


# ── No-aliasing regression (previously a real, live bug) ──────────────


def test_steps_no_aliasing_across_calls():
    """A previously-returned array must not change when a later call is
    made. The old cached-buffer scheme reused the same underlying memory
    for every no-out= call, so a second call silently overwrote the
    first call's already-returned data out from under any caller still
    holding a reference to it."""
    lo = LO(norm_freq=0.1)
    first = lo.steps(4)
    first_snapshot = first.copy()
    _ = lo.steps(4)
    np.testing.assert_array_equal(first, first_snapshot)


def test_steps_ctrl_no_aliasing_across_calls():
    lo = LO(norm_freq=0.0)
    ctrl1 = np.full(4, 0.25, dtype=np.float64)
    ctrl2 = np.full(4, 0.5, dtype=np.float64)
    first = lo.steps_ctrl(ctrl1)
    first_snapshot = first.copy()
    _ = lo.steps_ctrl(ctrl2)
    np.testing.assert_array_equal(first, first_snapshot)
