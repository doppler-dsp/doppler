"""`DsssBurstReceiver` — the Python face of the composed burst chain.

The C test (`native/tests/test_dsss_burst_receiver_core.c`) owns the
geometry derivations and the refine stage's behaviour; this file owns what
the binding exposes -- and one end-to-end decode, because "a burst goes in
and payload bits come out" is the claim a Python caller actually makes.

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


def test_silence_yields_no_burst() -> None:
    """Silence decodes nothing, and the buffer bound is block-independent.

    The control for the decode test below: a receiver that reported a
    burst for any input would pass that one.
    """
    r = _make()
    out = r.push(np.zeros(4096, np.complex64))
    assert isinstance(out, np.ndarray)
    assert out.size == 0
    assert r.n_bursts == 0
    # The bound scales with the input: push() returns EVERY burst the call
    # completed, so a constant payload_len would under-size the buffer the
    # moment one call carried two bursts (doppler#1008).
    assert r.push_max_out(1) >= PAYLOAD
    assert r.push_max_out(1 << 20) > r.push_max_out(1)


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


def _burst(f0: float = 0.0) -> tuple[np.ndarray, np.ndarray]:
    """One burst — preamble, then the spread sync|payload|CRC-16 frame.

    Built with `doppler.wfm.crc16`, the same C kernel the receiver checks
    on the way out, so the trailer is not a second implementation.
    """
    from doppler.wfm import crc16

    acq, data, sync = _codes()
    rng = np.random.default_rng(7)
    payload = rng.integers(0, 2, PAYLOAD).astype(np.uint8)
    c = crc16(payload)
    crc = np.array([(c >> (15 - j)) & 1 for j in range(16)], np.uint8)
    frame = np.concatenate([sync, payload, crc])

    def sgn(b):
        return np.where(np.asarray(b) & 1, -1.0, 1.0)

    chips = [np.tile(sgn(acq), REPS)] + [sgn(b) * sgn(data) for b in frame]
    bb = np.repeat(np.concatenate(chips), SPC)
    ph = np.exp(2j * np.pi * f0 * np.arange(len(bb)))
    return (bb * ph).astype(np.complex64), payload


def test_decodes_a_burst_and_publishes_the_event() -> None:
    """The end-to-end claim, through the binding.

    A burst placed at a known offset in a noise floor, streamed in small
    blocks, must come back as its payload bits with a valid CRC — and
    `preamble_start` must name the sample it actually began at. That
    field is the one a caller cannot compute, so it is the one worth
    asserting from Python rather than trusting the C test for.
    """
    burst, payload = _burst()
    at, n_cap = 5000, 40_000
    rng = np.random.default_rng(3)
    cap = (
        0.02 * (rng.standard_normal(n_cap) + 1j * rng.standard_normal(n_cap))
    ).astype(np.complex64)
    cap[at : at + burst.size] += burst

    r = _make()
    got = np.zeros(0, np.uint8)
    for off in range(0, n_cap, 777):
        got = r.push(cap[off : off + 777])
        if got.size:
            break

    assert got.size == PAYLOAD
    assert np.array_equal(got, payload)
    assert r.frame_valid == 1
    assert r.preamble_start == at
    assert r.n_bursts == 1
    assert r.dropped == 0
    # Resolved cleanly: the nearest rival period sits near (reps-1)/reps.
    assert 0.0 < r.refine_margin < 0.9


def test_the_event_survives_a_state_round_trip_mid_stream() -> None:
    """A receiver resumed from bytes still decodes the burst.

    The checkpoint lands mid-stream, before the burst has fully arrived,
    so the restored receiver has to carry the retained look-back with it
    — which is the half of the blob that would be easy to omit and
    impossible to notice on a fresh-instance round trip.
    """
    burst, payload = _burst()
    at, n_cap = 5000, 40_000
    rng = np.random.default_rng(3)
    cap = (
        0.02 * (rng.standard_normal(n_cap) + 1j * rng.standard_normal(n_cap))
    ).astype(np.complex64)
    cap[at : at + burst.size] += burst

    a = _make()
    cut = at + 800  # inside the preamble, before the frame has landed
    for off in range(0, cut, 777):
        assert a.push(cap[off : min(off + 777, cut)]).size == 0

    b = _make()
    b.set_state(a.get_state())

    got = np.zeros(0, np.uint8)
    for off in range(cut, n_cap, 777):
        got = b.push(cap[off : off + 777])
        if got.size:
            break

    assert got.size == PAYLOAD
    assert np.array_equal(got, payload)
    assert b.frame_valid == 1
    assert b.preamble_start == at
