"""Benchmark for Frame.

Run: pytest src/doppler/wfm/benchmarks/bench_frame.py --benchmark-only
"""

import numpy as np
import pytest

from doppler.wfm import Frame

BLOCK_64K = 65_536


@pytest.fixture
def obj():
    return Frame(
        preamble=np.zeros(1, dtype=np.uint8),
        sync=np.zeros(1, dtype=np.uint8),
        payload=np.zeros(1, dtype=np.uint8),
        preamble_kind="literal",
        preamble_nbits=0,
        preamble_reps=0,
        preamble_poly=0,
        preamble_seed=0,
        preamble_reg_bits=0,
        preamble_lfsr="galois",
        preamble_taps_a=0,
        preamble_seed_a=0,
        preamble_taps_b=0,
        preamble_seed_b=0,
        sync_kind="literal",
        sync_nbits=0,
        sync_poly=0,
        sync_seed=0,
        sync_reg_bits=0,
        sync_lfsr="galois",
        sync_taps_a=0,
        sync_seed_a=0,
        sync_taps_b=0,
        sync_seed_b=0,
        payload_kind="literal",
        payload_nbits=0,
        payload_poly=0,
        payload_seed=0,
        payload_reg_bits=0,
        payload_lfsr="galois",
        payload_taps_a=0,
        payload_seed_a=0,
        payload_taps_b=0,
        payload_seed_b=0,
        crc="none",
    )
