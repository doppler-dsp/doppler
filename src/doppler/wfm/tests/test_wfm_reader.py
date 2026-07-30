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

The file-format behaviour (mode parsing, detached captures, keywords) is
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


def test_header_exposes_the_hcb_under_the_format_s_own_names(tmp_path):
    """`header` is the 512-byte HCB, decoded, nothing renamed or omitted.

    The reader used to keep six fields out of the header and discard the
    rest, so from Python you could not tell whether a field was absent from
    the file or merely dropped on the way out.
    """
    p = tmp_path / "h.blue"
    w = Writer(str(p), file_type="blue", sample_type="cf32")
    w.write(np.ones(8, dtype=np.complex64))
    w.close()

    h = Reader(str(p)).header
    # The names are the format's, not ours.
    for name in (
        "version",
        "head_rep",
        "data_rep",
        "detached",
        "ext_start",
        "ext_size",
        "data_start",
        "data_size",
        "type",
        "format",
        "keylength",
        "xstart",
        "xdelta",
        "xunits",
    ):
        assert name in h, name
    # Each value arrives as the type its BLUE code declares.
    assert h["version"] == "BLUE"
    assert h["type"] == 1000 and isinstance(h["type"], int)
    assert isinstance(h["data_start"], float)
    assert isinstance(h["outbytes"], list) and len(h["outbytes"]) == 8


def test_keywords_merge_the_hcb_area_and_the_extended_header(tmp_path):
    """A key is a key: the caller cannot tell which block carried it.

    BLUE 1.1 3.4 reserves the 92-byte HCB keyword area for six standard
    keywords and puts everything else in the extended header -- other
    systems are licensed to delete user keywords found there. So `VER`
    goes to the HCB area and `NAME`/`SRATE` to the extended header, and
    all three come back from `.keywords`, the numeric one still numeric.
    """
    p = tmp_path / "kw.blue"
    w = Writer(str(p), file_type="blue", sample_type="cf32")
    w.add_keyword("VER", "A", "1.1")  # standard -> HCB keyword area
    w.add_keyword("NAME", "A", "hello")  # user -> extended header
    w.add_keyword("SRATE", "D", 2.048e6)  # typed -> extended header
    w.write(np.ones(8, dtype=np.complex64))
    w.close()

    r = Reader(str(p))
    assert r.keywords == {"VER": "1.1", "NAME": "hello", "SRATE": 2.048e6}
    assert isinstance(r.keywords["SRATE"], float)  # type survived
    # And the header shows where each went.
    h = r.header
    assert h["keylength"] > 0  # the HCB area was used
    assert h["ext_start"] > 0  # and so was the extended header


