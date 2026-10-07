"""Benchmark for DopplerChannel.

Two rows, because there are two forms: ``execute`` applies the closed-form
``(doppler_ppm, doppler_rate_ppm_s)`` line, and ``execute_profile`` applies a
Doppler array, one ppm value per input sample. The profile is flat at the
scalar row's Doppler, so the Doppler is the same and only the cost of the
*form* differs: validating the array, the position the resampler reports per
output, and a carrier read from it rather than from a closed form.

Run: pytest src/doppler/impairment/benchmarks/bench_doppler_channel.py
     --benchmark-only

The C side (``native/benchmarks/bench_doppler_channel_core.c``) is the number
to quote: this one includes the binding's array setup.

Each row asserts it produced a full block, because a refused or short call
returns an empty array and benchmarks very fast. The channel is not reset
between rounds: state accumulates, which is fine for throughput.
"""

import numpy as np
import pytest

from doppler.impairment import DopplerChannel

BLOCK_64K = 65_536
FS = 10.0e6
CARRIER = 2.2e9
PPM = 3.0


@pytest.fixture
def obj():
    return DopplerChannel(fs=FS, carrier_hz=CARRIER, doppler_ppm=PPM)


@pytest.fixture
def profiled():
    # No Doppler of its own: a profile is absolute, so the array is all of it.
    return DopplerChannel(fs=FS, carrier_hz=CARRIER)


def test_bench_execute_static_64k(benchmark, obj):
    x = np.ones(BLOCK_64K, dtype=np.complex64)
    y = benchmark(obj.execute, x)
    assert len(y) >= BLOCK_64K - 2, "a short path would time as a fast one"
    if benchmark.stats:
        benchmark.extra_info["MSa_s"] = (
            BLOCK_64K / benchmark.stats["mean"] / 1e6
        )


def test_bench_execute_profile_64k(benchmark, profiled):
    x = np.ones(BLOCK_64K, dtype=np.complex64)
    ppm = np.full(BLOCK_64K, PPM)
    y = benchmark(profiled.execute_profile, x, ppm)
    assert len(y) >= BLOCK_64K - 2, "a refused call would time as a fast one"
    if benchmark.stats:
        benchmark.extra_info["MSa_s"] = (
            BLOCK_64K / benchmark.stats["mean"] / 1e6
        )
