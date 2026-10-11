"""The sym_pos / nsps setters refuse what the kernel cannot index (#2142).

``set_nsps`` and ``set_sym_pos`` share one predicate in C: the hold is at
least 1, and ``0 <= sym_pos < nsps``. A refused value raises ``ValueError``
from the Python binding and leaves the synth unchanged. These tests drive
the binding directly, so they see the error path a Python caller sees.
"""

import pytest

from doppler.wfm.wfm import _SynthEngine


def _synth(sps: int = 8) -> _SynthEngine:
    return _SynthEngine(type="qpsk", sps=sps, snr=100.0)


def test_set_nsps_refuses_zero_and_negative_and_keeps_the_hold():
    e = _synth(sps=8)
    with pytest.raises(ValueError, match="nsps"):
        e.set_nsps(0)
    with pytest.raises(ValueError, match="nsps"):
        e.set_nsps(-4)
    assert e.get_nsps() == 8


def test_set_nsps_refuses_a_hold_at_or_below_sym_pos():
    e = _synth(sps=8)
    e.set_sym_pos(3)
    with pytest.raises(ValueError, match="sym_pos"):
        e.set_nsps(3)
    with pytest.raises(ValueError, match="sym_pos"):
        e.set_nsps(2)
    assert e.get_nsps() == 8
    assert e.get_sym_pos() == 3


def test_set_nsps_accepts_a_hold_above_sym_pos_and_set_sym_pos_first():
    e = _synth(sps=8)
    e.set_sym_pos(0)
    e.set_nsps(2)
    assert e.get_nsps() == 2
    assert e.get_sym_pos() == 0


def test_set_sym_pos_refuses_out_of_range_and_keeps_the_position():
    e = _synth(sps=8)
    e.set_sym_pos(3)
    with pytest.raises(ValueError, match="sym_pos"):
        e.set_sym_pos(-1)
    with pytest.raises(ValueError, match="sym_pos"):
        e.set_sym_pos(8)
    assert e.get_sym_pos() == 3


def test_set_sym_pos_accepts_the_last_index_of_the_hold():
    e = _synth(sps=8)
    e.set_sym_pos(7)
    assert e.get_sym_pos() == 7
