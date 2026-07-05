"""Tests for the LO Python extension.

Mirrors test_lo_core.c: lifecycle, DC tone, quarter-rate IQ,
phase continuity, ctrl-port FM shift, LUT accuracy, property accessors.
"""

import numpy as np
import pytest

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
    ctrl = np.full(8, 0.25, dtype=np.float32)
    out_ctrl = lo_ctrl.steps_ctrl(ctrl)
    out_ref = lo_ref.steps(8)
    np.testing.assert_allclose(out_ctrl.real, out_ref.real, atol=TOL)
    np.testing.assert_allclose(out_ctrl.imag, out_ref.imag, atol=TOL)


def test_steps_ctrl_no_base_mutation():
    """steps_ctrl must not modify the base norm_freq."""
    lo = LO(0.0)
    ctrl = np.full(8, 0.25, dtype=np.float32)
    lo.steps_ctrl(ctrl)
    assert lo.norm_freq == 0.0


def test_steps_ctrl_output_length():
    """Output length equals len(ctrl)."""
    lo = LO(0.1)
    ctrl = np.zeros(16, dtype=np.float32)
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
    ctrl = np.zeros(n, dtype=np.float32)
    y = LO(norm_freq=0.1).steps_ctrl(ctrl)
    assert y.shape == (n,)
    assert np.allclose(np.abs(y), 1.0, atol=TOL)


def test_steps_ctrl_out_writes_into_callers_buffer():
    lo = LO(norm_freq=0.1)
    ctrl = np.zeros(64, dtype=np.float32)
    out = np.zeros(max(lo.steps_ctrl_max_out(), len(ctrl)), dtype=np.complex64)
    y = lo.steps_ctrl(ctrl, out=out)
    assert np.shares_memory(y, out)


def test_steps_ctrl_out_undersized_raises():
    lo = LO(norm_freq=0.1)
    with pytest.raises(ValueError):
        lo.steps_ctrl(
            np.zeros(64, dtype=np.float32), out=np.zeros(1, dtype=np.complex64)
        )
