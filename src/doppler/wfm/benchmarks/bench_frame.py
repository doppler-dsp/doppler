"""Benchmark for Frame — materialising frames and checking their CRC.

Run: pytest src/doppler/wfm/benchmarks/bench_frame.py --benchmark-only

The frame is the C benchmark's (``native/benchmarks/bench_frame_core.c``):
a 32-bit alternating preamble repeated 4 times, a 64-bit sync word, a
1024-bit payload and a CRC-16 -- 1232 bits. Three rows:

- ``bits[1]``  — one frame per call.
- ``bits[16]`` — sixteen per call. Against ``bits[1]`` the gap is the
  per-call overhead a generator pays when it asks for frames one at a
  time, which is the question the batch exists to answer.
- ``crc_ok``   — check one received frame.

Every row asserts the frame it produced or checked: each emitted frame
carries its fields at their offsets and passes its own CRC, and the
checker refuses a frame with one payload bit flipped. A frame that stopped
being built would otherwise read as a faster one.
"""

import numpy as np
import pytest

from doppler.wfm import Frame

N_PRE, PRE_REPS, N_SYNC, N_PAY, N_CRC = 32, 4, 64, 1024, 16
BATCH = 16
NBITS = N_PRE * PRE_REPS + N_SYNC + N_PAY + N_CRC


@pytest.fixture(scope="module")
def fields():
    pre = (np.arange(N_PRE * PRE_REPS) & 1).astype(np.uint8)
    sync = ((0x1ACFFC1D >> (np.arange(N_SYNC) % 32)) & 1).astype(np.uint8)
    pay = np.random.default_rng(3).integers(0, 2, N_PAY).astype(np.uint8)
    return pre, sync, pay


@pytest.fixture(scope="module")
def frame(fields):
    pre, sync, pay = fields
    f = Frame(preamble=pre, sync=sync, payload=pay, crc="crc16")
    assert f.nbits == NBITS
    return f


def _assert_frames(f, bits, n, fields):
    pre, sync, pay = fields
    assert bits.size == n * NBITS
    s0, s1 = pre.size, pre.size + sync.size
    for fr in bits.reshape(n, NBITS):
        assert np.array_equal(fr[:s0], pre)
        assert np.array_equal(fr[s0:s1], sync)
        assert np.array_equal(fr[s1 : s1 + pay.size], pay)
        assert f.crc_ok(fr) == 1


def _rate(benchmark, frames):
    if benchmark.stats:
        sec = benchmark.stats["min"]
        benchmark.extra_info["Mbit_s"] = frames * NBITS / sec / 1e6
        benchmark.extra_info["us_per_frame"] = sec / frames * 1e6


def test_bench_bits_1(benchmark, frame, fields):
    bits = np.array(benchmark(frame.bits, 1))  # a view: copy it
    _assert_frames(frame, bits, 1, fields)
    _rate(benchmark, 1)


def test_bench_bits_16(benchmark, frame, fields):
    bits = np.array(benchmark(frame.bits, BATCH))
    _assert_frames(frame, bits, BATCH, fields)
    _rate(benchmark, BATCH)


def test_bench_crc_ok(benchmark, frame):
    rx = np.array(frame.bits(1))
    assert benchmark(frame.crc_ok, rx) == 1
    bad = rx.copy()
    bad[N_PRE * PRE_REPS + N_SYNC + N_PAY // 2] ^= 1
    assert frame.crc_ok(bad) == 0
    _rate(benchmark, 1)
