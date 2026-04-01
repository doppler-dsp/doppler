"""
Comprehensive tests for all doppler streaming patterns (C extension).

Patterns tested:
  - PUB/SUB    (broadcast)
  - PUSH/PULL  (pipeline/load-balancing)
  - REQ/REP    (request-reply)
  - CF64/CI32/CF128 (all sample types)
  - Timeout handling
  - Context managers
"""

import time
import numpy as np
import pytest

from doppler import (
    Publisher,
    Subscriber,
    Push,
    Pull,
    Requester,
    Replier,
    CF64,
    CI32,
    CF128,
)

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def _next_port(base: list[int]) -> str:
    """Return next unique loopback endpoint (avoids port collisions between tests)."""
    base[0] += 1
    return f"tcp://127.0.0.1:{base[0]}"


_PORT = [16100]  # mutable base; incremented per test


# ---------------------------------------------------------------------------
# PUSH / PULL — pipeline pattern
# ---------------------------------------------------------------------------


class TestPushPull:
    def test_cf64_pipeline(self):
        """CF64 samples flow through PUSH→PULL pipeline."""
        endpoint = _next_port(_PORT)

        push = Push(endpoint, CF64)
        pull = Pull(endpoint.replace("127.0.0.1", "localhost"))

        # Brief sleep for connection
        time.sleep(0.05)

        samples_out = np.array([1 + 2j, 3 + 4j], dtype=np.complex128)
        push.send(samples_out, sample_rate=1_000_000, center_freq=0)

        samples_in, header = pull.recv(timeout_ms=1000)

        assert samples_in.dtype == np.complex128
        assert len(samples_in) == 2
        assert np.allclose(samples_out, samples_in)
        assert header["sample_type"] == CF64

        push.close()
        pull.close()

    def test_ci32_pipeline(self):
        """CI32 samples flow through PUSH→PULL pipeline."""
        endpoint = _next_port(_PORT)

        push = Push(endpoint, CI32)
        pull = Pull(endpoint.replace("127.0.0.1", "localhost"))

        time.sleep(0.05)

        samples_out = np.array([100, 200, 300, 400], dtype=np.int32)
        push.send(samples_out, sample_rate=1_000_000, center_freq=0)

        samples_in, header = pull.recv(timeout_ms=1000)

        assert samples_in.dtype == np.int32
        assert len(samples_in) == 4
        assert np.array_equal(samples_out, samples_in)
        assert header["num_samples"] == 2  # 2 complex samples
        assert header["sample_type"] == CI32

        push.close()
        pull.close()

    def test_pull_timeout(self):
        """Pull raises TimeoutError when no data available."""
        endpoint = _next_port(_PORT)

        push = Push(endpoint, CF64)
        pull = Pull(endpoint.replace("127.0.0.1", "localhost"))

        time.sleep(0.05)

        # Don't send anything
        with pytest.raises(TimeoutError):
            pull.recv(timeout_ms=100)

        push.close()
        pull.close()

    def test_push_pull_context_manager(self):
        """Push/Pull work as context managers."""
        endpoint = _next_port(_PORT)

        with Push(endpoint, CF64) as push:
            with Pull(endpoint.replace("127.0.0.1", "localhost")) as pull:
                time.sleep(0.05)
                samples = np.array([7 + 8j], dtype=np.complex128)
                push.send(samples, sample_rate=1_000_000, center_freq=0)
                data, hdr = pull.recv(timeout_ms=1000)
                assert len(data) == 1
                assert data[0] == 7 + 8j


# ---------------------------------------------------------------------------
# REQ / REP — request-reply pattern
# ---------------------------------------------------------------------------


