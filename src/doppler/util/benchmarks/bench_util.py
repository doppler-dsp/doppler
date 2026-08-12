"""Benchmarks for the EMA primitive's Python face.

Run: pytest src/doppler/util/benchmarks/bench_util.py --benchmark-only

**What this can and cannot tell you.** A single `ema_step` call is three
flops; the C benchmark (`native/benchmarks/bench_util_core.c`) measures it
at ~1.8 ns. Anything measured here is therefore dominated by the CPython
call boundary, not by the arithmetic — which is exactly the useful
finding: it is the number that says *do not drive an EMA one sample at a
time from Python*. The comparison against a pure-Python expression makes
that explicit rather than leaving a reader to infer it from an absolute
number.

The real per-sample EMA work happens inside `agc_steps`, `acc_trace` and
the DSSS lock detector, all of which loop in C over a whole block; their
throughput is benchmarked with their own objects.
"""

import pytest

from doppler.util import ema_alpha_decim, ema_step

ALPHA = 0.05


def test_ema_step_binding(benchmark):
    """One binding call — dominated by the CPython boundary."""
    benchmark(ema_step, 1.0, 2.0, ALPHA)


def test_ema_step_pure_python(benchmark):
    """The same arithmetic without crossing the boundary.

    The gap between this and the binding is the FFI cost, and it is the
    reason the library never calls this per sample from Python.
    """

    def step(state=1.0, x=2.0, a=ALPHA):
        return state + a * (x - state)

    benchmark(step)


def test_ema_alpha_decim_binding(benchmark):
    """The compounded pole: expm1/log1p, computed once per chunk in C."""
    benchmark(ema_alpha_decim, ALPHA, 8)


@pytest.mark.parametrize("n", [1_000, 100_000])
def test_ema_block_python_loop(benchmark, n):
    """A whole block driven from Python, the way a user might try it.

    Present so the docs can point at a number instead of an assertion:
    this is what the C objects avoid by looping internally.
    """
    xs = [float(i % 8) for i in range(n)]

    def run():
        s = 0.0
        for x in xs:
            s = ema_step(s, x, ALPHA)
        return s

    benchmark(run)
