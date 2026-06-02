import math
import pytest
import numpy as np
from doppler.cvt import ADC


def test_create():
    obj = ADC(16, -10.0, 0)
    assert obj is not None


def test_step_runs():
    obj = ADC(16, -10.0, 0)
    y = obj.step(1.0)
    assert isinstance(y, int)


def test_steps_shape_dtype():
    obj = ADC(16, -10.0, 0)
    x = np.ones(64, dtype=np.float32)
    y = obj.steps(x)
    assert y.shape == (64,)
    assert y.dtype == np.int64


def test_steps_out_param():
    x   = np.ones(64, dtype=np.float32)
    buf = np.zeros(64, dtype=np.int64)
    obj1 = ADC(16, -10.0, 0)
    ret = obj1.steps(x, buf)
    assert ret is buf
    obj2 = ADC(16, -10.0, 0)
    np.testing.assert_array_equal(ret, obj2.steps(x))


def test_zero_input_returns_zero():
    """DC zero must quantise to exactly 0 for any bit depth."""
    for bits in (8, 16, 32, 64):
        obj = ADC(bits, -10.0, 0)
        assert obj.step(0.0) == 0


def test_scale_property():
    """scale == 2^(bits-1) * 10^(-dbfs/20)."""
    bits, dbfs = 16, -10.0
    obj = ADC(bits, dbfs)
    expected = (2 ** (bits - 1)) * 10 ** (-dbfs / 20.0)
    assert math.isclose(obj.scale, expected, rel_tol=1e-9)


def test_bits_property():
    obj = ADC(12, -6.0, 0)
    assert obj.bits == 12


def test_saturation_clips_and_sets_flag():
    """An absurdly large input must clamp to clip_max and set clipped."""
    obj = ADC(bits=8, dbfs=-10.0, dithering=0)
    y = obj.step(1e9)
    assert y == 127            # clip_max for 8-bit
    assert obj.clipped is True


def test_negative_saturation():
    obj = ADC(bits=8, dbfs=-10.0, dithering=0)
    y = obj.step(-1e9)
    assert y == -128           # clip_min for 8-bit
    assert obj.clipped is True


def test_clipped_sticky_cleared_by_reset():
    obj = ADC(bits=8, dbfs=-10.0, dithering=0)
    obj.step(1e9)
    assert obj.clipped is True
    obj.reset()
    assert obj.clipped is False


def test_bits_64():
    """64-bit ADC output must fit in int64."""
    obj = ADC(bits=64, dbfs=-10.0, dithering=0)
    y = obj.step(0.0)
    assert y == 0
    assert isinstance(y, int)


def test_dithering_changes_output():
    """With TPDF dither, a constant DC input must not always give the same code."""
    obj_d = ADC(bits=16, dbfs=-10.0, dithering=1)
    samples = np.full(1024, 0.01, dtype=np.float32)
    out = obj_d.steps(samples)
    # Dither should produce at least two distinct output codes.
    assert len(np.unique(out)) > 1


def test_no_dither_deterministic():
    """Without dither, identical inputs must produce identical outputs."""
    obj = ADC(bits=16, dbfs=-10.0, dithering=0)
    samples = np.linspace(-0.5, 0.5, 128, dtype=np.float32)
    out1 = obj.steps(samples)
    obj.reset()
    out2 = obj.steps(samples)
    np.testing.assert_array_equal(out1, out2)


def test_steps_matches_step_loop():
    """steps() and a step() loop must agree on every sample."""
    obj1 = ADC(bits=16, dbfs=-10.0, dithering=0)
    obj2 = ADC(bits=16, dbfs=-10.0, dithering=0)
    samples = np.linspace(-1.0, 1.0, 256, dtype=np.float32)
    batch = obj1.steps(samples)
    scalar = np.array([obj2.step(float(s)) for s in samples], dtype=np.int64)
    np.testing.assert_array_equal(batch, scalar)


def test_context_manager():
    with ADC(16, -10.0, 0) as obj:
        y = obj.step(1.0)
    assert isinstance(y, int)


def test_destroy():
    obj = ADC(16, -10.0, 0)
    obj.destroy()
    with pytest.raises(RuntimeError, match="destroyed"):
        obj.step(1.0)


def test_invalid_bits_returns_none():
    """Out-of-range bits must fail gracefully at the Python level."""
    with pytest.raises((MemoryError, ValueError)):
        ADC(bits=0)
    with pytest.raises((MemoryError, ValueError)):
        ADC(bits=65)
