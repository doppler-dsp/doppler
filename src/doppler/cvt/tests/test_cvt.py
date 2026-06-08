"""Signal-level tests for doppler.cvt converters.

Covers converters:

  F32ToI16      float  -> int16    (saturate, round-to-nearest)
  I16ToF32      int16  -> float    (multiply by 1/scale)
  F32ToI16U32   float  -> uint32   (Q15 zero-extended into lower 16 bits)
  F32ToI16U64   float  -> uint64   (Q15 zero-extended into lower 16 bits)
  I16U32ToF32   uint32 -> float    (lower 16 bits as signed int16)
  I16U64ToF32   uint64 -> float    (lower 16 bits as signed int16)
  F32ToUQ15     float  -> uint16   (offset-binary: 0.0 -> 32768)
  UQ15ToF32     uint16 -> float    (inverse offset-binary)

Key invariants:
  - I16ToF32( F32ToI16(x) ) ≈ x  for |x| < 1  (roundtrip within Q15 precision)
  - I16U32ToF32( F32ToI16U32(x) ) ≈ x          (same but via uint32 carrier)
  - I16U64ToF32( F32ToI16U64(x) ) ≈ x          (same but via uint64 carrier)
  - UQ15ToF32( F32ToUQ15(x) ) ≈ x              (offset-binary roundtrip)
  - Upper 16 bits of F32ToI16U32 output are always zero
  - Upper 48 bits of F32ToI16U64 output are always zero
  - steps() == per-sample step() loop for all types
"""

from __future__ import annotations

import numpy as np
import pytest

from doppler.cvt import (
    F32ToI16,
    F32ToI16U32,
    F32ToI16U64,
    F32ToUQ15,
    I16ToF32,
    I16U32ToF32,
    I16U64ToF32,
    UQ15ToF32,
)

SCALE = 32768.0
Q15_MAX = 32767
Q15_MIN = -32768


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def _ramp(n: int = 128) -> np.ndarray:
    """Float ramp from -1.5 to +1.5, covering saturation boundaries."""
    return np.linspace(-1.5, 1.5, n, dtype=np.float32)


def _q15_ramp(n: int = 64) -> np.ndarray:
    """int16 ramp from Q15_MIN to Q15_MAX."""
    return np.linspace(Q15_MIN, Q15_MAX, n, dtype=np.int16)


# ---------------------------------------------------------------------------
# F32ToI16
# ---------------------------------------------------------------------------


class TestF32ToI16:
    def test_positive_unity(self):
        """1.0 * 32768 = 32768, but clamped to 32767."""
        obj = F32ToI16()
        assert obj.step(1.0) == Q15_MAX

    def test_negative_unity(self):
        """-1.0 * 32768 = -32768, which fits in int16 exactly."""
        obj = F32ToI16()
        assert obj.step(-1.0) == Q15_MIN

    def test_zero(self):
        obj = F32ToI16()
        assert obj.step(0.0) == 0

    def test_saturation_positive(self):
        """Values above 1.0 saturate to Q15_MAX."""
        obj = F32ToI16()
        assert obj.step(2.0) == Q15_MAX
        assert obj.step(100.0) == Q15_MAX

    def test_saturation_negative(self):
        """Values below -1.0 saturate to Q15_MIN."""
        obj = F32ToI16()
        assert obj.step(-2.0) == Q15_MIN
        assert obj.step(-100.0) == Q15_MIN

    def test_round_to_nearest(self):
        """0.5 LSB = 0.5 / 32768; should round away from zero."""
        obj = F32ToI16()
        half_lsb = 0.5 / SCALE
        # 0 + 0.5 LSB rounds to 1
        assert obj.step(half_lsb) == 1
        # 0 - 0.5 LSB rounds to -1
        assert obj.step(-half_lsb) == -1

    def test_custom_scale(self):
        """Scale=1.0 maps [-1, +1] float directly to integer range [-1, +1]."""
        obj = F32ToI16(scale=1.0)
        assert obj.step(1.0) == 1
        assert obj.step(-1.0) == -1
        assert obj.step(0.7) == 1  # rounds to nearest
        assert obj.step(0.4) == 0

    def test_steps_agrees_with_step(self):
        obj = F32ToI16()
        x = _ramp()
        expected = np.array([obj.step(float(v)) for v in x], dtype=np.int16)
        obj2 = F32ToI16()
        got = obj2.steps(x)
        np.testing.assert_array_equal(got, expected)

    def test_steps_dtype(self):
        obj = F32ToI16()
        y = obj.steps(np.zeros(8, dtype=np.float32))
        assert y.dtype == np.int16

    def test_steps_out_buffer(self):
        obj = F32ToI16()
        x = np.linspace(-1.0, 1.0, 16, dtype=np.float32)
        buf = np.zeros(16, dtype=np.int16)
        ret = obj.steps(x, buf)
        assert ret is buf
        np.testing.assert_array_equal(ret, F32ToI16().steps(x))

    def test_invalid_scale(self):
        """scale <= 0 should raise MemoryError (create returns NULL)."""
        with pytest.raises((MemoryError, ValueError)):
            F32ToI16(scale=0.0)
        with pytest.raises((MemoryError, ValueError)):
            F32ToI16(scale=-1.0)

    def test_context_manager(self):
        with F32ToI16() as obj:
            assert obj.step(0.5) == Q15_MAX // 2 + 1  # 16384

    def test_destroy(self):
        obj = F32ToI16()
        obj.destroy()
        with pytest.raises(RuntimeError, match="destroyed"):
            obj.step(0.0)


