"""``doppler.wfm.field_bits`` -- the Field text form's door into Python.

The grammar is read by one C function and pinned there
(``native/tests/test_wfm_frame.c``); what is worth testing here is the
binding: that valid text comes back as the right bits, and that every
malformed spec RAISES rather than returning an empty array -- the
silent-gap shape just-makeit#1704 fixed for self-sizing functions.
"""

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


@pytest.mark.parametrize(
    "spec",
    [
        "",
        "pn::10",  # an empty field is not skipped
        "pn:12abc:5",  # a number is consumed whole
        "0102",  # a literal holds only 0 and 1
        "0x",  # no digits
        "pn:31:65",  # register past 64
        "pn:12:1",  # no m-sequence for a 1-bit register (doppler#1602)
        "data:1024",  # not supported until the data source exists
        "literal:0101",
    ],
)
def test_malformed_text_raises_never_returns_empty(spec):
    with pytest.raises(RuntimeError, match="failed"):
        field_bits(spec)
