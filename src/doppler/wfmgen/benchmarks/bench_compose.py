"""Benchmark for the Writer / Reader container codecs.

Round-trip throughput per container and sample type: the Writer quantises +
frames, the Reader parses + dequantises. Both delegate the hot loop to C.

Run: pytest src/doppler/wfmgen/benchmarks/bench_compose.py --benchmark-only
"""

import pytest

from doppler.wfmgen.compose import Composer, Reader, Writer

BLOCK = 65_536


@pytest.fixture
def samples():
    return Composer(type="qpsk", sps=8, num_samples=BLOCK).compose()


@pytest.mark.parametrize(
    "file_type,stype",
    [("raw", "cf32"), ("raw", "ci16"), ("blue", "cf32"), ("csv", "cf32")],
)
def test_bench_writer(benchmark, samples, tmp_path, file_type, stype):
    p = str(tmp_path / f"b_{file_type}.{stype}")

    def write():
        with Writer(p, file_type=file_type, sample_type=stype) as w:
            w.write(samples)

    benchmark(write)


@pytest.mark.parametrize(
    "file_type,stype", [("raw", "cf32"), ("raw", "ci16"), ("blue", "cf32")]
)
def test_bench_reader(benchmark, samples, tmp_path, file_type, stype):
    p = str(tmp_path / f"b_{file_type}.{stype}")
    with Writer(p, file_type=file_type, sample_type=stype) as w:
        w.write(samples)

    def read():
        Reader(p, sample_type=stype).read_all()

    benchmark(read)
