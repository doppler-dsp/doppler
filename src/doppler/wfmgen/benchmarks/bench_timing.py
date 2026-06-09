"""Benchmark for SampleClock (timing_core).

Pacing's wall-clock cost is the sleep, so the meaningful micro-benchmarks are
the per-call overheads: stamp() (pure arithmetic) and pace() at an impossible
fs (never sleeps → measures the scheduling overhead).

Run: pytest src/doppler/wfmgen/benchmarks/bench_timing.py --benchmark-only
"""

from doppler.wfmgen.compose import SampleClock


def test_bench_stamp(benchmark):
    clk = SampleClock(fs=1e6)
    benchmark(clk.stamp)


def test_bench_pace_nowait(benchmark):
    # fs so high every deadline is already past → pace() never sleeps.
    clk = SampleClock(fs=1e15, resync=True)
    benchmark(clk.pace, 1)
