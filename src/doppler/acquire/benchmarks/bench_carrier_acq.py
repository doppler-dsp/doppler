"""Benchmark for CarrierAcquisition — the PSD search, listening.

Run: pytest src/doppler/acquire/benchmarks/bench_carrier_acq.py
     --benchmark-only

This file was a jm scaffold that took the `benchmark` fixture and never
called it, so coarse carrier acquisition reached the Python snapshot in no
row at all (doppler#1010).

Acquisition is not a per-sample cost like the tracking loops: `steps()`
folds `n_fft`-sample blocks into a running PSD and **stops reading the
moment a detection fires**. So "64k samples in, time per call"
measures however many blocks the decision happened to need, and crediting
the call with all 64k would report samples it never read as throughput.

One row per zero-pad, ``search``: 64k of noise, nothing fires. Sequential
mode tests after every block and never stops, so all 64k are folded and
searched: the sustained price of LISTENING, and the only case where MSa/s is
honest over the whole block. The price of a DECISION -- a BPSK carrier found
on its first block, credited with the samples folded -- is the C twin's
``detect`` row (``native/benchmarks/bench_carrier_acq_core.c``), on the
stimulus this file's ``detect`` rows used. They were cut here (doppler#2386):
a decision at 20 dB is one block folded and tested, which costs what one
``search`` block costs (334.6 us against 5.642 ms / 16 = 352.6 us at pad 4).

The block length is a power of two (``RESOLUTION_HZ = FS / 4096``) on
purpose: the transform is ``next_pow_two(n_fft * zero_pad)``, so a
power-of-two `n_fft` makes the folded block readable back from the object
as ``nfft // zero_pad`` instead of restating the core's sizing rule here.
"""

import numpy as np
import pytest

from doppler.acquire import CarrierAcquisition
from doppler.wfm import Synth

BLOCK_64K = 65_536
FS = 4.0e6
SPS = 4
SYMBOL_RATE_HZ = FS / SPS
#: One `n_fft` block is 4096 samples, so 64k is exactly 16 looks.
N_FFT = 4096
RESOLUTION_HZ = FS / N_FFT
PADS = (4, 16)


@pytest.fixture(scope="module")
def noise():
    """64k of unit-power AWGN: what a receiver hears before anyone keys up."""
    x = np.asarray(Synth(type="noise", fs=FS, seed=3).steps(BLOCK_64K))
    return np.ascontiguousarray(x.astype(np.complex64))


def _acq(pad):
    return CarrierAcquisition(
        sample_rate_hz=FS,
        symbol_rate_hz=SYMBOL_RATE_HZ,
        resolution_hz=RESOLUTION_HZ,
        zero_pad=pad,
        psd_template=np.array([], dtype=np.float32),
        sequential=True,
    )


def _time(benchmark, ca, x):
    """Time one `steps(x)` from the post-create state, every round.

    `reset()` runs in pedantic's untimed setup: a detector that has already
    decided reads nothing more, so a reused instance would time an early
    return from round two onward.
    """
    benchmark.pedantic(
        ca.steps,
        setup=lambda: (ca.reset(), ((x,), {}))[1],
        rounds=30,
        warmup_rounds=1,
    )


@pytest.mark.parametrize("pad", PADS)
def test_bench_steps_search(benchmark, noise, pad):
    """Listening: every block folded and tested, no detection fires."""
    ca = _acq(pad)
    _time(benchmark, ca, noise)

    assert ca.nfft // pad == N_FFT, "the block length moved; re-derive N_FFT"
    # Asserted, because a false alarm stops the fold early and the row would
    # then credit 64k to a call that read one block.
    assert ca.ready is False, f"pad={pad}: noise fired a detection"
    assert ca.n_blocks == BLOCK_64K // N_FFT, (
        f"pad={pad}: folded {ca.n_blocks} blocks, not all "
        f"{BLOCK_64K // N_FFT} — the row would not be searching the block"
    )
    if benchmark.stats:
        sec = benchmark.stats["min"]
        benchmark.extra_info["MSa_s"] = BLOCK_64K / sec / 1e6
