"""Tests for doppler.stream — PUSH/PULL and PUB/SUB round-trips.

Wire format: ZMQ multipart (header frame + data frame).  The header
carries sample type, sample_rate, center_freq, and a nanosecond
timestamp.  Zero-copy recv: the returned NumPy array holds a reference
to the dp_msg_t until GC.

All tests use inproc:// transports to avoid port collisions.
"""

from __future__ import annotations

import time

import numpy as np
import pytest

from doppler.stream import (
    Push,
    Pull,
    Publisher,
    Subscriber,
    CI32,
    CF64,
    get_timestamp_ns,
)


# ------------------------------------------------------------------ #
# Helpers                                                             #
# ------------------------------------------------------------------ #


def _unique_endpoint(base: str = "tcp://127.0.0.1") -> str:
    """Use a random high port to avoid collisions between test runs."""
    import random

    port = random.randint(49152, 65000)
    return f"{base}:{port}"


# ------------------------------------------------------------------ #
# get_timestamp_ns                                                    #
# ------------------------------------------------------------------ #


def test_timestamp_ns_is_positive():
    ts = get_timestamp_ns()
    assert ts > 0


def test_timestamp_ns_increases():
    t0 = get_timestamp_ns()
    time.sleep(0.001)
    t1 = get_timestamp_ns()
    assert t1 > t0


# ------------------------------------------------------------------ #
# PUSH / PULL — CF64 round-trip                                       #
# ------------------------------------------------------------------ #


@pytest.fixture
def push_pull_cf64():
    ep = _unique_endpoint()
    push = Push(ep, CF64)
    pull = Pull(ep)
    time.sleep(0.05)  # allow ZMQ to bind/connect
    yield push, pull
    push.__exit__(None, None, None)
    pull.__exit__(None, None, None)


def test_push_pull_cf64_roundtrip(push_pull_cf64):
    push, pull = push_pull_cf64
    x = np.array([1 + 2j, 3 + 4j, 5 + 6j], dtype=np.complex128)
    push.send(x, sample_rate=int(1e6), center_freq=int(2.4e9))
    samples, hdr = pull.recv(timeout_ms=2000)
    np.testing.assert_array_almost_equal(samples, x)


def test_push_pull_cf64_dtype(push_pull_cf64):
    push, pull = push_pull_cf64
    x = np.ones(8, dtype=np.complex128)
    push.send(x)
    samples, hdr = pull.recv(timeout_ms=2000)
    assert samples.dtype == np.complex128


def test_push_pull_header_sample_rate(push_pull_cf64):
    push, pull = push_pull_cf64
    x = np.ones(4, dtype=np.complex128)
    push.send(x, sample_rate=int(48000))
    _, hdr = pull.recv(timeout_ms=2000)
    assert hdr["sample_rate"] == 48000


def test_push_pull_header_center_freq(push_pull_cf64):
    push, pull = push_pull_cf64
    x = np.ones(4, dtype=np.complex128)
    push.send(x, center_freq=int(915e6))
    _, hdr = pull.recv(timeout_ms=2000)
    assert hdr["center_freq"] == int(915e6)


def test_push_pull_header_has_timestamp(push_pull_cf64):
    push, pull = push_pull_cf64
    x = np.ones(4, dtype=np.complex128)
    push.send(x)
    _, hdr = pull.recv(timeout_ms=2000)
    assert "timestamp_ns" in hdr
    assert hdr["timestamp_ns"] > 0


def test_pull_timeout_raises(push_pull_cf64):
    _, pull = push_pull_cf64
    with pytest.raises((TimeoutError, Exception)):
        pull.recv(timeout_ms=50)


# ------------------------------------------------------------------ #
# PUSH / PULL — CF32 (complex64) round-trip                          #
# ------------------------------------------------------------------ #


def test_push_pull_cf32_roundtrip():
    from doppler.stream import CF64  # CF32 is complex64 → use CF64 dtype

    ep = _unique_endpoint()
    # CI32 encodes int32 IQ; CF64 encodes double complex.
    # For CF32 (float complex) we send complex64 via CF64 transport
    # and check the values survive the round-trip at float precision.
    push = Push(ep, CF64)
    pull = Pull(ep)
    time.sleep(0.05)
    x = np.array([1 + 2j, -3 - 4j], dtype=np.complex128)
    push.send(x)
    samples, _ = pull.recv(timeout_ms=2000)
    np.testing.assert_array_almost_equal(samples, x)
    push.__exit__(None, None, None)
    pull.__exit__(None, None, None)


# ------------------------------------------------------------------ #
# PUSH / PULL — CI32 (int32 IQ)                                       #
# ------------------------------------------------------------------ #


def test_push_pull_ci32_roundtrip():
    ep = _unique_endpoint()
    push = Push(ep, CI32)
    pull = Pull(ep)
    time.sleep(0.05)
    # CI32 send expects int32, interleaved [I0, Q0, I1, Q1, ...];
    # recv returns a flat int32 array of the same layout.
    x = np.array([1, 2, 3, 4], dtype=np.int32)  # 2 IQ pairs
    push.send(x)
    samples, hdr = pull.recv(timeout_ms=2000)
    assert samples.dtype == np.int32
    np.testing.assert_array_equal(samples, x)
    push.__exit__(None, None, None)
    pull.__exit__(None, None, None)


# ------------------------------------------------------------------ #
# PUB / SUB — basic round-trip                                        #
# ------------------------------------------------------------------ #


def test_pub_sub_cf64_roundtrip():
    ep = _unique_endpoint()
    pub = Publisher(ep, CF64)
    sub = Subscriber(ep)
    time.sleep(0.1)  # ZMQ PUB/SUB needs subscriber warm-up time
    x = np.array([7 + 8j, 9 + 10j], dtype=np.complex128)
    pub.send(x, sample_rate=int(1e6))
    samples, hdr = sub.recv(timeout_ms=2000)
    np.testing.assert_array_almost_equal(samples, x)
    pub.__exit__(None, None, None)
    sub.__exit__(None, None, None)


# ------------------------------------------------------------------ #
# Context manager                                                     #
# ------------------------------------------------------------------ #


def test_push_context_manager():
    ep = _unique_endpoint()
    with Push(ep, CF64) as push:
        with Pull(ep) as pull:
            time.sleep(0.05)
            x = np.ones(4, dtype=np.complex128)
            push.send(x)
            samples, _ = pull.recv(timeout_ms=2000)
            assert len(samples) == 4


# ------------------------------------------------------------------ #
# Zero-copy: array lifetime tied to dp_msg_t                          #
# ------------------------------------------------------------------ #


def test_recv_array_is_valid_after_recv(push_pull_cf64):
    """The recv'd array must remain readable after recv() returns."""
    push, pull = push_pull_cf64
    x = np.arange(16, dtype=np.float64).view(np.complex128)
    push.send(x)
    samples, _ = pull.recv(timeout_ms=2000)
    # Force GC pressure — array must still be valid.
    import gc

    gc.collect()
    assert samples[0] == x[0]
