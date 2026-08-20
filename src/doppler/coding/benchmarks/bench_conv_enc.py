"""Benchmark for ConvEncoder.

Run: make bench-python PYTEST_SELECT="-k conv_enc"

The encoder is the cheap direction by more than an order of magnitude — the
C benchmark measures the pair — so what this one is for is the Python call
overhead a block-at-a-time caller actually pays.
"""

import numpy as np
import pytest

from doppler.coding import ConvEncoder

BLOCK_8K = 8_192
BLOCK_64K = 65_536

# CCSDS 131.0-B-3 section 3's inner code: G1 = 171, G2 = 133 octal.
CCSDS_POLY = np.array([0o171, 0o133], dtype=np.uint32)


@pytest.fixture
def obj():
    return ConvEncoder(CCSDS_POLY, k=7, invert=0x2)


@pytest.mark.parametrize("n", [BLOCK_8K, BLOCK_64K])
def test_bench_encode(benchmark, obj, n):
    """Rate: information bits in per second."""
    rng = np.random.default_rng(20260820)
    x = rng.integers(0, 2, n).astype(np.uint8)
    benchmark(obj.encode, x)
    if benchmark.stats:
        benchmark.extra_info["Mbit_s"] = n / benchmark.stats["mean"] / 1e6


@pytest.mark.parametrize("k", [3, 5, 7, 9])
def test_bench_constraint_length(benchmark, k):
    """The encoder is `n` parity computations per input bit and does NOT walk
    a trellis, so unlike the decoder this should be close to flat in k —
    measured because that asymmetry is the reason the two are separate
    objects."""
    poly = {
        3: [0o7, 0o5],
        5: [0o23, 0o35],
        7: [0o171, 0o133],
        9: [0o753, 0o561],
    }
    e = ConvEncoder(np.array(poly[k], dtype=np.uint32), k=k)
    rng = np.random.default_rng(k)
    x = rng.integers(0, 2, BLOCK_8K).astype(np.uint8)
    benchmark(e.encode, x)
    if benchmark.stats:
        benchmark.extra_info["Mbit_s"] = (
            BLOCK_8K / benchmark.stats["mean"] / 1e6
        )
