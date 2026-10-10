"""Benchmark for Specan — the spectrum analyzer's per-block path.

Run: pytest src/doppler/analyzer/benchmarks/bench_specan.py --benchmark-only

`Specan` is the front end of the analyzer application: it mixes and
decimates to the requested span, transforms at the requested RBW,
averages, and hands back a display frame. It runs at the input sample rate
for as long as a display is open, so `execute` is timed per 64k block and
`MSa_s` is input samples over the minimum time per block.

The sweep is over RBW at a fixed span, because that is the control an
operator turns and it moves two things at once: a finer RBW needs a longer
transform but completes fewer frames per block. The three RBWs are chosen
so the transforms DIFFER -- 512, 4096 and 16384 points -- because the
transform has a 512-point floor (``nfft = max(n, 512)``, specan_core.h):
at this span every RBW from 10 kHz up lands on it, so a coarser row would
time the same FFT as the 10 kHz one under another name. Each row asserts
the transform it claims. `retune` is its own row --
it is what a sweeping or scrolling display calls between blocks, and it
claims not to rebuild the plan.

The C benchmark (``native/benchmarks/bench_specan_core.c``) measures the
same rows over the same stimulus; the gap between the two faces is the
binding's per-call overhead, which is what a Python benchmark is for.
"""

import itertools

import numpy as np
import pytest

from doppler.analyzer import Specan
from doppler.source import LO

BLOCK_64K = 65_536
FS = 10.0e6
SPAN = 2.0e6
#: The tone, in normalised frequency -- 310 kHz, inside the +-1 MHz span.
F_TONE = 0.031
A_TONE = 0.5


@pytest.fixture(scope="module")
def tone():
    return (A_TONE * LO(F_TONE).steps(BLOCK_64K)).astype(np.complex64)


def _peak_hz(sa, frame):
    """Display bin -> Hz offset from the display centre (DC-centred)."""
    i = int(np.argmax(frame))
    return (i - sa.display_size // 2) * sa.fs_out / sa.nfft


#: RBW -> the transform length it must produce at this span.
RBW_NFFT = {10.0e3: 512, 2.0e3: 4096, 500.0: 16384}


@pytest.mark.parametrize("rbw", list(RBW_NFFT))
def test_bench_execute(benchmark, tone, rbw):
    """One 64k block in, the latest completed display frame out."""
    sa = Specan(fs=FS, span=SPAN, rbw=rbw)
    assert sa.nfft == RBW_NFFT[rbw], (
        f"rbw={rbw:.0f} transforms {sa.nfft} points, not {RBW_NFFT[rbw]} -- "
        "the rows no longer time three different transforms"
    )
    # Prime outside the timed region: the first block also fills the
    # decimator's history, which a streaming display pays once.
    sa.execute(tone)

    frame = benchmark(sa.execute, tone)

    assert frame is not None, (
        f"rbw={rbw:.0f}: a 64k block completed no frame -- this row would "
        "time buffering, not analysis"
    )
    assert frame.shape == (sa.display_size,)
    bin_hz = sa.fs_out / sa.nfft
    assert _peak_hz(sa, frame) == pytest.approx(F_TONE * FS, abs=bin_hz), (
        "the tone is not where it was put"
    )
    # 0.5 amplitude complex tone reads -6.02 dBFS; the window's scalloping
    # is bounded, so allow a dB.
    assert float(frame.max()) == pytest.approx(
        20.0 * np.log10(A_TONE), abs=1.0
    )
    if benchmark.stats:
        # MIN, not mean: the least-disturbed observation, per
        # docs/dev/contributing/benchmarking.md.
        sec = benchmark.stats["min"]
        benchmark.extra_info["MSa_s"] = BLOCK_64K / sec / 1e6


def test_bench_retune(benchmark, tone):
    """Between-block LO retune, alternating +-100 kHz. Per call, not per
    sample, so it records no MSa_s. Asserts the retune took: the tone
    moves by exactly the offset in the next frame."""
    sa = Specan(fs=FS, span=SPAN, rbw=10.0e3)
    sa.execute(tone)
    centres = itertools.cycle((1.0e5, -1.0e5))

    benchmark(lambda: sa.retune(next(centres)))

    sa.retune(1.0e5)
    sa.execute(tone)  # drain the frame straddling the retune
    frame = sa.execute(tone)
    bin_hz = sa.fs_out / sa.nfft
    assert _peak_hz(sa, frame) == pytest.approx(
        F_TONE * FS - 1.0e5, abs=bin_hz
    ), "retune did not move the display centre"
