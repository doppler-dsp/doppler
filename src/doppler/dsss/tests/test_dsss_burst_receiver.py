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


# ── the frame is a DESCRIPTION, and both ends read the same one ─────────
#
# The receiver used to assume `sync | payload | CRC-16` and nothing else.
# A burst generated without a CRC decoded bit-exactly and was reported
# INVALID; one carrying a randomiser or an outer code could not be
# described at all, and the transmitter could not emit one either
# (doppler#1017). These tests drive the two ends against each other --
# `wfm.Composer` generates, `DsssBurstReceiver` receives -- because a frame
# both halves agree on is the entire claim.

_TX_ACQ_BITS, _TX_DATA_BITS = 8, 5
_TX_REPS, _TX_SPC = 5, 2
_TX_SYNC = np.array([0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 1, 0], np.uint8)


def _mls(stages, seed):
    """An m-sequence from doppler's own PN generator, not a random array."""
    from doppler.wfm import PN

    n = 2**stages - 1
    return (
        np.asarray(PN(poly=0, seed=seed, length=stages).generate(n)) & 1
    ).astype(np.uint8)


def _link(payload_bits=96, **stages):
    """`(capture, payload, receiver_kwargs)` for one generated burst.

    The stage kwargs go to BOTH ends: to the scene as the generator spells
    them (`crc`, `randomise`, `attach_asm`, `rs_depth`) and to the receiver
    as it does. That is the point of the exercise -- one set of choices,
    two faces, and no third place where the frame's shape is written down.
    """
    from doppler.wfm import Composer, Segment

    acq, data = _mls(_TX_ACQ_BITS, 1), _mls(_TX_DATA_BITS, 3)
    payload = (
        np.random.default_rng(0).integers(0, 2, payload_bits).astype(np.uint8)
    )
    seg = {
        "type": "dsss",
        "fs": 1.0e6 * _TX_SPC,
        "freq": 0.0,
        # A REAL noise floor, not a clean one: the CFAR reference reads the
        # floor, so a noiseless capture makes weak sidelobes cross the
        # threshold and the same burst detect twice. 12 dB Es/N0 is what the
        # rest of the burst suite uses.
        "snr": 12.0,
        "snr_mode": "esno",
        "seed": 1,
        "sps": _TX_SPC,
        "acq_code": acq.tobytes(),
        "acq_reps": _TX_REPS,
        "data_code": data.tobytes(),
        "sync": _TX_SYNC.tobytes(),
        "payload": payload.tobytes(),
        "gap_noise": "auto",
        "off_samples": 200_000,  # room for the receiver's retain span
    }
    seg.update(stages)
    rx_kw = {
        "acq_code": acq,
        "data_code": data,
        "sync": _TX_SYNC,
        "reps": _TX_REPS,
        "spc": _TX_SPC,
        "chip_rate": 1.0e6,
        "payload_len": payload_bits,
        "cn0_dbhz": 60.0,
        "doppler_uncertainty": 0.0,
        "pfa": 1e-3,
        "pd": 0.9,
        "carrier_hz": 0.0,
        "max_rate": 0.0,
        "est_segments": 10,
    }
    return np.asarray(Composer([Segment(**seg)]).compose()), payload, rx_kw


def test_a_frame_with_no_crc_decodes_and_says_it_carries_no_check() -> None:
    """`crc=0` is a frame 16 bits shorter, not a frame that failed.

    Measured before this changed: the payload came out BIT-EXACT and
    `frame_valid` read 0, because the receiver measured the burst against a
    trailer the transmitter never sent. "Carries no check" and "the check
    failed" are different facts, and an FER conflating them scores every
    unprotected frame as an error.
    """
    cap, payload, rx_kw = _link(crc="none")
    rx = DsssBurstReceiver(**rx_kw, crc=0)
    bits = np.asarray(rx.push(cap))

    assert bits.size >= payload.size
    assert np.array_equal(bits[: payload.size], payload)
    assert rx.frame_checked == 0, "there is no check in this frame to run"
    assert rx.frame_valid == 0, "...so nothing passed, either"


def test_the_randomiser_round_trips_and_a_mismatch_is_caught() -> None:
    """A stage is only useful if both ends run it, and only safe if a
    mismatch is visible."""
    cap, payload, rx_kw = _link(randomise=1)

    matched = DsssBurstReceiver(**rx_kw, randomise=1)
    bits = np.asarray(matched.push(cap))
    assert np.array_equal(bits[: payload.size], payload)
    assert matched.frame_valid == 1
    assert matched.frame_checked == 2, "the CRC and the randomiser both ran"

    # The receiver that does not derandomise gets noise-looking bits — and
    # the CRC says so rather than the payload quietly being wrong.
    plain = DsssBurstReceiver(**rx_kw)
    bits = np.asarray(plain.push(cap))
    assert not np.array_equal(bits[: payload.size], payload)
    assert plain.frame_checked == 1, "the CRC ran"
    assert plain.frame_valid == 0, "...and failed, which is the point"


