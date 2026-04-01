"""
Tests for dp_nco — Python binding for the doppler NCO.

Mirrors the C test_nco.c suite end-to-end: every test exercises the
same behaviour through the Python C extension layer.
"""

import numpy as np
import pytest

from doppler import Nco

TOL = 1e-5  # matches 2^16 LUT precision

# Quarter-rate phase increment = 2^30
Q = 1073741824


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def _cf32_near(a: complex, b: complex, tol: float = TOL) -> bool:
    return abs(a.real - b.real) <= tol and abs(a.imag - b.imag) <= tol


# ---------------------------------------------------------------------------
# 1. create / destroy / context manager
# ---------------------------------------------------------------------------


class TestLifecycle:
    def test_create_destroy(self):
        nco = Nco(0.0)
        assert nco is not None
        nco.destroy()

    def test_context_manager(self):
        with Nco(0.25) as nco:
            out = nco.execute_cf32(1)
        assert out.shape == (1,)

    def test_double_destroy_safe(self):
        nco = Nco(0.0)
        nco.destroy()
        nco.destroy()  # second destroy must not crash


# ---------------------------------------------------------------------------
# 2. zero frequency → all (1, 0)
# ---------------------------------------------------------------------------


class TestZeroFreq:
    def test_all_dc(self):
        with Nco(0.0) as nco:
            out = nco.execute_cf32(8)
        assert out.dtype == np.complex64
        assert out.shape == (8,)
        assert np.allclose(out.real, 1.0, atol=TOL)
        assert np.allclose(out.imag, 0.0, atol=TOL)


# ---------------------------------------------------------------------------
# 3. quarter-rate tone: 4-sample period with known I/Q
# ---------------------------------------------------------------------------


class TestQuarterRate:
    @pytest.fixture
    def out(self):
        with Nco(0.25) as nco:
            return nco.execute_cf32(8)

    def test_sample_0(self, out):
        assert _cf32_near(out[0], 1 + 0j)

    def test_sample_1(self, out):
        assert _cf32_near(out[1], 0 + 1j)

    def test_sample_2(self, out):
        assert _cf32_near(out[2], -1 + 0j)

    def test_sample_3(self, out):
        assert _cf32_near(out[3], 0 - 1j)

    def test_wrap(self, out):
        assert _cf32_near(out[4], out[0])
        assert _cf32_near(out[5], out[1])
        assert _cf32_near(out[6], out[2])
        assert _cf32_near(out[7], out[3])


# ---------------------------------------------------------------------------
# 4. half-rate: alternates (1,0) / (-1,0)
# ---------------------------------------------------------------------------


class TestHalfRate:
    def test_alternates(self):
        with Nco(0.5) as nco:
            out = nco.execute_cf32(4)
        assert _cf32_near(out[0], 1 + 0j)
        assert _cf32_near(out[1], -1 + 0j)
        assert _cf32_near(out[2], 1 + 0j)
        assert _cf32_near(out[3], -1 + 0j)


# ---------------------------------------------------------------------------
# 5. unity amplitude over 1024 samples
# ---------------------------------------------------------------------------


class TestUnitAmplitude:
    def test_amplitude(self):
        with Nco(0.137) as nco:
            out = nco.execute_cf32(1024)
        amp = np.abs(out)
        assert np.all(np.abs(amp - 1.0) < 1e-5)


# ---------------------------------------------------------------------------
# 6. phase continuity across execute calls
# ---------------------------------------------------------------------------


class TestPhaseContinuity:
    def test_split_equals_single(self):
        with Nco(0.25) as ref:
            expected = ref.execute_cf32(8)

        with Nco(0.25) as nco:
            a = nco.execute_cf32(4)
            b = nco.execute_cf32(4)

        assert np.allclose(a, expected[:4], atol=TOL)
        assert np.allclose(b, expected[4:], atol=TOL)


