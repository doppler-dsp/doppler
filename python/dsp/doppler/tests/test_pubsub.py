"""
Integration tests for doppler streaming (C extension).

Patterns tested:
  - PUB/SUB    (broadcast with slow-joiner workaround)
  - CF64/CI32  (sample types)
  - Timeout handling
  - Context managers
"""

import time
import numpy as np
import pytest

from doppler import (
    Publisher,
    Subscriber,
    CF64,
    CI32,
)

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def _next_port(base: list[int]) -> str:
    """Return next unique loopback endpoint (avoids port collisions between tests)."""
    base[0] += 1
    return f"tcp://127.0.0.1:{base[0]}"


_PORT = [15100]  # mutable base; incremented per test


# ---------------------------------------------------------------------------
# PUB / SUB — with slow-joiner workaround
# ---------------------------------------------------------------------------


class TestPubSub:
    def test_cf64_roundtrip(self):
        """CF64 samples survive a PUB→SUB round trip exactly."""
        pub_ep = _next_port(_PORT)
        sub_ep = pub_ep.replace("127.0.0.1", "localhost")

        pub = Publisher(pub_ep, CF64)
        sub = Subscriber(sub_ep)

        # Slow-joiner workaround: wait for SUB to connect
        time.sleep(0.1)

        samples_out = np.array([1 + 2j, 3 + 4j, 5 + 6j], dtype=np.complex128)
        pub.send(samples_out, sample_rate=1_000_000, center_freq=2_400_000_000)

        samples_in, header = sub.recv(timeout_ms=1000)

        assert samples_in.dtype == np.complex128
        assert len(samples_in) == 3
        assert np.allclose(samples_out, samples_in)

        # Header validation
        assert header["sequence"] == 0
        assert header["sample_rate"] == 1_000_000
        assert header["center_freq"] == 2_400_000_000
        assert header["num_samples"] == 3
        assert header["sample_type"] == CF64

        pub.close()
        sub.close()

    def test_ci32_roundtrip(self):
        """CI32 (int32 I/Q) samples survive a PUB→SUB round trip."""
        pub_ep = _next_port(_PORT)
        sub_ep = pub_ep.replace("127.0.0.1", "localhost")

        pub = Publisher(pub_ep, CI32)
        sub = Subscriber(sub_ep)

        time.sleep(0.1)

        # CI32: int32 array with interleaved I/Q
        samples_out = np.array([1000, 2000, 3000, 4000], dtype=np.int32)
        pub.send(samples_out, sample_rate=1_000_000, center_freq=0)

        samples_in, header = sub.recv(timeout_ms=1000)

        assert samples_in.dtype == np.int32
        assert len(samples_in) == 4
        assert np.array_equal(samples_out, samples_in)
        assert header["num_samples"] == 2  # 2 complex samples
        assert header["sample_type"] == CI32

        pub.close()
        sub.close()

    def test_multiple_frames(self):
        """Multiple frames received in order."""
        pub_ep = _next_port(_PORT)
        sub_ep = pub_ep.replace("127.0.0.1", "localhost")

        pub = Publisher(pub_ep, CF64)
        sub = Subscriber(sub_ep)

        time.sleep(0.1)

        for i in range(3):
            samples = np.array([i + 0j], dtype=np.complex128)
            pub.send(samples, sample_rate=1_000_000, center_freq=0)

        for i in range(3):
            data, hdr = sub.recv(timeout_ms=1000)
            assert data[0] == i + 0j
            assert hdr["sequence"] == i

        pub.close()
        sub.close()

    def test_timeout_raises(self):
        """recv() raises TimeoutError on timeout."""
        pub_ep = _next_port(_PORT)
        sub_ep = pub_ep.replace("127.0.0.1", "localhost")

        pub = Publisher(pub_ep, CF64)
        sub = Subscriber(sub_ep)

        time.sleep(0.1)

        # Don't send anything
        with pytest.raises(TimeoutError):
            sub.recv(timeout_ms=100)

        pub.close()
        sub.close()

    def test_context_manager(self):
        """Sockets work as context managers."""
        pub_ep = _next_port(_PORT)
        sub_ep = pub_ep.replace("127.0.0.1", "localhost")

        with Publisher(pub_ep, CF64) as pub:
            with Subscriber(sub_ep) as sub:
                time.sleep(0.1)
                samples = np.array([1 + 2j], dtype=np.complex128)
                pub.send(samples, sample_rate=1_000_000, center_freq=0)
                data, hdr = sub.recv(timeout_ms=1000)
                assert len(data) == 1
                assert data[0] == 1 + 2j

        # Should not raise (already closed)
