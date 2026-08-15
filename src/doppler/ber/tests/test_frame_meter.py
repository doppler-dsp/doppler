"""`FrameMeter` from Python — the fourth metric's shipped face.

Goal 9 of `docs/design/rx-test.md`: internal and external use are the same
path. A caller measuring THEIR receiver against THEIR capture uses what we
use, so every measurement piece lives in the library with a binding — and a
binding nobody exercises from Python is a capability nobody is testing.

What is checked here is the binding and the counting rules ACROSS it, not the
arithmetic (`native/tests/test_frame_meter_core.c` pins that, by mutation).
The state round-trip is bespoke rather than an entry in
`src/doppler/tests/test_state_serialization.py`, because that matrix drives
block-`execute` objects and this one is a push accumulator — the same carve-out
`acq` gets.
"""

import pytest

from doppler.ber import FrameMeter


def test_create():
    obj = FrameMeter(target_errors=200, conf=0.99)
    assert obj is not None
    assert obj.frames == 0 and obj.errors == 0


def test_conf_outside_the_unit_interval_is_refused():
    """A confidence level is a probability, and the ctor says so.

    `create_error`/`create_error_message` turn the NULL into the exception the
    component actually meant, rather than a blanket MemoryError.
    """
    for bad in (1.0, 1.5, -0.1):
        with pytest.raises(ValueError):
            FrameMeter(target_errors=10, conf=bad)


def test_the_delivery_rules_cross_the_binding_intact():
    """Which outcomes count as errors, asserted from the caller's side.

    Each of these is a convention that fails silently: score only the frames
    you managed to find and the FER IMPROVES as the receiver gets worse at
    finding them; count an unprotected frame as an error and the number
    measures the frame format instead of the receiver.
    """
    m = FrameMeter(target_errors=10, conf=0.99)
    m.add(1, 1)  # found, checked        -> delivered
    m.add(1, -1)  # found, no CRC carried -> delivered
    m.add(1, 0)  # found, failed CRC     -> ERROR
    m.add(0, -1)  # never found           -> ERROR
    m.add(0, 1)  # never found; a CRC verdict cannot rescue it

    assert m.frames == 5
    assert m.sync_detected == 3
    assert m.crc_passed == 1
    assert m.errors == 3


def test_both_rates_come_back_as_intervals():
    """FER and sync-miss are rates WITH limits, and `lo` is what you assert.

    Comparing the lower limit against a spec is the form that cannot flake on
    counting noise; comparing `p_hat` will.
    """
    m = FrameMeter(target_errors=10, conf=0.99)
    for i in range(200):
        m.add(i % 10 != 0, 1 if i % 7 else 0)

    fer = m.fer()
    assert fer.errors == m.errors and fer.symbols == m.frames
    assert fer.lo <= fer.p_hat <= fer.hi
    assert fer.conf == 0.99

    miss = m.sync_miss()
    assert miss.errors == m.frames - m.sync_detected
    assert miss.symbols == m.frames
    # A sync miss is one WAY to lose a frame, so it cannot exceed the FER.
    assert miss.errors <= fer.errors


def test_enough_counts_errors_not_frames():
    """The stopping rule, which is what makes the interval the right one.

    `ber_confidence` is exact for inverse-binomial sampling -- fix the errors,
    let the trials fall out -- and its relative standard error is 1/sqrt(r),
    a function of the error count ALONE. A meter that stopped on a frame count
    would have precision that depended on the very rate it was measuring.
    """
    m = FrameMeter(target_errors=10, conf=0.99)
    for _ in range(9):
        m.add(1, 0)
    assert not m.enough
    m.add(1, 0)
    assert m.enough

    clean = FrameMeter(target_errors=10, conf=0.99)
    for _ in range(10_000):
        clean.add(1, 1)
    assert not clean.enough, "10 000 error-free frames is never 'enough'"


def test_reset_clears_the_counters_and_keeps_the_configuration():
    m = FrameMeter(target_errors=10, conf=0.95)
    for _ in range(5):
        m.add(0, -1)
    m.reset()
    assert (m.frames, m.errors, m.sync_detected, m.crc_passed) == (0, 0, 0, 0)
    m.add(1, 0)
    assert m.fer().conf == 0.95


def test_a_record_resumes_from_a_blob():
    """Split a record across processes and it must still be ONE measurement.

    That is what the state triplet is for -- checkpoint, migrate, scale -- and
    the accumulator is exactly the kind of long-running state that needs it.
    """
    m = FrameMeter(target_errors=50, conf=0.99)
    for i in range(137):
        m.add(i % 4 != 0, 1 if i % 6 else 0)

    blob = m.get_state()
    assert isinstance(blob, bytes) and len(blob) == m.state_bytes()

    resumed = FrameMeter(target_errors=50, conf=0.99)
    resumed.set_state(blob)
    assert (resumed.frames, resumed.errors) == (m.frames, m.errors)
    assert resumed.sync_detected == m.sync_detected
    assert resumed.crc_passed == m.crc_passed

    # And it keeps accumulating in step with the original.
    m.add(0, -1)
    resumed.add(0, -1)
    assert resumed.errors == m.errors
    assert resumed.fer().p_hat == m.fer().p_hat


def test_a_wrong_blob_is_rejected_rather_than_reinterpreted():
    """The envelope check, from Python. A blob that is not ours must RAISE.

    Silently reinterpreting one would resume a measurement from another
    object's counters, which is the sort of thing that reads as a receiver
    result.
    """
    m = FrameMeter(target_errors=10, conf=0.99)
    m.add(1, 0)
    blob = bytearray(m.get_state())
    blob[0] ^= 0xFF
    with pytest.raises(ValueError):
        m.set_state(bytes(blob))
    with pytest.raises(ValueError):
        m.set_state(b"")
    with pytest.raises(TypeError):
        m.set_state("not bytes")


def test_context_manager():
    with FrameMeter(target_errors=200, conf=0.99) as obj:
        obj.add(1, 1)
        assert obj.frames == 1


def test_destroy():
    obj = FrameMeter(target_errors=200, conf=0.99)
    obj.destroy()
