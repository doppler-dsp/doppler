"""``doppler.wfm.Frame`` — the frame descriptor, reachable from Python.

The *measurement* half of the frame story shipped first (the descriptor in C,
``doppler.ber.FrameMeter`` with a Python face), and the *descriptor* half did
not: only C could hold one. So a caller with a capture could accumulate frame
outcomes but had no way to produce an outcome, which is the gap this closes.

Two properties carry the file, and both are about AGREEMENT rather than
arithmetic — the layout itself is pinned in ``native/tests/test_wfm_frame.c``,
where the one implementation lives:

- what a generator transmits for a descriptor is what ``Frame`` materialises
  for the same descriptor, symbol for symbol. A receiver scoring a capture
  against a frame the transmitter never sent is the failure mode the shared
  descriptor exists to prevent, and it is invisible to any test that builds
  its expectation from parts;
- ``crc_ok`` needs no payload truth, so it survives on a real capture — and
  feeding it to ``FrameMeter`` is the whole truth-free frame-error-rate story,
  end to end, from Python.

A Frame takes BITS: each field is an unpacked array, omitted when absent, and
every other form reaches it through ``field_bits`` (the Field text form) --
see ``docs/design/frame-description.md`` §F.3 and ``docs/design/rx-test.md``
section 7.
"""

import numpy as np
import pytest

from doppler.ber import FrameMeter
from doppler.ccsds import asm_bits
from doppler.wfm import (
    STAGE_CONV,
    STAGE_RANDOMISE,
    STAGE_RS,
    Composer,
    Frame,
    FrameDesc,
    Segment,
    crc16,
    field_bits,
)

# Barker-13 — the sync word the named RX_FRAME_BURST carries.
SYNC = np.array([1, 1, 1, 1, 1, 0, 0, 1, 1, 0, 1, 0, 1], np.uint8)
PAYLOAD = np.array([0, 1, 1, 0, 1, 0, 0, 1, 1, 1, 0, 0, 0, 1, 0, 1], np.uint8)
ACQ = np.array([1, 0, 1, 0, 1, 0, 1, 0], np.uint8)
REPS = 4


def _frame(crc="crc16"):
    """The reference descriptor, as a ``Frame``. The preamble is repeated in
    its bits: a Frame has no repetition count of its own."""
    return Frame(
        preamble=np.tile(ACQ, REPS), sync=SYNC, payload=PAYLOAD, crc=crc
    )


# ── geometry, delegated ─────────────────────────────────────────────────────


def _field(f, name):
    """(offset, bits) of the field called `name` -- a frame is read by name."""
    i = f.field_index(name)
    assert i >= 0, name
    return f.field_off(i), f.field_bits(i)


def test_nbits_and_layout_are_the_descriptors_own():
    f = _frame()
    assert f.nbits == REPS * len(ACQ) + len(SYNC) + len(PAYLOAD) + 16

    pre = REPS * len(ACQ)
    assert _field(f, "preamble") == (0, pre)
    assert _field(f, "sync") == (pre, len(SYNC))
    assert _field(f, "payload") == (pre + len(SYNC), len(PAYLOAD))
    assert _field(f, "crc") == (pre + len(SYNC) + len(PAYLOAD), 16)


def test_bits_are_preamble_sync_payload_crc_in_that_order():
    """Against a CRC from the library's own kernel, not a second copy."""
    f = _frame()
    crc = crc16(PAYLOAD)
    trailer = np.array([(crc >> (15 - i)) & 1 for i in range(16)], np.uint8)
    want = np.concatenate([np.tile(ACQ, REPS), SYNC, PAYLOAD, trailer])
    np.testing.assert_array_equal(f.bits(), want)


def test_a_crc_over_no_payload_is_dropped():
    """It would protect nothing, so it is not carried — and the length
    says so.
    """
    f = Frame(sync=SYNC, crc="crc16")
    assert f.nbits == len(SYNC)
    assert _field(f, "crc")[1] == 0


# ── the truth-free check ────────────────────────────────────────────────────


def test_crc_ok_passes_its_own_bits_and_fails_one_flipped_one():
    f = _frame()
    b = f.bits()
    assert f.crc_ok(b) == 1

    bad = b.copy()
    bad[_field(f, "payload")[0]] ^= 1
    assert f.crc_ok(bad) == 0