# ---------------------------------------------------------------------------
# I16ToF32
# ---------------------------------------------------------------------------


class TestI16ToF32:
    def test_positive_max(self):
        obj = I16ToF32()
        y = obj.step(Q15_MAX)
        np.testing.assert_allclose(y, Q15_MAX / SCALE, rtol=1e-6)

    def test_negative_min(self):
        obj = I16ToF32()
        y = obj.step(Q15_MIN)
        np.testing.assert_allclose(y, Q15_MIN / SCALE, rtol=1e-6)

    def test_zero(self):
        obj = I16ToF32()
        assert obj.step(0) == pytest.approx(0.0)

    def test_custom_scale(self):
        obj = I16ToF32(scale=1.0)
        assert obj.step(100) == pytest.approx(100.0)
        assert obj.step(-50) == pytest.approx(-50.0)

    def test_steps_dtype(self):
        obj = I16ToF32()
        y = obj.steps(np.zeros(8, dtype=np.int16))
        assert y.dtype == np.float32

    def test_steps_agrees_with_step(self):
        obj = I16ToF32()
        x = _q15_ramp()
        expected = np.array([obj.step(int(v)) for v in x], dtype=np.float32)
        obj2 = I16ToF32()
        np.testing.assert_array_almost_equal(
            obj2.steps(x), expected, decimal=6
        )

    def test_invalid_scale(self):
        with pytest.raises((MemoryError, ValueError)):
            I16ToF32(scale=0.0)


# ---------------------------------------------------------------------------
# Roundtrip: F32ToI16 ↔ I16ToF32
# ---------------------------------------------------------------------------


class TestRoundtrip:
    """F32 → int16 → F32 roundtrip within Q15 quantisation error."""

    def test_roundtrip_f32_i16_f32(self):
        """Roundtrip error must be ≤ 0.5 LSB = 1/(2*32768)."""
        enc = F32ToI16()
        dec = I16ToF32()
        x = np.linspace(-0.99, 0.99, 256, dtype=np.float32)
        y = dec.steps(enc.steps(x))
        error = np.max(np.abs(x - y))
        assert error <= 1.0 / SCALE + 1e-7, (
            f"Roundtrip error {error:.2e} exceeds 0.5 LSB = {1 / SCALE:.2e}"
        )


# ---------------------------------------------------------------------------
# F32ToI16U32
# ---------------------------------------------------------------------------


