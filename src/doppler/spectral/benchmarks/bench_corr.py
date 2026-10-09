"""Benchmark for Corr.

Run: pytest src/doppler/spectral/benchmarks/bench_corr.py --benchmark-only

``Corr.execute`` correlates ONE frame of exactly ``Corr.n`` samples per call
(the C ``n_in`` is documented "must equal state->n" and is not checked).
This benchmark used to build a 64-tap Corr and pass it 65 536 samples: the
kernel read the first 64 and ignored the rest, so a call that did one 64-point
FFT was credited with 65 536 samples and ``docs/benchmarks.md`` published
206 GSa/s (#1918). The frame here is as long as the block, so the samples a
call is credited with are the samples it correlated, and
``_assert_real_work`` proves the call did it: with the input equal to the
reference, the correlation is the reference's autocorrelation, whose peak is
at lag 0 and equals the reference's energy.
"""

import numpy as np
import pytest

from doppler.spectral import Corr

BLOCK_64K = 65_536
DWELL = 1  # every call is one complete correlation: one frame in, n out


def _reference(n=BLOCK_64K, seed=1):
    rng = np.random.default_rng(seed)
    return (rng.standard_normal(n) + 1j * rng.standard_normal(n)).astype(
        np.complex64
    )


def _assert_real_work(execute, ref):
    """Fail unless ``execute(ref)`` really correlated the frame.

    A speed figure that hides doing less is the defect this guards (#1918):
    an ``execute`` that returns at once, returns nothing, or looks at a
    prefix of the frame must not be timed. The check is the output itself:
    ``len(ref)`` samples, peak at lag 0, peak value = the reference's energy.
    """
    out = execute(ref)
    assert out is not None, "execute emitted nothing; it timed an early return"
    assert len(out) == len(ref), (len(out), len(ref))
    mag = np.abs(out)
    assert int(np.argmax(mag)) == 0, "correlation peak is not at lag 0"
    energy = float(np.sum(np.abs(ref.astype(np.complex128)) ** 2))
    assert abs(float(mag[0]) - energy) <= 1e-3 * energy, (mag[0], energy)


@pytest.fixture
def ref():
    return _reference()


@pytest.fixture
def obj(ref):
    return Corr(ref, DWELL)


def test_bench_execute_64k(benchmark, obj, ref):
    _assert_real_work(obj.execute, ref)
    benchmark(obj.execute, ref)
    if benchmark.stats:
        benchmark.extra_info["MSa_s"] = (
            len(ref) / benchmark.stats["mean"] / 1e6
        )


@pytest.mark.parametrize(
    "execute",
    [
        lambda x: None,  # returns at once
        lambda x: np.zeros(len(x), np.complex64),  # emits nothing real
        lambda x: np.ones(len(x) // 2, np.complex64),  # looks at a prefix
    ],
    ids=["returns-none", "returns-zeros", "returns-a-prefix"],
)
def test_the_check_refuses_work_that_was_not_done(execute, ref):
    with pytest.raises(AssertionError):
        _assert_real_work(execute, ref)


def test_the_check_accepts_the_real_kernel(ref):
    _assert_real_work(Corr(ref, DWELL).execute, ref)
