import numpy as np
import pytest

from doppler.source import AWGN, awgn


@pytest.mark.parametrize("amplitude", [float("nan"), float("inf"), -1.0])
def test_invalid_amplitude_raises(amplitude):
    """An amplitude outside the domain (finite, >= 0) is refused at create,
    as a ValueError naming it, rather than building a generator with NaN or
    negative noise (doppler#2084)."""
    with pytest.raises(ValueError):
        AWGN(0, amplitude)


def test_invalid_amplitude_setter_keeps_old_value():
    """The property does not raise: an invalid assignment is ignored and the
    amplitude keeps its old value, so a bad sigma is never taken."""
    obj = AWGN(0, 0.5)
    obj.amplitude = -1.0
    assert obj.amplitude == 0.5
    obj.amplitude = float("nan")
    assert obj.amplitude == 0.5
    obj.destroy()


def test_create():
    obj = AWGN(0, 1.0)
    assert obj is not None


def test_getter_setter():
    pass  # no auto-state; add assertions for your fields


def test_reset():
    pass  # no auto-state; add assertions for your reset


def test_context_manager():
    with AWGN(0, 1.0):
        pass


def test_destroy():
    obj = AWGN(0, 1.0)
    obj.destroy()


def test_awgn_function_shape_dtype():
    y = awgn(1024)
    assert y.shape == (1024,)
    assert y.dtype == np.complex64


def test_awgn_function_amplitude():
    y = awgn(65536, amplitude=0.5)
    assert abs(float(np.std(np.real(y))) - 0.5) < 0.02


def test_awgn_function_seed():
    a = awgn(256, seed=42)
    b = awgn(256, seed=42)
    assert np.array_equal(a, b)


def test_awgn_function_different_seeds():
    a = awgn(256, seed=1)
    b = awgn(256, seed=2)
    assert not np.array_equal(a, b)


def test_generate_large_n_no_overflow():
    """generate(n) past the internal cap sizes its buffer to n (no overflow).
    AWGN already grew correctly; this pins the behaviour alongside the LO/NCO
    fix (#116)."""
    import numpy as np

    n = 393_216
    g = AWGN(amplitude=0.5).generate(n)
    assert g.shape == (n,)
    assert np.isfinite(g).all()