class TestReqRep:
    def test_cf64_request_reply(self):
        """CF64 request-reply roundtrip."""
        endpoint = _next_port(_PORT)

        rep = Replier(endpoint, CF64)
        req = Requester(endpoint.replace("127.0.0.1", "localhost"), CF64)

        time.sleep(0.05)

        # Send request
        request_data = np.array([1 + 1j], dtype=np.complex128)
        req.send(request_data, sample_rate=1_000_000, center_freq=0)

        # Receive request
        recv_request, req_hdr = rep.recv(timeout_ms=1000)
        assert np.allclose(recv_request, request_data)

        # Send reply
        reply_data = np.array([2 + 2j], dtype=np.complex128)
        rep.send(reply_data, sample_rate=1_000_000, center_freq=0)

        # Receive reply
        recv_reply, rep_hdr = req.recv(timeout_ms=1000)
        assert np.allclose(recv_reply, reply_data)

        req.close()
        rep.close()

    def test_ci32_request_reply(self):
        """CI32 request-reply roundtrip."""
        endpoint = _next_port(_PORT)

        rep = Replier(endpoint, CI32)
        req = Requester(endpoint.replace("127.0.0.1", "localhost"), CI32)

        time.sleep(0.05)

        # Send request
        request_data = np.array([10, 20], dtype=np.int32)
        req.send(request_data, sample_rate=1_000_000, center_freq=0)

        # Receive request
        recv_request, req_hdr = rep.recv(timeout_ms=1000)
        assert np.array_equal(recv_request, request_data)
        assert req_hdr["num_samples"] == 1  # 1 complex sample

        # Send reply
        reply_data = np.array([30, 40], dtype=np.int32)
        rep.send(reply_data, sample_rate=1_000_000, center_freq=0)

        # Receive reply
        recv_reply, rep_hdr = req.recv(timeout_ms=1000)
        assert np.array_equal(recv_reply, reply_data)

        req.close()
        rep.close()

    def test_req_rep_context_manager(self):
        """Requester/Replier work as context managers."""
        endpoint = _next_port(_PORT)

        with Replier(endpoint, CF64) as rep:
            with Requester(endpoint.replace("127.0.0.1", "localhost"), CF64) as req:
                time.sleep(0.05)

                # Request
                req.send(
                    np.array([5 + 5j], dtype=np.complex128),
                    sample_rate=1_000_000,
                    center_freq=0,
                )
                data, _ = rep.recv(timeout_ms=1000)
                assert data[0] == 5 + 5j

                # Reply
                rep.send(
                    np.array([9 + 9j], dtype=np.complex128),
                    sample_rate=1_000_000,
                    center_freq=0,
                )
                reply, _ = req.recv(timeout_ms=1000)
                assert reply[0] == 9 + 9j


# ---------------------------------------------------------------------------
# CF128 — complex long double (quad precision)
# ---------------------------------------------------------------------------


class TestCF128:
    def test_cf128_pubsub(self):
        """CF128 samples survive PUB→SUB roundtrip."""
        pub_ep = _next_port(_PORT)
        sub_ep = pub_ep.replace("127.0.0.1", "localhost")

        pub = Publisher(pub_ep, CF128)
        sub = Subscriber(sub_ep)

        time.sleep(0.1)

        # Note: np.clongdouble is platform-dependent (may be 128-bit or 64-bit)
        samples_out = np.array([1 + 2j, 3 + 4j], dtype=np.clongdouble)
        pub.send(samples_out, sample_rate=1_000_000, center_freq=0)

        samples_in, header = sub.recv(timeout_ms=1000)

        assert samples_in.dtype == np.clongdouble
        assert len(samples_in) == 2
        assert np.allclose(samples_out, samples_in)
        assert header["sample_type"] == CF128

        pub.close()
        sub.close()

    def test_cf128_pipeline(self):
        """CF128 samples flow through PUSH→PULL pipeline."""
        endpoint = _next_port(_PORT)

        push = Push(endpoint, CF128)
        pull = Pull(endpoint.replace("127.0.0.1", "localhost"))

        time.sleep(0.05)

        samples_out = np.array([10 + 20j], dtype=np.clongdouble)
        push.send(samples_out, sample_rate=1_000_000, center_freq=0)

        samples_in, header = pull.recv(timeout_ms=1000)

        assert samples_in.dtype == np.clongdouble
        assert len(samples_in) == 1
        assert np.allclose(samples_out, samples_in)
        assert header["sample_type"] == CF128

        push.close()
        pull.close()
