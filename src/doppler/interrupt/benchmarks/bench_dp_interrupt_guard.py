"""What the Python face of a stoppable loop pays.

The C benchmark measures the primitive: ~0.2 ns for a bare volatile load,
~0.8 ns through the guard. Neither number is what a Python producer loop
experiences, because there the cost is the call itself, not the flag.

That is the question here. A loop written as ``while not it.interrupted():``
pays one extension call per block, and whether that matters depends
entirely on how much work a block is -- so the number to compare against
is the per-block work, not the C figure.

Run::

    pytest src/doppler/interrupt/benchmarks/ --benchmark-only
"""

import numpy as np
import pytest

from doppler.interrupt import Interrupt


@pytest.fixture
def guard():
    """A guard that arms nothing.

    Signal 0 cannot be installed, so the scaffold's ``np.zeros(1)`` would
    raise before the first measurement -- an empty array is the way to ask
    for a handle to the flag and no handlers.
    """
    return Interrupt(np.array([], dtype=np.int32))


def test_interrupted(benchmark, guard):
    """The per-iteration cost of being stoppable at all."""
    benchmark(guard.interrupted)


def test_interrupt_and_resume(benchmark, guard):
    """The stop itself, which happens once per run rather than per block."""

    def stop_and_clear():
        guard.interrupt()
        guard.resume()

    benchmark(stop_and_clear)


def test_arm_disarm(benchmark):
    """Constructing a guard: two sigaction calls, on the signal path."""
    import signal

    sigs = np.array([signal.SIGUSR1], dtype=np.int32)

    def cycle():
        with Interrupt(sigs):
            pass

    benchmark(cycle)
