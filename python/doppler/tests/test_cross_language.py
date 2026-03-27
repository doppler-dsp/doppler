"""
Cross-language integration test — C transmitter → Python subscriber.

Verifies the core doppler promise: a C publisher and a Python
subscriber can exchange signal frames over the shared wire format.

The test spawns the `xmit_cf64` C binary (built by cmake into
build/c/), which sends one CF64 frame of known samples.  Python
subscribes, receives, and validates every field.

Skip conditions:
  - `xmit_cf64` binary not found (library not built yet)
  - Running on a platform where subprocess spawning is unavailable
"""

from __future__ import annotations

import os
import subprocess
import time
from pathlib import Path

import numpy as np
import pytest

from doppler import CF64, Subscriber

# ---------------------------------------------------------------------------
# Locate the C binary
# ---------------------------------------------------------------------------

_REPO_ROOT = Path(__file__).parents[3]  # doppler/
_BINARY = _REPO_ROOT / "build" / "c" / "xmit_cf64"


def _binary_available() -> bool:
    return _BINARY.is_file() and os.access(_BINARY, os.X_OK)


# Known payload — must match xmit_cf64.c exactly
_EXPECTED_SAMPLES = np.array(
    [1 + 2j, 3 + 4j, 5 + 6j, 7 + 8j], dtype=np.complex128
)
_EXPECTED_SAMPLE_RATE = 2_048_000
_EXPECTED_CENTER_FREQ = 1_420_405_752   # hydrogen line


# ---------------------------------------------------------------------------
# Port helper
# ---------------------------------------------------------------------------

_PORT = [16400]


def _next_port() -> int:
    _PORT[0] += 1
    return _PORT[0]


# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------


@pytest.mark.skipif(
    not _binary_available(),
    reason=f"xmit_cf64 not built ({_BINARY}); run `make build` first",
)
class TestCToPython:
    def test_cf64_wire_format(self):
        """C PUB → Python SUB: samples and all header fields match."""
        port = _next_port()
        endpoint = f"tcp://127.0.0.1:{port}"

        sub = Subscriber(endpoint)

        # Start the C transmitter — it sleeps 150 ms before sending
        # so the subscriber has time to connect.
        proc = subprocess.Popen(
            [str(_BINARY), str(port)],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.PIPE,
        )

        try:
            samples, header = sub.recv(timeout_ms=3000)
        finally:
            proc.wait(timeout=5)
            sub.close()

        stderr = proc.stderr.read().decode()
        assert proc.returncode == 0, f"xmit_cf64 exited {proc.returncode}: {stderr}"

        # ── Sample data ──────────────────────────────────────────────────
        assert samples.dtype == np.complex128
        assert len(samples) == len(_EXPECTED_SAMPLES)
        assert np.allclose(samples, _EXPECTED_SAMPLES), (
            f"Sample mismatch: got {samples}, expected {_EXPECTED_SAMPLES}"
        )

        # ── Header fields ────────────────────────────────────────────────
        assert header["sample_type"] == CF64
        assert header["num_samples"] == len(_EXPECTED_SAMPLES)
        assert header["sample_rate"] == _EXPECTED_SAMPLE_RATE, (
            f"sample_rate: got {header['sample_rate']}, "
            f"expected {_EXPECTED_SAMPLE_RATE}"
        )
        assert header["center_freq"] == _EXPECTED_CENTER_FREQ, (
            f"center_freq: got {header['center_freq']}, "
            f"expected {_EXPECTED_CENTER_FREQ}"
        )
        assert header["sequence"] >= 0

    def test_timestamp_is_nonzero(self):
        """C transmitter populates a non-zero nanosecond timestamp."""
        port = _next_port()
        endpoint = f"tcp://127.0.0.1:{port}"

        sub = Subscriber(endpoint)
        proc = subprocess.Popen(
            [str(_BINARY), str(port)],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )

        try:
            _, header = sub.recv(timeout_ms=3000)
        finally:
            proc.wait(timeout=5)
            sub.close()

        assert header["timestamp_ns"] > 0, (
            "Expected a nonzero timestamp from the C transmitter"
        )
