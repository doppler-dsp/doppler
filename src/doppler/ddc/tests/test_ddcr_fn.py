"""Tests for the functional DDCR API (doppler.ddc.ddcr_*).

Lifecycle / safety coverage:
  - explicit destroy: correct teardown, double-destroy guard
  - GC path: no explicit destroy, destructor runs correctly
  - use-after-destroy: raises RuntimeError, never segfaults
  - view lifetime: output view lives in caller's buffer, not in the capsule;
    destroying or GC-ing the state must not affect a live view
  - read-only / wrong-dtype output: rejected early with TypeError
  - bad state arg (None, wrong capsule): raises TypeError
  - undersized output buffer: no overflow, just fewer outputs
"""
import gc
import pytest
import numpy as np

from doppler.ddc import (
    ddcr_create,
    ddcr_execute,
    ddcr_reset,
    ddcr_destroy,
    ddcr_get_norm_freq,
    ddcr_set_norm_freq,
    ddcr_get_rate,
)

N    = 1024
RATE = 0.25
BUF  = N          # safe upper bound for decimating DDC


# ------------------------------------------------------------------ #
# Helpers                                                              #
# ------------------------------------------------------------------ #

def _make_state():
    return ddcr_create(0.0, RATE)

def _buf():
    return np.empty(BUF, dtype=np.complex64)

def _x():
    return np.ones(N, dtype=np.float32)


# ------------------------------------------------------------------ #
# Lifecycle — explicit destroy                                         #
# ------------------------------------------------------------------ #

class TestDdcrFnLifecycle:
    def test_create_returns_capsule(self):
        s = _make_state()
        assert s is not None
        ddcr_destroy(s)

    def test_double_destroy_raises(self):
        s = _make_state()
        ddcr_destroy(s)
        with pytest.raises(RuntimeError, match="already been destroyed"):
            ddcr_destroy(s)

    def test_use_after_destroy_execute(self):
        s = _make_state()
        ddcr_destroy(s)
        with pytest.raises(RuntimeError):
            ddcr_execute(s, _x(), _buf())

    def test_use_after_destroy_reset(self):
        s = _make_state()
        ddcr_destroy(s)
        with pytest.raises(RuntimeError):
            ddcr_reset(s)

    def test_use_after_destroy_get_norm_freq(self):
        s = _make_state()
        ddcr_destroy(s)
        with pytest.raises(RuntimeError):
            ddcr_get_norm_freq(s)

    def test_use_after_destroy_set_norm_freq(self):
        s = _make_state()
        ddcr_destroy(s)
        with pytest.raises(RuntimeError):
            ddcr_set_norm_freq(s, 0.1)

    def test_use_after_destroy_get_rate(self):
        s = _make_state()
        ddcr_destroy(s)
        with pytest.raises(RuntimeError):
            ddcr_get_rate(s)


# ------------------------------------------------------------------ #
# Lifecycle — GC path (no explicit destroy)                           #
# ------------------------------------------------------------------ #

class TestDdcrFnGC:
    def test_gc_capsule_alone(self):
        # Destructor must run without crashing.
        s = _make_state()
        del s
        gc.collect()

    def test_gc_capsule_after_execute(self):
        # Destructor must free state even if execute was called.
        s = _make_state()
        buf = _buf()
        ddcr_execute(s, _x(), buf)
        del s
        gc.collect()

    def test_gc_capsule_with_live_view(self):
        # View lives in caller's buf, not in capsule.
        # GC-ing the capsule must not affect the view.
        s = _make_state()
        buf = _buf()
        y = ddcr_execute(s, _x(), buf)
        saved = y.copy()
        del s
        gc.collect()
        # buf (and thus y) still valid — no segfault on access
        np.testing.assert_array_equal(y, saved)

    def test_view_survives_explicit_destroy(self):
        s = _make_state()
        buf = _buf()
        y = ddcr_execute(s, _x(), buf)
        saved = y.copy()
        ddcr_destroy(s)
        del s
        gc.collect()
        # View is a slice of buf — completely independent of state
        assert np.shares_memory(y, buf)
        np.testing.assert_array_equal(y, saved)


# ------------------------------------------------------------------ #
# Bad state arguments                                                  #
# ------------------------------------------------------------------ #

_BAD_STATE_EXC = (TypeError, ValueError, RuntimeError)


