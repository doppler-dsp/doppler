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


# ── the SECOND grammar: jm's bit_pattern coercion on a Source ──────────────
#
# A composer bytes field with `coerce = "bit_pattern"` (just-makeit.toml:
# bits/payload, acq_code, data_code, sync) parses a `str` with jm's OWN
# grammar -- 0/1 or 0x hex -- not the Field's. It is kept because dropping it
# would also refuse numpy arrays and int sequences, and a literal Field is
# data. So the two grammars are pinned against each other: where they agree,
# and where they do not. The gaps are asserted as they are TODAY, so the jm
# fix (route a str through field_bits) turns them red on purpose -- and then
# the gap tests, and this second grammar, are deleted.

_JM1709 = (
    "just-makeit#1709: jm's coercion now agrees with field_bits here -- "
    "delete this gap test and the second grammar with it"
)


def _jm_grammar(spec: str) -> bool:
    """What jm's coercion reads: a 0/1 string, or 0x hex with no repeat."""
    body = spec[2:] if spec[:2].lower() == "0x" else spec
    return "*" not in spec and ":" not in spec and bool(body)


def _coerce(spec: str):
    from doppler.wfm import Segment

    return Segment(type="bits", payload=spec).bits


@pytest.mark.parametrize("spec", [s for s in VALID if _jm_grammar(s)])
def test_the_coercion_agrees_with_field_bits(spec):
    """Binary and hex: one text, the same bits through either door."""
    assert list(_coerce(spec)) == field_bits(spec).tolist()


@pytest.mark.parametrize("spec", [s for s in MALFORMED if s not in ("", "0x")])
def test_the_coercion_refuses_what_field_bits_refuses(spec):
    """The refusals the two grammars SHARE, pinned so neither drifts alone.

    If jm ever read "0102" as some pattern, the agreement tests above would
    stay green -- they only cover VALID text.
    """
    with pytest.raises(ValueError):
        _coerce(spec)


@pytest.mark.parametrize("spec", [s for s in VALID if not _jm_grammar(s)])
def test_gap_the_coercion_refuses_a_valid_field(spec):
    """A generated field and `*REPS` are Fields jm cannot read.

    `wfmgen --bits pn:31:5` and a scene's `"payload": "pn:31:5"` work;
    `Segment(payload="pn:31:5")` does not. Pass `field_bits(spec)` instead.
    """
    try:
        _coerce(spec)
    except ValueError:
        return
    pytest.fail(_JM1709)


@pytest.mark.parametrize("spec", ["", "0x"])
def test_gap_the_coercion_accepts_an_empty_field(spec):
    """`""` and a bare `0x` read as an absent pattern, not a typo."""
    with pytest.raises(ValueError):
        field_bits(spec)
    assert spec in MALFORMED
    try:
        got = _coerce(spec)
    except ValueError:
        pytest.fail(_JM1709)  # refused, as field_bits refuses: fixed
    assert got in (None, b""), "the coercion read an empty field as bits"


def test_a_field_past_the_bound_raises_not_a_numpy_error():
    # doppler#1622: 2^64 - 1 was cast to a negative dimension by the
    # self-sizing binding (just-makeit#1710) and surfaced as numpy's
    # "negative dimensions". The parser now refuses it, so the binding's
    # own refusal is what raises, naming the bound (just-makeit#1706).
    with pytest.raises(ValueError, match="Field bound") as e:
        field_bits("pn:18446744073709551615:5")
    assert "negative dimensions" not in str(e.value)
