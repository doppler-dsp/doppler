"""Benchmark for CarrierAcquisition.

Run: pytest src/doppler/acquire/benchmarks/bench_carrier_acq.py
     --benchmark-only
"""

import numpy as np
import pytest

from doppler.acquire import CarrierAcquisition

BLOCK_64K = 65_536


@pytest.fixture
def obj():
    return CarrierAcquisition(
        np.zeros(1, dtype=np.float32),
        0.0,
        0.0,
        0.0,
        4,
        "hann",
        0.0,
        1e-3,
        0.9,
        2.0,
        True,
    )
