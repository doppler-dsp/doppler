"""Benchmark for DsssReceiver.

Run: pytest src/doppler/dsss/benchmarks/bench_dsss_receiver.py --benchmark-only
"""

import numpy as np
import pytest

from doppler.dsss import DsssReceiver

BLOCK_64K = 65_536


@pytest.fixture
def obj():
    return DsssReceiver(
        code=np.zeros(1023, dtype=np.uint8),
        chip_rate=1.0e6,
        symbol_rate=1000.0,
        spc=2,
        m=2,
        cn0_dbhz=55.0,
        doppler_uncertainty=100.0,
        segments=4,
        sps=8,
    )
