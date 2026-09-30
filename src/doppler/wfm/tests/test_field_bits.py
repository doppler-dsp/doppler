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
    "data:1024",  # not supported until the data source exists
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


# ── the same grammar through a Source's bit_pattern coercion ───────────────
#
# A composer bytes field with `coerce = "bit_pattern"` (just-makeit.toml:
# bits/payload, acq_code, data_code, sync) reads a `str` through
# `coerce_str_fn = "dp_wfm_field_bits"` (just-makeit#1709): the Field parser
# above, not a grammar of jm's own. So the whole corpus is pinned through
# that door too -- every VALID spec gives field_bits' bits, and every
# MALFORMED spec is refused with field_bits' reason. A second grammar
# drifting back in would fail here on the first spec it read differently.


def _coerce(spec: str):
    from doppler.wfm import Segment

    return Segment(type="bits", payload=spec).bits


@pytest.mark.parametrize("spec", VALID)
def test_the_coercion_agrees_with_field_bits(spec):
    """One text, the same bits through either door -- generated kinds and
    `*REPS` included, which jm's own grammar refused before #1709."""
    assert list(_coerce(spec)) == field_bits(spec).tolist()


@pytest.mark.parametrize("spec", MALFORMED)
def test_the_coercion_refuses_what_field_bits_refuses(spec):
    """Every refusal, with the parser's own reason -- `""` and a bare `0x`
    included, which jm's own grammar read as an absent pattern."""
    with pytest.raises(ValueError, match=re.escape(REASON[spec])):
        _coerce(spec)


def test_a_field_past_the_bound_raises_not_a_numpy_error():
    # doppler#1622: 2^64 - 1 was cast to a negative dimension by the
    # self-sizing binding (just-makeit#1710) and surfaced as numpy's
    # "negative dimensions". The parser now refuses it, so the binding's
    # own refusal is what raises, naming the bound (just-makeit#1706).
    with pytest.raises(ValueError, match="Field bound") as e:
        field_bits("pn:18446744073709551615:5")
    assert "negative dimensions" not in str(e.value)
