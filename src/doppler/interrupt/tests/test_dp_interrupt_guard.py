"""The Python face of the interrupt facility.

The C tests (`native/tests/test_dp_interrupt_guard_core.c`) pin the
bookkeeping; these pin what the binding delivers of it, which is a
different claim. Both exist because the guard's logic used to live in the
binding, where only this half could reach it.
"""

from __future__ import annotations

import signal

import numpy as np
import pytest

from doppler.interrupt import Interrupt

#: A guard that arms nothing. `np.zeros(1)` -- the scaffold's default -- is
#: signal 0, which cannot be installed, so it raises rather than measuring
#: anything.
NO_SIGNALS = np.array([], dtype=np.int32)


def test_a_guard_that_arms_nothing_is_still_a_handle() -> None:
    it = Interrupt(NO_SIGNALS)
    assert it.interrupted() == 0
    it.interrupt()
    assert it.interrupted() != 0
    it.resume()
    assert it.interrupted() == 0


def test_arming_clears_a_stale_flag() -> None:
    """Or the first wait inside the block that just armed would refuse."""
    first = Interrupt(NO_SIGNALS)
    first.interrupt()
    assert first.interrupted() != 0

    second = Interrupt(NO_SIGNALS)
    assert second.interrupted() == 0


def test_two_guards_observe_one_flag() -> None:
    """Process-wide, and a reader would reasonably assume otherwise."""
    a, b = Interrupt(NO_SIGNALS), Interrupt(NO_SIGNALS)
    try:
        a.interrupt()
        assert b.interrupted() != 0
    finally:
        b.resume()


def test_context_manager_arms_and_restores() -> None:
    sig = np.array([signal.SIGUSR1], dtype=np.int32)
    before = signal.getsignal(signal.SIGUSR1)

    with Interrupt(sig) as it:
        assert it.interrupted() == 0
        signal.raise_signal(signal.SIGUSR1)
        assert it.interrupted() != 0

    assert signal.getsignal(signal.SIGUSR1) == before
    Interrupt(NO_SIGNALS)  # clears the flag for whatever runs next


def test_leaving_the_block_does_not_swallow_the_interrupt() -> None:
    """A caller that was interrupted still needs to see it afterwards."""
    outer = Interrupt(NO_SIGNALS)
    with Interrupt(NO_SIGNALS) as it:
        it.interrupt()
    assert outer.interrupted() != 0
    outer.resume()


def test_an_unarmable_signal_raises() -> None:
    """SIGKILL cannot be caught, and create says so rather than half-arming."""
    with pytest.raises(OSError):
        Interrupt(np.array([signal.SIGKILL], dtype=np.int32))


def test_more_signals_than_slots_is_refused() -> None:
    too_many = np.full(9, signal.SIGUSR1, dtype=np.int32)
    with pytest.raises(OSError):
        Interrupt(too_many)
