"""Tests for the functional DDCR API (doppler.ddc.ddcr_*)."""
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

N = 1024


# ------------------------------------------------------------------ #
# Lifecycle                                                            #
# ------------------------------------------------------------------ #


class TestDdcrFnLifecycle:
    def test_create_returns_capsule(self):
        state = ddcr_create(0.0, 0.25)
        assert state is not None
        ddcr_destroy(state)

    def test_double_destroy_raises(self):
        state = ddcr_create(0.0, 0.25)
        ddcr_destroy(state)
        with pytest.raises(RuntimeError, match="already been destroyed"):
            ddcr_destroy(state)

    def test_use_after_destroy_raises(self):
        state = ddcr_create(0.0, 0.25)
        ddcr_destroy(state)
        with pytest.raises(RuntimeError):
            ddcr_execute(state, np.zeros(64, dtype=np.float32))

    def test_gc_without_destroy(self):
        # GC should free resources without crashing.
        state = ddcr_create(0.0, 0.25)
        del state


# ------------------------------------------------------------------ #
# Accessors                                                            #
# ------------------------------------------------------------------ #


class TestDdcrFnAccessors:
    def test_get_norm_freq(self):
        state = ddcr_create(0.1, 0.25)
        assert abs(ddcr_get_norm_freq(state) - 0.1) < 1e-9
        ddcr_destroy(state)

    def test_get_rate(self):
        state = ddcr_create(0.0, 0.25)
        assert abs(ddcr_get_rate(state) - 0.25) < 1e-9
        ddcr_destroy(state)

    def test_set_norm_freq_roundtrip(self):
        state = ddcr_create(0.1, 0.25)
        ddcr_set_norm_freq(state, 0.3)
        assert abs(ddcr_get_norm_freq(state) - 0.3) < 1e-9
        ddcr_destroy(state)


# ------------------------------------------------------------------ #
# Execute                                                              #
# ------------------------------------------------------------------ #


class TestDdcrFnExecute:
    def test_returns_complex64(self):
        state = ddcr_create(0.0, 0.25)
        y = ddcr_execute(state, np.zeros(N, dtype=np.float32))
        assert y.dtype == np.complex64
        ddcr_destroy(state)

    def test_output_length_approx(self):
        state = ddcr_create(0.0, 0.25)
        x = np.ones(N, dtype=np.float32)
        y = ddcr_execute(state, x)
        expected = N * 0.25
        assert abs(len(y) / expected - 1.0) < 0.1
        ddcr_destroy(state)

    def test_empty_input(self):
        state = ddcr_create(0.0, 0.25)
        y = ddcr_execute(state, np.zeros(0, dtype=np.float32))
        assert len(y) == 0
        ddcr_destroy(state)

    def test_rejects_wrong_dtype(self):
        state = ddcr_create(0.0, 0.25)
        with pytest.raises((TypeError, ValueError)):
            ddcr_execute(state, np.zeros(N, dtype=np.complex64))
        ddcr_destroy(state)

    def test_buffer_reuse_across_calls(self):
        state = ddcr_create(0.0, 0.25)
        x = np.ones(N, dtype=np.float32)
        y1 = ddcr_execute(state, x)
        y2 = ddcr_execute(state, x)
        # Outputs are independent arrays (copy, not aliased buffer).
        assert y1 is not y2
        ddcr_destroy(state)

    def test_state_is_explicit_and_independent(self):
        # Two separate states process the same input independently.
        s1 = ddcr_create(0.0, 0.25)
        s2 = ddcr_create(0.1, 0.25)
        x = np.ones(N, dtype=np.float32)
        y1 = ddcr_execute(s1, x)
        y2 = ddcr_execute(s2, x)
        # Different norm_freq → different output.
        assert not np.allclose(y1, y2)
        ddcr_destroy(s1)
        ddcr_destroy(s2)


# ------------------------------------------------------------------ #
# Reset                                                                #
# ------------------------------------------------------------------ #


class TestDdcrFnReset:
    def test_reset_gives_same_output(self):
        rng = np.random.default_rng(42)
        x = rng.random(N).astype(np.float32)
        state = ddcr_create(0.0, 0.25)
        y1 = ddcr_execute(state, x)
        ddcr_reset(state)
        y2 = ddcr_execute(state, x)
        np.testing.assert_array_equal(y1, y2)
        ddcr_destroy(state)

    def test_set_norm_freq_no_reset(self):
        state = ddcr_create(0.1, 0.25)
        ddcr_execute(state, np.ones(N, dtype=np.float32))
        ddcr_set_norm_freq(state, 0.2)
        y = ddcr_execute(state, np.ones(N, dtype=np.float32))
        assert len(y) > 0
        ddcr_destroy(state)
