"""Benchmark for Reader.

Run: pytest src/doppler/wfm/benchmarks/bench_wfm_reader.py --benchmark-only

jm scaffolds an `obj()` fixture calling the constructor with placeholder
arguments, which a Reader cannot use -- it needs a real capture on disk. This
writes one first, then measures the decode path: file bytes -> unit-scale
complex64. `reset()` rewinds between rounds so each iteration reads the same
samples rather than hitting EOF after the first.
"""

import pytest

from doppler.wfm import Composer, Reader, Segment, Writer

BLOCK_64K = 65_536


@pytest.fixture
def obj(tmp_path):
    x = Composer([Segment("qpsk", sps=8, num_samples=BLOCK_64K)]).compose()
    p = tmp_path / "bench.blue"
    with Writer(p, file_type="blue", sample_type="cf32", fs=2.4e6) as w:
        w.write(x)
    with Reader(p) as r:
        yield r


def test_read_64k(benchmark, obj):
    def _read():
        obj.reset()
        return obj.read(BLOCK_64K)

    out = benchmark(_read)
    assert len(out) == BLOCK_64K