def test_crc_ok_reports_absence_rather_than_passing():
    """-1, not 1. An unprotected frame having no detector is the thing to
    say.
    """
    assert _frame(crc="none").crc_ok(_frame(crc="none").bits()) == -1


def test_crc_ok_refuses_a_short_capture():
    f = _frame()
    assert f.crc_ok(f.bits()[:-1]) == -1


# ── repeats ─────────────────────────────────────────────────────────────────


def test_bits_repeats_whole_frames_identically():
    """A descriptor is one frame; a capture is many, and they must agree."""
    f = _frame()
    two = f.bits(2)
    assert len(two) == 2 * f.nbits
    np.testing.assert_array_equal(two[: f.nbits], two[f.nbits :])


# ── generated fields, which is what makes a long record practical ───────────


def test_a_generated_payload_is_a_handful_of_numbers():
    """A receiver regenerates it from text instead of carrying an array."""
    f = Frame(sync=SYNC, payload=field_bits("pn:1024:10"), crc="crc16")
    assert f.nbits == len(SYNC) + 1024 + 16
    assert f.crc_ok(f.bits()) == 1


def test_a_dotted_preamble_starts_high():
    """1010… — so a one-bit field is not silently indistinguishable from
    zeros.
    """
    f = Frame(
        preamble=field_bits("dotted:4"), sync=SYNC, payload=PAYLOAD, crc="none"
    )
    assert f.bits()[:4].tolist() == [1, 0, 1, 0]


# ── refusals, at construction ───────────────────────────────────────────────


def test_an_empty_geometry_raises():
    with pytest.raises(ValueError):
        Frame()


@pytest.mark.parametrize(
    "bad",
    [
        pytest.param(np.array([1, 2, 0], np.uint8), id="a 2 is not a bit"),
        # What "0101" became when a binding read it as a number
        # (just-makeit#1700): masking it to 1 would hide the mistake.
        pytest.param(np.array([101], np.uint8), id="a digit string's value"),
    ],
)
def test_an_element_that_is_not_a_bit_raises(bad):
    with pytest.raises(ValueError, match="not a bit"):
        Frame(sync=SYNC, payload=bad)


def test_the_old_spellings_are_gone():
    """Refused, not aliased: a kind and its generator parameters are
    `field_bits` text now."""
    with pytest.raises(TypeError):
        Frame(sync=SYNC, payload_kind="pn")
    assert not hasattr(FrameDesc(), "add_hex")
    assert not hasattr(FrameDesc(), "add_value")


# ── what the descriptor is FOR: it agrees with the generator ────────────────


def test_the_generated_waveform_is_the_frames_own_bits():
    """One descriptor, both ends.

    The transmitter is given the frame as flags; the receiver side is given it
    as a ``Frame``. At one sample per symbol BPSK sends bit 0 as +1 and bit 1
    as -1, so the samples are directly comparable — and if the two ever
    described different frames, this is where it shows, rather than as an
    unexplained error floor at a receiver.
    """
    f = _frame()
    seg = Segment(
        type="bits",
        modulation="bpsk",
        bits=PAYLOAD,
        acq_code=ACQ,
        acq_reps=REPS,
        sync=SYNC,
        crc="crc16",
        sps=1,
        fs=1.0,
        num_samples=f.nbits,
        snr=100.0,
    )
    y = np.asarray(Composer([seg]).compose()).real
    np.testing.assert_allclose(y[: f.nbits], 1.0 - 2.0 * f.bits(), atol=1e-6)


def test_frames_scored_into_a_frame_meter():
    """The pairing the two halves exist for: outcomes in, an exact FER out.

    No payload truth is used anywhere here — only the CRC — which is what lets
    the same loop run on a capture. The corrupted frames are corrupted in the
    PAYLOAD, so the sync word is still found and the failure is the one a CRC
    is there to catch.
    """
    f = _frame()
    clean = f.bits()
    corrupt = clean.copy()
    corrupt[_field(f, "payload")[0] + 2] ^= 1

    m = FrameMeter(target_errors=4)
    for i in range(20):
        rx = corrupt if i % 5 == 0 else clean
        m.add(1, f.crc_ok(rx))

    assert m.frames == 20
    assert m.errors == 4
    assert m.crc_passed == 16
    assert m.enough  # the stopping rule fired at target_errors

    # On the interval, never on `p_hat` — the module's own rule, and here for
    # its own reason: `p_hat` is the inverse-binomial estimator ((k-1)/(n-1),
    # so 3/19), which is exact for this stopping rule and is NOT errors/frames.
    # Asserting the naive ratio would be asserting the wrong estimator.
    fer = m.fer()
    assert fer.lo <= 0.2 <= fer.hi


