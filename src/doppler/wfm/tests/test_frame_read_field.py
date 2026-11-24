"""A frame description reads a named field's received value back (#3309).

The receive mirror of ``add_field``: ``read_field`` and ``read_field_bits``
read a field from received bits at the place the description gives it,
most significant bit first, so a field reads back as the integer its bits
were given. Built here as a CCSDS TC transfer frame (232.0-B-4): the
primary header's fields by name, a data field and the FECF as the CRC
stage over both, so a header value is read by name with no parser.
"""

from __future__ import annotations

import numpy as np
import pytest

from doppler.wfm import STAGE_CRC16, FrameDesc


def _bits(value: int, n: int) -> np.ndarray:
    """``value`` as ``n`` bits, most significant first: the test's own
    expectation of the wire order, not the code under test's."""
    return np.array([(value >> (n - 1 - i)) & 1 for i in range(n)], np.uint8)


#: The primary header: (name, bits, value). 232.0-B-4 §4.1.2.
HEADER = [
    ("version", 2, 0),
    ("bypass", 1, 1),
    ("ctrl_cmd", 1, 0),
    ("spare", 2, 0),
    ("scid", 10, 42),
    ("vcid", 6, 1),
    ("length", 10, 5 + 3 + 2 - 1),
    ("seq", 8, 7),
]
DATA = np.array([0xCA, 0x1E, 0x5A], np.uint8)


@pytest.fixture
def frame() -> tuple[FrameDesc, np.ndarray]:
    d = FrameDesc()
    for name, n, value in HEADER:
        d.add_field(name, _bits(value, n))
    d.add_field("data", np.unpackbits(DATA))
    d.add_derived("fecf", 16)
    d.add_stage_over(STAGE_CRC16, "version", "fecf")
    d.build()
    return d, d.bits()


def test_every_header_field_reads_back_its_value(frame):
    """Rejects an LSB-first read (scid 42 would read 336) and an offset one
    bit off (each field takes its neighbour's bit)."""
    d, rx = frame
    for name, _, value in HEADER:
        assert d.read_field(rx, name) == value, name
    assert d.read_field(rx, "data") == 0xCA1E5A
    np.testing.assert_array_equal(
        d.read_field_bits(rx, "data"), np.unpackbits(DATA)
    )


def test_the_fecf_reads_as_received_not_recomputed(frame):
    """The trailer is read from the wire: a flipped trailer bit shows in the
    read while crc_ok fails. Rejects a read that recomputes the CRC."""
    d, rx = frame
    fecf = d.read_field(rx, "fecf")
    assert d.crc_ok(rx) == 1
    rx = rx.copy()
    rx[-1] ^= 1
    assert d.read_field(rx, "fecf") == fecf ^ 1
    assert d.crc_ok(rx) == 0
    # and a header field is still readable on the frame that failed
    assert d.read_field(rx, "scid") == 42


def test_a_refusal_says_why(frame):
    d, rx = frame
    with pytest.raises(ValueError, match="no field of that name"):
        d.read_field(rx, "nope")
    with pytest.raises(ValueError, match="no field of that name"):
        d.read_field_bits(rx, "nope")
    with pytest.raises(ValueError, match="fewer received bits"):
        d.read_field(rx[:-1], "scid")


def test_a_64_bit_field_with_its_top_bit_set_is_read_as_bits():
    """An int64 cannot hold it, so the integer form refuses and names the
    bits form, which reads it whole; never a truncated value."""
    d = FrameDesc()
    top = np.ones(64, np.uint8)
    d.add_field("wide", top)
    d.build()
    rx = d.bits()
    with pytest.raises(ValueError, match="read_field_bits"):
        d.read_field(rx, "wide")
    np.testing.assert_array_equal(d.read_field_bits(rx, "wide"), top)
