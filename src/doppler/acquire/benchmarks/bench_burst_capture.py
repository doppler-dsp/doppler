"""Benchmark for BurstCapture — search, refine, and the window copy.

Run:
    make bench-python PYTEST_ARGS="-k bench_burst_capture --benchmark-only"

Over the same 64k stimulus as every other object in the chain
(`_burst_stimulus.py`), so this number reads against `DsssBurstReceiver`'s
rather than in isolation.

One row, ``push_bursts``: the same 64k carrying complete bursts, at the
chain's real operating point (a 255x5 preamble at 2 samples per chip,
8,316-sample bursts, AWGN), with the captures asserted. The C
``burst_capture::push[4 bursts]`` row uses a smaller geometry, so this is a
different case rather than a re-measure.

Cut (doppler#2386): ``push_idle`` (the 4-burst row without refine),
``push_bursts_backed`` (the same cost as ``push_bursts``, 46.5 against 45.9
MSa/s) and ``construct`` (the same 6.6 ms as ``BurstAcquisition``, whose
sizing is the cost; ``bench_burst_acq.py`` keeps that row).
"""

import pytest

from doppler.acquire import BurstCapture
from doppler.acquire.tests._preamble import code_preamble
from doppler.dsss.benchmarks._burst_stimulus import (
    BURST_LEN,
    CHIP_RATE,
    REPS,
    SPC,
    burst_stimulus,
    on_fresh,
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


def _cap(acq_code):
    """Rebuilt per round: the object carries a ring, an acquisition
    accumulator and a detection queue, so a reused instance would measure a
    warm one from round two onward."""
    kw = {
        "burst_len": BURST_LEN,
        "reps": REPS,
        "fs": CHIP_RATE * SPC,
        "cn0_dbhz": 60.0,
    }
    return BurstCapture(code_preamble(acq_code, SPC), **kw)


def test_bench_push_bursts(benchmark, waveform, n_bursts):
    """Complete bursts in the block — refine and the window copy on top."""
    acq_code, _, _, bursts, idle = waveform
    win = on_fresh(benchmark, lambda: _cap(acq_code), lambda c: c.push(bursts))
    idle_win = _cap(acq_code).push(idle)
    assert win.size >= n_bursts * BURST_LEN, (
        f"only {win.size // BURST_LEN} windows for {n_bursts} bursts — this "
        "row is timing a capture that is no longer capturing"
    )
    assert win.size > idle_win.size, (
        f"{win.size} samples on bursts vs {idle_win.size} on noise — this "
        "row is not measuring a capture"
    )
    rate(benchmark)
