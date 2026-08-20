"""Benchmark for Viterbi.

Run: make bench-python PYTEST_SELECT="-k viterbi"

Two axes, because the decoder's cost has two terms that scale differently:
the butterfly is 2**(k-1) states per input bit, and the traceback walks
`depth` survivors per decision. The C benchmark
(``native/benchmarks/bench_viterbi_core.c``) sweeps both; this one measures
what a Python caller actually pays, block call overhead included.
"""

import numpy as np
import pytest

from doppler.coding import Viterbi

BLOCK_8K = 8_192
BLOCK_64K = 65_536

# CCSDS 131.0-B-3 section 3's inner code: G1 = 171, G2 = 133 octal.
CCSDS_POLY = np.array([0o171, 0o133], dtype=np.uint32)


@pytest.fixture
def obj():
    return Viterbi(CCSDS_POLY, k=7, depth=35)


def _llr(n, seed):
    """Noisy soft symbols, so the survivors genuinely compete: an all-agreeing
    stream lets the path metrics stay ordered and is not the workload."""
    rng = np.random.default_rng(seed)
    return (
        rng.integers(0, 2, n) * -2.0 + 1.0 + 0.7 * rng.standard_normal(n)
    ).astype(np.float32)


@pytest.mark.parametrize("n", [BLOCK_8K, BLOCK_64K])
def test_bench_decode(benchmark, obj, n):
    """Rate: channel symbols in per second — the number a link budget cares
    about, since it is what the demodulator upstream must not outrun."""
    x = _llr(n, seed=20260820)
    benchmark(obj.decode, x)
    if benchmark.stats:
        benchmark.extra_info["Msym_s"] = n / benchmark.stats["mean"] / 1e6


@pytest.mark.parametrize("k", [3, 5, 7, 9])
def test_bench_constraint_length(benchmark, k):
    """2**(k-1) states, so this doubles per step of k. Measured because the
    choice of code is the caller's, and this is what it costs them."""
    poly = {
        3: [0o7, 0o5],
        5: [0o23, 0o35],
        7: [0o171, 0o133],
        9: [0o753, 0o561],
    }
    v = Viterbi(np.array(poly[k], dtype=np.uint32), k=k, depth=5 * k + 25)
    x = _llr(BLOCK_8K, seed=k)
    benchmark(v.decode, x)
    if benchmark.stats:
        benchmark.extra_info["Msym_s"] = (
            BLOCK_8K / benchmark.stats["mean"] / 1e6
        )
