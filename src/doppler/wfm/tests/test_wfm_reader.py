"""Smoke tests for the `Reader` object binding itself.

jm scaffolds a `test_create()` here, but a Reader has no seedable constructor
(it needs a real capture on disk), so the generated version was skipped
outright. These replace it with the equivalent checks against a capture written
by `Writer` -- covering the four things the handle -> object migration had to
preserve, each of which needed a jm feature to survive:

  * an ``os.PathLike`` constructor argument   (jm gh-515)
  * a failed open that says what is wrong     (jm gh-514)
  * enum properties that return strings       (jm gh-519)
  * the class staying at ``doppler.wfm``      (jm gh-523)

The container-level behaviour (mode parsing, detached captures, keywords) is
covered in test_compose.py; this file is about the binding.
"""

import struct

import numpy as np
import pytest

from doppler.wfm import Composer, Reader, Segment, Writer


@pytest.fixture
def capture(tmp_path):
    """A small BLUE capture plus the samples that went into it."""
    x = Composer([Segment("qpsk", sps=8, num_samples=1024)]).compose()
    p = tmp_path / "cap.blue"
    with Writer(p, file_type="blue", sample_type="cf32", fs=2.4e6) as w:
        w.write(x)
    return p, x


def test_accepts_pathlike(capture):
    """The ctor takes a Path, not just a str (jm gh-515)."""
    p, x = capture
    with Reader(p) as r:  # a pathlib.Path, unstringified
        assert len(r.read(len(x))) == len(x)


def test_enum_properties_are_strings(capture):
    """file_type/sample_type/mode/endian decode via the SSOT (jm gh-519)."""
    p, _ = capture
    with Reader(p) as r:
        assert r.file_type == "blue"
        assert r.sample_type == "cf32"
        assert r.mode == "complex"
        assert r.endian == "le"


def test_failed_open_names_the_problem(tmp_path):
    """A NULL from create() raises the declared error (jm gh-514)."""
    with pytest.raises(ValueError, match="cannot open capture"):
        Reader(tmp_path / "nope.blue")


def test_read_reset_read_is_repeatable(capture):
    """reset() rewinds to the first sample, not to byte 0 of the file."""
    p, x = capture
    with Reader(p) as r:
        first = r.read(len(x))
        r.reset()
        second = r.read(len(x))
    # byte 0 would replay the 512-byte HCB as IQ; the payload starts at 512
    assert np.array_equal(first, x)
    assert np.array_equal(second, x)


def test_keywords_is_empty_without_an_extended_header(capture):
    """No extended header yields {}, never None — so a caller can just
    iterate it without a guard."""
    p, _ = capture
    with Reader(p) as r:
        assert r.keywords == {}


def _encode_keyword(tag: str, type_char: str, value_bytes: bytes) -> bytes:
    """One BLUE §3.3.1 keyword entry (little-endian), the inverse of the C
    encoder. lext is the NON-value length: 8-byte header + tag + padding."""
    ltag = len(tag)
    vbytes = len(value_bytes)
    n = 8 + vbytes + ltag
    lkey = n + ((8 - n % 8) % 8)  # pad to a multiple of 8
    lext = lkey - vbytes
    buf = bytearray(lkey)
    struct.pack_into("<i", buf, 0, lkey)
    struct.pack_into("<h", buf, 4, lext)
    buf[6] = ltag
    buf[7] = ord(type_char)
    buf[8 : 8 + vbytes] = value_bytes
    buf[8 + vbytes : 8 + vbytes + ltag] = tag.encode("ascii")
    return bytes(buf)


@pytest.fixture
def keyworded_capture(tmp_path):
    """A hand-built little-endian attached BLUE cf32 capture whose extended
    header carries one keyword of each Python-facing shape.

    The Python ``Writer`` cannot yet emit keywords (``wfm_writer_add_keyword``
    has no binding), and the C round-trip test never touches the Python value
    builder, so the ``.keywords`` type dispatch — new hand-written code behind
    ``value_type="object"`` — would otherwise ship unexercised from Python.
    The file is assembled directly against the Midas BLUE 1.1 wire format so
    the test needs no keyword-writing API.
    """
    fs = 1e6
    samples = np.array([1 + 2j, 3 + 4j, 5 + 6j, 7 + 8j], dtype=np.complex64)
    data = samples.view(np.float32).astype("<f4").tobytes()  # interleaved I/Q

    ext = b"".join(
        [
            _encode_keyword("COMMENT", "A", b"10 dB pad"),  # -> str
            _encode_keyword("F_C", "D", struct.pack("<d", 1.2345e9)),  # scalar
            _encode_keyword(  # multi-element -> list
                "GAINS",
                "F",
                b"".join(struct.pack("<f", g) for g in (1.5, -2.5, 3.5)),
            ),
            _encode_keyword("TRIM", "I", struct.pack("<h", -1234)),  # int16
            _encode_keyword(
                "TICKS", "X", struct.pack("<q", 1234567890123)
            ),  # int64
        ]
    )

    hcb = bytearray(512)
    hcb[0:4] = b"BLUE"
    hcb[4:8] = b"EEEI"  # little-endian
    struct.pack_into("<i", hcb, 12, 0)  # detached = 0 (attached)
    struct.pack_into("<i", hcb, 24, 2)  # ext_start: 512-byte block 2 -> 1024
    struct.pack_into("<i", hcb, 28, len(ext))  # ext_size: bytes
    struct.pack_into("<d", hcb, 32, 512.0)  # data_start: bytes
    struct.pack_into("<d", hcb, 40, float(len(data)))  # data_size: bytes
    hcb[52] = ord("C")  # format mode: complex
    hcb[53] = ord("F")  # format type: 32-bit float (cf32)
    struct.pack_into("<d", hcb, 264, 1.0 / fs)  # xdelta

    body = bytes(hcb) + data
    body += b"\x00" * (1024 - len(body))  # pad to the ext-header block
    p = tmp_path / "keyworded.blue"
    p.write_bytes(body + ext)
    return p, samples


def test_keywords_decode_with_the_right_python_types(keyworded_capture):
    """`.keywords` (gh-543) dispatches each keyword to its Python type: str for
    A, int/float for a scalar numeric, a list for a multi-element one."""
    p, samples = keyworded_capture
    with Reader(p) as r:
        assert np.array_equal(r.read(len(samples)), samples)  # samples intact
        kw = r.keywords

    assert kw["COMMENT"] == "10 dB pad"
    assert isinstance(kw["COMMENT"], str)

    assert kw["F_C"] == pytest.approx(1.2345e9)
    assert isinstance(kw["F_C"], float)

    assert kw["GAINS"] == pytest.approx([1.5, -2.5, 3.5])
    assert isinstance(kw["GAINS"], list)  # multi-element collapses to a list

    assert kw["TRIM"] == -1234
    assert isinstance(kw["TRIM"], int)  # a negative int16 stays signed

    assert kw["TICKS"] == 1234567890123  # 64-bit, past a 32-bit range
    assert isinstance(kw["TICKS"], int)


def test_close_is_idempotent_and_aliases_destroy(capture):
    """`close()` survived the migration; `destroy()` is jm's spelling."""
    p, _ = capture
    r = Reader(p)
    r.close()
    r.close()  # idempotent
    r.destroy()  # the generated name, same effect
