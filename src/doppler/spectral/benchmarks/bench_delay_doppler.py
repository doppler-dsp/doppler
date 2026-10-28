"""Benchmark for DelayDoppler — the delay-Doppler map of a raw stream.

One ``execute`` takes ``ny`` code periods of ``nx`` samples and, at dwell 1,
dumps the centered ``ny x nx`` map: ``ny`` length-``nx`` row correlations
(Corr2D's single-row fast path) then ``nx`` length-``ny`` slow-time FFTs.
Every case passes a preallocated ``out=``, the configuration a streaming
caller runs, so the timing is the kernel and not a fresh map's allocation.

(256, 4096) is #2666's measured case: Corr2D alone took 5.0 ms there, and
Corr2D plus a numpy slow-time FFT 27.8 ms, on one thread.

Run: make bench
"""

from typing import Any

import numpy as np

from doppler.spectral import DelayDoppler


def _map(ny: int, nx: int, nthreads: int, benchmark: Any) -> None:
    rng = np.random.default_rng(0)
    code = np.exp(2j * np.pi * rng.random(nx)).astype(np.complex64)
    x = (
        rng.standard_normal(ny * nx) + 1j * rng.standard_normal(ny * nx)
    ).astype(np.complex64)
    dd = DelayDoppler(code, ny=ny, nthreads=nthreads)
    out = np.empty(dd.execute_max_out(), np.complex64)
    benchmark(dd.execute, x, out=out)
    if benchmark.stats:
        benchmark.extra_info["N"] = ny * nx
        benchmark.extra_info["MSa_s"] = ny * nx / benchmark.stats["mean"] / 1e6


def test_bench_delay_doppler_256x4096_1t(benchmark: Any) -> None:
    """#2666's case on one thread: Corr2D's 5 ms plus the column FFTs."""
    _map(256, 4096, 1, benchmark)


def test_bench_delay_doppler_256x4096_all(benchmark: Any) -> None:
    """The same map on every online core (nthreads=0)."""
    _map(256, 4096, 0, benchmark)


def test_bench_delay_doppler_32x1024_1t(benchmark: Any) -> None:
    """A small map, where a thread hand-off would cost more than it buys."""
    _map(32, 1024, 1, benchmark)
