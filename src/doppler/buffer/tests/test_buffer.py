"""Tests for doppler.buffer — F32Buffer, F64Buffer, I16Buffer."""

from __future__ import annotations

import threading

import numpy as np
import pytest

from doppler.buffer import F32Buffer, F64Buffer, I16Buffer


# ── F32Buffer (complex64) ──────────────────────────────────────────────────────


class TestF32Buffer:
    def test_capacity(self):
        # On 4 KB-page systems capacity == request; on 16 KB pages a sub-page
        # request (f32(1024) = 8 KiB) rounds up to one page. Either way the
        # buffer holds at least what was asked and stays a power of two.
        buf = F32Buffer(1024)
        assert buf.capacity >= 1024
        assert buf.capacity & (buf.capacity - 1) == 0

    def test_initial_dropped_zero(self):
        buf = F32Buffer(1024)
        assert buf.dropped == 0

    def test_write_returns_true_when_space_available(self):
        buf = F32Buffer(1024)
        x = np.zeros(512, dtype=np.complex64)
        assert buf.write(x) is True

    def test_write_returns_false_when_full(self):
        buf = F32Buffer(1024)
        cap = buf.capacity  # may exceed 1024 on 16 KB-page systems
        assert buf.write(np.zeros(cap, dtype=np.complex64)) is True
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
        assert buf.capacity >= 512
        assert buf.capacity & (buf.capacity - 1) == 0

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
        cap = buf.capacity
        assert buf.write(np.zeros(cap, dtype=np.complex128)) is True
        assert buf.write(np.zeros(1, dtype=np.complex128)) is False

    def test_destroy(self):
        buf = F64Buffer(512)
        buf.destroy()


# ── I16Buffer (int16 IQ pairs) ─────────────────────────────────────────────────


class TestI16Buffer:
    def test_capacity(self):
        buf = I16Buffer(1024)
        assert buf.capacity >= 1024
        assert buf.capacity & (buf.capacity - 1) == 0

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
        cap = buf.capacity  # IQ pairs; flat int16 length is 2 * cap
        assert buf.write(np.zeros(2 * cap, dtype=np.int16)) is True
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


# ── Page-aware sizing (regression for the 16 KB-page mirror bug) ────────────────


import mmap  # noqa: E402  (kept local to this regression section)

PAGE = mmap.PAGESIZE


class TestPageRounding:
    """A sub-page request must round up to a working, page-mirrored buffer.

    Reproduces the macOS arm64 failure (#66): on 16 KB pages an ``f32(1024)``
    buffer is 8 KiB — below one page — so the VM mirror cannot be built. The
    fix rounds the capacity up to the smallest power-of-two that spans a whole
    page; these tests pin both the rounded size and that the mirror still wraps
    correctly afterwards.
    """

    @pytest.mark.parametrize(
        "cls, bytes_per_sample",
        [(F32Buffer, 8), (F64Buffer, 16), (I16Buffer, 4)],
    )
    def test_subpage_request_rounds_up_to_page(self, cls, bytes_per_sample):
        # Ask for a single sample — guaranteed sub-page on any real system.
        buf = cls(1)
        cap = buf.capacity
        assert cap & (cap - 1) == 0, "capacity must stay a power of two"
        assert cap * bytes_per_sample >= PAGE
        assert (cap * bytes_per_sample) % PAGE == 0

    def test_mirror_wraps_after_rounding(self):
        # Fill near the top, consume, then write a block that straddles the
        # wrap boundary; the double-mapping must return it contiguously.
        buf = F32Buffer(1)  # rounds up to the page minimum
        cap = buf.capacity
        prime = cap - 2
        buf.write(np.zeros(prime, dtype=np.complex64))
        buf.consume(prime)  # advance head and tail to cap-2
        straddle = np.arange(4, dtype=np.complex64) + 1j
        assert buf.write(straddle) is True  # indices [cap-2 .. cap+1] wrap
        view = buf.wait(4)
        np.testing.assert_array_equal(view, straddle)
        buf.consume(4)
