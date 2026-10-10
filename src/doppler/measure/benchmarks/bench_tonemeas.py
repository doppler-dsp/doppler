"""Benchmark for ToneMeasure — single-tone ADC metrics, per capture.

Run: pytest src/doppler/measure/benchmarks/bench_tonemeas.py --benchmark-only

`analyze` is a whole-capture measurement, not a streaming one: window,
FFT, find the fundamental, sum the harmonics, sum the rest. So the row is
one capture per call, swept over capture size -- the knob a bench operator
turns for a lower noise floor, and the one whose price they want to know.
`MSa_s` is capture samples over the minimum time per capture.

`analyze_complex` is measured beside `analyze` because it is a different
transform, not a wrapper: a real capture folds, a complex one does not.

The C benchmark (``native/benchmarks/bench_tonemeas_core.c``) sweeps the
same three sizes over the same stimulus; the gap between the two faces is
the binding's per-call overhead, which is what a Python benchmark is for.
"""

import numpy as np
import pytest

from doppler.measure import ToneMeasure
from doppler.source import LO

#: The C benchmark's sweep, so the two faces read against each other.
CAPTURES = (4_096, 16_384, 65_536)

#: Tone and harmonic amplitudes. The 2nd harmonic is the largest spur, so
#: SFDR is 20*log10(A1/A2) = 47.96 dBc by construction -- a known answer
#: the row must reproduce, or it timed a failed analysis.
F0 = 0.113
A1, A2, A3 = 0.5, 0.002, 0.001
SFDR_DBC = 20.0 * np.log10(A1 / A2)


def _real_tone(n):
    """A tone near mid-band with a little harmonic content,
    so the harmonic search finds something rather than reading a floor.
    Built from `source.LO`'s quadrature output (its imaginary rail is a
    sine), not from numpy trigonometry."""
    x = (
        A1 * LO(F0).steps(n).imag
        + A2 * LO(2 * F0).steps(n).imag
        + A3 * LO(3 * F0).steps(n).imag
    )
    return x.astype(np.float32)


def _meter(n):
    return ToneMeasure(
        n=n, fs=1.0, n_harmonics=8, full_scale=1.0, dynamic_range_db=90.0
    )


def _record(benchmark, n):
    if benchmark.stats:
        # MIN, not mean: the least-disturbed observation, per
        # docs/dev/contributing/benchmarking.md.
        benchmark.extra_info["MSa_s"] = n / benchmark.stats["min"] / 1e6


@pytest.mark.parametrize("n", CAPTURES)
def test_bench_analyze(benchmark, n):
    """Real capture: fold, harmonics, SFDR against the 2nd harmonic."""
    m = _meter(n)
    x = _real_tone(n)

    r = benchmark(m.analyze, x)

    assert r.fund_freq == pytest.approx(F0, abs=1.0 / n), (
        f"fundamental at {r.fund_freq}, not {F0}"
    )
    assert r.worst_spur_is_harm == 1, "the worst spur is the 2nd harmonic"
    assert r.worst_spur_freq == pytest.approx(2 * F0, abs=1.0 / n)
    assert r.sfdr_dbc == pytest.approx(SFDR_DBC, abs=0.5), (
        f"SFDR {r.sfdr_dbc:.2f} dBc, built as {SFDR_DBC:.2f} -- this row "
        "would be timing a failed analysis"
    )
    _record(benchmark, n)


@pytest.mark.parametrize("n", CAPTURES)
def test_bench_analyze_complex(benchmark, n):
    """Complex capture: no fold, and a pure tone, so no spur above the
    floor -- SFDR is bounded by the window's dynamic range instead."""
    m = _meter(n)
    x = (A1 * LO(F0).steps(n)).astype(np.complex64)

    r = benchmark(m.analyze_complex, x)

    assert r.fund_freq == pytest.approx(F0, abs=1.0 / n)
    assert r.fund_dbfs == pytest.approx(20.0 * np.log10(A1), abs=0.1)
    assert r.sfdr_dbc > 80.0, (
        f"SFDR {r.sfdr_dbc:.1f} dBc on a pure tone -- a spur appeared"
    )
    _record(benchmark, n)
