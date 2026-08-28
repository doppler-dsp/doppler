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
#: What push() returns per burst. The receiver stops at decisions, so the
#: frame comes back whole and the payload is a slice at SYNC_LEN
#: (doppler#1022) — `wfm.Frame.deframe()` is the shipped way to do that.
FRAME_SYMS = SYNC_LEN + PAYLOAD + 16


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
        "frame_syms": FRAME_SYMS,
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
        {"frame_syms": 0},
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
    # completed, so a constant frame_syms would under-size the buffer the
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

    # The FRAME comes back — the receiver stops at decisions, so the
    # payload is a slice and the CRC is the DeFramer's (doppler#1022).
    assert got.size == FRAME_SYMS
    assert np.array_equal(got[SYNC_LEN : SYNC_LEN + PAYLOAD], payload)
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

    assert got.size == FRAME_SYMS
    assert np.array_equal(got[SYNC_LEN : SYNC_LEN + PAYLOAD], payload)
    assert b.preamble_start == at


# ── the receiver stops at decisions; the frame is undone one layer up ──
#
# A receiver holds no description: it is told the sync word and how many
# symbols follow it, and it hands back those symbols as bits and as LLRs
# (doppler#1022). Whether they carry a CRC, a randomiser or an outer code —
# and whether any of it checked out — is `wfm.Frame`'s business.
#
# So these drive the two ends against each other through THREE objects:
# `wfm.Composer` generates, `DsssBurstReceiver` decides, `FrameDesc`
# deframes. A frame both halves agree on is the entire claim.

_TX_ACQ_BITS, _TX_DATA_BITS = 8, 5
_TX_REPS, _TX_SPC = 5, 2
_TX_SYNC = np.array([0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 1, 0], np.uint8)
_CRC_BITS = 16


def _mls(stages, seed):
    """An m-sequence from doppler's own PN generator, not a random array."""
    from doppler.wfm import PN

    n = 2**stages - 1
    return (
        np.asarray(PN(poly=0, seed=seed, length=stages).generate(n)) & 1
    ).astype(np.uint8)


