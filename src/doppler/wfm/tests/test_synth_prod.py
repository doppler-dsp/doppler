"""_SynthEngine.prod / rotate: the waveform-multiplication
operator and its special case, a rotation about a stated center.

The C tests (``test_wfm_synth_core.c``) pin the kernel bit-for-bit; these pin
the Python face and ``rotate``'s contract: the product, the shared
stream with ``steps()``, the input never written, and the errors.
"""

from __future__ import annotations

import numpy as np
import pytest

from doppler.wfm import _SynthEngine, rotate, sweep_ends


def _x(n: int, seed: int = 3) -> np.ndarray:
    rng = np.random.default_rng(seed)
    return (rng.standard_normal(n) + 1j * rng.standard_normal(n)).astype(
        np.complex64
    )


def _engine() -> _SynthEngine:
    return _SynthEngine(type="tone", fs=1e6, freq=1.25e5, snr=100.0)


def test_engine_prod_is_the_elementwise_product_with_steps():
    x = _x(700)
    a, b = _engine(), _engine()
    # numpy's complex multiply may differ from the C kernel's by an ulp (FMA
    # contraction); the C test pins the bit-exact product.
    np.testing.assert_allclose(
        a.prod(x), x * b.steps(700), rtol=1e-6, atol=1e-6
    )


def test_engine_prod_matches_steps_for_every_waveform_family():
    x = _x(300)
    for kw in (
        {"type": "tone", "freq": 0.125},
        {"type": "bpsk", "sps": 4, "snr": 12.0, "seed": 7},
        {"type": "noise", "seed": 5},
    ):
        a, b = _SynthEngine(**kw), _SynthEngine(**kw)
        np.testing.assert_allclose(
            a.prod(x), x * b.steps(300), rtol=1e-6, atol=1e-6
        )


def test_split_calls_equal_one_call_and_share_the_steps_stream():
    x = _x(100)
    one = _SynthEngine(type="bpsk", sps=2, snr=15.0, seed=1)
    split = _SynthEngine(type="bpsk", sps=2, snr=15.0, seed=1)
    ref = one.prod(x)
    got = np.concatenate(
        [split.prod(x[:3]), split.prod(x[3:8]), split.prod(x[8:])]
    )
    np.testing.assert_array_equal(got, ref)
    # prod and steps draw from ONE stream: a steps() after the prods
    # continues where they stopped.
    np.testing.assert_array_equal(split.steps(10), one.steps(10))


def test_empty_block_does_not_advance_the_synth():
    s, r = (
        _SynthEngine(type="tone", freq=0.1),
        _SynthEngine(type="tone", freq=0.1),
    )
    out = s.prod(np.zeros(0, np.complex64))
    assert out.shape == (0,) and out.dtype == np.complex64
    np.testing.assert_array_equal(s.steps(5), r.steps(5))


def test_the_input_is_not_modified_and_the_result_is_new():
    x = _x(64)
    keep = x.copy()
    y = _SynthEngine(type="tone", freq=0.2).prod(x)
    np.testing.assert_array_equal(x, keep)
    assert y is not x and not np.shares_memory(y, x)


def test_a_non_contiguous_input_is_accepted():
    x = _x(128)
    a, b = (
        _SynthEngine(type="tone", freq=0.2),
        _SynthEngine(type="tone", freq=0.2),
    )
    np.testing.assert_allclose(
        a.prod(x[::2]), x[::2] * b.steps(64), rtol=1e-6, atol=1e-6
    )


def test_prod_refuses_a_wrong_dtype_shape_or_type():
    s = _engine()
    with pytest.raises(TypeError, match="complex64"):
        s.prod(np.ones(4, np.complex128))
    with pytest.raises(TypeError, match="complex64"):
        s.prod(np.ones(4, np.float32))
    with pytest.raises(TypeError, match="complex64"):
        s.prod([1 + 0j, 1 + 0j])
    with pytest.raises(ValueError, match="1-D"):
        s.prod(np.ones((2, 2), np.complex64))
    with pytest.raises(ValueError, match="1-D"):
        s.prod(np.array(1, np.complex64))  # 0-d


def test_rotate_tone_is_the_closed_form_up_to_a_constant():
    n, f = 500, 0.0437
    y = rotate(np.ones(n, np.complex64), freq=f)
    u = np.arange(n) - 0.5 * (n - 1)
    want = np.exp(2j * np.pi * f * u)
    ratio = y / want
    np.testing.assert_allclose(ratio, ratio[0], atol=2e-4)
    assert abs(abs(ratio[0]) - 1.0) < 1e-5


@pytest.mark.parametrize(
    "fs,freq,rate,center",
    [
        (1.0, 0.02, 1e-4, None),
        (2000.0, 300.0, 2e3, None),
        (1.0, -0.01, -5e-5, 100.0),
    ],
)
def test_rotate_chirp_is_the_closed_form_up_to_a_constant(
    fs, freq, rate, center
):
    n = 1000
    c = 0.5 * (n - 1) if center is None else center
    y = rotate(np.ones(n, np.complex64), freq, rate, fs, center)
    u = (np.arange(n) - c) / fs
    want = np.exp(2j * np.pi * (freq * u + 0.5 * rate * u * u))
    ratio = y / want
    np.testing.assert_allclose(ratio, ratio[0], atol=2e-3)
    # the frequency really is where claimed at both ends (cycles/sample)
    inst = np.angle(y[1:] * np.conj(y[:-1])) / (2 * np.pi)
    for i in (0, n - 2):
        ui = (i + 0.5 - c) / fs
        assert inst[i] == pytest.approx((freq + rate * ui) / fs, abs=1e-4)


def test_rotate_multiplies_the_input_and_does_not_mutate_it():
    x = _x(256)
    keep = x.copy()
    y = rotate(x, 0.05, 1e-5)
    np.testing.assert_array_equal(x, keep)
    np.testing.assert_allclose(np.abs(y), np.abs(x), rtol=1e-5)
    assert y.dtype == np.complex64 and y.shape == x.shape


def test_rotate_empty_and_errors():
    assert rotate(np.zeros(0, np.complex64), 0.1).shape == (0,)
    with pytest.raises(TypeError, match="complex64"):
        rotate(np.ones(4, np.float32), 0.1)
    with pytest.raises(TypeError, match="complex64"):
        rotate([1, 2, 3], 0.1)  # type: ignore[arg-type]
    with pytest.raises(ValueError, match="1-D"):
        rotate(np.ones((2, 2), np.complex64), 0.1)
    with pytest.raises(ValueError, match="fs"):
        rotate(np.ones(4, np.complex64), 0.1, fs=0.0)


def test_sweep_ends_is_the_one_home_of_the_centering_arithmetic():
    # f0 = f_mid - k*(center - 1/2); f_end = f0 + k*n; k = rate/fs per sample
    f0, f_end = sweep_ends(2000.0, 300.0, 1e3, 1000, 100.0)
    assert f0 == pytest.approx(300.0 - 0.5 * (100.0 - 0.5), abs=1e-9)
    assert f_end == pytest.approx(f0 + 0.5 * 1000.0, abs=1e-9)
    # rotate puts the instantaneous frequency where sweep_ends says it will
    n = 1000
    y = rotate(np.ones(n, np.complex64), 300.0, 1e3, 2000.0, 100.0)
    inst = np.angle(y[1] * np.conj(y[0])) * 2000.0 / (2 * np.pi)
    assert inst == pytest.approx(f0, abs=0.2)