class TestF32ToI16U32:
    def test_positive_unity(self):
        obj = F32ToI16U32()
        y = obj.step(1.0)
        assert y == Q15_MAX  # 0x00007FFF

    def test_negative_unity(self):
        obj = F32ToI16U32()
        y = obj.step(-1.0)
        # -32768 as int16 → 0x8000 as uint16 → 0x00008000 as uint32
        assert y == 0x00008000

    def test_upper_bits_always_zero(self):
        obj = F32ToI16U32()
        x = np.linspace(-1.5, 1.5, 256, dtype=np.float32)
        y = obj.steps(x)
        assert np.all((y & 0xFFFF0000) == 0), "Upper 16 bits must be zero"

    def test_lower_16_matches_f32_to_i16(self):
        """Lower 16 bits of F32ToI16U32 must match F32ToI16 bit-for-bit."""
        x = np.linspace(-1.5, 1.5, 256, dtype=np.float32)
        y_u32 = F32ToI16U32().steps(x)
        y_i16 = F32ToI16().steps(x)
        # Extract lower 16 bits of uint32 as uint16, view as int16
        lower = (y_u32 & 0xFFFF).astype(np.uint16).view(np.int16)
        np.testing.assert_array_equal(lower, y_i16)

    def test_steps_dtype(self):
        obj = F32ToI16U32()
        y = obj.steps(np.zeros(8, dtype=np.float32))
        assert y.dtype == np.uint32

    def test_steps_out_buffer(self):
        obj = F32ToI16U32()
        x = np.linspace(-1.0, 1.0, 16, dtype=np.float32)
        buf = np.zeros(16, dtype=np.uint32)
        ret = obj.steps(x, buf)
        assert ret is buf

    def test_roundtrip_via_i16u32_to_f32(self):
        """F32ToI16U32 → I16U32ToF32 roundtrip ≤ 0.5 LSB."""
        enc = F32ToI16U32()
        dec = I16U32ToF32()
        x = np.linspace(-0.99, 0.99, 256, dtype=np.float32)
        y = dec.steps(enc.steps(x))
        error = float(np.max(np.abs(x - y)))
        assert error <= 1.0 / SCALE + 1e-7


# ---------------------------------------------------------------------------
# F32ToI16U64
# ---------------------------------------------------------------------------


class TestF32ToI16U64:
    def test_positive_unity(self):
        obj = F32ToI16U64()
        assert obj.step(1.0) == Q15_MAX

    def test_negative_unity(self):
        obj = F32ToI16U64()
        assert obj.step(-1.0) == 0x0000000000008000

    def test_upper_bits_always_zero(self):
        obj = F32ToI16U64()
        x = np.linspace(-1.5, 1.5, 256, dtype=np.float32)
        y = obj.steps(x)
        assert np.all((y & 0xFFFFFFFFFFFF0000) == 0), (
            "Upper 48 bits must be zero"
        )

    def test_lower_16_matches_f32_to_i16(self):
        x = np.linspace(-1.5, 1.5, 256, dtype=np.float32)
        y_u64 = F32ToI16U64().steps(x)
        y_i16 = F32ToI16().steps(x)
        lower = (y_u64 & 0xFFFF).astype(np.uint16).view(np.int16)
        np.testing.assert_array_equal(lower, y_i16)

    def test_steps_dtype(self):
        obj = F32ToI16U64()
        y = obj.steps(np.zeros(8, dtype=np.float32))
        assert y.dtype == np.uint64

    def test_roundtrip_via_i16u64_to_f32(self):
        enc = F32ToI16U64()
        dec = I16U64ToF32()
        x = np.linspace(-0.99, 0.99, 256, dtype=np.float32)
        y = dec.steps(enc.steps(x))
        error = float(np.max(np.abs(x - y)))
        assert error <= 1.0 / SCALE + 1e-7


# ---------------------------------------------------------------------------
# I16U32ToF32
# ---------------------------------------------------------------------------


