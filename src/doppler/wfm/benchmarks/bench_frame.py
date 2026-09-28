"""Benchmark for Frame.

Run: pytest src/doppler/wfm/benchmarks/bench_frame.py --benchmark-only
"""

import numpy as np
import pytest

from doppler.wfm import Frame

BLOCK_64K = 65_536


@pytest.fixture
def obj():
    # No preamble: the old fixture passed preamble_reps=0, which meant none.
    return Frame(
        sync=np.zeros(1, dtype=np.uint8),
        payload=np.zeros(1, dtype=np.uint8),
        crc="none",
    )
