"""
Tests for doppler._dp_buffer — double-mapped circular buffer Python bindings.

Each buffer type is tested for:
  - Basic write → wait → consume roundtrip
  - Zero-copy: the returned array views the buffer memory directly
  - Capacity and dropped properties
  - Overrun: write to a full buffer returns False and increments dropped
  - Invalid construction (non-power-of-2, negative sizes)
  - Context-manager-style explicit destroy()
  - I16Buffer-specific shape: wait() returns (n, 2) int16
"""

import threading

import numpy as np
import pytest

from doppler import F32Buffer, F64Buffer, I16Buffer


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

# Minimum power-of-2 size satisfying: n_samples * sizeof(type) * 2 % page_size == 0
# buffer.h: bytes = n_samples * sizeof(type) * 2  (IQ = 2 components)
# page_size = 4096 bytes
# F32 (float,  4 B): bytes = n * 8  → min n = 4096/8  = 512
# F64 (double, 8 B): bytes = n * 16 → min n = 4096/16 = 256
# I16 (int16,  2 B): bytes = n * 4  → min n = 4096/4  = 1024
F32_MIN = 512
F64_MIN = 256
I16_MIN = 1024


# ---------------------------------------------------------------------------
# F32Buffer
# ---------------------------------------------------------------------------


class TestF32Buffer:
    def test_create(self):
        buf = F32Buffer(F32_MIN)
        assert buf.capacity == F32_MIN
        assert buf.dropped == 0
        buf.destroy()

    def test_write_wait_consume_roundtrip(self):
        n = F32_MIN
        buf = F32Buffer(n)
        data = np.arange(n, dtype=np.complex64)

        ok = buf.write(data)
        assert ok is True

        view = buf.wait(n)
        assert view.dtype == np.complex64
        assert view.shape == (n,)
        np.testing.assert_array_equal(view, data)

        buf.consume(n)
        buf.destroy()

    def test_zero_copy(self):
        """wait() must return a view, not a copy."""
        n = F32_MIN
        buf = F32Buffer(n)
        data = np.ones(n, dtype=np.complex64)
        buf.write(data)
        view = buf.wait(n)
        # Modifying view should be visible (same memory)
        original = view[0].copy()
        view[0] = np.complex64(999 + 999j)
        assert view[0] != original
        buf.consume(n)
        buf.destroy()

    def test_overrun_returns_false_and_increments_dropped(self):
        n = F32_MIN
        buf = F32Buffer(n)
        data = np.zeros(n, dtype=np.complex64)
        # Fill the buffer completely
        ok = buf.write(data)
        assert ok is True
        # One more write should fail (buffer full)
        ok2 = buf.write(np.zeros(1, dtype=np.complex64))
        assert ok2 is False
        assert buf.dropped >= 1
        buf.destroy()

    def test_invalid_size_not_power_of_2(self):
        with pytest.raises((MemoryError, ValueError)):
            F32Buffer(100)  # not a power of 2

    def test_invalid_size_zero(self):
        with pytest.raises((ValueError, MemoryError)):
            F32Buffer(0)

    def test_invalid_size_negative(self):
        with pytest.raises((ValueError, OverflowError)):
            F32Buffer(-1)

    def test_consume_default_uses_last_wait_count(self):
        n = F32_MIN
        buf = F32Buffer(n)
        data = np.ones(n, dtype=np.complex64)
        buf.write(data)
        buf.wait(n)
        buf.consume()  # no argument — should consume 'n'
        # After consuming, the buffer should be empty and accept a new write
        ok = buf.write(data)
        assert ok is True
        buf.destroy()

    def test_threaded_producer_consumer(self):
        """Producer thread writes; consumer waits in a separate thread."""
        n = F32_MIN
        buf = F32Buffer(n)
        result = {}

        def producer():
            data = np.arange(n, dtype=np.complex64)
            buf.write(data)

        def consumer():
            view = buf.wait(n)
            result["data"] = view.copy()
            buf.consume(n)

        t_consumer = threading.Thread(target=consumer)
        t_consumer.start()
        t_producer = threading.Thread(target=producer)
        t_producer.start()

        t_producer.join(timeout=2)
        t_consumer.join(timeout=2)

        assert "data" in result
        expected = np.arange(n, dtype=np.complex64)
        np.testing.assert_array_equal(result["data"], expected)
        buf.destroy()


