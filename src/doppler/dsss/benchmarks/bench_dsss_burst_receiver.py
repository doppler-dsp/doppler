"""Benchmark for DsssBurstReceiver.

Run::

    pytest src/doppler/dsss/benchmarks/bench_dsss_burst_receiver.py \
        --benchmark-only

Nothing is timed yet: `push()` is a declared no-op until the three stages
land, and timing a stub measures the binding, not the receiver.
"""

import numpy as np
import pytest

from doppler.dsss import DsssBurstReceiver

BLOCK_64K = 65_536


@pytest.fixture
def obj():
    return DsssBurstReceiver(
        acq_code=np.zeros(1, dtype=np.uint8),
        data_code=np.zeros(1, dtype=np.uint8),
        sync=np.zeros(1, dtype=np.uint8),
        reps=5,
        spc=4,
        chip_rate=1000000.0,
        payload_len=64,
        cn0_dbhz=50.0,
        doppler_uncertainty=0.0,
        pfa=1e-3,
        pd=0.9,
        carrier_hz=0.0,
        max_rate=0.0,
        est_segments=10,
    )