class TestDdcrFnBadState:
    def test_none_state_raises(self):
        with pytest.raises(_BAD_STATE_EXC):
            ddcr_execute(None, _x(), _buf())

    def test_int_state_raises(self):
        with pytest.raises(_BAD_STATE_EXC):
            ddcr_execute(42, _x(), _buf())

    def test_plain_object_state_raises(self):
        with pytest.raises(_BAD_STATE_EXC):
            ddcr_execute(object(), _x(), _buf())

    def test_reset_none_state_raises(self):
        with pytest.raises(_BAD_STATE_EXC):
            ddcr_reset(None)

    def test_get_norm_freq_none_raises(self):
        with pytest.raises(_BAD_STATE_EXC):
            ddcr_get_norm_freq(None)


# ------------------------------------------------------------------ #
# Output buffer validation                                             #
# ------------------------------------------------------------------ #

class TestDdcrFnOutputBuf:
    def test_read_only_out_raises(self):
        s = _make_state()
        buf = _buf()
        buf.flags.writeable = False
        with pytest.raises(TypeError):
            ddcr_execute(s, _x(), buf)
        ddcr_destroy(s)

    def test_wrong_out_dtype_raises(self):
        s = _make_state()
        with pytest.raises(TypeError):
            ddcr_execute(s, _x(), np.empty(BUF, dtype=np.float32))
        ddcr_destroy(s)

    def test_wrong_in_dtype_raises(self):
        s = _make_state()
        with pytest.raises((TypeError, ValueError)):
            ddcr_execute(s, np.zeros(N, dtype=np.complex64), _buf())
        ddcr_destroy(s)

    def test_undersized_out_limits_output(self):
        # Small buffer: n_out capped at buffer capacity, no overflow.
        s = _make_state()
        small = np.empty(4, dtype=np.complex64)
        y = ddcr_execute(s, _x(), small)
        assert len(y) <= 4
        assert np.shares_memory(y, small)
        ddcr_destroy(s)

    def test_output_is_view_of_out(self):
        s = _make_state()
        buf = _buf()
        y = ddcr_execute(s, _x(), buf)
        assert np.shares_memory(y, buf)
        ddcr_destroy(s)

    def test_empty_input(self):
        s = _make_state()
        y = ddcr_execute(s, np.zeros(0, dtype=np.float32), _buf())
        assert len(y) == 0
        ddcr_destroy(s)


# ------------------------------------------------------------------ #
# Accessors                                                            #
# ------------------------------------------------------------------ #

class TestDdcrFnAccessors:
    def test_get_norm_freq(self):
        s = ddcr_create(0.1, RATE)
        assert abs(ddcr_get_norm_freq(s) - 0.1) < 1e-9
        ddcr_destroy(s)

    def test_get_rate(self):
        s = _make_state()
        assert abs(ddcr_get_rate(s) - RATE) < 1e-9
        ddcr_destroy(s)

    def test_set_norm_freq_roundtrip(self):
        s = ddcr_create(0.1, RATE)
        ddcr_set_norm_freq(s, 0.3)
        assert abs(ddcr_get_norm_freq(s) - 0.3) < 1e-9
        ddcr_destroy(s)


# ------------------------------------------------------------------ #
# Execute correctness                                                  #
# ------------------------------------------------------------------ #

class TestDdcrFnExecute:
    def test_returns_complex64(self):
        s = _make_state()
        y = ddcr_execute(s, _x(), _buf())
        assert y.dtype == np.complex64
        ddcr_destroy(s)

    def test_output_length_approx(self):
        s = _make_state()
        y = ddcr_execute(s, _x(), _buf())
        assert abs(len(y) / (N * RATE) - 1.0) < 0.1
        ddcr_destroy(s)

    def test_independent_states(self):
        s1 = ddcr_create(0.0, RATE)
        s2 = ddcr_create(0.1, RATE)
        y1 = ddcr_execute(s1, _x(), _buf())
        y2 = ddcr_execute(s2, _x(), _buf())
        assert not np.allclose(y1, y2)
        ddcr_destroy(s1)
        ddcr_destroy(s2)

    def test_reset_determinism(self):
        rng = np.random.default_rng(42)
        x = rng.random(N).astype(np.float32)
        s = _make_state()
        buf = _buf()
        y1 = ddcr_execute(s, x, buf).copy()
        ddcr_reset(s)
        y2 = ddcr_execute(s, x, buf)
        np.testing.assert_array_equal(y1, y2)
        ddcr_destroy(s)

    def test_set_norm_freq_no_reset(self):
        s = ddcr_create(0.1, RATE)
        buf = _buf()
        ddcr_execute(s, _x(), buf)
        ddcr_set_norm_freq(s, 0.2)
        y = ddcr_execute(s, _x(), buf)
        assert len(y) > 0
        ddcr_destroy(s)
