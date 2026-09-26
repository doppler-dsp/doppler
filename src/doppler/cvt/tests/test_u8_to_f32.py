"""U8ToF32 -- the offset-binary uint8 (RTL-SDR ``cu8``) converter.

The C test (``native/tests/test_u8_to_f32_core.c``) owns the arithmetic over
all 256 codes. These tests own what the binding adds on top: the ``mode``
string, the numpy block path and ``out=``, and the two relationships a
Python caller relies on -- the tie to ``I8ToF32`` and the complex view of an
interleaved I/Q buffer.
"""

import numpy as np
import pytest

from doppler.cvt import I8ToF32, U8ToF32

CODES = np.arange(256, dtype=np.uint8)


def test_default_mode_is_shift():
    np.testing.assert_array_equal(
        U8ToF32().steps(CODES), U8ToF32(mode="shift").steps(CODES)
    )


def test_unknown_mode_is_refused():
    with pytest.raises(ValueError):
        U8ToF32(mode="round")


def test_shift_is_exact_over_every_code():
    want = (CODES.astype(np.float64) - 128.0) / 128.0
    got = U8ToF32(mode="shift").steps(CODES)
    assert got.dtype == np.float32
    np.testing.assert_array_equal(got, want.astype(np.float32))


def test_shift_equals_i8_to_f32_of_the_flipped_code():
    # The documented equivalence: flipping the top bit turns offset-binary
    # into two's complement, and I8ToF32 at scale 128 is then bit-identical.
    flipped = (CODES ^ 0x80).view(np.int8)
    np.testing.assert_array_equal(
        U8ToF32(mode="shift").steps(CODES), I8ToF32(128.0).steps(flipped)
    )


def test_midpoint_is_symmetric_and_reaches_both_rails():
    y = U8ToF32(mode="midpoint").steps(CODES)
    want = (CODES.astype(np.float64) - 127.5) / 127.5
    np.testing.assert_allclose(y, want, rtol=0, atol=np.finfo(np.float32).eps)
    np.testing.assert_array_equal(y, -y[::-1])  # exact antisymmetry
    assert float(y.astype(np.float64).sum()) == 0.0  # hence zero mean
    assert y[0] == pytest.approx(-1.0, abs=1e-7)
    assert y[-1] == pytest.approx(1.0, abs=1e-7)


def test_shift_reads_half_an_lsb_low_on_an_analog_zero():
    # An analog zero dithers between codes 127 and 128. shift maps them to
    # -1/128 and 0, so it reads 0.5/128 low; midpoint maps them to +-x.
    zero = np.array([127, 128] * 512, dtype=np.uint8)
    assert U8ToF32(mode="shift").steps(zero).mean() == -0.5 / 128
    assert U8ToF32(mode="midpoint").steps(zero).mean() == 0.0


def test_step_agrees_with_steps():
    rng = np.random.default_rng(7)
    x = rng.integers(0, 256, 257, dtype=np.uint8)
    for mode in ("shift", "midpoint"):
        c = U8ToF32(mode=mode)
        np.testing.assert_array_equal(
            c.steps(x), np.array([c.step(int(v)) for v in x], np.float32)
        )


def test_interleaved_cu8_views_as_complex():
    cu8 = np.array([128, 0, 192, 64], dtype=np.uint8)  # (0,-1), (0.5,-0.5)
    z = U8ToF32().steps(cu8).view(np.complex64)
    np.testing.assert_array_equal(
        z, np.array([0 - 1j, 0.5 - 0.5j], np.complex64)
    )


@pytest.mark.parametrize("mode", ["shift", "midpoint"])
def test_steps_out_param(mode):
    buf = np.zeros(CODES.size, dtype=np.float32)
    ret = U8ToF32(mode=mode).steps(CODES, out=buf)
    assert ret is buf
    np.testing.assert_array_equal(buf, U8ToF32(mode=mode).steps(CODES))


def test_reset_is_a_no_op():
    c = U8ToF32(mode="midpoint")
    before = c.steps(CODES)
    c.reset()
    np.testing.assert_array_equal(c.steps(CODES), before)


def test_context_manager():
    with U8ToF32() as obj:
        y = obj.step(1)
    assert isinstance(y, float)


def test_destroy():
    obj = U8ToF32()
    obj.destroy()
    with pytest.raises(RuntimeError, match="destroyed"):
        obj.step(1)
