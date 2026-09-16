"""Tests for doppler.buffer — F32Buffer, F64Buffer, I16Buffer."""

from __future__ import annotations

import threading

import numpy as np
import pytest

from doppler.buffer import F32Buffer, F64Buffer, I16Buffer

# ── F32Buffer (complex64) ────────────────────────────────────────────────────


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


# ── available (the wait() guard) ─────────────────────────────────────────────
# `wait()` has no timeout and no short return: ask for one sample more than
# has been written and it spins forever. `available` is how a consumer sizes
# a block without shadow-counting what it fed in, so it has to track write
# and consume exactly -- an over-report is a hang, not a wrong number.


class TestAvailable:
    @pytest.mark.parametrize(
        ("cls", "make"),
        [
            (F32Buffer, lambda n: np.zeros(n, dtype=np.complex64)),
            (F64Buffer, lambda n: np.zeros(n, dtype=np.complex128)),
            (I16Buffer, lambda n: np.zeros((n, 2), dtype=np.int16)),
        ],
    )
    def test_tracks_write_and_consume(self, cls, make):
        buf = cls(1024)
        assert buf.available == 0
        buf.write(make(100))
        assert buf.available == 100
        _ = buf.wait(60)
        assert buf.available == 100, "wait() alone must not release samples"
        buf.consume(60)
        assert buf.available == 40
        buf.write(make(10))
        assert buf.available == 50

    def test_survives_the_wrap(self):
        """The count is head-tail, not an index difference, so a buffer
        that has wrapped many times still reports the true backlog."""
        buf = F32Buffer(1024)
        cap = buf.capacity
        for _ in range(5):
            buf.write(np.zeros(cap, dtype=np.complex64))
            _ = buf.wait(cap)
            buf.consume(cap)
            assert buf.available == 0
        buf.write(np.zeros(7, dtype=np.complex64))
        assert buf.available == 7

    def test_a_rejected_write_does_not_count(self):
        """An overrun increments `dropped`, and those samples are not
        readable -- so `available` must not include them."""
        buf = F32Buffer(1024)
        cap = buf.capacity
        buf.write(np.zeros(cap, dtype=np.complex64))
        assert buf.write(np.zeros(1, dtype=np.complex64)) is False
        assert buf.dropped == 1
        assert buf.available == cap

    def test_is_a_safe_lower_bound_under_a_producer(self):
        """Read concurrently it may lag, but never over-reports: whatever
        it returns, wait() for that many returns without spinning."""
        N = 4096
        buf = F32Buffer(1 << 13)
        done = threading.Event()

        def producer():
            for i in range(0, N, 64):
                buf.write(np.full(64, i, dtype=np.complex64))
            done.set()

        t = threading.Thread(target=producer)
        t.start()
        seen = 0
        while seen < N:
            n = buf.available
            if n == 0:
                continue
            _ = buf.wait(n)  # must not spin: n was already readable
            buf.consume(n)
            seen += n
        t.join()
        assert done.is_set()
        assert seen == N
        assert buf.available == 0


# ── F64Buffer (complex128) ───────────────────────────────────────────────────


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


# ── I16Buffer (int16 IQ pairs) ───────────────────────────────────────────────


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


# ── Page-aware sizing (regression for the 16 KB-page mirror bug) ─────────────


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


# ── the unsatisfiable wait (doppler#1335) ────────────────────────────────────


