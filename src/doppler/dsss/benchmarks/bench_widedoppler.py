"""Wide-Doppler acquisition: 2-D roll vs column-FFT mixer bank.

Backs the method-selection trade in ``docs/dev/dsss-use-cases.md``. For a wide
Doppler uncertainty spanning ``D`` native windows (``D = Δf / (1/T_epoch)``),
two methods cover it:

* **2-D roll** — one forward ``FFT_nx`` of a single epoch, then ``D`` inverse
  ``FFT_nx`` (one per Doppler-shifted code spectrum). ``D`` **coarse** bins
  (``1/nx`` resolution), one epoch's gain. The fast, latency-bound choice.
* **Column-FFT mixer bank** — ``D`` channels, each a ``Corr2D(ny, nx)`` tile
  integrating ``ny`` epochs. ``D·ny`` **fine** bins (``1/(ny·nx)``), full
  ``10·log10(ny·N)`` coherent gain — but a 2-D FFT over ``ny`` epochs per
  channel. The sensitive, latency-tolerant choice.

Both primitives are shipped doppler (``spectral.FFT`` / ``spectral.Corr2D``).
The benchmark reports per-method cost so the doc's "fastest that acquires"
claims are measured, not asserted: the bank buys ``ny`` finer bins *and* ``ny``
epochs of gain, so it runs tens-of-times the roll for the same span (the factor
tracks ``ny``).

Run: pytest src/doppler/dsss/benchmarks/bench_widedoppler.py --benchmark-only
"""

import numpy as np
import pytest

from doppler.spectral import FFT, Corr2D

# (label, ny coherent epochs, nx epoch samples, D Doppler windows to cover Δf)
GRIDS = [
    ("gps_16x2046", 16, 2046, 79),  # L1-ish: ±39 kHz / 1 kHz window
    ("burst_5x4094", 5, 4094, 49),  # long acq code, 5 reps: ±60 kHz / 2.44 kHz
]


def _roll_factory(nx, d):
    """Return a callable doing one 2-D roll: 1 forward + d inverse FFT_nx."""
    rng = np.random.default_rng(0)
    code = (rng.standard_normal(nx) + 1j * rng.standard_normal(nx)).astype(
        np.complex64
    )
    x = (rng.standard_normal(nx) + 1j * rng.standard_normal(nx)).astype(
        np.complex64
    )
    fwd = FFT(nx, -1, 1)
    inv = FFT(nx, +1, 1)
    # Pre-roll the conjugate code spectrum for each Doppler bin (stored ref).
    cspec = np.conj(fwd.execute_cf32(code))
    rolls = [np.roll(cspec, m).astype(np.complex64) for m in range(d)]

    def roll():
        bigx = fwd.execute_cf32(x)
        return [inv.execute_cf32(bigx * r) for r in rolls]

    return roll


@pytest.mark.parametrize("label,ny,nx,d", GRIDS)
def test_bench_2d_roll(benchmark, label, ny, nx, d):
    """One 2-D roll sweeps all D coarse Doppler bins from a single epoch."""
    roll = _roll_factory(nx, d)
    benchmark(roll)
    if benchmark.stats:
        mean = benchmark.stats["mean"]
        benchmark.extra_info["grid"] = label
        benchmark.extra_info["doppler_bins"] = d
        benchmark.extra_info["us_per_bin"] = mean / d * 1e6
        # one epoch of input (nx samples) covers all D coarse bins
        benchmark.extra_info["MSa_s"] = nx / mean / 1e6


@pytest.mark.parametrize("label,ny,nx,d", GRIDS)
def test_bench_column_tile(benchmark, label, ny, nx, d):
    """One mixer-bank channel: a Corr2D(ny, nx) tile (ny fine bins, ny epochs).

    The full fine bank over the same span is ``d`` of these; ``extra_info``
    records that implied bank cost next to the roll above.
    """
    rng = np.random.default_rng(0)
    ref = (
        rng.standard_normal((ny, nx)) + 1j * rng.standard_normal((ny, nx))
    ).astype(np.complex64)
    x = (
        rng.standard_normal((ny, nx)) + 1j * rng.standard_normal((ny, nx))
    ).astype(np.complex64)
    c = Corr2D(ref, 1)
    benchmark(c.execute, x)
    if benchmark.stats:
        mean = benchmark.stats["mean"]
        benchmark.extra_info["grid"] = label
        benchmark.extra_info["fine_bins_per_tile"] = ny
        benchmark.extra_info["MSa_s"] = ny * nx / mean / 1e6
        benchmark.extra_info["bank_ms_for_span"] = d * mean * 1e3
