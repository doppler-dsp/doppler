"""Benchmark for power_to_db_f32 — the library's one dB conversion (#2094).

Run: pytest src/doppler/spectral/benchmarks/bench_power_to_db.py \
--benchmark-only

The same conversion as the C row ``power_to_db_f32[4096]`` in
``native/benchmarks/spectral/bench_spectral_core.c``, at three lengths, beside
numpy's ``10 * log10`` over the same float32 power. PSD and the Spectrogram
read every dB through this function, so its row is the dB share of a dB
spectrum; numpy's row is what a caller converting power rows by hand would
otherwise pay. The gap between this row and the C one is the binding's
per-call overhead, which shrinks with the length.

Each row checks its own output against double ``10 * log10`` to the C
tests' pinned bound (5e-4 dB), so a conversion that got faster by getting
wrong fails here rather than reading as a win.
"""

import numpy as np
import pytest

from doppler.spectral import power_to_db_f32

#: The bound ``test_spectral_core.c`` pins (``DP_P2DB_PIN_DB``); the
#: header's contract is the looser 0.01 dB.
PIN_DB = 5e-4
LENGTHS = [1_024, 16_384, 262_144]


def _power(n):
    """Noise power around 1e-3 with a few strong bins: a spectrum's spread."""
    rng = np.random.default_rng(2094)
    p = rng.exponential(1e-3, n).astype(np.float32)
    p[:: max(n // 4, 1)] = 1.0
    return p


def _check(db, p, label):
    want = 10.0 * np.log10(p.astype(np.float64))
    worst = float(np.max(np.abs(db.astype(np.float64) - want)))
    assert worst <= PIN_DB, f"{label}: {worst:.2e} dB off 10*log10"


def _record(benchmark, n):
    if benchmark.stats:
        # MIN, not mean: the least-disturbed observation, per
        # docs/dev/contributing/benchmarking.md.
        benchmark.extra_info["ns_per_bin"] = benchmark.stats["min"] / n * 1e9


@pytest.mark.parametrize("n", LENGTHS)
def test_bench_power_to_db_f32(benchmark, n):
    """The fast conversion, float32 power in, float32 dB out."""
    p = _power(n)
    db = benchmark(power_to_db_f32, p)
    _check(db, p, f"power_to_db_f32[{n}]")
    _record(benchmark, n)


@pytest.mark.parametrize("n", LENGTHS)
def test_bench_numpy_log10(benchmark, n):
    """numpy's float32 10*log10 over the same power: the by-hand cost."""
    p = _power(n)
    db = benchmark(lambda: np.float32(10.0) * np.log10(p))
    _check(db, p, f"numpy_log10[{n}]")
    _record(benchmark, n)
