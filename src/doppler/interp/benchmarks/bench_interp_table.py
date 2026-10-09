"""Benchmark for InterpolatedTable — table lookup, per method.

Run: pytest src/doppler/interp/benchmarks/bench_interp_table.py
     --benchmark-only

The same shape as the C benchmark
(``native/benchmarks/bench_interp_table_core.c``): a 1024-entry complex
table, 64k lookups per call, one row per method. The gap between the two
faces is the binding's per-call overhead.

The lookup points are spread across the table rather than a ramp: a
monotone sweep keeps every lookup in the same cache line and measures the
L1 hit, not the interpolation. They are also fractional (``+ 0.37``), so
``nearest`` and ``linear`` do the work that distinguishes them from
``floor`` instead of all three landing on an index.

``execute`` writes into a caller-owned ``out=`` buffer, as the C row does,
so the row times the kernel and not a 1 MiB allocation per call.
"""

import numpy as np
import pytest

from doppler.interp import InterpolatedTable

BLOCK_64K = 65_536
TABLE_N = 1_024


@pytest.fixture(scope="module")
def table():
    i = np.arange(TABLE_N)
    return (np.cos(i * 0.01) + 1j * np.sin(i * 0.017)).astype(np.complex128)


@pytest.fixture(scope="module")
def points():
    rng = np.random.default_rng(0x4D2B)
    return rng.integers(0, TABLE_N - 4, BLOCK_64K).astype(np.float64) + 0.37


def _expect(method, table, x):
    """The value each method must return at an in-range fractional point.

    An oracle for the assertion, not for the timed path: the bench times
    the library and only checks its answer here.
    """
    i = np.floor(x).astype(np.int64)
    frac = x - i
    if method == "floor":
        return table[i]
    if method == "nearest":
        return table[np.where(frac > 0.5, i + 1, i)]
    return table[i] * (1.0 - frac) + table[i + 1] * frac


@pytest.mark.parametrize("method", ["floor", "nearest", "linear"])
def test_bench_execute(benchmark, table, points, method):
    """64k spread, fractional lookups into a 1024-entry table."""
    t = InterpolatedTable(table, method)
    out = np.empty(BLOCK_64K, dtype=np.complex128)

    got = benchmark(t.execute, points, out=out)

    assert got.shape == (BLOCK_64K,), (
        f"{method}: {got.shape} from {BLOCK_64K} points — a short return "
        "times less work than the row is credited with"
    )
    np.testing.assert_allclose(got, _expect(method, table, points), atol=1e-12)

    if benchmark.stats:
        # MIN, not mean: the least-disturbed observation, per
        # docs/dev/contributing/benchmarking.md.
        sec = benchmark.stats["min"]
        benchmark.extra_info["MSa_s"] = BLOCK_64K / sec / 1e6
