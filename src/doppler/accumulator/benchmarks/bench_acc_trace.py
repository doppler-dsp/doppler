"""Benchmark for AccTrace — spectrum-trace accumulation, per mode.

Run: pytest src/doppler/accumulator/benchmarks/bench_acc_trace.py
     --benchmark-only

The same rows as the C benchmark
(``native/benchmarks/bench_acc_trace_core.c``), over the same shape: a
4096-bin trace (a typical analyzer FFT) fed 16 frames per round, one row per
reduction mode, plus the ``value`` read-out. The gap between the two faces is
the binding's per-call overhead, which is what a Python benchmark is for.

The 16 frames are **distinct** draws, not one frame fed 16 times. For the
hold modes that is the whole measurement: replaying a single frame means
every compare after the first fails and the update branch is never taken,
so maxhold would read as a pure compare loop no real display sees.

Each round starts from ``reset()`` (in ``pedantic``'s untimed setup), so
every round folds the same 16 frames into an empty trace — the work per
round is constant, and the result can be asserted exactly.
"""

import numpy as np
import pytest

from doppler.accumulator import AccTrace

NBINS = 4_096
FRAMES = 16
ROUNDS = 100


@pytest.fixture(scope="module")
def frames():
    rng = np.random.default_rng(0x9E3D)
    return rng.random((FRAMES, NBINS), dtype=np.float32)


#: mode -> the per-bin reduction the trace must hold after FRAMES frames.
#: `exp` has no closed form worth restating here; it is bounded instead.
_EXPECT = {
    "mean": lambda f: f.mean(axis=0),
    "maxhold": lambda f: f.max(axis=0),
    "minhold": lambda f: f.min(axis=0),
}


@pytest.mark.parametrize("mode", ["mean", "exp", "maxhold", "minhold"])
def test_bench_accumulate(benchmark, frames, mode):
    """Fold 16 distinct 4096-bin frames into an empty trace."""
    acc = AccTrace(n=NBINS, mode=mode, alpha=0.1)
    rows = list(frames)  # one contiguous float32 row per call

    def fold():
        for f in rows:
            acc.accumulate(f)

    benchmark.pedantic(fold, setup=acc.reset, rounds=ROUNDS)

    assert acc.count == FRAMES, (
        f"{mode}: count {acc.count} after one round of {FRAMES} frames — "
        "a fold that stopped counting stopped folding"
    )
    got = acc.value(NBINS)
    assert got.shape == (NBINS,)
    if mode in _EXPECT:
        np.testing.assert_allclose(got, _EXPECT[mode](frames), rtol=1e-5)
    else:
        # An EMA of draws in [0, 1) stays inside the frames' envelope and
        # is not simply the last frame (alpha < 1 keeps history).
        assert np.all(got >= frames.min(axis=0) - 1e-6)
        assert np.all(got <= frames.max(axis=0) + 1e-6)
        assert not np.allclose(got, frames[-1])

    if benchmark.stats:
        # MIN, not mean: the least-disturbed observation, per
        # docs/dev/contributing/benchmarking.md.
        sec = benchmark.stats["min"]
        benchmark.extra_info["MSa_s"] = FRAMES * NBINS / sec / 1e6


def test_bench_value(benchmark, frames):
    """Read the averaged trace out — once per display refresh, not per
    frame, so it is its own row rather than folded into accumulate."""
    acc = AccTrace(n=NBINS, mode="mean", alpha=0.1)
    for f in frames:
        acc.accumulate(f)
    out = np.empty(NBINS, dtype=np.float32)

    got = benchmark(acc.value, NBINS, out=out)

    assert got.shape == (NBINS,)
    np.testing.assert_allclose(got, frames.mean(axis=0), rtol=1e-5)
    # No MSa_s: a read-out consumes no samples, so per bench_report.py it is
    # a latency row -- time per call.