def _link(payload_bits=96, *, crc=True, **stages):
    """`(capture, payload, receiver_kwargs)` for one generated burst.

    The stage kwargs go to the SCENE only. The receiver is told nothing
    about them — that is the split — so its `frame_syms` is the frame's
    length and nothing else.
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
        # A REAL noise floor: the CFAR reference reads the floor, so a clean
        # capture makes weak sidelobes cross the threshold and one burst
        # detect twice. 12 dB Es/N0 is what the rest of the burst suite uses.
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
        "crc": "crc16" if crc else "none",
    }
    seg.update(stages)
    # The frame's LENGTH is all the receiver is told, and every stage that
    # adds a field adds to it: an outer code's check symbols are on the wire
    # like everything else.
    frame_syms = len(_TX_SYNC) + payload_bits + (_CRC_BITS if crc else 0)
    frame_syms += 32 * 8 * int(stages.get("rs_depth", 0))
    rx_kw = {
        "acq_code": acq,
        "data_code": data,
        "sync": _TX_SYNC,
        "reps": _TX_REPS,
        "spc": _TX_SPC,
        "chip_rate": 1.0e6,
        "frame_syms": frame_syms,
        "cn0_dbhz": 60.0,
        "doppler_uncertainty": 0.0,
        "pfa": 1e-3,
        "pd": 0.9,
        "carrier_hz": 0.0,
        "max_rate": 0.0,
        "est_segments": 10,
    }
    return np.asarray(Composer([Segment(**seg)]).compose()), payload, rx_kw


def _deframer(payload_bits=96, *, crc=True, randomise=False, rs_depth=0):
    """The frame the transmitter built, described for the receive side.

    Field by field, because that is what a description IS — and the covers
    are DECLARED: a randomiser reaches over the payload group and not over
    the sync word, which a receiver correlates and must find unchanged.
    """
    from doppler.wfm import FrameDesc

    empty = np.empty(0, np.uint8)
    d = FrameDesc(empty, empty, empty)
    d.add_field(_TX_SYNC)
    d.add_field(
        np.zeros(payload_bits, np.uint8)
    )  # geometry; bits arrive later
    n_data = 1
    stage = 0
    if crc:
        d.add_field(empty, derived_by=stage + 1, derived_bits=_CRC_BITS)
        n_data += 1
    if rs_depth:
        d.add_field(
            empty, derived_by=2 if crc else 1, derived_bits=32 * rs_depth * 8
        )
        n_data += 1
    if crc:
        d.add_stage(0, first_field=1, n_fields=2)
        stage += 1
    if rs_depth:
        d.add_stage(1, first_field=1, n_fields=n_data, depth=rs_depth)
        stage += 1
    if randomise:
        d.add_stage(2, first_field=1, n_fields=n_data)
    d.build()
    return d


def test_the_receiver_returns_the_frame_and_the_deframer_reads_it() -> None:
    """The whole split, in one pass: decide, then deframe.

    `push()` returns `frame_syms` bits — the frame as received, sync word
    first — and `FrameDesc.deframe()` turns them into a payload and a
    verdict. Neither object knows the other's job.
    """
    cap, payload, rx_kw = _link()
    rx = DsssBurstReceiver(**rx_kw)
    bits = np.asarray(rx.push(cap))
    assert bits.size == rx_kw["frame_syms"], "one frame's worth of decisions"

    d = _deframer()
    got = np.asarray(d.deframe(bits))
    assert d.rx_checked == 1 and d.rx_ok == d.rx_units == 1, "the CRC ran"
    off = d.field_off(1)
    assert np.array_equal(got[off : off + payload.size], payload)


def test_a_frame_with_no_crc_carries_no_check_rather_than_a_failed_one():
    """`crc="none"` shortens the frame; nothing about it "fails".

    Measured before the receiver held a description at all: the payload came
    out bit-exact and the old `frame_valid` read 0, because the receiver
    measured
    the burst against a trailer the transmitter never sent. The receiver no
    longer has an opinion, and the DeFramer reports the honest one —
    `rx_checked == 0`, which is a different fact from a check that failed.
    """
    cap, payload, rx_kw = _link(crc=False)
    rx = DsssBurstReceiver(**rx_kw)
    bits = np.asarray(rx.push(cap))
    assert bits.size == len(_TX_SYNC) + payload.size

    d = _deframer(crc=False)
    got = np.asarray(d.deframe(bits))
    assert d.rx_checked == 0, "there is no check in this frame to run"
    assert d.rx_ok == 0, "...so nothing passed, either"
    off = d.field_off(1)
    assert np.array_equal(got[off : off + payload.size], payload)


def test_the_randomiser_round_trips_and_a_mismatch_is_caught() -> None:
    """A stage is only useful if both ends run it, and only safe if a
    mismatch is visible — and neither end is the receiver."""
    cap, payload, rx_kw = _link(randomise=1)
    bits = np.asarray(DsssBurstReceiver(**rx_kw).push(cap))

    matched = _deframer(randomise=True)
    got = np.asarray(matched.deframe(bits))
    off = matched.field_off(1)
    assert np.array_equal(got[off : off + payload.size], payload)
    assert matched.rx_ok == matched.rx_units == 2, "the CRC and the randomiser"

    # A description that does not derandomise gets noise-looking bits — and
    # the CRC says so, rather than the payload quietly being wrong.
    plain = _deframer()
    got = np.asarray(plain.deframe(bits))
    assert not np.array_equal(got[off : off + payload.size], payload)
    assert plain.rx_checked == 1 and plain.rx_ok == 0, (
        "the CRC ran, and failed"
    )


def test_an_outer_code_repairs_before_the_payload_is_read() -> None:
    """RS(255,223) corrects the frame, and the correction is the DeFramer's.

    Errors are injected by INVERTING whole data symbols, so their count is
    exact rather than a function of how the noise fell: eight bit errors,
    well inside a depth-1 codeword's 16-byte correction capacity.
    """
    n_pay = 223 * 8 - _CRC_BITS  # payload + CRC = one RS codeword's data
    errs = [20, 40, 60, 80, 100, 120, 140, 160]

    def corrupt(cap):
        sym = (2**_TX_DATA_BITS - 1) * _TX_SPC
        pre = _TX_REPS * (2**_TX_ACQ_BITS - 1) * _TX_SPC
        out = cap.copy()
        for k in errs:
            out[pre + k * sym : pre + (k + 1) * sym] *= -1
        return out

    cap, payload, rx_kw = _link(payload_bits=n_pay)
    bits = np.asarray(DsssBurstReceiver(**rx_kw).push(corrupt(cap)))
    d = _deframer(payload_bits=n_pay)
    got = np.asarray(d.deframe(bits))
    off = d.field_off(1)
    assert int(np.sum(got[off : off + n_pay] != payload)) == len(errs), (
        "without an outer code the injected errors reach the payload"
    )

    cap, payload, rx_kw = _link(payload_bits=n_pay, rs_depth=1)
    bits = np.asarray(DsssBurstReceiver(**rx_kw).push(corrupt(cap)))
    d = _deframer(payload_bits=n_pay, rs_depth=1)
    got = np.asarray(d.deframe(bits))
    off = d.field_off(1)
    assert np.array_equal(got[off : off + n_pay], payload), "RS repaired it"
    assert d.rx_ok == d.rx_units, "and every check came out good"


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
    cap, _payload, rx_kw = _link()
    rx = DsssBurstReceiver(**rx_kw)
    bits = np.asarray(rx.push(cap))
    llr = np.asarray(rx.llrs(rx.llrs_max_out(1)))

    assert llr.size == rx_kw["frame_syms"], "one LLR per frame symbol"
    # `push()` returns the same decisions as bits, so the two faces of one
    # rule must agree symbol for symbol, sync word included.
    hard = (llr < 0).astype(np.uint8)
    assert np.array_equal(hard, bits)
    assert np.array_equal(hard[: len(_TX_SYNC)], _TX_SYNC)


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
        lo = len(_TX_SYNC)
        assert np.array_equal(bits[lo : lo + payload.size], payload), (
            f"the burst must decode at {esno} dB for its LLRs to mean anything"
        )
        llr = np.asarray(rx.llrs(rx.llrs_max_out(1)))
        mags.append(float(np.mean(np.abs(llr))))

    for lo, hi in zip(mags, mags[1:]):
        assert 2.5 < hi / lo < 6.0, (
            f"6 dB should scale the LLRs by about 4x, measured {hi / lo:.2f}x "
            f"({mags})"
        )
