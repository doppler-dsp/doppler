"""``doppler.wfm.field_bits`` -- the Field text form's door into Python.

The grammar is read by one C function and pinned there
(``native/tests/test_wfm_frame.c``); what is worth testing here is the
binding: that valid text comes back as the right bits, and that every
malformed spec RAISES rather than returning an empty array -- the
silent-gap shape just-makeit#1704 fixed for self-sizing functions.
"""

import re

import numpy as np
import pytest

from doppler.wfm import field_bits


def test_hex_is_msb_first_like_unpackbits():
    want = np.unpackbits(np.array([0x1A, 0xCF, 0xFC, 0x1D], np.uint8))
    got = field_bits("0x1ACFFC1D")
    assert got.dtype == np.uint8
    assert np.array_equal(got, want)


def test_binary_reads_left_to_right():
    assert field_bits("1101").tolist() == [1, 1, 0, 1]


def test_a_repeat_is_the_same_bits_again():
    one = field_bits("pn:31:5")
    four = field_bits("pn:31:5*4")
    assert len(four) == 4 * 31
    assert np.array_equal(four, np.tile(one, 4))


def test_dotted_starts_high():
    assert field_bits("dotted:5").tolist() == [1, 0, 1, 0, 1]


#: The corpus, shared by every test below so the parity test cannot drift
#: from what the grammar tests pin. VALID parses; MALFORMED must raise.
VALID = [
    "0x1ACFFC1D",
    "1101",
    "0101",
    "0xAA55",
    "0xA",
    "0X0F",  # the prefix is case-blind
    "pn:31:5",
    "pn:31:5*4",
    "0101*2",
    "dotted:5",
]
MALFORMED = [
    "",
    "pn::10",  # an empty field is not skipped
    "pn:12abc:5",  # a number is consumed whole
    "0102",  # a literal holds only 0 and 1
    "0x",  # no digits
    "pn:31:65",  # register past 64
    "pn:12:1",  # no m-sequence for a 1-bit register (doppler#1602)
    "data:1024",  # a data field has no bits of its own (they are its source's)
    "literal:0101",
]

#: A stable piece of the C parser's sentence for each MALFORMED spec: the
#: binding raises that sentence (just-makeit#1706), so a match proves the
#: REASON crossed the boundary, not only that something raised.
REASON = {
    "": "empty field",
    "pn::10": "LEN",
    "pn:12abc:5": "LEN",
    "0102": "binary literal",
    "0x": "hex literal",
    "pn:31:65": "REG",
    "pn:12:1": "POLY",
    "data:1024": "data source",
    "literal:0101": "literal:",
}


@pytest.mark.parametrize("spec", VALID)
def test_valid_text_parses(spec):
    assert field_bits(spec).size > 0


def test_every_malformed_spec_has_a_reason():
    assert set(REASON) == set(MALFORMED)


@pytest.mark.parametrize("spec", MALFORMED)
def test_malformed_text_raises_its_reason_never_returns_empty(spec):
    with pytest.raises(ValueError, match=re.escape(REASON[spec])):
        field_bits(spec)


# ── a source's bit field takes bits, never text ──────────────────────────────
#
# An object takes bits; module helpers make them (docs/design/
# frame-description.md, F.3). A composer source's bit fields (data, fill,
# acq_code, data_code; `sync` is retired, see test_frame_source.py)
# keep jm's `coerce = "bit_pattern"`, because
# without it jm refuses arrays, and name `coerce_str_fn =
# "dp_wfm_source_bits_refuse_text"`, which refuses every str with one reason
# pointing here. So text has one door, field_bits(), and the object one
# shape.

_REFUSED = re.escape("build them from text with field_bits()")

#: Every source bit field, and a kwarg that reaches it.
_FIELDS = ["data", "fill", "acq_code", "data_code"]


def _synth(**kw):
    from doppler.wfm import Synth

    return Synth(type="dsss", **kw)


@pytest.mark.parametrize("field", _FIELDS)
@pytest.mark.parametrize("spec", ["0101", "0xAA55", "pn:31:5", ""])
def test_a_source_bit_field_refuses_text_naming_field_bits(field, spec):
    """Valid Field text or not, a str is refused -- on the constructor."""
    with pytest.raises(ValueError, match=_REFUSED):
        _synth(**{field: spec})


@pytest.mark.parametrize("field", ["data", "acq_code", "data_code"])
def test_the_setter_refuses_text_and_keeps_the_bits(field):
    src = _synth(**{field: field_bits("0101")})
    with pytest.raises(ValueError, match=_REFUSED):
        setattr(src, field, "1100")
    assert list(getattr(src, field)) == [0, 1, 0, 1]


def test_a_segment_kwarg_refuses_text_too():
    from doppler.wfm import Segment

    with pytest.raises(ValueError, match=_REFUSED):
        Segment(type="bits", data="pn:31:5")


@pytest.mark.parametrize("spec", VALID)
def test_field_bits_output_is_what_a_source_takes(spec):
    """The door text goes through: its bits are taken as they are."""
    from doppler.wfm import Segment

    got = Segment(type="bits", data=field_bits(spec)).data
    assert list(got) == field_bits(spec).tolist()


def test_a_field_past_the_bound_raises_not_a_numpy_error():
    # doppler#1622: 2^64 - 1 was cast to a negative dimension by the
    # self-sizing binding (just-makeit#1710) and surfaced as numpy's
    # "negative dimensions". The parser now refuses it, so the binding's
    # own refusal is what raises, naming the bound (just-makeit#1706).
    with pytest.raises(ValueError, match="Field bound") as e:
        field_bits("pn:18446744073709551615:5")
    assert "negative dimensions" not in str(e.value)
