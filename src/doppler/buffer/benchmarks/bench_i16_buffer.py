"""Benchmark for I16Buffer: one frame through each of the ring's two surfaces.

``write`` + ``wait`` (the threaded surface) against ``write_some`` +
``peek`` (the non-blocking one), over the same N-sample frame so they
compare directly. The order FLIPS between the faces: in C the second pair
costs ~3% more, from Python it is the cheaper one, because ``wait``
releases and retakes the GIL around its spin and ``peek``, which returns
at once, has no reason to. ``oversize`` is the case only ``write_some``
can do at all: a chunk larger than the ring, fed by looping.

Run: make bench-python PYTEST_BENCH_DIRS=src/doppler/buffer/benchmarks
"""

import numpy as np
import pytest

from doppler.buffer import I16Buffer

N = 1024

# One q15 complex sample: the i16 ring speaks a record, not a bare int16.
IQ16 = np.dtype([("i", "<i2"), ("q", "<i2")])


@pytest.fixture
def obj():
    buf = I16Buffer(N * 16)
    yield buf
    buf.destroy()


def test_bench_write_wait(benchmark, obj):
    x = np.ones(N, dtype=IQ16)

    def run():
        obj.write(x)
        view = obj.wait(N)
        obj.consume()
        return view

    benchmark(run)
    if benchmark.stats:
        benchmark.extra_info["MSa_s"] = N / benchmark.stats["mean"] / 1e6


def test_bench_write_some_peek(benchmark, obj):
    x = np.ones(N, dtype=IQ16)

    def run():
        obj.write_some(x)
        view = obj.peek(N)
        obj.consume()
        return view

    assert run() is not None, "N fits an empty ring, so peek never says no"
    benchmark(run)
    if benchmark.stats:
        benchmark.extra_info["MSa_s"] = N / benchmark.stats["mean"] / 1e6


def test_bench_write_some_oversize(benchmark, obj):
    # 4x the ring: write() would refuse this whole, forever.
    total = 4 * obj.capacity
    x = np.ones(total, dtype=IQ16)

    def run():
        fed = 0
        while fed < total:
            k = obj.write_some(x[fed:])
            assert k, "the ring was just drained, so it takes something"
            fed += k
            obj.peek(obj.available)
            obj.consume()
        return fed

    assert run() == total
    benchmark(run)
    if benchmark.stats:
        benchmark.extra_info["MSa_s"] = total / benchmark.stats["mean"] / 1e6