class TestWaitBeyondCapacity:
    """`wait(n)` with `n > capacity` can never be satisfied.

    The ring holds at most `capacity`, so no producer can make
    `head - tail >= n`. Before the guard this fell through to the spin loop
    -- which has exits for end-of-stream and for an interrupt, and none for
    this -- and hung forever at 100% CPU with no diagnostic.

    Every test here runs the call on a THREAD with a join deadline rather
    than calling it directly. That is the whole point: delete the guard and
    these must go RED, and a direct call would hang the suite instead of
    failing it.
    """

    @staticmethod
    def _call_with_deadline(fn, timeout=5.0):
        """Run `fn` on a thread; return its exception, or raise on a hang."""
        box: list[BaseException | None] = [None]
        done = threading.Event()

        def run() -> None:
            try:
                fn()
            except BaseException as exc:
                box[0] = exc
            finally:
                done.set()

        t = threading.Thread(target=run, daemon=True)
        t.start()
        if not done.wait(timeout):
            raise AssertionError(
                f"wait() did not return within {timeout}s -- the "
                "unsatisfiable-n guard is gone and it is spinning (#1335)"
            )
        t.join(timeout)
        return box[0]

    @pytest.mark.parametrize(
        "cls,dtype",
        [
            (F32Buffer, np.complex64),
            (F64Buffer, np.complex128),
            (I16Buffer, np.int16),
        ],
    )
    def test_raises_rather_than_spinning(self, cls, dtype):
        buf = cls(1024)
        over = buf.capacity + 1
        exc = self._call_with_deadline(lambda: buf.wait(over))
        assert isinstance(exc, ValueError), f"expected ValueError, got {exc!r}"
        # The message must name BOTH numbers: the bound is `capacity`, which
        # is rounded UP from the constructor argument, so a caller reasoning
        # about the number they passed cannot work it out from `n` alone.
        assert str(over) in str(exc) and str(buf.capacity) in str(exc), (
            f"message names neither n nor capacity: {exc}"
        )

    def test_is_not_reported_as_end_of_stream(self):
        """The failure mode the guard replaces was a lie, not just a hang.

        Falling through to the C wait's NULL would land in the branch that
        reports EOFError -- sending the caller to look at a producer that is
        fine. An open, un-closed ring must never raise EOFError.
        """
        buf = F32Buffer(1024)
        assert not buf.closed
        exc = self._call_with_deadline(lambda: buf.wait(buf.capacity + 1))
        assert not isinstance(exc, EOFError), "an open ring is not at EOF"
        assert isinstance(exc, ValueError)

    def test_exactly_capacity_is_still_allowed(self):
        """The bound is inclusive: n == capacity is satisfiable, and works.

        Without this the guard could be off by one in the safe-looking
        direction and nothing above would notice.
        """
        buf = F32Buffer(1024)
        cap = buf.capacity
        assert buf.write(np.ones(cap, dtype=np.complex64)) is True
        view = buf.wait(cap)
        assert len(view) == cap
        buf.consume(cap)


# ── the shape invariant the three instantiations share (doppler#1346) ────────
# `wait(n)` hands back one element per SAMPLE on f32/f64 and an I/Q pair ROW
# on i16, so `len()` agrees across the three while the ELEMENT does not.
# Anything generic over them -- a helper, a benchmark, a consumer written
# against one width and pointed at another -- then means something different
# without saying so. The fix is upstream: the ring becomes a jm-generated
# template (just-makeit#1310) returning `[('i','<i2'),('q','<i2')]` for i16,
# which is what buffer_ext.c's own header comment has claimed all along.


class TestTheThreeInstantiationsAgree:
    """The macro generates a type per instantiation, so f32 passing says
    nothing about f64 and i16 -- the same reason the EOS vocabulary test is
    parametrized. These pin the SHAPE contract across all three."""

    @staticmethod
    def _make(cls, n):
        if cls is F32Buffer:
            return np.zeros(n, dtype=np.complex64)
        if cls is F64Buffer:
            return np.zeros(n, dtype=np.complex128)
        return np.zeros((n, 2), dtype=np.int16)

    @pytest.mark.parametrize("cls", [F32Buffer, F64Buffer, I16Buffer])
    def test_len_is_the_sample_count(self, cls):
        """`len(wait(n)) == n` on every width.

        This half already holds, and it is exactly why the divergence below
        went unnoticed: the count agrees, so a caller checking only `len()`
        sees three types that look identical.
        """
        buf = cls(1024)
        buf.write(self._make(cls, 64))
        view = buf.wait(64)
        assert len(view) == 64
        buf.consume(64)

    @pytest.mark.xfail(
        strict=True,
        reason=(
            "doppler#1346: I16Buffer.wait() returns 2-D (n, 2) while f32/f64 "
            "return 1-D, and buffer_ext.c's header comment already promises a "
            "structured array. The fix is just-buildit/just-makeit#1310, "
            "which makes the ring a generated template returning "
            "[('i','<i2'),('q','<i2')] -- 1-D, one element per sample. STRICT "
            "on purpose: xfail_strict is not set repo-wide, so a bare marker "
            "would XPASS in silence. Strict makes the adoption announce "
            "itself -- CI goes red until this marker is removed."
        ),
    )
    def test_rank_agrees_across_widths(self):
        """Rank is the invariant that silently broke.

        numpy has no complex-integer dtype, so i16 cannot take the f32/f64
        route literally -- but a structured dtype gets one element per sample
        AND refuses arithmetic loudly, where a packed int32 would carry across
        the I/Q boundary and corrupt silently (measured on #1346).
        """
        ranks = {}
        for cls in (F32Buffer, F64Buffer, I16Buffer):
            buf = cls(1024)
            buf.write(self._make(cls, 64))
            view = buf.wait(64)
            ranks[cls.__name__] = view.ndim
            buf.consume(64)
        assert len(set(ranks.values())) == 1, ranks
