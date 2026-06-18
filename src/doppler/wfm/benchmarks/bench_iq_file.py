"""Benchmark for IqFile.

Run: pytest src/doppler/wfm/benchmarks/bench_iq_file.py --benchmark-only
"""

import pytest

from doppler.wfm import IqFile

BLOCK_1K = 1_024
BLOCK_64K = 65_536


@pytest.fixture
def obj():
    return IqFile("cf32", "le", "")
