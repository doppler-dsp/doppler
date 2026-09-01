"""Benchmark for BurstCapture.

Run: pytest src/doppler/dsss/benchmarks/bench_burst_capture.py --benchmark-only
"""

import numpy as np
import pytest

from doppler.dsss import BurstCapture

BLOCK_64K = 65_536


@pytest.fixture
def obj():
    return BurstCapture(
        acq_code=np.zeros(1, dtype=np.uint8),
        burst_len=8192,
        reps=5,
        spc=4,
        chip_rate=1000000.0,
        cn0_dbhz=50.0,
        doppler_uncertainty=0.0,
        pfa=1e-3,
        pd=0.9,
    )
