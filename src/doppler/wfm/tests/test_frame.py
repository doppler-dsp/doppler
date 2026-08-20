"""``doppler.wfm.Frame`` — the frame descriptor, reachable from Python.

The *measurement* half of the frame story shipped first (``wfm_frame_t`` in C,
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

The empty-array convention is deliberate and is ``wfm_seq_t``'s own: a field
with zero length is absent. See ``docs/design/rx-test.md`` section 7.
"""

import numpy as np
import pytest

from doppler.ber import FrameMeter
from doppler.wfm import (
    Composer,
    Frame,
    FrameDesc,
    Segment,
    ccsds_asm_bits,
    crc16,
)

EMPTY = np.empty(0, np.uint8)
# Barker-13 — the sync word the named RX_FRAME_BURST carries.
SYNC = np.array([1, 1, 1, 1, 1, 0, 0, 1, 1, 0, 1, 0, 1], np.uint8)
PAYLOAD = np.array([0, 1, 1, 0, 1, 0, 0, 1, 1, 1, 0, 0, 0, 1, 0, 1], np.uint8)
ACQ = np.array([1, 0, 1, 0, 1, 0, 1, 0], np.uint8)
REPS = 4


def _frame(crc="crc16"):
    """The reference descriptor, as a ``Frame``."""
    return Frame(ACQ, SYNC, PAYLOAD, preamble_reps=REPS, crc=crc)


# ── geometry, delegated ─────────────────────────────────────────────────────


def test_nbits_and_layout_are_the_descriptors_own():
    f = _frame()
    assert f.nbits == REPS * len(ACQ) + len(SYNC) + len(PAYLOAD) + 16

    lay = f.layout()
    assert lay.preamble_off == 0
    assert lay.preamble_bits == REPS * len(ACQ)
    assert lay.sync_off == lay.preamble_bits
    assert lay.sync_bits == len(SYNC)
    assert lay.payload_off == lay.sync_off + lay.sync_bits
    assert lay.payload_bits == len(PAYLOAD)
    assert lay.crc_off == lay.payload_off + lay.payload_bits
    assert lay.crc_bits == 16
    assert lay.total_bits == f.nbits


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
    f = Frame(EMPTY, SYNC, EMPTY, crc="crc16")
    assert f.nbits == len(SYNC)
    assert f.layout().crc_bits == 0


# ── the truth-free check ────────────────────────────────────────────────────


def test_crc_ok_passes_its_own_bits_and_fails_one_flipped_one():
    f = _frame()
    b = f.bits()
    assert f.crc_ok(b) == 1

    bad = b.copy()
    bad[f.layout().payload_off] ^= 1
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


def test_a_generated_payload_needs_no_array():
    """A handful of numbers a receiver can regenerate, instead of an array."""
    f = Frame(
        EMPTY,
        SYNC,
        EMPTY,
        payload_kind="pn",
        payload_nbits=1024,
        payload_reg_bits=10,
        crc="crc16",
    )
    assert f.nbits == len(SYNC) + 1024 + 16
    assert f.crc_ok(f.bits()) == 1


def test_a_dotted_preamble_starts_high():
    """1010… — so a one-bit field is not silently indistinguishable from
    zeros.
    """
    f = Frame(
        EMPTY,
        SYNC,
        PAYLOAD,
        preamble_kind="dotted",
        preamble_nbits=4,
        preamble_reps=1,
        crc="none",
    )
    assert f.bits()[:4].tolist() == [1, 0, 1, 0]


# ── refusals, at construction ───────────────────────────────────────────────


@pytest.mark.parametrize(
    "kwargs",
    [
        pytest.param({}, id="an empty geometry is not a frame"),
        pytest.param(
            {"payload_kind": "pn", "payload_nbits": 64},
            id="a PN field with no register width cannot be built",
        ),
    ],
)
def test_an_unbuildable_descriptor_raises(kwargs):
    with pytest.raises(ValueError):
        Frame(EMPTY, EMPTY, EMPTY, **kwargs)


def test_an_unknown_kind_names_the_choices():
    with pytest.raises(ValueError, match="literal"):
        Frame(EMPTY, SYNC, PAYLOAD, sync_kind="barker")


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
    corrupt[f.layout().payload_off + 2] ^= 1

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
# `Frame` names four fields. `FrameDesc` takes the SAME arguments and stops
# before materialising, so the four are a starting point a caller extends.
# That is what lets Python describe a frame doppler has never heard of --
# including a CCSDS CADU, whose coding has no binding of its own and would
# otherwise be unreachable from here.