class TestI16U32ToF32:
    def test_positive_q15max(self):
        """0x00007FFF (32767 as int16) → 32767/32768 ≈ 0.9999..."""
        obj = I16U32ToF32()
        y = obj.step(0x00007FFF)
        np.testing.assert_allclose(y, Q15_MAX / SCALE, rtol=1e-6)

    def test_negative_q15min(self):
        """0x00008000 (−32768 as two's complement) → -1.0."""
        obj = I16U32ToF32()
        y = obj.step(0x00008000)
        np.testing.assert_allclose(y, Q15_MIN / SCALE, rtol=1e-6)

    def test_upper_bits_ignored(self):
        """Upper 16 bits of the uint32 carrier must be ignored."""
        obj = I16U32ToF32()
        a = obj.step(0x00007FFF)
        b = obj.step(0xDEAD7FFF)  # same lower 16, junk upper 16
        assert a == pytest.approx(b, abs=1e-7)

    def test_zero(self):
        obj = I16U32ToF32()
        assert obj.step(0) == pytest.approx(0.0)

    def test_steps_dtype(self):
        obj = I16U32ToF32()
        y = obj.steps(np.zeros(8, dtype=np.uint32))
        assert y.dtype == np.float32

    def test_steps_out_buffer(self):
        obj = I16U32ToF32()
        x = np.array([0, 0x8000, 0x7FFF, 0x4000], dtype=np.uint32)
        buf = np.zeros(4, dtype=np.float32)
        ret = obj.steps(x, buf)
        assert ret is buf


# ---------------------------------------------------------------------------
# I16U64ToF32
# ---------------------------------------------------------------------------


class TestI16U64ToF32:
    def test_positive_q15max(self):
        obj = I16U64ToF32()
        y = obj.step(0x0000000000007FFF)
        np.testing.assert_allclose(y, Q15_MAX / SCALE, rtol=1e-6)

    def test_negative_q15min(self):
        obj = I16U64ToF32()
        y = obj.step(0x0000000000008000)
        np.testing.assert_allclose(y, Q15_MIN / SCALE, rtol=1e-6)

    def test_upper_bits_ignored(self):
        obj = I16U64ToF32()
        a = obj.step(0x0000000000007FFF)
        b = obj.step(0xDEADBEEFCAFE7FFF)
        assert a == pytest.approx(b, abs=1e-7)

    def test_steps_dtype(self):
        obj = I16U64ToF32()
        y = obj.steps(np.zeros(8, dtype=np.uint64))
        assert y.dtype == np.float32


# ---------------------------------------------------------------------------
# F32ToUQ15
# ---------------------------------------------------------------------------


class TestF32ToUQ15:
    """Offset-binary encoding: 0.0 → 32768, -1.0 → 0, +32767/32768 → 65535."""

    def test_zero_maps_to_midscale(self):
        assert F32ToUQ15().step(0.0) == 32768

    def test_negative_unity_maps_to_zero(self):
        assert F32ToUQ15().step(-1.0) == 0

    def test_positive_clipped_maps_to_max(self):
        """1.0 * 32768 = 32768 > 32767; clips to 32767, then +32768 = 65535."""
        obj = F32ToUQ15()
        assert obj.step(1.0) == 65535
        assert obj.clipped

    def test_sub_unity_positive(self):
        """32767/32768 is just below full scale; should not clip."""
        obj = F32ToUQ15()
        x = 32767.0 / 32768.0
        y = obj.step(x)
        assert y == 65535
        assert not obj.clipped

    def test_clipped_flag_sticky(self):
        obj = F32ToUQ15()
        obj.step(0.5)  # in-range
        assert not obj.clipped
        obj.step(2.0)  # clips
        assert obj.clipped
        obj.step(0.1)  # back in range — but flag stays set
        assert obj.clipped

    def test_clipped_cleared_by_reset(self):
        obj = F32ToUQ15()
        obj.step(2.0)
        assert obj.clipped
        obj.reset()
        assert not obj.clipped
        obj.step(0.0)
        assert not obj.clipped

    def test_clipped_not_set_for_in_range(self):
        obj = F32ToUQ15()
        obj.step(0.0)
        obj.step(-0.5)
        obj.step(0.5)
        assert not obj.clipped

    def test_output_range_always_uint16(self):
        """All outputs must be in [0, 65535] even for extreme inputs."""
        obj = F32ToUQ15()
        x = np.linspace(-2.0, 2.0, 256, dtype=np.float32)
        y = obj.steps(x)
        assert y.dtype == np.uint16
        assert np.all(y >= 0)
        assert np.all(y <= 65535)

    def test_steps_agrees_with_step(self):
        obj = F32ToUQ15()
        x = np.linspace(-1.5, 1.5, 256, dtype=np.float32)
        expected = np.array([obj.step(float(v)) for v in x], dtype=np.uint16)
        got = F32ToUQ15().steps(x)
        np.testing.assert_array_equal(got, expected)

    def test_bipolar_vs_unipolar_bias(self):
        """F32ToUQ15 output must equal F32ToI16 output viewed as uint16 + 32768."""
        x = np.linspace(-0.9, 0.9, 128, dtype=np.float32)
        q15 = F32ToI16().steps(x).view(np.uint16).astype(np.uint32)
        uq15 = F32ToUQ15().steps(x).astype(np.uint32)
        # UQ15 = Q15_uint16 + 32768 (mod 2^16)
        np.testing.assert_array_equal((q15 + 32768) & 0xFFFF, uq15)

    def test_invalid_scale(self):
        with pytest.raises((MemoryError, ValueError)):
            F32ToUQ15(scale=0.0)

    def test_steps_out_buffer(self):
        obj = F32ToUQ15()
        x = np.linspace(-1.0, 1.0, 16, dtype=np.float32)
        buf = np.zeros(16, dtype=np.uint16)
        ret = obj.steps(x, buf)
        assert ret is buf
        np.testing.assert_array_equal(ret, F32ToUQ15().steps(x))

    def test_context_manager(self):
        with F32ToUQ15() as obj:
            assert obj.step(0.0) == 32768