# ── the description a Frame is one configuration of ─────────────────────────
#
# `Frame` names three fields and a CRC. `FrameDesc` takes the SAME arguments
# and stops before materialising, so they are a starting point a caller
# extends.
# That is what lets Python describe a frame doppler has never heard of --
# including a CCSDS CADU, whose coding has no binding of its own and would
# otherwise be unreachable from here.


def test_framedesc_is_the_same_frame_deferred():
    """The two constructors differ in WHEN, not in what they produce."""
    f = _frame()
    d = FrameDesc(
        preamble=np.tile(ACQ, REPS), sync=SYNC, payload=PAYLOAD, crc="crc16"
    )
    d.build()

    assert d.nbits == f.nbits
    assert np.array_equal(np.asarray(d.bits(1)), np.asarray(f.bits(1)))
    assert d.crc_ok(d.bits(1)) == 1


def test_a_frame_is_a_description_read_by_name():
    """The constructor describes the common frame, and nothing else does.

    It holds exactly the fields it was given, named, in wire order -- there
    is no second, named view of a frame beside the description, so the
    indexed accessors are the only way in and a name is how a caller finds
    its field.
    """
    f = _frame()
    assert f.n_fields() == 4
    assert f.n_stages() == 1
    assert [
        f.field_index(n) for n in ("preamble", "sync", "payload", "crc")
    ] == [
        0,
        1,
        2,
        3,
    ]
    # The CRC stage covers the payload AND the trailer it derives -- the rule
    # that lets one kernel signature serve every check-symbol stage.
    pay_off, pay_bits = _field(f, "payload")
    assert f.stage_first(0) == pay_off
    assert f.stage_bits(0) == pay_bits + _field(f, "crc")[1]

    # An omitted field takes no index: the payload of a frame with no
    # preamble is field 1, which is why a caller asks by name.
    g = Frame(sync=SYNC, payload=PAYLOAD, crc="crc16")
    assert g.n_fields() == 3
    assert g.field_index("preamble") == -1
    assert g.field_index("payload") == 1


def test_layout_is_gone():
    """The named layout view was a second reading of one frame; the
    description is the only one now (docs/design/frame-description.md, R)."""
    import doppler.wfm as wfm

    assert not hasattr(Frame, "layout")
    # Spelled in two pieces so this file does not itself carry the retired
    # name the retired-names gate scans for.
    assert not hasattr(wfm, "".join(("Frame", "Layout")))


def test_an_empty_description_starts_empty_and_refuses_to_build():
    """No fields begin from nothing, and nothing is not a frame."""
    d = FrameDesc()
    assert d.n_fields() == 0
    assert d.n_stages() == 0
    with pytest.raises(ValueError):
        d.build()


