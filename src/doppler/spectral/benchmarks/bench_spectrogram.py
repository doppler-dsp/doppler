"""Benchmark for Spectrogram.

Run through the bench targets; see docs/dev/contributing/benchmarking.md.
"""

import pytest

from doppler.spectral import Spectrogram

BLOCK_64K = 65_536


@pytest.fixture
def obj():
    return Spectrogram(
        nfft=256, hop=256, window="hann", beta=0.0, mode="power"
    )


def test_bench_create(benchmark):
    benchmark(
        lambda: Spectrogram(
            nfft=256, hop=256, window="hann", beta=0.0, mode="power"
        )
    )