def test_an_outer_code_repairs_before_the_payload_is_read() -> None:
    """RS(255,223) corrects the frame, then the payload is read from it.

    Errors are injected by INVERTING whole data symbols, so their count is
    exact rather than a function of how the noise fell: eight bit errors,
    well inside a depth-1 codeword's 16-byte correction capacity.
    """
    n_pay = 223 * 8 - 16  # payload + CRC = exactly one RS codeword's data
    errs = [20, 40, 60, 80, 100, 120, 140, 160]

    def corrupt(cap, data_len):
        sym = data_len * _TX_SPC
        pre = _TX_REPS * (2**_TX_ACQ_BITS - 1) * _TX_SPC
        out = cap.copy()
        for k in errs:
            out[pre + k * sym : pre + (k + 1) * sym] *= -1
        return out

    data_len = 2**_TX_DATA_BITS - 1

    cap, payload, rx_kw = _link(payload_bits=n_pay)
    bits = np.asarray(DsssBurstReceiver(**rx_kw).push(corrupt(cap, data_len)))
    assert int(np.sum(bits[:n_pay] != payload)) == len(errs), (
        "without an outer code the injected errors reach the payload"
    )

    cap, payload, rx_kw = _link(payload_bits=n_pay, rs_depth=1)
    rx = DsssBurstReceiver(**rx_kw, rs_depth=1)
    bits = np.asarray(rx.push(corrupt(cap, data_len)))
    assert np.array_equal(bits[:n_pay], payload), "the outer code repaired it"
    assert rx.frame_valid == 1 and rx.frame_checked == 2


def test_the_marker_is_part_of_the_frame_both_ends_describe() -> None:
    """An ASM-carrying burst decodes, and only against a receiver told so.

    The marker is a field like any other: it lengthens the frame, so a
    receiver that does not know about it is looking for a shorter frame in
    the wrong place — which is exactly what a description prevents.
    """
    cap, payload, rx_kw = _link(attach_asm=1)

    told = DsssBurstReceiver(**rx_kw, attach_asm=1)
    bits = np.asarray(told.push(cap))
    assert np.array_equal(bits[: payload.size], payload)
    assert told.frame_valid == 1

    untold = DsssBurstReceiver(**rx_kw)
    out = np.asarray(untold.push(cap))
    assert not (
        out.size >= payload.size
        and np.array_equal(out[: payload.size], payload)
    ), "a receiver that does not know about the marker must not decode"


# ── the soft bits ───────────────────────────────────────────────────────
#
# `crealf(sym * derot)` IS the log-likelihood ratio up to a scale, and the
# demodulator computed it, sliced it to one bit and freed it on every burst
# (doppler#1018). A hard decision throws away roughly 2 dB of the coding
# gain a soft-input decoder exists to deliver.


def _link_at(esno_db, **stages):
    """`_link`, at a stated Es/N0 — the axis the LLR scale is measured on."""
    from doppler.wfm import Composer, Segment

    _cap, payload, rx_kw = _link(**stages)
    acq, data = _mls(_TX_ACQ_BITS, 1), _mls(_TX_DATA_BITS, 3)
    seg = {
        "type": "dsss",
        "fs": 1.0e6 * _TX_SPC,
        "freq": 0.0,
        "snr": esno_db,
        "snr_mode": "esno",
        "seed": 1,
        "sps": _TX_SPC,
        "acq_code": acq.tobytes(),
        "acq_reps": _TX_REPS,
        "data_code": data.tobytes(),
        "sync": _TX_SYNC.tobytes(),
        "payload": payload.tobytes(),
        "gap_noise": "auto",
        "off_samples": 200_000,
    }
    return np.asarray(Composer([Segment(**seg)]).compose()), payload, rx_kw


def test_the_soft_bits_are_the_hard_ones_seen_a_second_way() -> None:
    """`L < 0` reproduces `push()`'s bits exactly, over the whole frame.

    The repository has ONE decision rule (`mpsk_demap`'s); this is a second
    view of it, not a second copy, and the identity is asserted rather than
    assumed. The span is the FRAME, not the payload, because a code covers
    what its description says it covers.
    """
    cap, payload, rx_kw = _link()
    rx = DsssBurstReceiver(**rx_kw)
    bits = np.asarray(rx.push(cap))
    llr = np.asarray(rx.llrs(rx.llrs_max_out(1)))

    assert llr.size == len(_TX_SYNC) + payload.size + 16, (
        "one LLR per frame bit: sync, payload and the CRC trailer"
    )
    hard = (llr < 0).astype(np.uint8)
    lo = len(_TX_SYNC)
    assert np.array_equal(hard[lo : lo + payload.size], bits[: payload.size])
    # ...and the sync word's own bits are in there too, which is what makes
    # them usable by a decoder whose code covered them.
    assert np.array_equal(hard[:lo], _TX_SYNC)


def test_the_llr_scale_tracks_the_link() -> None:
    """Scaled by the burst's own noise estimate, so bursts are comparable.

    An LLR is `2*a*r/n0`, so its magnitude is proportional to the LINEAR
    SNR: every 6 dB should multiply it by about four. That is the property
    that makes combining across bursts meaningful, and a raw `Re(y)` (which
    a Viterbi would accept just as happily) does not have it.
    """
    mags = []
    for esno in (6.0, 12.0, 18.0):
        cap, payload, rx_kw = _link_at(esno)
        rx = DsssBurstReceiver(**rx_kw)
        bits = np.asarray(rx.push(cap))
        assert np.array_equal(bits[: payload.size], payload), (
            f"the burst must decode at {esno} dB for its LLRs to mean anything"
        )
        llr = np.asarray(rx.llrs(rx.llrs_max_out(1)))
        mags.append(float(np.mean(np.abs(llr))))

    for lo, hi in zip(mags, mags[1:]):
        assert 2.5 < hi / lo < 6.0, (
            f"6 dB should scale the LLRs by about 4x, measured {hi / lo:.2f}x "
            f"({mags})"
        )
