"""Benchmark for IMDMeasure — two-tone IMD, per capture.

Run: pytest src/doppler/measure/benchmarks/bench_imdmeas.py --benchmark-only

`analyze` is a whole-capture measurement: window, FFT, locate the two
tones, then search the 2nd- and 3rd-order product bins. So the row is one
capture per call, swept over capture size as the C benchmark
(``native/benchmarks/bench_imdmeas_core.c``) does, over the same stimulus;
the gap between the two faces is the binding's per-call overhead. `MSa_s`
is capture samples over the minimum time per capture.

The stimulus carries deliberate 3rd-order products, so the product search
finds real spurs rather than the noise floor -- which would time the same
FFT but a degenerate search.
"""

import numpy as np
import pytest

from doppler.measure import IMDMeasure
from doppler.source import LO

#: The C benchmark's sweep, so the two faces read against each other.
CAPTURES = (4_096, 16_384, 65_536)

F1, F2 = 0.101, 0.123
A_TONE, A_IM3 = 0.4, 0.002
#: Both IM3 products are planted at A_IM3, so IMD3 = 20*log10(A_IM3/A_TONE)
#: = -46.02 dBc by construction.
IMD3_DBC = 20.0 * np.log10(A_IM3 / A_TONE)


def _two_tone(n):
    """Two tones plus planted products at 2*f1-f2 and 2*f2-f1, built from
    `source.LO`'s sine rail."""
    x = (
        A_TONE * LO(F1).steps(n).imag
        + A_TONE * LO(F2).steps(n).imag
        + A_IM3 * LO(2 * F1 - F2).steps(n).imag
        + A_IM3 * LO(2 * F2 - F1).steps(n).imag
    )
    return x.astype(np.float32)


@pytest.mark.parametrize("n", CAPTURES)
def test_bench_analyze(benchmark, n):
    m = IMDMeasure(n=n, fs=1.0, full_scale=1.0, dynamic_range_db=90.0)
    x = _two_tone(n)

    r = benchmark(m.analyze, x)

    assert r.f1 == pytest.approx(F1, abs=1.0 / n)
    assert r.f2 == pytest.approx(F2, abs=1.0 / n)
    assert r.imd3_lo_freq == pytest.approx(2 * F1 - F2, abs=1.0 / n)
    assert r.imd3_hi_freq == pytest.approx(2 * F2 - F1, abs=1.0 / n)
    assert r.imd3_dbc == pytest.approx(IMD3_DBC, abs=0.5), (
        f"IMD3 {r.imd3_dbc:.2f} dBc, built as {IMD3_DBC:.2f} -- this row "
        "would be timing a failed analysis"
    )
    if benchmark.stats:
        # MIN, not mean: the least-disturbed observation, per
        # docs/dev/contributing/benchmarking.md.
        benchmark.extra_info["MSa_s"] = n / benchmark.stats["min"] / 1e6
