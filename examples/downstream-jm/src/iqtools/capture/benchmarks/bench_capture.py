"""Benchmarks for Capture — how fast a capture reads from Python.

Run via:  make bench

Each `test_bench_*` below is a pytest-benchmark case: hand `benchmark` a
callable and it does the warm-up, the repeats and the statistics. `make bench`
collects these alongside the C results.
"""

import numpy as np
import pytest

from doppler.wfm import Composer, Segment, Writer
from iqtools.capture import Capture

FS = 2_400_000.0
FC = 1_200_000_000.0
NUM_SAMPLES = 65_536
BLOCK = 4_096


@pytest.fixture(scope="module")
def capture_path(tmp_path_factory):
    """A BLUE capture to read, written with doppler's own writer."""
    scene = Segment("qpsk", sps=8, snr=15, fs=FS, num_samples=NUM_SAMPLES)
    samples = Composer([scene]).compose()

    # Back the payload off full scale so the ci16 quantisation never clips.
    peak = float(np.max(np.abs(samples)))
    headroom_db = 20.0 * np.log10(peak) + 1.0 if peak > 0.0 else 1.0

    path = tmp_path_factory.mktemp("bench") / "capture.blue"
    with Writer(
        path,
        file_type="blue",
        sample_type="ci16",
        fs=FS,
        fc=FC,
        headroom=headroom_db,
    ) as w:
        w.write(samples)
    return path


def test_bench_open(benchmark, capture_path):
    """Opening a capture: the header parse, paid once per file."""

    def open_and_close():
        with Capture(capture_path):
            pass

    benchmark(open_and_close)


def test_bench_read_block(benchmark, capture_path):
    """One block — the hot loop, ci16 on disk to cf32 in memory."""
    with Capture(capture_path) as cap:

        def read_one():
            cap.reset()
            return cap.read(BLOCK)

        benchmark(read_one)


def test_bench_read_whole(benchmark, capture_path):
    """The whole capture, block by block, the way a consumer reads it."""
    with Capture(capture_path) as cap:

        def read_all():
            cap.reset()
            total = 0
            while True:
                got = cap.read(BLOCK)
                if got.size == 0:
                    return total
                total += got.size

        benchmark(read_all)


def test_bench_summary(benchmark, capture_path):
    """The metadata record — no sample data touched."""
    with Capture(capture_path) as cap:
        benchmark(cap.summary)
