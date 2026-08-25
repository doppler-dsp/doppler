"""`DsssBurstReceiver` — the Python face of the composed burst chain.

`push()` is not implemented yet (the three stages are the next commit), so
these pin the surface that IS real: construction and its refusals, the
derived read-backs, reset, and the state round-trip. The C test
(`native/tests/test_dsss_burst_receiver_core.c`) owns the geometry
derivations; this file owns what the binding exposes.

Deliberately not a copy of the C test. Testing at both layers is the
repo's rule, and the thing only reachable from here is the binding: the
`ValueError` a NULL `create()` becomes, the bytes interface, and the
`out=`/context-manager glue jm generates.
"""

from __future__ import annotations

import numpy as np
import pytest

from doppler.dsss import DsssBurstReceiver

ACQ_SF, DATA_SF, SYNC_LEN = 31, 8, 13
REPS, SPC, PAYLOAD = 4, 4, 32


def _codes():
    rng = np.random.default_rng(0)
    return (
        rng.integers(0, 2, ACQ_SF).astype(np.uint8),
        rng.integers(0, 2, DATA_SF).astype(np.uint8),
        rng.integers(0, 2, SYNC_LEN).astype(np.uint8),
    )


def _make(**kw):
    acq, data, sync = _codes()
    args = {
        "acq_code": acq,
        "data_code": data,
        "sync": sync,
        "reps": REPS,
        "spc": SPC,
        "chip_rate": 1.0e6,
        "payload_len": PAYLOAD,
        "cn0_dbhz": 50.0,
    }
    args.update(kw)
    return DsssBurstReceiver(**args)


def test_construct_and_read_back_the_event_fields() -> None:
    """Every event field is reachable and starts cleared.

    These are the record a consumer receives, so "the binding exposes
    them" is the claim -- a field present in C and missing from the
    binding would be a hand-off that cannot cross the boundary, which is
    the whole point of the object.
    """
    r = _make()
    assert r.preamble_start == 0
    assert r.frame_valid == 0
    assert r.doppler_hz_est == 0.0
    assert r.doppler_res_hz == 0.0
    assert r.cn0_dbhz_est == 0.0
    assert r.est_freq_hz == 0.0
    assert r.est_rate_hz == 0.0
    assert r.est_snr_db == 0.0
    assert r.refine_margin == 0.0
    assert r.pending == 0
    assert r.dropped == 0
    assert r.n_bursts == 0


@pytest.mark.parametrize(
    "bad",
    [
        {"acq_code": np.zeros(0, np.uint8)},
        {"data_code": np.zeros(0, np.uint8)},
        {"sync": np.zeros(0, np.uint8)},
        {"reps": 0},
        {"spc": 0},
        {"chip_rate": 0.0},
        {"payload_len": 0},
        {"pfa": 1.0},
        {"pd": 0.0},
    ],
)
def test_refuses_bad_arguments_as_value_error(bad: dict) -> None:
    """A rejected parameter set is a ValueError naming the constraint.

    Not a MemoryError: `create_error`/`create_error_message` in the
    manifest are what turn a NULL `create()` into the exception the
    component actually meant. Each case varies ONE field, so the raise is
    attributable.
    """
    with pytest.raises(ValueError):
        _make(**bad)


def test_a_valid_parameter_set_still_builds() -> None:
    """The control for the refusals above.

    Without it, a constructor that rejected everything would pass every
    one of the nine cases -- a reject test that cannot fail
    independently is decoration.
    """
    assert _make() is not None


def test_push_reports_no_burst_and_is_bounded() -> None:
    """`push()` is a declared no-op until the three stages land.

    Pinned rather than skipped so that implementing it has to change a
    test rather than quietly satisfy one.
    """
    r = _make()
    out = r.push(np.zeros(4096, np.complex64))
    assert isinstance(out, np.ndarray)
    assert out.size == 0
    assert r.n_bursts == 0
    # At most one burst per call, so the caller's buffer is the payload
    # length whatever the block size.
    assert r.push_max_out(1) == PAYLOAD
    assert r.push_max_out(1 << 20) == PAYLOAD


def test_reset_clears_the_event() -> None:
    """reset() returns the receiver to searching.

    Only the observable half is checkable from Python -- the read-backs
    are already zero on a fresh object, so this asserts reset is a legal
    no-op on a clean receiver rather than claiming to have proven it
    clears a dirty one. The C test owns that claim, where the state can
    be dirtied directly.
    """
    r = _make()
    r.push(np.zeros(1024, np.complex64))
    r.reset()
    assert r.frame_valid == 0
    assert r.pending == 0
    assert r.preamble_start == 0


def test_state_round_trips_through_bytes() -> None:
    """The bytes interface every stateful object speaks.

    Size, round-trip, and both rejects -- a short blob and a non-bytes
    argument. The receiver checkpoints BETWEEN bursts, so a fresh
    instance restoring a fresh blob is the case this can reach without
    `push()`.
    """
    r = _make()
    n = r.state_bytes()
    assert n > 0

    blob = r.get_state()
    assert isinstance(blob, bytes)
    assert len(blob) == n

    other = _make()
    other.set_state(blob)
    assert other.state_bytes() == n

    with pytest.raises(ValueError):
        other.set_state(blob[:-1])
    with pytest.raises(TypeError):
        other.set_state("not bytes")


def test_a_clobbered_envelope_is_rejected() -> None:
    """A wrong blob is refused, never reinterpreted.

    `dp_state_validate` is what every `set_state` opens with, and this is
    the check that proves it runs: flipping the magic's first byte must
    fail even though the length is right.
    """
    r = _make()
    blob = bytearray(r.get_state())
    blob[0] ^= 0xFF
    with pytest.raises(ValueError):
        r.set_state(bytes(blob))


def test_works_as_a_context_manager() -> None:
    """The generated `__enter__`/`__exit__` glue closes the receiver."""
    with _make() as r:
        assert r.push(np.zeros(64, np.complex64)).size == 0
