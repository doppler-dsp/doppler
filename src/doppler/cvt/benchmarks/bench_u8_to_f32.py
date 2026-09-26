"""Benchmark for U8ToF32, both mappings.

``shift`` is the documented fast path and ``midpoint`` the opt-in unbiased
one, so both are measured: the gap between them is the cost of the choice.
The mode is a test parameter rather than a parametrized fixture so the
fixture gate (``test_benchmark_fixtures``), which builds fixtures outside
pytest and has no ``request`` to hand one, has nothing it cannot construct.

Run: pytest src/doppler/cvt/benchmarks/bench_u8_to_f32.py --benchmark-only
"""

import numpy as np
import pytest

from doppler.cvt import U8ToF32

BLOCK_64K = 65_536
MODES = ["shift", "midpoint"]


@pytest.mark.parametrize("mode", MODES)
def test_bench_step(benchmark, mode):
    benchmark(U8ToF32(mode=mode).step, 1)


@pytest.mark.parametrize("mode", MODES)
def test_bench_steps_64k(benchmark, mode):
    x = np.ones(BLOCK_64K, dtype=np.uint8)
    benchmark(U8ToF32(mode=mode).steps, x)
