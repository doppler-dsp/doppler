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
    # Keyword-only on purpose: the generated kwlist reorders whenever an
    # init_param is added or its kind changes, and a positional fixture
    # rots silently because the perf gate is advisory (see
    # tests/test_benchmark_fixtures.py).
    return CarrierAcquisition(
        psd_template=np.array([], dtype=np.float32),
        sample_rate_hz=8000.0,
        symbol_rate_hz=1000.0,
    )
