"""Benchmark for DopplerChannel.

Run: pytest src/doppler/impairment/benchmarks/bench_doppler_channel.py
     --benchmark-only

The question these answer is not "how fast is the impairment" in the
abstract — it is whether a harness can afford to apply it. Two rows, because
there are two ways to specify the Doppler and a caller choosing between them
deserves the cost of each:

- ``execute`` drives the create-time ``(doppler_ppm, doppler_rate_ppm_s)``
  closed form, a straight line;
- ``execute_profile`` drives a per-sample array, which is what a real pass
  needs and a line cannot express.

The C benchmark measures the same pair and finds the array form marginally
*cheaper* (0.96x): both divide once per sample to turn a Doppler into a rate
and both feed the same resampler, and what the closed form adds is deriving
``t`` per sample on both clocks. So a measured LEO profile is not a stimulus
to ration.
"""

import numpy as np
import pytest

from doppler.impairment import DopplerChannel

BLOCK_64K = 65_536
FS = 10.0e6
FC = 2.2e9


@pytest.fixture
def obj():
    return DopplerChannel(FS, FC, 3.0, 0.0)


@pytest.fixture
def plain():
    """No create-time Doppler: the profile supplies all of it."""
    return DopplerChannel(FS, FC, 0.0, 0.0)


def test_bench_execute_64k(benchmark, obj):
    x = np.ones(BLOCK_64K, dtype=np.complex64)
    benchmark(obj.execute, x)
    if benchmark.stats:
        benchmark.extra_info["MSa_s"] = (
            BLOCK_64K / benchmark.stats["mean"] / 1e6
        )


def test_bench_execute_profile_64k(benchmark, plain):
    x = np.ones(BLOCK_64K, dtype=np.complex64)
    # A curved pass, not a constant: the shape the array form exists for,
    # so the row measures the real use rather than a degenerate one.
    ppm = 3.0 * np.cos(np.pi * np.arange(BLOCK_64K) / BLOCK_64K)
    benchmark(plain.execute_profile, x, ppm)
    if benchmark.stats:
        benchmark.extra_info["MSa_s"] = (
            BLOCK_64K / benchmark.stats["mean"] / 1e6
        )
