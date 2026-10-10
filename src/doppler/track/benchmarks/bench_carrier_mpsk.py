"""Benchmark for CarrierMpsk — the M-PSK carrier loop, per M and per FLL.

Run: pytest src/doppler/track/benchmarks/bench_carrier_mpsk.py --benchmark-only

This file was a jm scaffold that took the `benchmark` fixture and never
called it, so the carrier tracker inside every M-PSK receiver reached the
Python snapshot in no row at all (doppler#1010).

Six rows over one 64k block per M, the same sweep as the C twin
(``native/benchmarks/bench_carrier_mpsk_core.c``): M in {2, 4, 8}, because
the slicer and discriminator are not the same work at each, and the FLL
assist on or off, because ``bn_fll > 0`` adds a frequency discriminator that
is paid on every symbol, not only while unlocked. Same loop tuning, same
``tsamps``, same 0.001 cycles/sample residual, so the faces are comparable
and the gap between them is the binding's per-call overhead — which is what
a Python benchmark is for.

**Each row asserts that its loop is still locked onto the residual**, because
the ways this measurement breaks do not make it slower. A loop that has
slipped off the carrier still emits one prompt per symbol at full rate, so
the count cannot see it; the frequency estimate, the lock metric and the
settled EVM (from `doppler.ber`, the library's own meter) can.
"""

import numpy as np
import pytest

from doppler.ber import ber_evm_db
from doppler.mpsk import mpsk_map
from doppler.track import CarrierMpsk
from doppler.wfm import PN, Synth

BLOCK_64K = 65_536
SPS = 4
BN = 0.01
#: The residual carrier the loop is there to remove, cycles/sample.
RESIDUAL = 0.001
NSYM = BLOCK_64K // SPS
#: Prompts skipped before the EVM is read: the first call is the untimed
#: prime, so by the timed call the loop is long settled; this only keeps the
#: block's own wrap-around phase step out of the reading.
SETTLE = 2_000


def _stimulus(m):
    """64k of rect-pulse M-PSK at `SPS`, `RESIDUAL` off centre, noiseless.

    Labels from doppler's PN generator, mapped by `mpsk_map` and shaped and
    offset by `Synth`, the shipped waveform engine. Noiseless on purpose:
    noise changes what the loop concludes, not what it costs, and this file
    measures cost.
    """
    k = int(np.log2(m))
    bits = np.asarray(PN(poly=0, seed=1, length=15).generate(NSYM * k)) & 1
    labels = (bits.reshape(-1, k) @ (1 << np.arange(k)[::-1])).astype(np.uint8)
    syms = mpsk_map(labels, m).astype(np.complex64)
    x = Synth(
        type="symbols",
        symbols=syms,
        pulse="rect",
        sps=SPS,
        fs=1.0,
        freq=RESIDUAL,
        snr=999.0,
        seed=7,
    ).steps(BLOCK_64K)
    x = np.ascontiguousarray(np.asarray(x).astype(np.complex64))
    assert x.size == BLOCK_64K, "the block is the denominator; it must be full"
    return x


@pytest.mark.parametrize("bn_fll", [0.0, 0.005], ids=["pll", "fll"])
@pytest.mark.parametrize("m", [2, 4, 8])
def test_bench_steps(benchmark, m, bn_fll):
    """One block through the loop, already locked from a primed call.

    Built ONCE and primed outside the timed region: state carries across
    calls by design — a streaming caller's contiguous blocks — so every
    round times the tracking loop, which is what runs per sample in a
    receiver, rather than a pull-in from cold.
    """
    x = _stimulus(m)
    c = CarrierMpsk(
        bn=BN,
        zeta=0.707,
        init_norm_freq=0.0,
        tsamps=SPS,
        bn_fll=bn_fll,
        m=m,
    )
    c.steps(x)

    out = np.asarray(benchmark(c.steps, x))

    assert out.size == NSYM, (
        f"m={m}: emitted {out.size} prompts from {BLOCK_64K} samples, not "
        f"{NSYM} — a row that stops emitting reports a no-op as throughput"
    )
    assert c.norm_freq == pytest.approx(RESIDUAL, abs=1e-5), (
        f"m={m}: tracking {c.norm_freq:.5f} c/s, not the {RESIDUAL} residual"
    )
    assert c.lock_metric > 0.99, f"m={m}: lock_metric {c.lock_metric:.3f}"
    evm = float(ber_evm_db(out, SETTLE, out.size, m))
    assert evm < -40.0, (
        f"m={m}: settled EVM {evm:.1f} dB — an open eye is what says this "
        "row timed a locked loop"
    )

    if benchmark.stats:
        # MIN, not mean: the least-disturbed observation, per
        # docs/dev/contributing/benchmarking.md.
        sec = benchmark.stats["min"]
        benchmark.extra_info["MSa_s"] = BLOCK_64K / sec / 1e6
        benchmark.extra_info["Msym_s"] = out.size / sec / 1e6
        benchmark.extra_info["evm_db"] = evm
