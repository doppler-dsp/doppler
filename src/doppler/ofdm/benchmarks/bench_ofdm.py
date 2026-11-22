# Hand-owned: the modulate rows are not ones jm generates.
"""Benchmark for Ofdm.

Run: pytest src/doppler/ofdm/benchmarks/bench_ofdm.py --benchmark-only

``test_bench_create`` times building the object. ``test_bench_modulate``
renders a whole layout in one call, at the transform lengths and window of the
C rows (``native/benchmarks/ofdm``): the same generic layout (four pilots
under a two-entry polarity sequence, 7/8 occupancy, a quarter-length cyclic
prefix) and about 64k samples a call, so the Python row against the C row is
the cost of the binding.
"""

import numpy as np
import pytest

from doppler.ofdm import Ofdm

SAMPLES = 65_536


def layout(n: int, window: int):
    """The generic layout of the C rows: n_fft ``n``, a ``n/4`` prefix."""
    used = np.zeros(n, np.uint8)  # index k + n/2
    used[n // 16 : n - n // 16] = 1
    used[n // 2] = 0  # DC is null
    o = Ofdm()
    cls = o.add_class(n, used, cyclic_prefix=n // 4, window=window)
    o.add_polarity(cls, np.array([1, -1], np.complex64))
    for k in (-(n // 4), -(n // 8), n // 8, n // 4):
        o.add_fixed(cls, 0xFFFFFFFF, k, 1.0)
    o.add_run(cls, SAMPLES // (n + n // 4))
    return o


def test_bench_create(benchmark):
    benchmark(lambda: Ofdm())


@pytest.mark.parametrize(
    "n, window", [(64, 0), (64, 2), (256, 0), (1024, 0), (4096, 0)]
)
def test_bench_modulate(benchmark, n, window):
    o = layout(n, window)
    points = np.full(o.points_remaining(), 0.7 + 0.7j, np.complex64)
    out = np.empty(o.modulate_max_out(points.size), np.complex64)

    def run():
        o.reset()
        return o.modulate(points, out)

    assert run().size == out.size
    benchmark(run)
