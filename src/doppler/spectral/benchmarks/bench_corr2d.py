"""Benchmark for Corr2D — the FFT-based 2-D correlator.

``Corr2D.execute`` processes exactly one ``(ny, nx)`` frame per call (FFT2 →
multiply by the conjugate reference spectrum → IFFT2 → accumulate, dumping
every ``dwell`` frames). It is the dominant cost inside an acquisition tile
(``Acquisition``): ~95-100% of the whole tile, so the engine is 2-D-FFT bound.

The 2-D FFT runs on **pffft** (pre-allocated SIMD work buffers, no per-call
allocation) **only when both axes are a multiple of 16 and 5-smooth (2/3/5),
N≥16**; otherwise it falls back to vendored pocketfft, which mallocs/frees a
work buffer per 1-D transform and is slow on large prime factors. Real DSSS
code lengths are 2**n-1 (1023 = 3·11·31), so a naive grid hits the fallback —
the two benchmarks below show the ~5x gap.

Run: pytest src/doppler/spectral/benchmarks/bench_corr2d.py --benchmark-only
"""

import numpy as np

from doppler.spectral import Corr2D


def _frame(ny, nx, benchmark, dwell=1):
    rng = np.random.default_rng(0)
    ref = (
        rng.standard_normal((ny, nx)) + 1j * rng.standard_normal((ny, nx))
    ).astype(np.complex64)
    x = (
        rng.standard_normal((ny, nx)) + 1j * rng.standard_normal((ny, nx))
    ).astype(np.complex64)
    c = Corr2D(ref, dwell)
    for _ in range(dwell):  # warm + align the dwell cycle
        c.execute(x)
    benchmark(c.execute, x)
    if benchmark.stats:
        benchmark.extra_info["N"] = ny * nx
        benchmark.extra_info["MSa_s"] = ny * nx / benchmark.stats["mean"] / 1e6


def test_bench_corr2d_fallback(benchmark):
    """GPS-class grid ny=10, nx=2046 (length-1023 code, spc=2) → pocketfft
    fallback (both axes prime/odd). Measured ~21 MSa/s (mallocs per transform).
    """
    _frame(10, 2046, benchmark)


def test_bench_corr2d_pffft(benchmark):
    """pffft-friendly grid ny=16, nx=2000 (=2**4·5**3) → pre-allocated, SIMD,
    malloc-free path. Measured ~100 MSa/s — ~5x the fallback. Achievable only
    when the code length is 2/3/5-smooth (e.g. L=1000); 2**n-1 codes need the
    sub-block decomposition to reach a pffft-friendly FFT size.
    """
    _frame(16, 2000, benchmark)


def test_bench_corr2d_pffft_dwell8(benchmark):
    """Same grid at dwell=8. Frequency-domain accumulation defers the inverse
    to the dump, so the per-execute cost amortizes toward forward-only:
    ~1.6-1.9x faster per frame than dwell=1 (the inverse runs 1/8 as often).
    """
    _frame(16, 2000, benchmark, dwell=8)
