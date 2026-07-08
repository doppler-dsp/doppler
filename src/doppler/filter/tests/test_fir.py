import numpy as np
import pytest

from doppler.filter import FIR

# ── Lifecycle ────────────────────────────────────────────────────────────────


def test_create_real():
    taps = np.array([0.5, 0.25, 0.125], dtype=np.float32)
    f = FIR(taps)
    assert f.num_taps == 3
    assert f.is_real is True


def test_create_complex():
    taps = np.array([1.0 + 0.0j, 0.0 + 1.0j], dtype=np.complex64)
    f = FIR(taps)
    assert f.num_taps == 2
    assert f.is_real is False


def test_create_bad_dtype_raises():
    with pytest.raises(TypeError):
        FIR(np.array([1.0, 0.5], dtype=np.float64))


def test_context_manager():
    taps = np.array([1.0], dtype=np.float32)
    with FIR(taps) as f:
        assert f.num_taps == 1


# ── Impulse response ─────────────────────────────────────────────────────────


def test_impulse_real():
    taps = np.array([0.5, 0.25, 0.125], dtype=np.float32)
    f = FIR(taps)
    x = np.zeros(6, dtype=np.complex64)
    x[0] = 1.0 + 0.0j
    y = f.execute(x)
    assert y.dtype == np.complex64
    assert len(y) == 6
    np.testing.assert_allclose(y[0].real, 0.5, atol=1e-6)
    np.testing.assert_allclose(y[1].real, 0.25, atol=1e-6)
    np.testing.assert_allclose(y[2].real, 0.125, atol=1e-6)
    np.testing.assert_allclose(y[3].real, 0.0, atol=1e-6)


def test_impulse_complex_taps():
    taps = np.array([1.0 + 0.0j, 0.0 + 1.0j], dtype=np.complex64)
    f = FIR(taps)
    x = np.zeros(4, dtype=np.complex64)
    x[0] = 1.0 + 0.0j
    y = f.execute(x)
    np.testing.assert_allclose(y[0].real, 1.0, atol=1e-6)
    np.testing.assert_allclose(y[0].imag, 0.0, atol=1e-6)
    np.testing.assert_allclose(y[1].real, 0.0, atol=1e-6)
    np.testing.assert_allclose(y[1].imag, 1.0, atol=1e-6)


# ── DC gain ──────────────────────────────────────────────────────────────────


def test_dc_gain():
    taps = np.array([0.5, 0.25, 0.125], dtype=np.float32)
    f = FIR(taps)
    x = np.ones(64, dtype=np.complex64)
    y = f.execute(x)
    expected_gain = 0.5 + 0.25 + 0.125
    np.testing.assert_allclose(y[-1].real, expected_gain, atol=1e-5)


# ── Continuity (split vs whole) ──────────────────────────────────────────────


def test_continuity():
    taps = np.array([0.5, 0.25, 0.125], dtype=np.float32)
    x = (
        np.random.default_rng(42).standard_normal(64)
        + 1j * np.random.default_rng(43).standard_normal(64)
    ).astype(np.complex64)

    f_whole = FIR(taps)
    y_whole = f_whole.execute(x).copy()

    f_split = FIR(taps)
    y_a = f_split.execute(x[:32]).copy()
    y_b = f_split.execute(x[32:]).copy()
    y_split = np.concatenate([y_a, y_b])

    np.testing.assert_allclose(y_whole, y_split, atol=1e-5)


# ── Reset ────────────────────────────────────────────────────────────────────


def test_reset_clears_delay():
    taps = np.array([0.5, 0.25, 0.125], dtype=np.float32)
    f = FIR(taps)
    x = np.zeros(6, dtype=np.complex64)
    x[0] = 1.0 + 0.0j

    f.execute(x)  # loads delay with tail of impulse
    f.reset()
    y = f.execute(x)  # should look like fresh impulse again

    np.testing.assert_allclose(y[0].real, 0.5, atol=1e-6)


# ── Low-pass attenuation ─────────────────────────────────────────────────────


def test_lowpass_attenuation():
    from scipy.signal import firwin

    n_taps = 63
    # cutoff = 0.2 × Nyquist = 0.1 × fs
    # passband: 0.02 × fs (well inside pass)
    # stopband: 0.4  × fs (well inside stop)
    h = firwin(n_taps, 0.2).astype(np.float32)
    f = FIR(h)

    t = np.arange(512)
    passband = np.exp(2j * np.pi * 0.02 * t).astype(np.complex64)
    stopband = np.exp(2j * np.pi * 0.40 * t).astype(np.complex64)

    # copy before calling execute again — output is a zero-copy view
    y_pass = f.execute(passband).copy()
    f.reset()
    y_stop = f.execute(stopband).copy()

    p_pass = float(np.mean(np.abs(y_pass[n_taps:]) ** 2))
    p_stop = float(np.mean(np.abs(y_stop[n_taps:]) ** 2))

    assert p_pass > 0.9, f"passband power {p_pass:.4f} < 0.9"
    assert p_stop < 1e-3, f"stopband power {p_stop:.2e} >= 1e-3"


# ── out= buffer ──────────────────────────────────────────────────────────────


def test_execute_out_param_writes_into_callers_buffer():
    taps = np.array([1.0, 0.5, 0.25], dtype=np.complex64)
    f = FIR(taps)
    x = np.ones(8, dtype=np.complex64)

    buf = np.empty(8, dtype=np.complex64)
    result = f.execute(x, out=buf)
    assert np.shares_memory(result, buf)

    f2 = FIR(taps)
    expected = f2.execute(x).copy()
    np.testing.assert_array_equal(result, expected)


def test_execute_out_param_undersized_raises():
    """Regression test: fir_execute_max_out() always returns 0 (FIR is a 1:1
    transform, not a bounded-capacity one), but the out= validation used to
    check `_cap < _omax` alone -- since sizes are unsigned, `_cap < 0` is
    never true, so an undersized out= buffer passed validation and then
    fir_execute() (which has no max_out clamp) overflowed it. The fix
    requires capacity for max(max_out(), len(x))."""
    taps = np.array([1.0, 0.5, 0.25], dtype=np.complex64)
    f = FIR(taps)
    x = np.ones(1000, dtype=np.complex64)

    assert f.execute_max_out() == 0  # the misleading value from the docstring
    undersized = np.empty(0, dtype=np.complex64)
    with pytest.raises(ValueError, match="need >= 1000"):
        f.execute(x, out=undersized)
