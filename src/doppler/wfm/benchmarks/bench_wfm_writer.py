"""Benchmark for Writer.

Run: pytest src/doppler/wfm/benchmarks/bench_wfm_writer.py --benchmark-only

jm scaffolds an `obj()` fixture calling the constructor with placeholders,
which a Writer cannot use -- it needs a path. This measures the encode path:
unit-scale complex64 -> quantised interleaved samples on disk, for a float wire
type (a reinterpret) and an integer one (a SIMD rescale plus clip tracking).
"""

import pytest

from doppler.wfm import Composer, Segment, Writer

BLOCK_64K = 65_536


@pytest.fixture
def scene():
    return Composer([Segment("qpsk", sps=8, num_samples=BLOCK_64K)]).compose()


@pytest.mark.parametrize("sample_type", ["cf32", "ci16"])
def test_write_64k(benchmark, tmp_path, scene, sample_type):
    def _write():
        with Writer(
            tmp_path / f"b.{sample_type}", sample_type=sample_type
        ) as w:
            return w.write(scene)

    assert benchmark(_write) == BLOCK_64K
