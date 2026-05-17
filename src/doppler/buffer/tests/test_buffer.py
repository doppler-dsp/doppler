"""Tests for doppler.buffer — F32Buffer, F64Buffer, I16Buffer."""

from __future__ import annotations

import threading

import numpy as np
import pytest

from doppler.buffer import F32Buffer, F64Buffer, I16Buffer


# ── F32Buffer (complex64) ──────────────────────────────────────────────────────


class TestF32Buffer:
    def test_capacity(self):
        buf = F32Buffer(1024)
        assert buf.capacity == 1024

    def test_initial_dropped_zero(self):
        buf = F32Buffer(1024)
        assert buf.dropped == 0

    def test_write_returns_true_when_space_available(self):
        buf = F32Buffer(1024)
        x = np.zeros(512, dtype=np.complex64)
        assert buf.write(x) is True

    def test_write_returns_false_when_full(self):
        buf = F32Buffer(1024)
        assert buf.write(np.zeros(1024, dtype=np.complex64)) is True
        assert buf.write(np.zeros(1, dtype=np.complex64)) is False

    def test_roundtrip_values(self):
        buf = F32Buffer(1024)
        x = np.array([1 + 2j, 3 + 4j, 5 + 6j], dtype=np.complex64)
        buf.write(x)
        view = buf.wait(3)
        np.testing.assert_array_equal(view, x)
        buf.consume(3)

    def test_wait_dtype_is_complex64(self):
        buf = F32Buffer(1024)
        buf.write(np.ones(4, dtype=np.complex64))
        view = buf.wait(4)
        assert view.dtype == np.complex64
        buf.consume()

    def test_consume_default_uses_last_wait(self):
        buf = F32Buffer(1024)
        buf.write(np.ones(8, dtype=np.complex64))
        _ = buf.wait(8)
        buf.consume()
        buf.write(np.ones(8, dtype=np.complex64))

    def test_write_wrong_dtype_raises(self):
        buf = F32Buffer(1024)
        with pytest.raises(TypeError):
            buf.write(np.zeros(4, dtype=np.complex128))

    def test_write_non_contiguous_raises(self):
        buf = F32Buffer(1024)
        x = np.zeros((8, 2), dtype=np.complex64)
        with pytest.raises(ValueError):
            buf.write(x[::2])

    def test_threaded_producer_consumer(self):
        N = 512
        buf = F32Buffer(1024)
        sent = (np.arange(N) + 1j * np.arange(N)).astype(np.complex64)

        def producer():
            buf.write(sent)

        t = threading.Thread(target=producer)
        t.start()
        view = buf.wait(N)
        np.testing.assert_array_equal(view, sent)
        buf.consume()
        t.join()

    def test_destroy(self):
        buf = F32Buffer(1024)
        buf.destroy()


# ── F64Buffer (complex128) ─────────────────────────────────────────────────────


class TestF64Buffer:
    def test_capacity(self):
        buf = F64Buffer(512)
        assert buf.capacity == 512

    def test_roundtrip_values(self):
        buf = F64Buffer(512)
        x = np.array([1 + 2j, 3 + 4j], dtype=np.complex128)
        buf.write(x)
        view = buf.wait(2)
        assert view.dtype == np.complex128
        np.testing.assert_array_equal(view, x)
        buf.consume()

    def test_write_wrong_dtype_raises(self):
        buf = F64Buffer(512)
        with pytest.raises(TypeError):
            buf.write(np.zeros(4, dtype=np.complex64))

    def test_full_then_overflow(self):
        buf = F64Buffer(512)
        assert buf.write(np.zeros(512, dtype=np.complex128)) is True
        assert buf.write(np.zeros(1, dtype=np.complex128)) is False

    def test_destroy(self):
        buf = F64Buffer(512)
        buf.destroy()


# ── I16Buffer (int16 IQ pairs) ─────────────────────────────────────────────────


class TestI16Buffer:
    def test_capacity(self):
        buf = I16Buffer(1024)
        assert buf.capacity == 1024

    def test_roundtrip_flat_iq(self):
        buf = I16Buffer(1024)
        flat = np.array([1, 2, 3, 4, 5, 6], dtype=np.int16)
        buf.write(flat)
        view = buf.wait(3)
        assert view.shape == (3, 2)
        assert view.dtype == np.int16
        np.testing.assert_array_equal(view.ravel(), flat)
        buf.consume()

    def test_write_odd_length_raises(self):
        buf = I16Buffer(1024)
        with pytest.raises(ValueError):
            buf.write(np.zeros(3, dtype=np.int16))

    def test_write_wrong_dtype_raises(self):
        buf = I16Buffer(1024)
        with pytest.raises(TypeError):
            buf.write(np.zeros(4, dtype=np.int32))

    def test_full_then_overflow(self):
        buf = I16Buffer(1024)
        assert buf.write(np.zeros(2 * 1024, dtype=np.int16)) is True
        assert buf.write(np.zeros(2, dtype=np.int16)) is False

    def test_threaded_producer_consumer(self):
        N = 256
        buf = I16Buffer(1024)
        flat = np.arange(N * 2, dtype=np.int16)

        def producer():
            buf.write(flat)

        t = threading.Thread(target=producer)
        t.start()
        view = buf.wait(N)
        np.testing.assert_array_equal(view.ravel(), flat)
        buf.consume()
        t.join()

    def test_destroy(self):
        buf = I16Buffer(1024)
        buf.destroy()