def _blue_with_slack(path, *, ext_before, gap, trailing, shrink=0):
    """A BLUE file with NON-ZERO slack in every gap the spec permits.

    BLUE 1.1 3.3 allows empty space between HCB and Data, between Data and
    the Extended Header, and after the last section -- and explicitly does
    NOT require it to be zero-filled, so that HEADERMOD can shorten the Data
    section without rewriting the data. Filling the gaps with garbage is
    therefore the honest test, not zeros.
    """
    import struct

    ext = _encode_keyword("SRATE", "D", struct.pack("<d", 2.048e6))
    data = np.arange(1, 9, dtype=np.float32).view(np.complex64).tobytes()
    garb = bytes(range(1, 256)) * 8

    h = bytearray(512)
    h[0:4], h[4:8], h[8:12] = b"BLUE", b"EEEI", b"EEEI"
    struct.pack_into("<i", h, 48, 1000)
    h[52], h[53] = ord("C"), ord("F")
    struct.pack_into("<d", h, 264, 1e-6)

    body = bytearray()
    if ext_before:
        ext_off = 512
        body += ext + garb[: 512 - len(ext)]
        data_off = 1024 + gap
        body += garb[:gap] + data
    else:
        data_off = 512 + gap
        body += garb[:gap] + data
        pad = (512 - ((data_off + len(data)) % 512)) % 512
        body += garb[:pad]
        ext_off = data_off + len(data) + pad
        body += ext
    body += garb[:trailing]

    struct.pack_into("<i", h, 24, ext_off // 512)
    struct.pack_into("<i", h, 28, len(ext))
    struct.pack_into("<d", h, 32, float(data_off))
    struct.pack_into("<d", h, 40, float(len(data) - shrink))
    path.write_bytes(bytes(h) + bytes(body))
    return path


@pytest.mark.parametrize("ext_before", [False, True])
def test_non_zero_slack_between_sections_is_ignored(tmp_path, ext_before):
    """Garbage in the permitted gaps must not reach the caller.

    Neither as samples nor as keywords -- the sections are located by
    data_start/data_size and ext_start/ext_size, so whatever lies between
    them is none of the reader's business.
    """
    f = _blue_with_slack(
        tmp_path / "s.blue", ext_before=ext_before, gap=512, trailing=300
    )
    r = Reader(str(f))
    assert r.keywords == {"SRATE": 2.048e6}
    got = r.read(64)
    assert got.real.astype(int).tolist() == [1, 3, 5, 7]


def test_data_size_bounds_the_read_after_a_headermod_shrink(tmp_path):
    """HEADERMOD shortens data_size without rewriting the data.

    The bytes of the old final sample are still on disk; the header says
    they are no longer payload, and the header wins. Returning them would be
    silent corruption -- the caller gets a sample that is not in the capture.
    """
    f = _blue_with_slack(
        tmp_path / "hm.blue", ext_before=False, gap=0, trailing=0, shrink=8
    )  # one cf32 sample fewer
    got = Reader(str(f)).read(64)
    assert got.real.astype(int).tolist() == [1, 3, 5]


# ── centre frequency ─────────────────────────────────────────────────────────
#
# BLUE type-1000 has no HCB field for centre frequency, so an RF capture
# conveys it as a keyword. `Reader.fc` used to ignore every one of them and
# return 0.0 -- indistinguishable from a genuine baseband capture, on files
# doppler did not write and could not have known were wrong.


def _blue_with_hcb_keyword(path, pair: bytes, *, fs=1e6):
    """A minimal attached BLUE cf32 capture whose ONLY metadata is one ASCII
    `KEY=VALUE\\0` pair in the HCB keyword area (BLUE 1.1 3.1.1.24.1).

    This is the shape a real X-Midas type-1000 capture arrives in: no extended
    header at all, the frequency sitting in the 92 bytes at offset 164 as text.
    Built by hand rather than via `Writer` precisely so it is not a test of our
    own writer -- it is a test of reading somebody else's file.
    """
    data = np.arange(1, 9, dtype=np.float32).view(np.complex64).tobytes()
    h = bytearray(512)
    h[0:4], h[4:8], h[8:12] = b"BLUE", b"EEEI", b"EEEI"
    struct.pack_into("<i", h, 48, 1000)
    struct.pack_into("<d", h, 32, 512.0)
    struct.pack_into("<d", h, 40, float(len(data)))
    h[52], h[53] = ord("C"), ord("F")
    struct.pack_into("<i", h, 160, len(pair))  # keylength
    h[164 : 164 + len(pair)] = pair
    struct.pack_into("<d", h, 264, 1.0 / fs)
    path.write_bytes(bytes(h) + data)
    return path


def test_fc_round_trips_through_a_blue_capture(tmp_path):
    """What the Writer was handed comes back out."""
    p = tmp_path / "rf.blue"
    with Writer(
        p, file_type="blue", sample_type="ci16", fs=1e6, fc=2.4e9
    ) as w:
        w.write(np.zeros(8, dtype=np.complex64))
    with Reader(p) as r:
        assert r.fc == pytest.approx(2.4e9)
        assert r.fc_source == "FREQ"


def test_fc_is_read_from_the_hcb_keyword_area_alone(tmp_path):
    """The wild-file case: ASCII text in the HCB, no extended header.

    Values in that area have no type field (3.1.1.24.1 makes them ASCII by
    definition), so the reader has to parse the characters -- reading only the
    typed extended-header form would miss every capture in this shape.
    """
    p = _blue_with_hcb_keyword(tmp_path / "wild.blue", b"FREQ=2400000000\x00")
    with Reader(p) as r:
        assert r.keywords == {
            "FREQ": "2400000000"
        }  # a str, as the area implies
        assert r.fc == pytest.approx(2.4e9)
        assert r.fc_source == "FREQ"


@pytest.mark.parametrize(
    "tag,text,want",
    [
        ("FREQ", "1.5e9", 1.5e9),
        ("RF_FREQ", "1420405751.768", 1420405751.768),
        ("CENTER_FREQ", "70e6", 70e6),
        ("F_C", "-3", -3.0),
    ],
)
def test_fc_accepts_each_conventional_tag(tmp_path, tag, text, want):
    """No tag for this is standardised -- 3.1.2.6.4.4 defines FREQ only as a
    type-6000 column name -- so the reader tries the conventional set."""
    p = _blue_with_hcb_keyword(
        tmp_path / f"{tag}.blue", f"{tag}={text}".encode() + b"\x00"
    )
    with Reader(p) as r:
        assert r.fc == pytest.approx(want)
        assert r.fc_source == tag


def test_fc_source_separates_declared_baseband_from_not_found(tmp_path):
    """0.0 is a legitimate centre frequency, which is the whole reason
    `fc_source` exists: without it the two cases below are one answer."""
    declared = _blue_with_hcb_keyword(tmp_path / "zero.blue", b"FREQ=0\x00")
    with Reader(declared) as r:
        assert r.fc == 0.0
        assert r.fc_source == "FREQ"  # the capture SAYS baseband

    silent = _blue_with_hcb_keyword(tmp_path / "none.blue", b"COMMENT=hi\x00")
    with Reader(silent) as r:
        assert r.keywords == {"COMMENT": "hi"}  # a keyword block, just not one
        assert r.fc == 0.0
        assert r.fc_source == "none"  # nothing said anything


def test_fc_is_not_guessed_from_a_value_that_is_not_a_bare_number(tmp_path):
    """A units suffix means the file's convention is not one we understand.

    Reporting 2.4e9 from "2.4e9 Hz" would be luck; reporting it from "2.4 GHz"
    would be wrong by a factor of a billion. The value stays visible in
    `.keywords` for a caller who knows what it means.
    """
    p = _blue_with_hcb_keyword(tmp_path / "units.blue", b"FREQ=2.4e9 Hz\x00")
    with Reader(p) as r:
        assert r.keywords == {"FREQ": "2.4e9 Hz"}  # present, so not vacuous
        assert r.fc_source == "none"
        assert r.fc == 0.0


def test_the_typed_copy_wins_over_the_ascii_mirror(tmp_path):
    """A capture we wrote carries FREQ twice -- ASCII in the HCB area, typed
    in the extended header -- so that both an X-Midas reader and a
    full-precision one find it. They must never be read as disagreeing: the
    typed copy is the one that did not round-trip through a decimal string.
    """
    p = tmp_path / "both.blue"
    fc = 1234567890.123456
    with Writer(p, file_type="blue", sample_type="cf32", fc=fc) as w:
        w.write(np.zeros(8, dtype=np.complex64))
    raw = p.read_bytes()
    keylength = struct.unpack_from("<i", raw, 160)[0]
    assert keylength > 0, "the ASCII mirror should be in the HCB area"
    assert raw[164 : 164 + keylength].startswith(b"FREQ=")
    with Reader(p) as r:
        assert struct.unpack_from("<i", raw, 28)[0] > 0  # extended header too
        assert r.fc == fc  # exactly, not merely approximately
        assert r.fc_source == "FREQ"


def test_detached_header_carries_fc(tmp_path):
    """A detached capture's header is written without any writer state, by a
    different function -- so it is a separate chance to drop the frequency."""
    from doppler.wfm import write_blue_header

    hdr = tmp_path / "d.hdr"
    write_blue_header(
        hdr,
        sample_type="ci16",
        endian="le",
        fs=1e6,
        fc=915e6,
        data_start=0.0,
        total=8,
        detached=1,
    )
    (tmp_path / "d.det").write_bytes(b"\x00" * 32)
    with Reader(hdr) as r:
        assert r.fc == pytest.approx(915e6)
        assert r.fc_source == "FREQ"
        assert r.num_samples == 8


# ── file type detection, by content ──────────────────────────────────────────


def test_csv_is_detected_by_content_not_by_extension(tmp_path):
    """A capture that got renamed is still the capture."""
    p = tmp_path / "capture.dat"  # no .csv anywhere
    p.write_text("1.0,2.0\n3.0,4.0\n5.0,6.0\n")
    with Reader(p) as r:
        assert r.file_type == "csv"
        assert r.read(8).tolist() == [1 + 2j, 3 + 4j, 5 + 6j]


def test_a_blue_capture_named_csv_is_still_blue(tmp_path):
    """The other direction: the magic outranks the name."""
    p = tmp_path / "capture.csv"
    with Writer(p, file_type="blue", sample_type="cf32", fs=1e6) as w:
        w.write(np.ones(8, dtype=np.complex64))
    with Reader(p) as r:
        assert r.file_type == "blue"
        assert r.num_samples == 8


def test_a_csv_with_a_header_row_still_opens_as_csv(tmp_path):
    """Content decides, but the name still gets a vote.

    A leading column header fails the content test; falling through to raw
    would read the text as binary IQ, which is worse than what the extension
    already told us.
    """
    p = tmp_path / "headed.csv"
    p.write_text("I,Q\n1.0,2.0\n")
    with Reader(p) as r:
        assert r.file_type == "csv"


# ── length and the wrong-hint tell ───────────────────────────────────────────


def test_csv_reports_its_length(tmp_path):
    """`num_samples` was 0 for every CSV -- which reads as "empty capture"."""
    p = tmp_path / "n.csv"
    p.write_text("".join(f"{i}.0,{i}.5\n" for i in range(37)))
    with Reader(p) as r:
        assert r.num_samples == 37
        # counting must not disturb the read position, in either order
        assert len(r.read(37)) == 37
        assert r.num_samples == 37


def test_trailing_bytes_is_zero_when_the_hint_is_right(tmp_path):
    """The baseline the next two tests are measured against."""
    p = tmp_path / "ok.raw"
    with Writer(p, file_type="raw", sample_type="ci8") as w:
        w.write(np.zeros(5, dtype=np.complex64))
    with Reader(p, sample_type="ci8") as r:
        assert r.trailing_bytes == 0
        assert r.num_samples == 5


def test_trailing_bytes_flags_a_wrong_sample_type_hint(tmp_path):
    """A headerless file type cannot check the hint against the file, so a
    wrong one does not fail -- it returns garbage at the wrong stride. The
    leftover bytes are the only signal there is."""
    p = tmp_path / "hint.raw"
    with Writer(p, file_type="raw", sample_type="ci8") as w:
        w.write(np.zeros(5, dtype=np.complex64))  # 10 bytes on the wire
    with Reader(p, sample_type="cf32") as r:  # would need 8 bytes/sample
        assert r.trailing_bytes == 2
        assert r.num_samples == 1


def test_trailing_bytes_flags_a_capture_cut_mid_sample(tmp_path):
    """The other thing leftover bytes can mean; the reader cannot tell which,
    and says so rather than picking one."""
    p = tmp_path / "cut.raw"
    with Writer(p, file_type="raw", sample_type="cf32") as w:
        w.write(np.zeros(8, dtype=np.complex64))
    with p.open("r+b") as f:
        f.truncate(p.stat().st_size - 3)
    with Reader(p) as r:
        assert r.trailing_bytes == 5  # 8 bytes/sample, 3 of them gone
        assert r.num_samples == 7