# ---------------------------------------------------------------------------
# UQ15ToF32
# ---------------------------------------------------------------------------


class TestUQ15ToF32:
    """Offset-binary decode: 0 → -1.0, 32768 → 0.0, 65535 → +32767/32768."""

    def test_midscale_maps_to_zero(self):
        assert UQ15ToF32().step(32768) == pytest.approx(0.0, abs=1e-7)

    def test_zero_maps_to_negative_unity(self):
        y = UQ15ToF32().step(0)
        np.testing.assert_allclose(y, -1.0, rtol=1e-6)

    def test_max_maps_to_near_positive_unity(self):
        y = UQ15ToF32().step(65535)
        np.testing.assert_allclose(y, 32767.0 / 32768.0, rtol=1e-6)

    def test_steps_dtype(self):
        y = UQ15ToF32().steps(np.zeros(8, dtype=np.uint16))
        assert y.dtype == np.float32

    def test_steps_agrees_with_step(self):
        obj = UQ15ToF32()
        x = np.arange(0, 65536, 256, dtype=np.uint16)
        expected = np.array([obj.step(int(v)) for v in x], dtype=np.float32)
        got = UQ15ToF32().steps(x)
        np.testing.assert_array_almost_equal(got, expected, decimal=6)

    def test_steps_out_buffer(self):
        obj = UQ15ToF32()
        x = np.array([0, 32768, 65535], dtype=np.uint16)
        buf = np.zeros(3, dtype=np.float32)
        ret = obj.steps(x, buf)
        assert ret is buf

    def test_invalid_scale(self):
        with pytest.raises((MemoryError, ValueError)):
            UQ15ToF32(scale=0.0)


# ---------------------------------------------------------------------------
# Roundtrip: F32ToUQ15 ↔ UQ15ToF32
# ---------------------------------------------------------------------------


class TestUQ15Roundtrip:
    """UQ15 offset-binary roundtrip error must be ≤ 0.5 LSB."""

    def test_roundtrip_error(self):
        enc = F32ToUQ15()
        dec = UQ15ToF32()
        x = np.linspace(-0.99, 0.99, 256, dtype=np.float32)
        y = dec.steps(enc.steps(x))
        error = float(np.max(np.abs(x - y)))
        assert error <= 1.0 / SCALE + 1e-7, (
            f"Roundtrip error {error:.2e} exceeds 0.5 LSB = {1 / SCALE:.2e}"
        )

    def test_roundtrip_matches_q15_roundtrip(self):
        """UQ15 roundtrip error must equal the Q15 roundtrip error."""
        x = np.linspace(-0.99, 0.99, 256, dtype=np.float32)
        q15_err = np.abs(x - I16ToF32().steps(F32ToI16().steps(x)))
        uq15_err = np.abs(x - UQ15ToF32().steps(F32ToUQ15().steps(x)))
        np.testing.assert_array_almost_equal(uq15_err, q15_err, decimal=5)
