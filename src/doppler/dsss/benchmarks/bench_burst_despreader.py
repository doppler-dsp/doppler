"""Benchmark for BurstDespreader — the burst chain's TRACKED branch.

Run: pytest src/doppler/dsss/benchmarks/bench_burst_despreader.py
     --benchmark-only

The same two rows over the same 64k stimulus as the rest of the chain
(`_burst_stimulus.py`). This object is the alternative to `BurstDemod`:
it closes carrier and code loops across the burst instead of estimating
once and going open-loop, which is better when the burst is long enough to
converge and unavailable when it is not.

- ``steps_idle``   — 64k of noise. The loops run; nothing locks.
- ``steps_bursts`` — the same 64k carrying four bursts.

Both rows run the same per-sample loop arithmetic, so the interesting
comparison is not between them but against `bench_burst_demod.py` over the
identical block: closed-loop tracking against a one-shot estimate, priced.

`lock_metric` is asserted on the burst row and not on the idle one. The
header documents both ends -- ~1 locked, 2/pi = 0.6366 with no carrier --
and 64k of noise is not long enough for the unlocked end to settle, so
pinning it here would be pinning the noise draw rather than the object.
The certification measures both properly
(``src/doppler/dsss/tests/validation/burst_despreader/results.md``).
"""

import numpy as np
import pytest

from doppler.dsss import BurstDespreader
from doppler.dsss.benchmarks._burst_stimulus import (
    DATA_SF,
    REPS,
    SPC,
    burst_stimulus,
    rate,
)


@pytest.fixture(scope="module")
def waveform():
    return burst_stimulus()


def _despreader(acq_code, data_code):
    d = BurstDespreader(data_code, DATA_SF, SPC, 0.0, 0.0, 0.05, 0.01)
    d.set_acq(acq_code, REPS)
    return d


def test_bench_steps_idle(benchmark, waveform):
    """Noise in: the loops still run on every sample."""
    acq_code, data_code, _, _, idle = waveform
    # Rebuilt per round: the loops carry NCO phase and a code phase, so a
    # reused instance would start round two already dragged by round one.
    out = benchmark(lambda: _despreader(acq_code, data_code).steps(idle))
    assert np.asarray(out).size > 0, "the despreader emitted no prompts"
    rate(benchmark)


def test_bench_steps_bursts(benchmark, waveform):
    """Four bursts in the block — the same loop arithmetic, now locking."""
    acq_code, data_code, _, bursts, _ = waveform
    out = benchmark(lambda: _despreader(acq_code, data_code).steps(bursts))
    assert np.asarray(out).size > 0, "the despreader emitted no prompts"
    # Measured on a fresh instance rather than read off the benchmarked one,
    # whose final state belongs to whichever round pytest-benchmark ran last.
    d = _despreader(acq_code, data_code)
    d.steps(bursts)
    assert d.lock_metric > 2.0 / np.pi, (
        f"lock_metric {d.lock_metric:.3f} is at or below the no-carrier "
        "value 2/pi — this row is timing a despreader that never locked"
    )
    rate(benchmark)
