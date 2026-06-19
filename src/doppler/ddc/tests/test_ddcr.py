"""Tests for doppler.ddc.Ddcr (the generated kind="handle" class).

Ddcr consolidates the former DDCR object class and the seven ddcr_* free
functions into one typed handle over the opaque ddcr_state_t. execute() takes a
caller-provided writable complex64 output buffer (jm#316 shape d) and returns a
zero-copy view out[:n_out].
"""

from __future__ import annotations

import numpy as np
import pytest

from doppler.ddc import Ddcr

N = 4096


def _real_tone(freq: float, n: int) -> np.ndarray:
    t = np.arange(n, dtype=np.float64)
    return np.cos(2 * np.pi * freq * t).astype(np.float32)


def _dominant_freq(y: np.ndarray) -> float:
    S = np.abs(np.fft.fft(y))
    return float(np.fft.fftfreq(len(y))[np.argmax(S)])


def _run(r: Ddcr, x: np.ndarray) -> np.ndarray:
    """execute() with a caller buffer sized to the (decimating) input — safe
    since the chain's rate <= 0.5, so n_out <= len(x)."""
    out = np.empty(len(x), dtype=np.complex64)
    return r.execute(x, out)


# ------------------------------------------------------------------ #
# Construction                                                         #
# ------------------------------------------------------------------ #


class TestDdcrConstruction:
    def test_create_basic(self):
        assert Ddcr(0.0, 0.25) is not None

    def test_invalid_rate_zero(self):
        with pytest.raises((ValueError, Exception)):
            Ddcr(0.0, 0.0)

    def test_invalid_rate_ge_half(self):
        with pytest.raises((ValueError, Exception)):
            Ddcr(0.0, 0.5)

    def test_defaults(self):
        r = Ddcr()
        assert abs(r.rate - 0.25) < 1e-9
        assert abs(r.norm_freq) < 1e-9

    def test_rate_property(self):
        assert abs(Ddcr(0.0, 0.25).rate - 0.25) < 1e-9

    def test_rate_read_only(self):
        r = Ddcr(0.0, 0.25)
        with pytest.raises(AttributeError):
            r.rate = 0.1

    def test_get_norm_freq(self):
        assert abs(Ddcr(0.1, 0.25).norm_freq - 0.1) < 1e-5

    def test_context_manager(self):
        with Ddcr(0.0, 0.25) as r:
            y = _run(r, np.zeros(512, dtype=np.float32))
        assert y.dtype == np.complex64


# ------------------------------------------------------------------ #
# execute()                                                            #
# ------------------------------------------------------------------ #


class TestDdcrExecute:
    def test_returns_complex64_view(self):
        r = Ddcr(0.0, 0.25)
        out = np.empty(N, dtype=np.complex64)
        y = r.execute(np.zeros(N, dtype=np.float32), out)
        assert y.dtype == np.complex64
        assert y.base is out  # zero-copy view into the caller's buffer

    def test_output_length_approx(self):
        r = Ddcr(0.0, 0.25)
        y = _run(r, np.ones(N, dtype=np.float32))
        expected = N * 0.25  # total rate 0.25 → output ≈ N * 0.25
        assert abs(len(y) / expected - 1.0) < 0.1, (
            f"got {len(y)}, expected ≈{expected:.0f}"
        )

    def test_rejects_wrong_input_dtype(self):
        r = Ddcr(0.0, 0.25)
        out = np.empty(N, dtype=np.complex64)
        with pytest.raises((TypeError, ValueError)):
            r.execute(np.zeros(N, dtype=np.complex64), out)

    def test_rejects_wrong_out_dtype(self):
        r = Ddcr(0.0, 0.25)
        with pytest.raises((TypeError, ValueError)):
            r.execute(
                np.zeros(N, dtype=np.float32), np.empty(N, dtype=np.float32)
            )


# ------------------------------------------------------------------ #
# Tuning and reset                                                    #
# ------------------------------------------------------------------ #


class TestDdcrTuning:
    def test_set_norm_freq_roundtrip(self):
        r = Ddcr(0.1, 0.25)
        r.norm_freq = 0.2  # writable property (jm#316)
        assert abs(r.norm_freq - 0.2) < 1e-5

    def test_reset_gives_same_output(self):
        rng = np.random.default_rng(13)
        x = rng.standard_normal(N).astype(np.float32)
        r1, r2 = Ddcr(0.0, 0.25), Ddcr(0.0, 0.25)
        out1 = _run(r1, x)
        _run(r2, x)
        r2.reset()
        out2 = _run(r2, x)
        np.testing.assert_array_equal(out1, out2)


# ------------------------------------------------------------------ #
# Lifecycle (RAII close / use-after-close)                            #
# ------------------------------------------------------------------ #


class TestDdcrLifecycle:
    def test_close_then_execute_raises(self):
        r = Ddcr(0.0, 0.25)
        r.close()
        with pytest.raises(Exception):
            _run(r, np.zeros(N, dtype=np.float32))

    def test_close_idempotent(self):
        r = Ddcr(0.0, 0.25)
        r.close()
        r.close()  # second close is a no-op, not a crash

    def test_gc_after_execute(self):
        # Destructor must free the state even after execute ran.
        import gc

        r = Ddcr(0.0, 0.25)
        _run(r, np.zeros(N, dtype=np.float32))
        del r
        gc.collect()


# ------------------------------------------------------------------ #
# Spectral: real tone at f_carrier tuned to DC                        #
# ------------------------------------------------------------------ #


def test_real_tone_to_dc():
    """Real tone at 0.1 fs_in appears near DC after the Ddcr chain.

    With the embedded fs/4 shift in the halfband R2C, a real cosine at
    f_carrier (input rate) appears at the intermediate rate at
    f_int = 2*(f_carrier + 0.25) = 2*f_carrier + 0.5. To shift to DC at the
    intermediate stage, the NCO is tuned to norm_freq = -(2*f_carrier + 0.5).
    """
    f_carrier = 0.1
    r = Ddcr(-(2 * f_carrier + 0.5), 0.25)
    y = _run(r, _real_tone(f_carrier, N * 8))
    if len(y) < 16:
        pytest.skip("output too short for spectral check")
    dominant = _dominant_freq(y)
    assert abs(dominant) < 0.05, f"dominant at {dominant:.4f}, expected near 0"
