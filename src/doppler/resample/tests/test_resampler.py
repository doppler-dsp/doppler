"""Tests for doppler.resample.Resampler."""

import numpy as np
import pytest

from doppler.resample import (
    Resampler,
    _build_bank,
    _kaiser_num_taps,
    _num_phases_for_rejection,
)

# ------------------------------------------------------------------ #
# Helpers                                                              #
# ------------------------------------------------------------------ #


def _ones(n: int) -> np.ndarray:
    return np.ones(n, dtype=np.complex64)


def _ramp(n: int) -> np.ndarray:
    return (
        np.arange(n, dtype=np.float32) + 1j * -np.arange(n, dtype=np.float32)
    ).astype(np.complex64)


# ------------------------------------------------------------------ #
# Construction                                                         #
# ------------------------------------------------------------------ #


class TestConstruction:
    def test_default_properties(self):
        r = Resampler(1.0)
        assert r.rate == 1.0
        assert r.num_phases == 4096
        assert r.num_taps == 19

    def test_rate_stored(self):
        r = Resampler(0.5)
        assert r.rate == 0.5

    def test_rate_setter(self):
        r = Resampler(1.0)
        r.rate = 2.0
        assert r.rate == 2.0

    def test_kaiser_path_changes_properties(self):
        # Build a custom 80 dB bank and pass it explicitly.
        # 80 dB → num_phases >= 10^4 = 10000, round up to 16384
        num_phases = _num_phases_for_rejection(80.0)
        num_taps = _kaiser_num_taps(num_phases, 80.0, 0.4, 0.6)
        bank = _build_bank(num_phases, num_taps, 80.0, 0.4, 0.6)
        r = Resampler(1.0, bank=bank)
        assert r.num_phases == 16384
        assert r.num_taps > 19

    def test_custom_bank_path(self):
        bank = (
            np.random.default_rng(0)
            .standard_normal((256, 11))
            .astype(np.float32)
        )
        r = Resampler(1.0, bank=bank)
        assert r.num_phases == 256
        assert r.num_taps == 11

    def test_custom_bank_must_be_2d(self):
        with pytest.raises(ValueError, match="2-D"):
            Resampler(1.0, bank=np.ones(16, dtype=np.float32))

    def test_direct_construction(self):
        rs = Resampler(0.5)
        assert rs.rate == 0.5


# ------------------------------------------------------------------ #
# Unity-rate pass-through                                             #
# ------------------------------------------------------------------ #


class TestUnityRate:
    def test_output_count(self):
        r = Resampler(1.0)
        x = _ramp(64)
        y = r.execute(x)
        assert len(y) == 64

    def test_pass_through_values(self):
        r = Resampler(1.0)
        x = _ramp(64)
        y = r.execute(x)
        np.testing.assert_allclose(
            y.real, x.real, atol=1e-4, err_msg="real part mismatch"
        )
        np.testing.assert_allclose(
            y.imag, x.imag, atol=1e-4, err_msg="imag part mismatch"
        )

    def test_output_dtype(self):
        r = Resampler(1.0)
        y = r.execute(_ones(16))
        assert y.dtype == np.complex64


# ------------------------------------------------------------------ #
# Decimation                                                           #
# ------------------------------------------------------------------ #


class TestDecimation:
    def test_2x_output_count(self):
        r = Resampler(0.5)
        y = r.execute(_ones(128))
        assert 56 <= len(y) <= 64

    def test_3x_output_count(self):
        r = Resampler(1.0 / 3.0)
        y = r.execute(_ones(120))
        assert 30 <= len(y) <= 40

    def test_dc_passes(self):
        r = Resampler(0.5)
        x = _ones(512)
        y = r.execute(x)
        # DC (all-ones) input → output magnitude near 1.0 after startup
        np.testing.assert_allclose(np.abs(y[10:]), 1.0, atol=0.05)


# ------------------------------------------------------------------ #
# Interpolation                                                        #
# ------------------------------------------------------------------ #


class TestInterpolation:
    def test_2x_output_count(self):
        r = Resampler(2.0)
        y = r.execute(_ones(64))
        assert 120 <= len(y) <= 132

    def test_3x_output_count(self):
        r = Resampler(3.0)
        y = r.execute(_ones(64))
        assert 176 <= len(y) <= 200


