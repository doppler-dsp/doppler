"""Benchmark for ReedSolomon.

Run: pytest src/doppler/coding/benchmarks/bench_rs_codec.py --benchmark-only

`native/benchmarks/bench_rs_codec_core.c` measures the object in C — the
table build, the cost of placing a systematic codeword, and decode at both
ends of the correction range. What only this face can show is what a Python
caller pays on top: the per-call binding, and the allocation `encode` and
`syndromes` do that the in-place `decode` does not.
"""

import numpy as np
import pytest

from doppler.coding import ReedSolomon

# CCSDS 131.0-B 4.3's outer code, as the C benchmark uses.
CCSDS = {
    "nroots": 32,
    "field_poly": 0x87,
    "first_root": 112,
    "root_stride": 11,
}


@pytest.fixture
def rs():
    return ReedSolomon(**CCSDS)


@pytest.fixture
def clean(rs):
    return rs.encode(np.arange(rs.k, dtype=np.uint8))


def test_bench_create(benchmark):
    """The table build, which a caller may or may not be able to hoist."""
    benchmark(ReedSolomon, **CCSDS)


def test_bench_encode(benchmark, rs):
    info = np.arange(rs.k, dtype=np.uint8)
    benchmark(rs.encode, info)


def test_bench_decode_clean(benchmark, rs, clean):
    """The path a good link runs almost every frame: syndromes, all zero."""
    word = clean.copy()
    benchmark(rs.decode, word)


def test_bench_decode_full(benchmark, rs, clean):
    """E errors — the whole Berlekamp-Massey / Chien / Forney chain.

    The word is re-damaged inside the timed call rather than once outside
    it: a decode that succeeded on the first round would leave a clean
    codeword behind and every later round would measure the e=0 path.
    """
    pos = np.arange(rs.e) * 7

    def run():
        word = clean.copy()
        word[pos] ^= 0xA5
        return rs.decode(word)

    benchmark(run)


def test_bench_codeword_ok(benchmark, rs, clean):
    """Syndromes and nothing else — a receiver's cheap error detector."""
    benchmark(rs.codeword_ok, clean)
