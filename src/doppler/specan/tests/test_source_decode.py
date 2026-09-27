"""specan's frame decoding: integer wire formats are interleaved I/Q.

Regression for the sources' old ``data.astype(np.complex64)``, which turned a
``CI8``/``CI16``/``CI32`` frame of n samples into 2n real-valued ones -- a
spectrum mirrored about DC, at twice the rate, with nothing raising.
"""

import numpy as np
import pytest

from doppler.specan.source import to_complex64


@pytest.mark.parametrize(
    "dtype, full_scale",
    [(np.int8, 128.0), (np.int16, 32768.0), (np.int32, 2147483648.0)],
)
def test_interleaved_integers_decode_to_normalised_complex(dtype, full_scale):
    rng = np.random.default_rng(3)
    info = np.iinfo(dtype)
    raw = rng.integers(info.min, info.max, 2 * 257, dtype=dtype)
    got = to_complex64(raw)
    assert got.dtype == np.complex64
    assert got.shape == (257,)  # n samples, not 2n
    want = (raw[0::2] + 1j * raw[1::2]) / full_scale
    np.testing.assert_allclose(got, want.astype(np.complex64), rtol=1e-6)
    assert np.any(got.imag != 0)  # Q survived


def test_complex_frames_pass_through():
    x = np.array([1 + 2j, -0.5j], np.complex64)
    assert to_complex64(x) is x
    y = to_complex64(np.array([1 + 2j], np.complex128))
    assert y.dtype == np.complex64 and y[0] == 1 + 2j


def test_an_unknown_format_is_refused():
    with pytest.raises(TypeError):
        to_complex64(np.zeros(4, np.uint8))


def test_the_docstring_example():
    # to_complex64's docstring example, executed: no gate collects module
    # doctests, so this is what keeps it true.
    got = to_complex64(np.array([64, -128, 0, 127], np.int8)).tolist()
    assert got == [(0.5 - 1j), 0.9921875j]