# ---------------------------------------------------------------------------
# 7. reset restores phase to zero
# ---------------------------------------------------------------------------


class TestReset:
    def test_reset(self):
        with Nco(0.25) as nco:
            nco.execute_cf32(3)  # advance 3 samples
            nco.reset()
            out = nco.execute_cf32(4)
        assert _cf32_near(out[0], 1 + 0j)
        assert _cf32_near(out[1], 0 + 1j)
        assert _cf32_near(out[2], -1 + 0j)
        assert _cf32_near(out[3], 0 - 1j)


# ---------------------------------------------------------------------------
# 8. set_freq changes frequency without resetting phase
# ---------------------------------------------------------------------------


class TestSetFreq:
    def test_set_freq(self):
        with Nco(0.0) as nco:
            pre = nco.execute_cf32(2)
            nco.set_freq(0.25)
            post = nco.execute_cf32(4)

        assert np.allclose(pre, [1 + 0j, 1 + 0j], atol=TOL)
        assert _cf32_near(post[0], 1 + 0j)
        assert _cf32_near(post[1], 0 + 1j)
        assert _cf32_near(post[2], -1 + 0j)
        assert _cf32_near(post[3], 0 - 1j)


# ---------------------------------------------------------------------------
# 9. negative frequency → conjugate rotation
# ---------------------------------------------------------------------------


class TestNegativeFreq:
    def test_negative_quarter(self):
        with Nco(-0.25) as nco:
            out = nco.execute_cf32(4)
        assert _cf32_near(out[0], 1 + 0j)
        assert _cf32_near(out[1], 0 - 1j)
        assert _cf32_near(out[2], -1 + 0j)
        assert _cf32_near(out[3], 0 + 1j)


# ---------------------------------------------------------------------------
# 10. ctrl port: zero deviation = free-running
# ---------------------------------------------------------------------------


class TestCtrlZero:
    def test_zero_ctrl_matches_free(self):
        ctrl = np.zeros(8, dtype=np.float32)
        with Nco(0.25) as ref:
            expected = ref.execute_cf32(8)
        with Nco(0.25) as nco:
            out = nco.execute_cf32_ctrl(ctrl)
        assert np.allclose(out, expected, atol=TOL)

    def test_output_shape_and_dtype(self):
        ctrl = np.zeros(16, dtype=np.float32)
        with Nco(0.1) as nco:
            out = nco.execute_cf32_ctrl(ctrl)
        assert out.shape == (16,)
        assert out.dtype == np.complex64


# ---------------------------------------------------------------------------
# 11. ctrl port: +0.25 deviation doubles quarter-rate frequency
# ---------------------------------------------------------------------------


class TestCtrlFreqShift:
    def test_doubled_rate(self):
        ctrl = np.full(4, 0.25, dtype=np.float32)
        with Nco(0.25) as nco:
            out = nco.execute_cf32_ctrl(ctrl)
        assert _cf32_near(out[0], 1 + 0j)
        assert _cf32_near(out[1], -1 + 0j)
        assert _cf32_near(out[2], 1 + 0j)
        assert _cf32_near(out[3], -1 + 0j)

    def test_base_inc_unchanged(self):
        ctrl = np.full(4, 0.25, dtype=np.float32)
        with Nco(0.25) as nco:
            nco.execute_cf32_ctrl(ctrl)
            nco.reset()
            after = nco.execute_cf32(4)
        # base freq still 0.25 → quarter-rate sequence
        assert _cf32_near(after[1], 0 + 1j)


# ---------------------------------------------------------------------------
# 12. u32: exact phase values at f_n = 0.25
# ---------------------------------------------------------------------------


class TestU32PhaseValues:
    def test_phase_sequence(self):
        with Nco(0.25) as nco:
            ph = nco.execute_u32(8)
        assert ph.dtype == np.uint32
        expected = np.array([0, Q, 2 * Q, 3 * Q, 0, Q, 2 * Q, 3 * Q], dtype=np.uint32)
        np.testing.assert_array_equal(ph, expected)


