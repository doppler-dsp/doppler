"""Synth.prod / rotate: the waveform-multiplication
operator and its special case, a rotation about a stated center.

The C tests (``test_wfm_synth_core.c``) pin the kernel bit-for-bit; these pin
the Python face and ``rotate``'s contract: the product, the shared
stream with ``steps()``, the input never written, and the errors.
"""

from __future__ import annotations

import numpy as np
import pytest

from doppler.wfm import Synth, _sweep_ends, _SynthEngine, rotate


def _x(n: int, seed: int = 3) -> np.ndarray:
    rng = np.random.default_rng(seed)
    return (rng.standard_normal(n) + 1j * rng.standard_normal(n)).astype(
        np.complex64
    )


def _engine() -> Synth:
    return Synth(type="tone", fs=1e6, freq=1.25e5, snr=100.0)


def test_synth_prod_is_the_elementwise_product_with_steps():
    x = _x(700)
    a, b = (_SynthEngine(type="tone", freq=0.125) for _ in range(2))
    # numpy's complex multiply may differ from the C kernel's by an ulp (FMA
    # contraction); the C test pins the bit-exact product.
    np.testing.assert_allclose(
        a.prod(x), x * b.steps(700), rtol=1e-6, atol=1e-6
    )


def test_synth_prod_matches_steps_for_every_waveform_family():
    x = _x(300)
    for kw in (
        {"type": "tone", "freq": 0.125},
        {"type": "bpsk", "sps": 4, "snr": 12.0, "seed": 7},
        {"type": "noise", "seed": 5},
    ):
        a, b = Synth(**kw), Synth(**kw)
        np.testing.assert_allclose(
            a.prod(x), x * b.steps(300), rtol=1e-6, atol=1e-6
        )


# Two ways of computing the same product agree to ROUNDING, not to the bit: the
# compiler may contract the multiply (and steps()'s noise-add) into fused
# multiply-adds differently on the vector body and the scalar tail and between
# split calls -- the repo's -ffast-math builds do on arm64, and
# dp_wfm_synth_steps's own comment (wfm_synth_core.c, "an ULP on FMA targets
# (arm64)") says callers needing byte-for-byte must drive one path. So the
# comparisons across paths below use a RELATIVE tolerance, a few float32 ulp of
# the per-element magnitude (noisy BPSK is not unit-modulus); the input
# being unmodified stays an exact comparison. A wrong answer (a dropped input,
# a stray advance) errs by O(1) of that magnitude, about a million times it.
_RTOL = 8 * float(np.finfo(np.float32).eps)


def _close(a, b, scale):
    """``|a - b| <= _RTOL * scale`` element by element."""
    err = np.abs(a.astype(np.complex128) - b.astype(np.complex128))
    assert np.all(err <= _RTOL * scale), float(
        (err / np.maximum(scale, 1e-30)).max()
    )


def test_split_calls_equal_one_call_and_share_the_steps_stream():
    x = _x(100)
    one = Synth(type="bpsk", sps=2, snr=15.0, seed=1)
    split = Synth(type="bpsk", sps=2, snr=15.0, seed=1)
    y = Synth(type="bpsk", sps=2, snr=15.0, seed=1).steps(100)
    ref = one.prod(x)
    got = np.concatenate(
        [split.prod(x[:3]), split.prod(x[3:8]), split.prod(x[8:])]
    )
    _close(got, ref, np.abs(x) * np.abs(y))
    # prod and steps draw from ONE stream: a steps() after the prods
    # continues where they stopped.
    a, b = split.steps(10), one.steps(10)
    _close(a, b, np.abs(b).max())


def test_empty_block_does_not_advance_the_synth():
    # fs = 1 so one sample is 0.1 cycle: with the default 1 MHz rate a 0.1 Hz
    # tone moves 6e-7 rad a sample, inside the rounding tolerance, and a stray
    # advance would be invisible.
    s, r = (
        Synth(type="tone", fs=1.0, freq=0.1),
        Synth(type="tone", fs=1.0, freq=0.1),
    )
    out = s.prod(np.zeros(0, np.complex64))
    assert out.shape == (0,) and out.dtype == np.complex64
    a, b = s.steps(5), r.steps(5)
    _close(a, b, np.abs(b).max())


