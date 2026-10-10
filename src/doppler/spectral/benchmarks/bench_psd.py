"""Benchmark for PSD — Welch accumulation, and the read-out that is not free.

Run: pytest src/doppler/spectral/benchmarks/bench_psd.py --benchmark-only

The same rows as the C benchmark (``native/benchmarks/bench_psd_core.c``):
for each of three FFT sizes, ``accumulate`` (cf32) and ``accumulate_real``
(f32) over a 64k block, and the ``power_onesided`` read-out. Accumulation
and read-out are separate rows because they do not run equally often: a
display folds every block and reads the trace once per refresh. The gap
between the two faces is the binding's per-call overhead.

The stimulus is one tone exactly on bin ``nfft/64`` at every size, so each
row can assert its spectrum peaks where the tone is — a fold that stopped
transforming, or stopped folding, fails rather than getting faster.
"""

import numpy as np
import pytest

from doppler.spectral import PSD

BLOCK_64K = 65_536
FS = 1.0e6
#: The tone, in cycles/sample: 1/64 lands on bin nfft/64 for every nfft
#: below, so no size sees leakage the others do not.
TONE = 1.0 / 64.0
NFFTS = [1_024, 4_096, 16_384]


@pytest.fixture(scope="module")
def tone():
    ph = 2.0 * np.pi * TONE * np.arange(BLOCK_64K)
    return np.exp(1j * ph).astype(np.complex64), np.cos(ph).astype(np.float32)


def _psd(nfft):
    return PSD(n=nfft, fs=FS, window="hann", mode="mean", alpha=0.1)


def _check_peak(p, nfft, label):
    spec = p.power_onesided(p.power_onesided_max_out())
    assert spec.shape == (nfft // 2 + 1,)
    peak = int(np.argmax(spec))
    assert peak == nfft // 64, (
        f"{label}[nfft={nfft}] peaks at bin {peak}, not {nfft // 64} — the "
        "row is no longer transforming the block it is credited with"
    )


def _record(benchmark, n):
    if benchmark.stats:
        # MIN, not mean: the least-disturbed observation, per
        # docs/dev/contributing/benchmarking.md.
        benchmark.extra_info["MSa_s"] = n / benchmark.stats["min"] / 1e6


@pytest.mark.parametrize("nfft", NFFTS)
def test_bench_accumulate(benchmark, tone, nfft):
    """Window, FFT and fold 64k cf32 samples (64k/nfft frames)."""
    x, _ = tone
    p = _psd(nfft)

    benchmark(p.accumulate, x)

    # Every call folds the same whole number of frames; however many
    # rounds ran, the count is a multiple of it, and nonzero.
    per_call = BLOCK_64K // nfft
    assert p.count > 0 and p.count % per_call == 0, (
        f"count {p.count} is not a multiple of {per_call} frames per call"
    )
    _check_peak(p, nfft, "accumulate")
    _record(benchmark, BLOCK_64K)


@pytest.mark.parametrize("nfft", NFFTS)
def test_bench_accumulate_real(benchmark, tone, nfft):
    """The real-input fold over the same block — does f32 buy half an FFT?"""
    _, xr = tone
    p = _psd(nfft)

    benchmark(p.accumulate_real, xr)

    per_call = BLOCK_64K // nfft
    assert p.count > 0 and p.count % per_call == 0
    _check_peak(p, nfft, "accumulate_real")
    _record(benchmark, BLOCK_64K)


@pytest.mark.parametrize("nfft", NFFTS)
def test_bench_power_onesided(benchmark, tone, nfft):
    """The read-out: linear in the bin count, once per display refresh.

    A latency row (no ``MSa_s``): this call never sees a sample, so a
    per-sample rate would be a per-bin one under the wrong unit.
    """
    x, _ = tone
    p = _psd(nfft)
    p.accumulate(x)
    cap = p.power_onesided_max_out()
    out = np.empty(cap, dtype=np.float32)

    spec = benchmark(p.power_onesided, cap, out=out)

    assert spec.shape == (nfft // 2 + 1,)
    assert int(np.argmax(spec)) == nfft // 64
