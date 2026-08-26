"""Benchmark for RateSync — the timing loop, one row per detector.

Run: pytest src/doppler/track/benchmarks/bench_ratesync.py --benchmark-only

This file was a jm scaffold that took the `benchmark` fixture and never
called it, so the timing loop every M-PSK receiver runs reached the Python
snapshot in no row at all (doppler#1010).

Two rows over the SAME 64k block, one per TED, because the detector is the
one thing a caller still chooses after the link is fixed: `sps` comes from
the clock ratio and the pulse from the modulation, while `ted` is a free
choice between a blind detector (Gardner: one product on the transition
gate) and a decision-directed one (DTTL: slice first, then correlate the
sign). Whether that slice is free is the question the pair answers, and a
single blended figure over both would answer neither.

Both rows publish MSa/s and Msym/s, and Msym/s is the one to size a link
with: the loop runs per input SAMPLE and delivers per SYMBOL, so `sps`
moves the two in opposite directions. The C twin
(``native/benchmarks/bench_ratesync_core.c``) measures the same two
detectors over the same block length and prints the same reading, so the
faces are comparable and the gap between them is the binding's per-call
overhead — which is what a Python benchmark is for.

**Each row asserts that its loop is still locked**, because every way this
measurement can break makes it look FASTER. Emitting nothing is the C
twin's documented trap: sizing its output buffer from
`ratesync_steps_max_out()` made every call return zero symbols and the
benchmark report 3.2 THz, which is the bail-out path wearing a throughput
number. A loop that has come UNLOCKED is the quieter one — it still emits
symbols at full rate, so the count alone cannot see it, and the row would
then be timing an open-loop cascade under a tracking loop's name. Count,
lock statistic and settled EVM are therefore all checked, from the
library's own primitives (`doppler.ber`) rather than a private copy.
"""

import numpy as np
import pytest

from doppler.ber import ber_evm_db, ber_settle_syms
from doppler.track import RateSync
from doppler.wfm import PN, Synth

BLOCK_64K = 65_536
SPS = 4
BETA = 0.35
SPAN = 8
BN = 0.01
#: Symbols the block carries. The object emits one per `SPS` inputs once
#: it is tracking, so this is also the row's Msym/s denominator.
NSYM_OUT = BLOCK_64K // SPS

#: Generation grid, in samples per symbol. Twice `SPS` so a standing timing
#: offset is placed by DECIMATION PHASE — exact, and with no second
#: implementation of the pulse to interpolate with.
FINE = 2 * SPS
#: Quarter-symbol standing offset, so the loop has to pull in rather than
#: start on its own equilibrium. Deliberately not the HALF-symbol point:
#: that is the detector's other equilibrium (unstable, so the loop does
#: leave it — see the header's T/2 section), and a benchmark has no reason
#: to start on a knife edge it is not measuring.
TAU_FINE = FINE // 4


@pytest.fixture(scope="module")
def rx():
    """One 64k block of RRC-shaped BPSK at `SPS`, offset a quarter symbol.

    Shaped by **`Synth`**, the shipped waveform engine, and not by
    `rrc_taps` through a FIR here. The distinction is not cosmetic: those
    taps are unit-ENERGY, so their peak falls as the grid is refined, while
    the analytic pulse the C shapes with has a fixed peak. The validation
    report records what bridging that by hand costs when it is got wrong —
    a loop under-driven by ~20 dB — and there is nothing to bridge if the
    stream comes from the engine.

    Noiseless on purpose. Noise changes what the loop CONCLUDES and not
    what it costs, and this file measures cost; the sensitivity axis is the
    certification's job (`src/doppler/track/tests/validation/ratesync/`).

    Module-scoped: both rows must be indexed by the same denominator, and
    the shaping filter is the expensive part of the setup.
    """
    nsym = NSYM_OUT + 2 * SPAN + 4  # + the shaping filter's own fill
    bits = np.asarray(PN(poly=0, seed=1, length=15).generate(nsym)) & 1
    syms = np.where(bits > 0, 1.0, -1.0).astype(np.complex64)
    gen = Synth(
        type="symbols",
        symbols=syms,
        pulse="rrc",
        rrc_beta=BETA,
        rrc_span=SPAN,
        sps=FINE,
        fs=1.0,
        snr=999.0,
        seed=7,
    )
    fine = np.asarray(gen.steps(nsym * FINE)).astype(np.complex64)
    x = np.ascontiguousarray(fine[TAU_FINE :: FINE // SPS][:BLOCK_64K])
    assert x.size == BLOCK_64K, "the block is the denominator; it must be full"
    return x


def _ratesync(ted):
    return RateSync(
        sps=float(SPS),
        pulse="rrc",
        beta=BETA,
        span=SPAN,
        m=2,
        num_phases=1024,
        bn=BN,
        zeta=0.707,
        ted=ted,
    )


def _measure(benchmark, ted, x):
    """Time one detector over the block, and prove the row still tracks.

    The object is built ONCE and primed with a call outside the timed
    region: `RateSync` designs a 1024-arm bank in its constructor, which
    is the same order of cost as processing the block, so a per-round
    rebuild would measure the planner. State carries across calls by
    design — a streaming caller's contiguous blocks — and each round
    re-acquires the quarter-symbol offset because the block restarts,
    which both rows pay identically.
    """
    rs = _ratesync(ted)
    rs.steps(x)

    out = np.asarray(benchmark(rs.steps, x))

    assert out.size == pytest.approx(NSYM_OUT, rel=0.01), (
        f"{ted} emitted {out.size} symbols from {BLOCK_64K} samples, not "
        f"~{NSYM_OUT} — a row that stops emitting reports the bail-out "
        "path as throughput"
    )
    assert rs.clipped is False, (
        f"{ted} clipped — this times a saturated cascade"
    )
    assert rs.locked is True, f"{ted} lost lock (lock_stat={rs.lock_stat})"
    # The eye, measured only where the loop is allowed to have settled.
    # `ber_settle_syms` is the library's own answer to where that window
    # may start; a fraction of the record would move with the block size.
    lo = int(ber_settle_syms(BN, 0.0))
    evm = float(ber_evm_db(out, lo, out.size, 2))
    assert evm < -30.0, (
        f"{ted} settled EVM {evm:.1f} dB — an open eye is what says this "
        "row timed a tracking loop rather than a cascade running open"
    )

    if benchmark.stats:
        # MIN, not mean: the least-disturbed observation, per
        # docs/dev/contributing/benchmarking.md.
        sec = benchmark.stats["min"]
        benchmark.extra_info["MSa_s"] = BLOCK_64K / sec / 1e6
        benchmark.extra_info["Msym_s"] = out.size / sec / 1e6
        benchmark.extra_info["evm_db"] = evm


def test_bench_steps_gardner(benchmark, rx):
    """The blind detector: mid-sample times the conjugate difference."""
    _measure(benchmark, "gardner", rx)


def test_bench_steps_dttl(benchmark, rx):
    """The decision-directed one: a slicer per symbol ahead of the same PI
    loop. Lower self-noise near lock — this row is what it costs."""
    _measure(benchmark, "dttl", rx)
