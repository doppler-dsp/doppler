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


# The hypothesis bank (#2683 phase 3): one frame through 12 carriers (three
# whole bins x L = 4) against the 12 hand-corrected copies a caller would
# otherwise build and batch, and ruling 9's coarse search: L = 2 alone, and
# bank_detect's L = 2 search refined to L = 4. The same rows as
# bench_corr2d_core.c's bench_bank, so the gap is the binding's overhead.
NO_RATES = np.empty(0, np.complex64)


def _bank_setup() -> tuple[DelayDoppler, np.ndarray, np.ndarray]:
    rng = np.random.default_rng(2683)
    ny, nx = 64, 512
    code = rng.choice(np.array([-1.0, 1.0]), nx).astype(np.complex64)
    m = np.arange(ny * nx)
    x = (
        0.1
        * np.tile(np.roll(code, 77), ny)
        * np.exp(2j * np.pi * 0.25 / nx * m)
        + rng.standard_normal(ny * nx)
        + 1j * rng.standard_normal(ny * nx)
    ).astype(np.complex64)
    n = np.arange(nx)
    copies = np.concatenate(
        [
            x * np.tile(np.exp(-2j * np.pi * g * n / (4 * nx)), ny)
            for g in range(-4, 8)
        ]
    ).astype(np.complex64)
    return DelayDoppler(code, ny=ny, nthreads=1), x, copies


def test_bench_bank_64x512_c12_corrected_batch_1t(benchmark: Any) -> None:
    """12 hand-corrected copies through execute_power_max."""
    dd, _, copies = _bank_setup()
    benchmark(dd.execute_power_max, copies)


def test_bench_bank_64x512_c12_l4_1t(benchmark: Any) -> None:
    """The same 12 carriers as one bank: the forward FFTs shared."""
    dd, x, _ = _bank_setup()
    pw, _h = benchmark(dd.bank_power_max, x, 4, -4, 7, NO_RATES)
    assert (
        int(np.argmax(pw)) % 512 == 77
    )  # the code's lag, or it searched nothing


def test_bench_bank_64x512_c6_l2_1t(benchmark: Any) -> None:
    """Ruling 9's search grid: half the carriers."""
    dd, x, _ = _bank_setup()
    benchmark(dd.bank_power_max, x, 2, -2, 3, NO_RATES)


def test_bench_bank_64x512_detect_l2_l4_1t(benchmark: Any) -> None:
    """bank_detect: the L = 2 search, its one candidate refined to L = 4."""
    dd, x, _ = _bank_setup()
    pw, _ = dd.bank_power_max(x, 2, -2, 3, NO_RATES)
    t = 0.5 * float(pw.max())
    p, g, *_ = benchmark(dd.bank_detect, x, 2, 4, -2, 3, t, t, NO_RATES)
    assert (
        len(p) == 1 and int(g[0]) == 1
    )  # a quarter bin, on the 1/(4 nx) grid
