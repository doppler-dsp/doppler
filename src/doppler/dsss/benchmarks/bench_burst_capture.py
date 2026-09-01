"""Benchmark for BurstCapture — search, refine, and the window copy.

Run: pytest src/doppler/dsss/benchmarks/bench_burst_capture.py --benchmark-only

The same two rows, over the same 64k stimulus, as every other object in the
chain (`_burst_stimulus.py`), so this number reads against `BurstAcquisition`'s
and `DsssBurstReceiver`'s rather than in isolation. `BurstCapture` is
acquisition plus the refine stage and the window copy, so the gap between this
and `bench_burst_acq` is exactly what the capture layer costs.

- ``push_idle``   — 64k of noise. What listening costs.
- ``push_bursts`` — the same 64k carrying complete bursts.
- ``push_bursts_backed`` — the same block through a file-backed ring. The
  design claims backing the ring costs nothing, because the pages ARE the
  file rather than being copied to it; a claim nobody times is prose.
"""

import pytest

from doppler.dsss import BurstCapture, PersistentBurstCapture
from doppler.dsss.benchmarks._burst_stimulus import (
    BURST_LEN,
    CHIP_RATE,
    REPS,
    SPC,
    burst_stimulus,
    packing,
    rate,
)


@pytest.fixture(scope="module")
def waveform():
    return burst_stimulus()


@pytest.fixture(scope="module")
def n_bursts(waveform):
    acq_code, data_code, *_ = waveform
    return packing(acq_code, data_code)[1]


def _cap(acq_code, path=None):
    """Rebuilt per round: the object carries a ring, an acquisition
    accumulator and a detection queue, so a reused instance would measure a
    warm one from round two onward."""
    kw = {
        "burst_len": BURST_LEN,
        "reps": REPS,
        "spc": SPC,
        "chip_rate": CHIP_RATE,
        "cn0_dbhz": 60.0,
    }
    if path is None:
        return BurstCapture(acq_code, **kw)
    return PersistentBurstCapture(path, acq_code, **kw)


def test_bench_push_idle(benchmark, waveform):
    """The floor — what listening costs when nothing is there."""
    acq_code, _, _, _, idle = waveform
    win = benchmark(lambda: _cap(acq_code).push(idle))
    assert win.size % BURST_LEN == 0, "a partial window is not a window"
    rate(benchmark)


def test_bench_push_bursts(benchmark, waveform, n_bursts):
    """Complete bursts in the block — refine and the window copy on top."""
    acq_code, _, _, bursts, idle = waveform
    win = benchmark(lambda: _cap(acq_code).push(bursts))
    idle_win = _cap(acq_code).push(idle)
    assert win.size >= n_bursts * BURST_LEN, (
        f"only {win.size // BURST_LEN} windows for {n_bursts} bursts — this "
        "row is timing a capture that is no longer capturing"
    )
    assert win.size > idle_win.size, (
        f"{win.size} samples on bursts vs {idle_win.size} on noise — the two "
        "rows are not measuring different work"
    )
    rate(benchmark)


def test_bench_push_bursts_backed(benchmark, waveform, n_bursts, tmp_path):
    """The same block, with the ring's pages backed by a file.

    Read against `push_bursts`: the design's claim is that this row costs
    nothing extra, because MAP_SHARED means the samples ARE the file rather
    than being copied into it. If this row ever separates from its sibling,
    that claim has stopped being true.
    """
    acq_code, _, _, bursts, _ = waveform
    path = tmp_path / "ring.cf32"
    win = benchmark(lambda: _cap(acq_code, path).push(bursts))
    assert win.size >= n_bursts * BURST_LEN
    rate(benchmark)