def test_framedesc_is_the_same_frame_deferred():
    """The two constructors differ in WHEN, not in what they produce."""
    f = _frame()
    d = FrameDesc(ACQ, SYNC, PAYLOAD, preamble_reps=REPS, crc="crc16")
    d.build()

    assert d.nbits == f.nbits
    assert np.array_equal(np.asarray(d.bits(1)), np.asarray(f.bits(1)))
    assert d.crc_ok(d.bits(1)) == 1


def test_the_indexed_view_reads_a_configured_frame_too():
    """`wfm_frame_t` IS a configuration, so the general accessors reach it.

    Checked against the NAMED view rather than against literals: the two are
    the same layout read two ways, and a description that disagreed with the
    struct it was built from would be two descriptors again.
    """
    f = _frame()
    lay = f.layout()

    assert f.n_fields() == 4
    assert f.n_stages() == 1
    assert (f.field_off(0), f.field_bits(0)) == (
        lay.preamble_off,
        lay.preamble_bits,
    )
    assert (f.field_off(2), f.field_bits(2)) == (
        lay.payload_off,
        lay.payload_bits,
    )
    assert (f.field_off(3), f.field_bits(3)) == (lay.crc_off, lay.crc_bits)
    # The CRC stage covers the payload AND the trailer it derives -- the rule
    # that lets one kernel signature serve every check-symbol stage.
    assert f.stage_first(0) == lay.payload_off
    assert f.stage_bits(0) == lay.payload_bits + lay.crc_bits


def test_an_empty_description_starts_empty_and_refuses_to_build():
    """Empty arrays begin from nothing, and nothing is not a frame."""
    d = FrameDesc(EMPTY, EMPTY, EMPTY)
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

    The bits are checked byte-for-byte against `ccsds_tm_frame_encode` in
    `native/tests/test_frame_core.c`, where both sides are reachable. What is
    checked here is that the description survives the binding.
    """
    K, N, E2, DEPTH = 223, 255, 32, 2
    _CRC16, RS, RANDOMISE, CONV = 0, 1, 2, 3

    octets = np.array(
        [(i * 29 + 5) & 0xFF for i in range(K * DEPTH)], np.uint8
    )
    fbits = np.unpackbits(octets).astype(np.uint8)
    # Not a transcription of 0x1ACFFC1D: a test that spells the marker out
    # itself agrees with a receiver that spells it out the same wrong way.
    asm_bits = ccsds_asm_bits()

    d = FrameDesc(EMPTY, EMPTY, EMPTY)
    assert d.add_field(asm_bits) == 0
    assert d.add_field(fbits) == 1
    assert d.add_field(EMPTY, derived_by=1, derived_bits=E2 * DEPTH * 8) == 2
    assert d.add_stage(RS, first_field=1, n_fields=2, depth=DEPTH) == 0
    assert d.add_stage(RANDOMISE, first_field=1, n_fields=2) == 1
    assert (
        d.add_stage(CONV, first_field=0, n_fields=3, emit_num=2, emit_den=1)
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
    d = FrameDesc(EMPTY, SYNC, PAYLOAD, crc="crc16")
    d.build()
    assert d.add_field(PAYLOAD) == -1
    assert d.add_stage(0, first_field=0, n_fields=1) == -1
    with pytest.raises(ValueError):
        d.build()


# ── the scoring path: what a coded frame reports, and why it beats a CRC ────


def _cadu(depth=5):
    """A CCSDS codeblock behind a marker, described field by field.

    No inner code: a frame checker begins after the Viterbi and after frame
    synchronisation, which is where `check()` begins too.
    """
    K, E2 = 223, 32
    RS, RANDOMISE = 1, 2
    octets = np.array(
        [(i * 37 + 11) & 0xFF for i in range(K * depth)], np.uint8
    )
    fbits = np.unpackbits(octets).astype(np.uint8)
    asm_bits = ccsds_asm_bits()

    d = FrameDesc(EMPTY, EMPTY, EMPTY)
    d.add_field(asm_bits)
    d.add_field(fbits)
    d.add_field(EMPTY, derived_by=1, derived_bits=E2 * depth * 8)
    d.add_stage(RS, first_field=1, n_fields=2, depth=depth)
    d.add_stage(RANDOMISE, first_field=1, n_fields=2)
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
    d = FrameDesc(EMPTY, SYNC, PAYLOAD, crc="none")
    d.build()
    r = d.check(d.bits(1))
    assert r.checked == 0
    assert r.units == 0
