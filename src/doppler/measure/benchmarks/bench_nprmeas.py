"""Benchmark for NPRMeasure — notched-noise power ratio, per capture.

Run: pytest src/doppler/measure/benchmarks/bench_nprmeas.py --benchmark-only

`analyze` is a whole-capture measurement: window, FFT, then integrate the
active band and the notch (less the guard) and take their ratio. So the
row is one capture per call, swept over capture size as the C benchmark
(``native/benchmarks/bench_nprmeas_core.c``) does; the gap between the two
faces is the binding's per-call overhead. `MSa_s` is capture samples over
the minimum time per capture.

The stimulus is notched noise -- the signal NPR is defined on -- with a
notch of KNOWN depth, so the row asserts the ratio it measures. The C
benchmark feeds flat noise and only checks the result is finite; carving
the notch here costs nothing inside the timed region and turns "it
returned a number" into "it returned the right one".
"""

import numpy as np
import pytest

from doppler.measure import NPRMeasure
from doppler.source import AWGN
from doppler.spectral import FFT

#: The C benchmark's sweep, so the two faces read against each other.
CAPTURES = (4_096, 16_384, 65_536)

#: The C benchmark's geometry, in normalised frequency.
ACTIVE = (0.05, 0.45)
NOTCH = (0.20, 0.24)
GUARD = 0.005
#: The notch is carved this deep, so NPR reads it back.
DEPTH_DB = 60.0


def _notched_noise(n):
    """White noise from `source.AWGN`, band-limited and notched in the
    frequency domain with `spectral.FFT` (forward, mask, inverse) -- the
    same doppler-native construction as ``examples/measure_imd_npr_demo.py``.
    The mask is Hermitian-symmetric, so the real rail carries it."""
    k = np.arange(n)
    f = np.minimum(k, n - k) / n
    band = (f >= ACTIVE[0]) & (f <= ACTIVE[1])
    notch = (f >= NOTCH[0]) & (f <= NOTCH[1])
    mask = np.where(notch, 10.0 ** (-DEPTH_DB / 20.0), 1.0) * band
    spec = FFT(n, -1).execute_cf32(AWGN(1, 1.0).generate(n)) * mask
    x = FFT(n, 1).execute_cf32(spec.astype(np.complex64)).real
    return (0.3 * x / np.sqrt(np.mean(x * x))).astype(np.float32)


@pytest.mark.parametrize("n", CAPTURES)
def test_bench_analyze(benchmark, n):
    m = NPRMeasure(n=n, fs=1.0, full_scale=1.0, dynamic_range_db=90.0)
    x = _notched_noise(n)

    r = benchmark(m.analyze, x, *ACTIVE, *NOTCH, GUARD)

    assert r.n_inband_bins > 0 and r.n_notch_bins > 0
    assert r.npr_db == pytest.approx(DEPTH_DB, abs=3.0), (
        f"NPR {r.npr_db:.1f} dB on a {DEPTH_DB:.0f} dB notch -- this row "
        "would be timing a failed analysis"
    )
    if benchmark.stats:
        # MIN, not mean: the least-disturbed observation, per
        # docs/dev/contributing/benchmarking.md.
        benchmark.extra_info["MSa_s"] = n / benchmark.stats["min"] / 1e6
