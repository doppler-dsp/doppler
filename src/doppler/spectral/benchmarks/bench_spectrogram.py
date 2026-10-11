"""Benchmarks for Spectrogram: construction and streaming push."""

import numpy as np

from doppler.spectral import Spectrogram


def test_bench_create(benchmark):
    """Construction cost: the window and the PSD plan."""
    benchmark(lambda: Spectrogram(nfft=256, hop=256, window="hann"))


def test_bench_push(benchmark):
    """Streaming push: one 64 KiB chunk, rows out."""
    x = np.ones(8192, np.complex64)
    sp = Spectrogram(nfft=256, hop=256, window="hann")
    benchmark(lambda: sp.push(x))
