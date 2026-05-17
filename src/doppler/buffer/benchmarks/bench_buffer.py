"""Benchmark for buffer write throughput.

Run: pytest src/doppler/buffer/benchmarks/bench_buffer.py --benchmark-only
"""

import numpy as np
import pytest

from doppler.buffer import F32Buffer, F64Buffer

N = 1024


@pytest.fixture
def f32buf():
    buf = F32Buffer(N * 16)
    yield buf
    buf.destroy()


@pytest.fixture
def f64buf():
    buf = F64Buffer(N * 16)
    yield buf
    buf.destroy()


def test_bench_f32_write(benchmark, f32buf):
    x = np.ones(N, dtype=np.complex64)

    def run():
        f32buf.write(x)
        view = f32buf.wait(N)
        f32buf.consume()
        return view

    benchmark(run)
    if benchmark.stats:
        benchmark.extra_info["MSa_s"] = N / benchmark.stats["mean"] / 1e6


def test_bench_f64_write(benchmark, f64buf):
    x = np.ones(N, dtype=np.complex128)

    def run():
        f64buf.write(x)
        view = f64buf.wait(N)
        f64buf.consume()
        return view

    benchmark(run)
    if benchmark.stats:
        benchmark.extra_info["MSa_s"] = N / benchmark.stats["mean"] / 1e6
