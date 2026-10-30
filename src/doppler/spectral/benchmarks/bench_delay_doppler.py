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


# ── Batches (#2683) ───────────────────────────────────────────────────────
#
# The request's geometry: a 64 x 512 map, H = 48 frames, one thread. The
# per-map work is the same as 48 execute() calls; what a batch saves is the
# per-call Python and C overhead. #2683 measured the loop at about 186 us a
# map (8.91 ms for 48). Each case reuses its output, as a sweep would.

_BNY, _BNX, _BH = 64, 512, 48


def _batch_setup() -> tuple[DelayDoppler, np.ndarray]:
    rng = np.random.default_rng(0)
    code = np.exp(2j * np.pi * rng.random(_BNX)).astype(np.complex64)
    n = _BH * _BNY * _BNX
    x = (rng.standard_normal(n) + 1j * rng.standard_normal(n)).astype(
        np.complex64
    )
    return DelayDoppler(code, ny=_BNY, nthreads=1), x


def _per_frame(benchmark: Any) -> None:
    if benchmark.stats:
        benchmark.extra_info["frames"] = _BH
        benchmark.extra_info["us_per_frame"] = (
            benchmark.stats["mean"] / _BH * 1e6
        )


def test_bench_batch_64x512_h48_execute_loop_1t(benchmark: Any) -> None:
    """The baseline: 48 execute() calls, one map each."""
    dd, x = _batch_setup()
    n = _BNY * _BNX
    out = np.empty(dd.execute_max_out(), np.complex64)

    def loop() -> None:
        for h in range(_BH):
            dd.execute(x[h * n : (h + 1) * n], out=out)

    benchmark(loop)
    _per_frame(benchmark)


def test_bench_batch_64x512_h48_all_1t(benchmark: Any) -> None:
    """Every map in one call, into a reused (48, n_out) out=."""
    dd, x = _batch_setup()
    out = np.empty((_BH, dd.n_out), np.complex64)
    benchmark(dd.execute_all, x, out=out)
    _per_frame(benchmark)


def test_bench_batch_64x512_h48_power_max_1t(benchmark: Any) -> None:
    """The best cell over 48 maps and the map it came from."""
    dd, x = _batch_setup()
    benchmark(dd.execute_power_max, x)
    _per_frame(benchmark)
