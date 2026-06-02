import pytest
import numpy as np
from doppler.cvt import ADCIQ


def _tone(n=1024, freq=0.05, amplitude=None, bits=12, dbfs=-10.0):
    if amplitude is None:
        amplitude = 10 ** (dbfs / 20.0)
    t = np.arange(n, dtype=np.float64)
    return (amplitude * np.exp(2j * np.pi * freq * t)).astype(np.complex64)


def test_create_defaults():
    adc = ADCIQ()
    assert adc.bits == 16


def test_steps_dtype_and_shape():
    adc = ADCIQ(bits=12, dbfs=-10.0)
    x = _tone()
    y = adc.steps(x)
    assert y.dtype == np.int16
    assert y.shape == (2 * len(x),)


def test_steps_i_channel_is_even_q_is_odd():
    """I samples live at even indices, Q at odd — standard IQ interleaving."""
    adc = ADCIQ(bits=12, dbfs=-10.0)
    x = _tone()
    y = adc.steps(x)
    # Reconstruct approximate float and compare sign with input
    scale = adc.scale
    i_out = y[0::2].astype(np.float64) / scale
    q_out = y[1::2].astype(np.float64) / scale
    np.testing.assert_allclose(i_out, x.real, atol=1.0 / scale)
    np.testing.assert_allclose(q_out, x.imag, atol=1.0 / scale)


def test_dc_zero_input_is_zero():
    adc = ADCIQ(bits=10)
    x = np.zeros(64, dtype=np.complex64)
    y = adc.steps(x)
    np.testing.assert_array_equal(y, 0)


def test_step_returns_i_q_tuple():
    adc = ADCIQ(bits=12, dbfs=-10.0)
    i, q = adc.step(0.1 + 0.2j)
    assert isinstance(i, int)
    assert isinstance(q, int)


def test_step_matches_steps():
    """step() and steps() must agree on every sample."""
    adc1 = ADCIQ(bits=12, dbfs=-10.0)
    adc2 = ADCIQ(bits=12, dbfs=-10.0)
    x = _tone(n=32)
    batch = adc1.steps(x)
    scalar_i = np.array([adc2.step(s)[0] for s in x], dtype=np.int16)
    scalar_q = np.array([adc2.step(s)[1] for s in x], dtype=np.int16)
    np.testing.assert_array_equal(batch[0::2], scalar_i)
    np.testing.assert_array_equal(batch[1::2], scalar_q)


def test_clipped_flag():
    adc = ADCIQ(bits=8, dbfs=-10.0)
    x = np.full(16, 1e6 + 1e6j, dtype=np.complex64)
    adc.steps(x)
    assert adc.clipped is True


def test_reset_clears_clipped():
    adc = ADCIQ(bits=8, dbfs=-10.0)
    adc.steps(np.full(4, 1e6 + 1e6j, dtype=np.complex64))
    adc.reset()
    assert adc.clipped is False


def test_scale_property():
    import math
    bits, dbfs = 12, -10.0
    adc = ADCIQ(bits=bits, dbfs=dbfs)
    expected = (2 ** (bits - 1)) * 10 ** (-dbfs / 20.0)
    assert math.isclose(adc.scale, expected, rel_tol=1e-9)


def test_bits_gt_16_raises():
    with pytest.raises(ValueError, match="int16"):
        ADCIQ(bits=17)


def test_context_manager():
    with ADCIQ(bits=12) as adc:
        y = adc.steps(np.zeros(8, dtype=np.complex64))
    assert y.dtype == np.int16


def test_destroy():
    adc = ADCIQ(bits=12)
    adc.destroy()
    with pytest.raises(RuntimeError, match="destroyed"):
        adc.steps(np.zeros(4, dtype=np.complex64))