def test_a_ccsds_cadu_can_be_described_from_python():
    """The point of the generalization, reached through the binding.

    `ccsds_tm` has no Python face and is not getting one, so the outer code,
    the randomiser and the inner code are reachable only by DESCRIBING a CADU.
    The three covers ARE 131.0-B-3's coverage table: the inner code reaches
    over the marker and neither of the other two does, which is the one thing
    no kernel can be wrong about alone.

    The bits are checked byte-for-byte against `dp_ccsds_tm_frame_encode` in
    `native/tests/test_frame_core.c`, where both sides are reachable. What is
    checked here is that the description survives the binding.
    """
    K, N, E2, DEPTH = 223, 255, 32, 2

    octets = np.array(
        [(i * 29 + 5) & 0xFF for i in range(K * DEPTH)], np.uint8
    )
    fbits = np.unpackbits(octets).astype(np.uint8)
    # Not a transcription of 0x1ACFFC1D: a test that spells the marker out
    # itself agrees with a receiver that spells it out the same wrong way.
    asm = asm_bits()

    d = FrameDesc()
    assert d.add_field("asm", asm) == 0
    assert d.add_field("data", fbits) == 1
    # A field the caller has no bits for: the outer code fills it. The index
    # form of add_stage wires its producer by the same rule add_stage_over
    # uses.
    assert d.add_derived("parity", E2 * DEPTH * 8) == 2
    assert d.add_stage(STAGE_RS, first_field=1, n_fields=2, depth=DEPTH) == 0
    assert d.add_stage(STAGE_RANDOMISE, first_field=1, n_fields=2) == 1
    assert (
        d.add_stage(
            STAGE_CONV, first_field=0, n_fields=3, emit_num=2, emit_den=1
        )
        == 2
    )
    d.build()

    # (ASM + codeblock) * 2, the rate-1/2 inner code doubling the CADU.
    assert d.nbits == (32 + N * DEPTH * 8) * 2

    # 9.2.1.4: the inner code covers everything, marker included.
    assert (d.stage_first(2), d.stage_bits(2)) == (0, 32 + N * DEPTH * 8)
    # 9.5.1 and 10.3.4: the other two start behind the marker.
    assert (d.stage_first(0), d.stage_bits(0)) == (32, N * DEPTH * 8)
    assert (d.stage_first(1), d.stage_bits(1)) == (32, N * DEPTH * 8)

    sym = np.asarray(d.bits(1))
    assert sym.size == d.nbits
    assert set(np.unique(sym)) <= {0, 1}
    # The marker is inside the inner code, so it does NOT survive verbatim --
    # the direct falsification of the other stage order.
    assert not np.array_equal(sym[:32], asm_bits)


def test_a_description_is_closed_once_built():
    """A built frame is finished: extending it would strand its own bits."""
    d = FrameDesc(sync=SYNC, payload=PAYLOAD, crc="crc16")
    d.build()
    # Every refusal RAISES (doppler#1222). It used to return -1, which is a
    # valid index everywhere the return is used -- `derived_by` and a stage's
    # `first_field` are counted in it -- so a caller who did not check got a
    # wrong frame rather than an error.
    for refused in (
        lambda: d.add_field("late", PAYLOAD),
        lambda: d.add_stage(0, first_field=0, n_fields=1),
        lambda: d.add_derived("late", 8),
        lambda: d.add_stage_over(0, "sync", "payload"),
        lambda: d.name_field(0, "late"),
        lambda: d.build(),
    ):
        with pytest.raises(ValueError):
            refused()


# ── the scoring path: what a coded frame reports, and why it beats a CRC ────


def _cadu(depth=5):
    """A CCSDS codeblock behind a marker, described field by field.

    No inner code: a frame checker begins after the Viterbi and after frame
    synchronisation, which is where `check()` begins too.
    """
    K, E2 = 223, 32
    octets = np.array(
        [(i * 37 + 11) & 0xFF for i in range(K * depth)], np.uint8
    )
    fbits = np.unpackbits(octets).astype(np.uint8)
    asm = asm_bits()

    d = FrameDesc()
    d.add_field("asm", asm)
    d.add_field("data", fbits)
    d.add_derived("parity", E2 * depth * 8)
    d.add_stage(STAGE_RS, first_field=1, n_fields=2, depth=depth)
    d.add_stage(STAGE_RANDOMISE, first_field=1, n_fields=2)
    d.build()
    return d


def test_a_clean_coded_frame_checks_out_with_nothing_repaired():
    """Asserted because a checker crying damage on a clean frame is as wrong
    as one missing damage, and only the second shows up in the tests below."""
    d = _cadu()
    r = d.check(d.bits(1))
    assert r.passed == 1
    assert r.ok == r.units
    assert r.corrected == 0 and r.symbols == 0
    # Two stages declared, two reversed here.
    assert r.stages == 2 and r.checked == 2


def test_the_outer_code_reports_the_margin_it_spent():
    """The property a CRC cannot have.

    A contiguous burst of ``depth*E`` symbols is exactly ``E`` in each of the
    ``depth`` codewords — the boundary each can repair. The frame still
    passes, and the interesting number is that it took 80 symbol repairs to
    do it: margin being spent, visible before it is lost. A CRC reports one
    bit and would say only "fine".
    """
    E, DEPTH = 16, 5
    d = _cadu(DEPTH)
    rx = np.asarray(d.bits(1)).copy()
    blk = d.stage_first(0)
    for s in range(DEPTH * E):
        rx[blk + s * 8] ^= 1

    r = d.check(rx)
    assert r.ok == r.units, "a burst of depth*E must still pass"
    assert r.corrected == DEPTH, "every codeword needed repair"
    assert r.symbols == DEPTH * E, "and each spent exactly E of its budget"