# ---------------------------------------------------------------------------
# 13. u32 / cf32 consistency: same phase → same cos/sin
# ---------------------------------------------------------------------------


class TestU32CF32Consistency:
    def test_phase_to_iq(self):
        with Nco(0.137) as nco:
            ph = nco.execute_u32(64)
        angle = ph.astype(np.float64) / 4294967296.0 * 2.0 * np.pi
        expected_i = np.cos(angle).astype(np.float32)
        expected_q = np.sin(angle).astype(np.float32)

        with Nco(0.137) as nco2:
            out = nco2.execute_cf32(64)

        assert np.allclose(out.real, expected_i, atol=1e-4)
        assert np.allclose(out.imag, expected_q, atol=1e-4)


# ---------------------------------------------------------------------------
# 14. u32_ctrl: zero deviation = u32 free-running
# ---------------------------------------------------------------------------


class TestU32CtrlZero:
    def test_zero_ctrl(self):
        ctrl = np.zeros(16, dtype=np.float32)
        with Nco(0.25) as ref:
            expected = ref.execute_u32(16)
        with Nco(0.25) as nco:
            out = nco.execute_u32_ctrl(ctrl)
        assert out.dtype == np.uint32
        np.testing.assert_array_equal(out, expected)


# ---------------------------------------------------------------------------
# 15. u32_ovf: carry fires every 4th sample at f_n = 0.25
# ---------------------------------------------------------------------------


class TestOvfQuarterRate:
    def test_carry_pattern(self):
        with Nco(0.25) as nco:
            ph, carry = nco.execute_u32_ovf(16)
        assert ph.dtype == np.uint32
        assert carry.dtype == np.uint8
        assert ph.shape == (16,)
        assert carry.shape == (16,)

        # carry fires at indices 3, 7, 11, 15
        expected = np.array(
            [(1 if i % 4 == 3 else 0) for i in range(16)],
            dtype=np.uint8,
        )
        np.testing.assert_array_equal(carry, expected)


# ---------------------------------------------------------------------------
# 16. u32_ovf: carry fires every 2nd sample at f_n = 0.5
# ---------------------------------------------------------------------------


class TestOvfHalfRate:
    def test_carry_pattern(self):
        with Nco(0.5) as nco:
            ph, carry = nco.execute_u32_ovf(8)
        expected = np.array([0, 1, 0, 1, 0, 1, 0, 1], dtype=np.uint8)
        np.testing.assert_array_equal(carry, expected)


# ---------------------------------------------------------------------------
# 17. u32_ovf_ctrl: zero deviation = u32_ovf free-running
# ---------------------------------------------------------------------------


class TestOvfCtrlZero:
    def test_zero_ctrl(self):
        ctrl = np.zeros(16, dtype=np.float32)
        with Nco(0.25) as ref:
            ph_ref, c_ref = ref.execute_u32_ovf(16)
        with Nco(0.25) as nco:
            ph, c = nco.execute_u32_ovf_ctrl(ctrl)
        assert ph.dtype == np.uint32
        assert c.dtype == np.uint8
        np.testing.assert_array_equal(ph, ph_ref)
        np.testing.assert_array_equal(c, c_ref)


# ---------------------------------------------------------------------------
# 18. u32_ovf_ctrl: +0.25 deviation → half-rate carry pattern
# ---------------------------------------------------------------------------


class TestOvfCtrlFreqShift:
    def test_carry_doubled(self):
        ctrl = np.full(8, 0.25, dtype=np.float32)
        with Nco(0.25) as nco:
            ph, carry = nco.execute_u32_ovf_ctrl(ctrl)
        expected = np.array([0, 1, 0, 1, 0, 1, 0, 1], dtype=np.uint8)
        np.testing.assert_array_equal(carry, expected)
