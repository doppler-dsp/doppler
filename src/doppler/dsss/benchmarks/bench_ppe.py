"""Benchmark for PolynomialPhaseEstimator — the estimate BurstDemod drives.

Run: pytest src/doppler/dsss/benchmarks/bench_ppe.py --benchmark-only

The same 64k stimulus as the rest of the burst chain
(`_burst_stimulus.py`), so the estimator's share of `BurstDemod`'s number
is readable rather than inferred.

- ``estimate_idle``   — 64k of noise. The search runs in full.
- ``estimate_bursts`` — the same 64k carrying four bursts.

**The cost is identical on both, by construction**, and that is the
property worth pinning rather than a surprise: the estimator is a coherent
2-D matched-filter search over (chirp rate x frequency), so it dechirps and
transforms every hypothesis regardless of what is in the segment. There is
no early exit and no data-dependent branch — which is exactly why a
feedforward burst receiver can budget for it as a constant.

`max_rate = 0` collapses the rate axis to a single FFT (pure Doppler),
which is the configuration `DsssBurstReceiver` uses at this geometry. The
chirp-searching cost is linear in the hypothesis count; see
``docs/design/ppe.md``.
"""

import pytest

from doppler.dsss import PolynomialPhaseEstimator
from doppler.dsss.benchmarks._burst_stimulus import (
    BLOCK_64K,
    burst_stimulus,
    rate,
)


@pytest.fixture(scope="module")
def waveform():
    return burst_stimulus()


@pytest.fixture(scope="module")
def obj():
    # Stateless and by-value, so one instance serves every round: an
    # estimate depends only on the segment handed to that call.
    return PolynomialPhaseEstimator(BLOCK_64K, 0.0)


def test_bench_estimate_idle(benchmark, obj, waveform):
    """Noise in: the full (rate x frequency) search still runs."""
    *_, idle = waveform
    est = benchmark(lambda: obj.estimate(idle))
    assert -0.5 <= est.freq_norm < 0.5, (
        f"freq_norm {est.freq_norm} is outside the documented [-0.5, 0.5)"
    )
    assert est.rate_norm == 0.0, "max_rate=0 must force the rate to exactly 0"
    rate(benchmark)


def test_bench_estimate_bursts(benchmark, obj, waveform):
    """Four bursts in the block — the same search, same cost."""
    *_, bursts, _ = waveform
    est = benchmark(lambda: obj.estimate(bursts))
    assert -0.5 <= est.freq_norm < 0.5
    assert est.rate_norm == 0.0
    # A real signal must read above the noise-only surface, or the peak the
    # estimator found is not the signal's.
    assert est.snr_db > obj.estimate(waveform[4]).snr_db, (
        "the burst block's peak-to-mean is no higher than pure noise's"
    )
    rate(benchmark)