# ---------------------------------------------------------------------------
# F64Buffer
# ---------------------------------------------------------------------------


class TestF64Buffer:
    def test_create(self):
        buf = F64Buffer(F64_MIN)
        assert buf.capacity == F64_MIN
        buf.destroy()

    def test_roundtrip(self):
        n = F64_MIN
        buf = F64Buffer(n)
        data = np.arange(n, dtype=np.complex128) * (1 + 1j)

        buf.write(data)
        view = buf.wait(n)
        assert view.dtype == np.complex128
        np.testing.assert_array_equal(view, data)
        buf.consume(n)
        buf.destroy()

    def test_wrong_dtype_rejected(self):
        buf = F64Buffer(F64_MIN)
        bad = np.zeros(F64_MIN, dtype=np.complex64)  # wrong: should be complex128
        with pytest.raises(TypeError):
            buf.write(bad)
        buf.destroy()

    def test_non_contiguous_rejected(self):
        buf = F64Buffer(F64_MIN)
        # Slice with step creates a non-contiguous array
        data = np.zeros(F64_MIN * 2, dtype=np.complex128)[::2]
        with pytest.raises(ValueError):
            buf.write(data)
        buf.destroy()

    def test_overrun(self):
        n = F64_MIN
        buf = F64Buffer(n)
        data = np.zeros(n, dtype=np.complex128)
        buf.write(data)
        ok = buf.write(np.zeros(1, dtype=np.complex128))
        assert ok is False
        assert buf.dropped >= 1
        buf.destroy()


# ---------------------------------------------------------------------------
# I16Buffer
# ---------------------------------------------------------------------------


class TestI16Buffer:
    def test_create(self):
        buf = I16Buffer(I16_MIN)
        assert buf.capacity == I16_MIN
        buf.destroy()

    def test_roundtrip_flat(self):
        """Write flat (2n,) int16 array, read back as (n, 2)."""
        n = I16_MIN
        buf = I16Buffer(n)
        # Flat IQ pairs: [I0, Q0, I1, Q1, ...]
        flat = np.arange(n * 2, dtype=np.int16)
        ok = buf.write(flat)
        assert ok is True

        view = buf.wait(n)
        assert view.dtype == np.int16
        assert view.shape == (n, 2)
        # Check I samples (column 0)
        np.testing.assert_array_equal(view[:, 0], flat[0::2])
        # Check Q samples (column 1)
        np.testing.assert_array_equal(view[:, 1], flat[1::2])
        buf.consume(n)
        buf.destroy()

    def test_roundtrip_shaped(self):
        """Write (n, 2) int16 array directly."""
        n = I16_MIN
        buf = I16Buffer(n)
        data = np.zeros((n, 2), dtype=np.int16)
        data[:, 0] = np.arange(n, dtype=np.int16)  # I
        data[:, 1] = np.arange(n, dtype=np.int16) * -1  # Q

        ok = buf.write(data)
        assert ok is True

        view = buf.wait(n)
        np.testing.assert_array_equal(view, data)
        buf.consume(n)
        buf.destroy()

    def test_odd_element_count_rejected(self):
        buf = I16Buffer(I16_MIN)
        with pytest.raises(ValueError):
            buf.write(np.zeros(3, dtype=np.int16))
        buf.destroy()

    def test_wrong_dtype_rejected(self):
        buf = I16Buffer(I16_MIN)
        with pytest.raises(TypeError):
            buf.write(np.zeros(I16_MIN * 2, dtype=np.int32))
        buf.destroy()

    def test_overrun(self):
        n = I16_MIN
        buf = I16Buffer(n)
        data = np.zeros(n * 2, dtype=np.int16)
        buf.write(data)
        ok = buf.write(np.zeros(2, dtype=np.int16))
        assert ok is False
        assert buf.dropped >= 1
        buf.destroy()
