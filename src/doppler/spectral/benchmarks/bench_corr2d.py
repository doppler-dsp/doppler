"""Benchmark for Corr2D — the FFT-based 2-D correlator.

``Corr2D.execute`` processes exactly one ``(ny, nx)`` frame per call (FFT2 →
multiply by the conjugate reference spectrum → IFFT2 → accumulate, dumping
every ``dwell`` frames). It is the dominant cost inside an acquisition tile
(``Acquirer``): at the worked grid below it is ~95-100% of the whole tile, so
the engine is 2-D-FFT bound.

These benchmark one frame at ``dwell=1`` (dump every call) at the
acquisition-relevant grids and record per-frame sample throughput.

Run: pytest src/doppler/spectral/benchmarks/bench_corr2d.py --benchmark-only
"""

import numpy as np

from doppler.spectral import Corr2D


def _frame(ny, nx, benchmark):
    rng = np.random.default_rng(0)
    ref = (
        rng.standard_normal((ny, nx)) + 1j * rng.standard_normal((ny, nx))
    ).astype(np.complex64)
    x = (
        rng.standard_normal((ny, nx)) + 1j * rng.standard_normal((ny, nx))
    ).astype(np.complex64)
    c = Corr2D(ref, 1)  # dwell=1 → one execute == one correlate-and-dump
    benchmark(c.execute, x)
    if benchmark.stats:
        benchmark.extra_info["N"] = ny * nx
        benchmark.extra_info["MSa_s"] = ny * nx / benchmark.stats["mean"] / 1e6


def test_bench_corr2d_test_grid(benchmark):
    """Test-suite grid: ny=16, nx=124 → N=1984."""
    _frame(16, 124, benchmark)


def test_bench_corr2d_worked(benchmark):
    """Worked-case grid: ny=10, nx=2046 (GPS-class length-1023 code, spc=2).

    Measured (12-core x86, this machine): ~0.6 ms/frame, ~30 MSa/s per core —
    essentially the entire Acquirer tile (the slow-time FFT + ring + CFAR add
    only a few percent).
    """
    _frame(10, 2046, benchmark)