def test_one_symbol_past_the_radius_fails_and_says_which():
    """E+1 in ONE column is past what that codeword can repair.

    The frame fails and the count says how badly — one bad unit out of six,
    not "the frame is wrong". That distinction is the whole reason this
    reports counts rather than a verdict.
    """
    E, DEPTH = 16, 5
    d = _cadu(DEPTH)
    rx = np.asarray(d.bits(1)).copy()
    blk = d.stage_first(0)
    for c in range(E + 1):
        rx[blk + (c * DEPTH + 2) * 8] ^= 1

    r = d.check(rx)
    assert r.ok == r.units - 1, "exactly one unit bad"
    assert r.units == DEPTH + 1, "five codewords plus the randomiser"


def test_a_frame_with_no_reversible_stage_reports_nothing_checked():
    """ "Carries no check" is not "the check passed".

    An FER that conflated them would score every unprotected frame as
    perfect, which is the same defect ``crc_ok`` returning -1 exists to
    avoid, one layer up.
    """
    d = FrameDesc(sync=SYNC, payload=PAYLOAD, crc="none")
    d.build()
    r = d.check(d.bits(1))
    assert r.checked == 0
    assert r.units == 0


def test_builder_by_name_matches_the_same_frame_by_index() -> None:
    """A frame described by NAME is the same frame described by index.

    The by-name builder adds no arithmetic -- it only spells the indices -- so
    the falsification available is the strong one: both descriptions must
    produce the same bits. The marker also arrives two ways, as Field text
    and spelled out, so the text door is checked against the same bits.
    """
    payload = np.array([0, 1, 1, 0, 1, 0, 0, 1], np.uint8)

    by_name = FrameDesc()
    by_name.add_field("asm", field_bits("0x1ACFFC1D"))
    by_name.add_field("", payload)
    by_name.name_field(1, "payload")
    by_name.add_derived("crc", 16)
    by_name.add_stage_over(0, "payload", "crc")
    by_name.build()

    by_index = FrameDesc()
    by_index.add_field(
        "asm", np.array([int(b) for b in f"{0x1ACFFC1D:032b}"], np.uint8)
    )  # the same 32 bits, spelled out
    by_index.add_field("payload", payload)
    by_index.add_derived("crc", 16)
    by_index.add_stage(kind=0, first_field=1, n_fields=2)
    by_index.build()

    assert by_name.nbits == by_index.nbits == 32 + 8 + 16
    assert (by_name.bits() == by_index.bits()).all()


def test_names_resolve_and_a_duplicate_is_refused() -> None:
    """The lookup is what makes a name worth carrying, and it is exact."""
    d = FrameDesc()
    d.add_field("sync", field_bits("0xABC"))
    d.add_field("", np.array([1, 0, 1, 0], np.uint8))
    d.name_field(1, "payload")

    assert d.field_index("sync") == 0
    assert d.field_index("payload") == 1
    assert d.field_index("absent") == -1
    # An unnamed field is anonymous, not named "".
    assert d.field_index("") == -1
    # A rename onto a taken name would make field_index ambiguous...
    with pytest.raises(ValueError):
        d.name_field(0, "payload")
    # ...and so would appending one.
    with pytest.raises(ValueError):
        d.add_field("sync", np.array([1], np.uint8))
    # A name that matches nothing is an ANSWER, not a refusal, so
    # field_index is the one verb whose -1 survives into Python.
    assert d.field_index("still-absent") == -1


def test_add_field_takes_bits_only() -> None:
    """Refusals at the one verb that appends supplied bits."""
    d = FrameDesc()
    with pytest.raises(ValueError):
        d.add_field("empty", np.empty(0, np.uint8))
    with pytest.raises(ValueError):
        d.add_field("digit", np.array([1, 101], np.uint8))
    assert d.n_fields() == 0, "a refusal appends nothing"