def test_the_input_is_not_modified_and_the_result_is_new():
    x = _x(64)
    keep = x.copy()
    y = Synth(type="tone", freq=0.2).prod(x)
    np.testing.assert_array_equal(x, keep)
    assert y is not x and not np.shares_memory(y, x)


def test_a_strided_input_is_refused_not_silently_copied():
    # `strict` (jm): a wrong dtype, shape or stride is an error, never a
    # silent cast or copy.
    s = Synth(type="tone", freq=0.2)
    with pytest.raises(ValueError, match="contiguous"):
        s.prod(_x(128)[::2])


def test_rotate_accepts_a_strided_block():
    # Synth.prod refuses a strided block (above); rotate, the supported way
    # in, takes any 1-D complex64 view and copies it once.
    x = _x(128)
    view = x[::2]
    assert not view.flags.c_contiguous
    got = rotate(view, 0.01, rate=1e-4)
    want = rotate(view.copy(), 0.01, rate=1e-4)
    np.testing.assert_array_equal(got, want)


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


@pytest.mark.parametrize(
    "fs",
    [0.0, -1.0, float("nan"), float("inf")],
    ids=["zero", "negative", "nan", "inf"],
)
def test_rotate_refuses_a_sample_rate_that_is_not_positive_and_finite(fs):
    # The centering arithmetic divides by fs (dp_wfm_synth_sweep_ends keeps
    # its documented precondition fs > 0), so the public wrapper is where a
    # bad rate is refused -- on an empty block too, which has no other
    # reason to look at it.
    with pytest.raises(ValueError, match="fs"):
        rotate(np.ones(4, np.complex64), 0.1, fs=fs)
    with pytest.raises(ValueError, match="fs"):
        rotate(np.zeros(0, np.complex64), 0.1, rate=1.0, fs=fs)


def test_sweep_ends_is_the_one_home_of_the_centering_arithmetic():
    # f0 = f_mid - k*(center - 1/2); f_end = f0 + k*n; k = rate/fs per sample
    f0, f_end = _sweep_ends(2000.0, 300.0, 1e3, 1000, 100.0)
    assert f0 == pytest.approx(300.0 - 0.5 * (100.0 - 0.5), abs=1e-9)
    assert f_end == pytest.approx(f0 + 0.5 * 1000.0, abs=1e-9)
    # rotate puts the instantaneous frequency where sweep_ends says it will
    n = 1000
    y = rotate(np.ones(n, np.complex64), 300.0, 1e3, 2000.0, 100.0)
    inst = np.angle(y[1] * np.conj(y[0])) * 2000.0 / (2 * np.pi)
    assert inst == pytest.approx(f0, abs=0.2)


def test_the_generator_type_exposes_prod_too():
    # The engine (_SynthEngine) and the public Synth are two types over one
    # kernel; both declare prod, and the engine's agrees with its own steps().
    x = _x(300)
    a, b = (
        _SynthEngine(type="tone", freq=0.125),
        _SynthEngine(type="tone", freq=0.125),
    )
    np.testing.assert_allclose(
        a.prod(x), x * b.steps(300), rtol=1e-6, atol=1e-6
    )


def test_the_engine_face_is_strict_too():
    # `Synth.prod` refuses a strided or complex128 block (`strict`), and so
    # does the engine's, now that jm renders its fragment whole (doppler
    # #1886). Until then the body was transplanted into a hand-owned
    # fragment, which dropped `strict` (just-makeit#2271): the engine copied
    # a view and cast a wide block, and this test was a strict xfail.
    x = _x(128)
    e = _SynthEngine(type="tone", freq=0.125)
    with pytest.raises(ValueError, match="contiguous"):
        e.prod(x[::2])
    with pytest.raises(TypeError, match="complex64"):
        e.prod(x.astype(np.complex128))
    with pytest.raises(ValueError, match="1-D"):
        e.prod(x.reshape(8, 16))
