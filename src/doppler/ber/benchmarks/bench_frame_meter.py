"""Benchmark for FrameMeter — the per-frame tally and its interval.

Run: pytest src/doppler/ber/benchmarks/bench_frame_meter.py --benchmark-only

``add()`` is a per-FRAME call: a caller makes one per frame it decodes, so
from Python the row is a loop of ``add`` calls and the figure is what one
frame outcome costs through the binding -- which is the number a Python
receiver loop pays, and the reason this row exists beside the C one. The
C benchmark (``native/benchmarks/bench_frame_meter_core.c``) times the same
loop in C and the interval accessors besides; ``fer`` is kept here because
it is the call a sweep makes once per point.

- ``add`` — 4096 frame outcomes, every 97th a CRC failure.
- ``fer`` — the exact interval over those outcomes.

Both assert the tally the stimulus implies, so a meter that stopped
counting fails instead of getting faster.
"""

from doppler.ber import FrameMeter

#: Frames per round. A Python loop, so smaller than the C block: enough to
#: put the round well above the timer, not so many the suite waits on it.
N_FRAMES = 4_096
FAIL_EVERY = 97
N_FAILED = len(range(0, N_FRAMES, FAIL_EVERY))


def _feed(m):
    add = m.add
    for i in range(N_FRAMES):
        add(1, 0 if i % FAIL_EVERY == 0 else 1)


def test_bench_add(benchmark):
    """One ``add`` per frame; every 97th frame fails its CRC."""
    m = FrameMeter(target_errors=200, conf=0.99)
    benchmark.pedantic(
        _feed, args=(m,), setup=m.reset, rounds=50, warmup_rounds=2
    )
    assert m.frames == N_FRAMES
    assert m.errors == N_FAILED
    assert m.crc_passed == N_FRAMES - N_FAILED
    assert m.sync_detected == N_FRAMES
    if benchmark.stats:
        sec = benchmark.stats["min"]
        benchmark.extra_info["MSa_s"] = N_FRAMES / sec / 1e6
        benchmark.extra_info["ns_per_frame"] = sec / N_FRAMES * 1e9


def test_bench_fer(benchmark):
    """The interval over a filled meter brackets the observed rate."""
    m = FrameMeter(target_errors=200, conf=0.99)
    _feed(m)
    iv = benchmark(m.fer)
    assert iv.errors == N_FAILED
    assert iv.symbols == N_FRAMES
    assert iv.lo < N_FAILED / N_FRAMES < iv.hi