# ------------------------------------------------------------------ #
# State continuity                                                     #
# ------------------------------------------------------------------ #


class TestStateContinuity:
    def test_decimation_across_blocks(self):
        r_full = Resampler(0.5)
        r_blk = Resampler(0.5)
        x = _ones(256)

        y_full = r_full.execute(x)
        y1 = r_blk.execute(x[:128])
        y2 = r_blk.execute(x[128:])
        y_blk = np.concatenate([y1, y2])

        assert abs(len(y_full) - len(y_blk)) <= 1

    def test_reset_zeroes_state(self):
        r = Resampler(0.5)
        y1 = r.execute(_ones(128))
        r.reset()
        y2 = r.execute(_ones(128))
        np.testing.assert_allclose(np.abs(y1), np.abs(y2), atol=1e-5)

    def test_reset_preserves_rate(self):
        r = Resampler(0.5)
        r.reset()
        assert r.rate == 0.5

    def test_rate_change_between_blocks(self):
        r = Resampler(0.5)
        r.execute(_ones(64))
        r.rate = 2.0
        y2 = r.execute(_ones(64))
        assert len(y2) >= 120


# ------------------------------------------------------------------ #
# execute_ctrl                                                         #
# ------------------------------------------------------------------ #


class TestExecuteCtrl:
    def test_unity_rate_zero_ctrl_count(self):
        r = Resampler(1.0)
        x = _ones(64)
        ctrl = np.zeros(64, dtype=np.complex64)
        y = r.execute_ctrl(x, ctrl)
        assert len(y) == 64

    def test_ctrl_too_short_raises(self):
        r = Resampler(1.0)
        x = _ones(64)
        ctrl = np.zeros(32, dtype=np.complex64)
        with pytest.raises(ValueError):
            r.execute_ctrl(x, ctrl)

    def test_positive_ctrl_increases_output(self):
        r_base = Resampler(1.0)
        r_ctrl = Resampler(1.0)
        x = _ones(128)
        ctrl = np.full(128, 0.5, dtype=np.complex64)
        y_base = r_base.execute(x)
        y_ctrl = r_ctrl.execute_ctrl(x, ctrl)
        assert len(y_ctrl) > len(y_base)

    def test_ctrl_accepts_float32(self):
        r = Resampler(1.0)
        x = _ones(64)
        ctrl = np.zeros(64, dtype=np.float32)
        y = r.execute_ctrl(x, ctrl)
        assert len(y) == 64

    def test_decimation_with_ctrl(self):
        r = Resampler(0.5)
        x = _ones(128)
        ctrl = np.zeros(128, dtype=np.complex64)
        y = r.execute_ctrl(x, ctrl)
        assert 56 <= len(y) <= 64


class TestOutParam:
    def test_execute_out_writes_into_callers_buffer(self):
        r = Resampler(1.0)
        x = _ones(64)
        out = np.zeros(max(r.execute_max_out(), len(x)), dtype=np.complex64)
        y = r.execute(x, out=out)
        assert np.shares_memory(y, out)

    def test_execute_out_undersized_raises(self):
        r = Resampler(1.0)
        with pytest.raises(ValueError):
            r.execute(_ones(64), out=np.zeros(1, dtype=np.complex64))

    def test_execute_ctrl_out_writes_into_callers_buffer(self):
        r = Resampler(1.0)
        x = _ones(64)
        ctrl = np.zeros(64, dtype=np.complex64)
        out = np.zeros(
            max(r.execute_ctrl_max_out(), len(x)), dtype=np.complex64
        )
        y = r.execute_ctrl(x, ctrl, out=out)
        assert np.shares_memory(y, out)

    def test_execute_ctrl_out_undersized_raises(self):
        r = Resampler(1.0)
        x = _ones(64)
        ctrl = np.zeros(64, dtype=np.complex64)
        with pytest.raises(ValueError):
            r.execute_ctrl(x, ctrl, out=np.zeros(1, dtype=np.complex64))


# ------------------------------------------------------------------ #
# Context manager                                                      #
# ------------------------------------------------------------------ #


class TestContextManager:
    def test_with_block(self):
        with Resampler(1.0) as rs:
            x = _ones(16)
            y = rs.execute(x)
            assert len(y) == 16
