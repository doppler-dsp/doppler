"""Smoke tests for the `Writer` object binding itself.

jm scaffolds a skipped `test_create()` here -- a Writer has no seedable
constructor, since it needs a path. These replace it with the equivalent checks
against a real capture, covering what the handle -> object migration had to
preserve and the two behaviours that are hand-written in the sacred fragment:

  * an ``os.PathLike`` constructor argument            (jm gh-515)
  * ``track_clipping()`` keeping its default argument
  * ``close()`` reporting a failed final flush             (jm gh-541/544)
  * ``reset()`` being absent rather than a no-op            (jm gh-542)
  * ``add_keyword()`` marshaling every keyword type        (hand-written)

File-format behaviour lives in test_api_surface.py / test_compose.py; this
file is about the binding.
"""

import json
import struct
import subprocess
import sys
import textwrap

import numpy as np
import pytest

from doppler.wfm import Composer, Reader, Segment, Writer


@pytest.fixture
def scene():
    return Composer([Segment("qpsk", sps=8, num_samples=1024)]).compose()


# ── add_keyword: the write-side mirror of Reader.keywords ────────────────────
# The C add_keyword takes an untyped `const void *value` whose element type is
# decided at runtime by the `type` char, so the Python -> C marshaling is
# hand-written (jm cannot generate it -- the input twin of gh-543). These
# exercise it end to end: write via add_keyword, read back via Reader.keywords,
# which is the only *fully-Python* keyword round-trip the library has.


def test_add_keyword_round_trips_every_type(tmp_path, scene):
    """Each type code marshals to the right Python value on read-back."""
    p = tmp_path / "kw.blue"
    with Writer(p, file_type="blue", sample_type="cf32", fs=1e6) as w:
        w.write(scene)
        w.add_keyword("COMMENT", "A", "10 dB pad")  # str
        w.add_keyword("F_C", "D", 1.2345e9)  # scalar double
        w.add_keyword("GAINS", "F", [1.5, -2.5, 3.5])  # list of float
        w.add_keyword("FLAG", "B", -7)  # int8
        w.add_keyword("TRIM", "I", -1234)  # int16
        w.add_keyword("OFFSET", "L", -70000)  # int32
        w.add_keyword("TICKS", "X", 1234567890123)  # int64 (past 32-bit)

    with Reader(p) as r:
        assert np.array_equal(r.read(len(scene)), scene)  # samples intact
        kw = r.keywords

    assert kw["COMMENT"] == "10 dB pad" and isinstance(kw["COMMENT"], str)
    assert kw["F_C"] == pytest.approx(1.2345e9) and isinstance(
        kw["F_C"], float
    )
    assert kw["GAINS"] == pytest.approx([1.5, -2.5, 3.5])
    assert isinstance(kw["GAINS"], list)  # multi-element -> list
    assert kw["FLAG"] == -7  # negative int8 stays signed
    assert kw["TRIM"] == -1234
    assert kw["OFFSET"] == -70000
    assert kw["TICKS"] == 1234567890123


def test_add_keyword_rejects_non_blue(tmp_path):
    """Only BLUE has an extended header; other file types refuse.

    The variant codec (gh-554) surfaces a non-zero ``sink_fn`` return as a
    generic ``ValueError: add_keyword failed`` -- the C encoder rejects a
    non-BLUE writer.
    """
    with (
        Writer(tmp_path / "c.raw", fs=1e6) as w,  # raw, not blue
        pytest.raises(ValueError, match="add_keyword failed"),
    ):
        w.add_keyword("X", "D", 1.0)


def test_add_keyword_type_validation(tmp_path):
    """The type code and value type are checked before the C call.

    Error surfaces come from the generated variant-codec binding: a multi-char
    ``type`` fails the ``C`` format parse (TypeError), an unknown code is an
    ``unsupported code`` ValueError, and an ``A`` with a non-str value is a
    ``value must be a str`` TypeError.
    """
    with Writer(tmp_path / "c.blue", fs=1e6, file_type="blue") as w:
        with pytest.raises(TypeError, match="unicode character"):
            w.add_keyword("X", "DD", 1.0)  # 'C' format wants exactly one char
        with pytest.raises(ValueError, match="unsupported code"):
            w.add_keyword("X", "Z", 1.0)
        with pytest.raises(TypeError, match="must be a str"):
            w.add_keyword("X", "A", 123)  # A needs a str, not an int


def test_accepts_pathlike_and_round_trips(tmp_path, scene):
    """The ctor takes a Path, and the capture reads back (jm gh-515)."""
    p = tmp_path / "cap.blue"
    with Writer(p, file_type="blue", sample_type="cf32", fs=2.4e6) as w:
        assert w.write(scene) == len(scene)
    with Reader(p) as r:
        assert np.array_equal(r.read(len(scene)), scene)


def test_track_clipping_defaults_to_on(tmp_path, scene):
    """`track_clipping()` takes no argument -- the documented spelling."""
    p = tmp_path / "cap.ci16"
    with Writer(p, fs=1e6, sample_type="ci16") as w:
        w.track_clipping()  # no argument
        w.write(scene)
        assert w.clip_fraction == 0.0
        assert w.clipped is False
        assert w.peak_dbfs < 0.0


def test_writer_has_no_reset(tmp_path):
    """A writer cannot be reset; the method is absent, not a no-op.

    Declared `no_reset` (jm gh-542), so there is no `reset()` at all --
    ``hasattr`` is False and calling it is an ``AttributeError``, the honest
    Python answer for a type that has nothing to reset. Build a new Writer for
    a new capture. (Previously this raised ``NotImplementedError`` from a
    hand-written stub; the stub is gone with the whole method.)
    """
    with Writer(tmp_path / "c.cf32", fs=1e6) as w:
        assert not hasattr(w, "reset")
        with pytest.raises(AttributeError):
            w.reset()


def test_close_is_idempotent_and_destroy_agrees(tmp_path, scene):
    """`close()` survived the migration and stays idempotent."""
    w = Writer(tmp_path / "c.cf32", fs=1e6)
    w.write(scene)
    w.close()
    w.close()  # idempotent
    w.destroy()  # jm's spelling, same effect


def test_close_finalises_the_blue_header(tmp_path, scene):
    """close() is what patches data_size -- so the size must be right after."""
    p = tmp_path / "cap.blue"
    w = Writer(p, file_type="blue", sample_type="cf32", fs=1e6)
    w.write(scene)
    w.close()  # patches data_size from the actual count
    with Reader(p) as r:
        assert r.num_samples == len(scene)


def _run_under_fsize_limit(body: str) -> subprocess.CompletedProcess:
    """Run `body` in a subprocess with a 4 KiB RLIMIT_FSIZE.

    A subprocess because the limit is process-wide and would break every other
    test that touches a file. SIGXFSZ is ignored so an over-limit write fails
    with an error instead of killing the process.
    """
    script = textwrap.dedent(
        """
        import pathlib, resource, signal, tempfile
        signal.signal(signal.SIGXFSZ, signal.SIG_IGN)
        resource.setrlimit(resource.RLIMIT_FSIZE, (4096, 4096))
        from doppler.wfm import Composer, Segment, Writer

        x = Composer([Segment("qpsk", sps=8, num_samples=1 << 16)]).compose()
        # TemporaryDirectory, not mkdtemp: this subprocess is *expected* to
        # fail its write, and a bare mkdtemp would leave the partial capture
        # behind on every run. The object's finaliser clears it at exit.
        tmp = tempfile.TemporaryDirectory()
        p = pathlib.Path(tmp.name) / "big.blue"
        """
    ) + textwrap.dedent(body)
    return subprocess.run(
        [sys.executable, "-c", script], capture_output=True, text=True
    )


def test_a_failed_capture_propagates_out_of_a_with_block():
    """The whole point of `close()` reporting: a `with` block must still raise.

    This is the assertion that fails if `__exit__` ever reverts to jm's
    generated form, which calls the void `destroy()` and cannot report
    anything. `with` is how nearly every caller uses a Writer, so an error
    swallowed here is an error nobody ever sees -- and for BLUE the capture on
    disk is genuinely wrong, not merely short.
    """
    out = _run_under_fsize_limit(
        """
        try:
            with Writer(p, file_type="blue", sample_type="cf32", fs=1e6) as w:
                w.write(x)
        except OSError as e:
            print("RAISED", e)
        else:
            print("SWALLOWED")
        """
    )
    assert out.returncode == 0, out.stderr
    assert out.stdout.startswith("RAISED"), (
        "a failed capture was swallowed by the with-block: "
        f"{out.stdout.strip()!r}"
    )


def test_short_write_is_reported():
    """`write()` returns the count that landed, independently of close().

    Two separate signals for a failed capture -- the short count here, and the
    OSError from close() above -- because a caller streaming blocks wants to
    know at the block that failed, not only at the end.
    """
    out = _run_under_fsize_limit(
        """
        w = Writer(p, file_type="blue", sample_type="cf32", fs=1e6)
        n = w.write(x)
        try:
            w.close()
        except OSError:
            pass
        print(n, len(x))
        """
    )
    assert out.returncode == 0, out.stderr
    wrote, asked = (int(v) for v in out.stdout.split())
    assert wrote < asked, "a refused write must be reported, not swallowed"
    assert wrote > 0, "the header and the first block should still land"


# ── SigMF is a pair ──────────────────────────────────────────────────────────
#
# `file_type="sigmf"` used to write only the samples. The datatype, sample rate
# and centre frequency live exclusively in the sidecar, so the result was not a
# lean capture but an unreadable one -- and the only way to get a working pair
# was to go through `Composer`, which produced the JSON itself.


def test_sigmf_writer_emits_its_sidecar(tmp_path):
    """One call, a complete capture, no Composer."""
    p = tmp_path / "cap.sigmf-data"
    x = np.linspace(0, 0.5, 16).astype(np.complex64)
    with Writer(
        p, file_type="sigmf", sample_type="ci16", fs=2e6, fc=1.2e9
    ) as w:
        w.write(x)

    meta = tmp_path / "cap.sigmf-meta"
    assert meta.exists(), "the half that carries the datatype"
    doc = json.loads(meta.read_text())
    assert doc["global"]["core:datatype"] == "ci16_le"
    assert doc["global"]["core:sample_rate"] == 2e6
    assert doc["captures"][0]["core:frequency"] == 1.2e9

    with Reader(p) as r:  # and it round-trips through our own reader
        assert r.file_type == "sigmf"
        assert r.sample_type == "ci16"
        assert r.fs == 2e6
        assert r.fc == 1.2e9
        assert r.fc_source == "core:frequency"
        assert r.num_samples == len(x)


def test_sigmf_requires_the_sigmf_data_extension(tmp_path):
    """Both halves of a SigMF capture are found by name, so the name is part
    of the format. Writing `other.bin` + `other.sigmf-meta` would produce a
    pair no SigMF reader looks for; rewriting the caller's path under them
    would be the bigger surprise. So it is refused, and says why."""
    with pytest.raises(OSError, match=r"\.sigmf-data"):
        Writer(tmp_path / "other.bin", file_type="sigmf", fs=1e6)
    assert not (tmp_path / "other.sigmf-meta").exists()


# ── the raw/CSV sidecar ──────────────────────────────────────────────────────
#
# Raw and CSV take `fs`, `fc` and `t0` like every other file type and have
# nowhere to store them, so they used to throw them away -- the caller's own
# metadata, discarded by the library that asked for it, leaving a file nobody
# (its author included) could interpret afterwards. The containers have no
# room; a sidecar is room.


@pytest.mark.parametrize("suffix", ["raw", "csv"])
def test_raw_and_csv_keep_their_metadata_in_a_sidecar(tmp_path, suffix):
    p = tmp_path / f"cap.{suffix}"
    with Writer(p, fs=2.4e6, file_type=suffix, fc=1.2e9, t0=1785903330.0) as w:
        w.write(np.zeros(4, dtype=np.complex64))

    # APPENDED, not swapped: `cap.raw.sigmf-meta`, so it is 1:1 with the file
    # it describes and cannot collide with a real pair's `cap.sigmf-meta`.
    meta = tmp_path / f"cap.{suffix}.sigmf-meta"
    assert meta.exists()
    assert not (tmp_path / "cap.sigmf-meta").exists()

    doc = json.loads(meta.read_text())
    assert doc["global"]["core:sample_rate"] == 2.4e6
    assert doc["captures"][0]["core:frequency"] == 1.2e9
    assert doc["captures"][0]["core:datetime"] == "2026-08-05T04:15:30.000000Z"


def test_the_sidecar_claims_only_what_was_stated(tmp_path):
    """Silence stays silence. An undeclared rate, centre frequency or capture
    time is an ABSENT key, not a confident zero -- the whole point of writing
    the file is to stop it asserting things nobody said."""
    p = tmp_path / "bare.raw"
    with Writer(p, fs=0.0) as w:  # nothing known but the datatype
        w.write(np.zeros(4, dtype=np.complex64))

    doc = json.loads((tmp_path / "bare.raw.sigmf-meta").read_text())
    assert "core:sample_rate" not in doc["global"]
    assert "core:frequency" not in doc["captures"][0]
    assert "core:datetime" not in doc["captures"][0]
    # ...but it is still a valid document: SigMF 1.0.0 requires exactly these.
    assert doc["global"]["core:datatype"] == "cf32_le"
    assert doc["global"]["core:version"] == "1.0.0"


def test_sidecar_can_be_turned_off(tmp_path):
    """An extra file beside the capture can break a downstream glob, so it is
    a choice -- just a choice whose default is "keep the metadata"."""
    p = tmp_path / "plain.raw"
    with Writer(p, fs=1e6, fc=1e9, sidecar=False) as w:
        w.write(np.zeros(4, dtype=np.complex64))
    assert list(tmp_path.iterdir()) == [p]


def test_blue_never_gets_a_sidecar(tmp_path):
    """Its header already carries fs, fc and the timecode. A second copy is
    only somewhere for them to drift."""
    p = tmp_path / "cap.blue"
    with Writer(p, fs=1e6, file_type="blue", fc=1e9, sidecar=True) as w:
        w.write(np.zeros(4, dtype=np.complex64))
    assert list(tmp_path.iterdir()) == [p]


def test_a_raw_capture_cannot_clobber_a_sigmf_pairs_metadata(tmp_path):
    """Why the name is appended rather than swapped. Two captures sharing a
    base name must not share a sidecar: with the swapped spelling, writing
    `cap.raw` would silently retype the `cap.sigmf-data` beside it."""
    x = np.zeros(4, dtype=np.complex64)
    with Writer(
        tmp_path / "cap.sigmf-data",
        fs=5e6,
        file_type="sigmf",
        sample_type="ci16",
    ) as w:
        w.write(x)
    with Writer(tmp_path / "cap.raw", fs=7e6) as w:
        w.write(x)

    pair = json.loads((tmp_path / "cap.sigmf-meta").read_text())
    assert pair["global"]["core:sample_rate"] == 5e6
    assert pair["global"]["core:datatype"] == "ci16_le", "not retyped"
    raw = json.loads((tmp_path / "cap.raw.sigmf-meta").read_text())
    assert raw["global"]["core:sample_rate"] == 7e6


# ── centre frequency on the BLUE write side ──────────────────────────────────


def test_blue_fc_is_written_to_both_keyword_blocks(tmp_path):
    """Deliberate duplication, for two different readers.

    The ASCII copy in the HCB keyword area is where an X-Midas reader looks --
    3.4 reserves that area for six standard keywords and warns that X-Midas may
    delete a user keyword found there, which is exactly why it is the mirror.
    The typed copy in the extended header is 3.4-compliant and keeps full
    double precision, so it is the one that survives.
    """
    p = tmp_path / "rf.blue"
    with Writer(
        p, fs=1e6, file_type="blue", sample_type="cf32", fc=2.4e9
    ) as w:
        w.write(np.zeros(8, dtype=np.complex64))
    raw = p.read_bytes()

    keylength = struct.unpack_from("<i", raw, 160)[0]
    assert raw[164 : 164 + keylength] == b"FREQ=2400000000\x00"  # ASCII, HCB
    assert struct.unpack_from("<i", raw, 28)[0] > 0  # extended header written
    with Reader(p) as r:
        assert r.keywords["FREQ"] == 2.4e9  # the typed copy, as a float


def test_blue_without_fc_adds_no_keyword(tmp_path):
    """0.0 is also the default for "not supplied", and the two are
    indistinguishable here -- so nothing is written and the capture stays as
    empty as it was before this feature existed."""
    p = tmp_path / "bb.blue"
    with Writer(p, fs=1e6, file_type="blue", sample_type="cf32") as w:
        w.write(np.zeros(8, dtype=np.complex64))
    assert struct.unpack_from("<i", p.read_bytes(), 160)[0] == 0  # keylength
    with Reader(p) as r:
        assert r.keywords == {}
        assert r.fc_source == "none"


def test_fc_survives_a_close_time_hcb_keyword_patch(tmp_path):
    """Any other HCB keyword makes close() rewrite the whole 92-byte area.

    The frequency was written there at open, by a different function, so this
    is the path on which it could be silently overwritten away.
    """
    p = tmp_path / "both.blue"
    w = Writer(p, fs=1e6, file_type="blue", sample_type="cf32", fc=915e6)
    w.add_keyword("VER", "A", "1.1")  # standard -> the same HCB area
    w.write(np.zeros(8, dtype=np.complex64))
    w.close()
    with Reader(p) as r:
        assert r.fc == pytest.approx(915e6)
        assert r.keywords["VER"] == "1.1"


# ── scalar (real) sample types — doppler#1032 ────────────────────────────
#
# doppler could READ a BLUE file in mode 'S' and could not write one: the
# writer hardcoded `h[52] = 'C'` while the function beside it discarded the
# very mode character `dp_format_chars` handed back. Nothing noticed, because
# no test ever round-tripped a real capture through our own writer -- which is
# what these do.


@pytest.mark.parametrize("file_type", ["raw", "csv", "blue", "sigmf"])
@pytest.mark.parametrize("stype", ["f32", "f64", "i32", "i16", "i8"])
def test_a_real_capture_round_trips_through_our_own_writer(
    tmp_path, file_type, stype
):
    """Write a real waveform, read it back, get the same samples.

    The count is the assertion that matters. A scalar file read as complex
    yields HALF the samples with every other one landing in Q -- plausible
    output from a file that said otherwise, which is the failure this whole
    change is about.
    """
    p = tmp_path / ("cap.sigmf-data" if file_type == "sigmf" else "cap.bin")
    x = np.exp(2j * np.pi * 0.03 * np.arange(64)).astype(np.complex64)

    with Writer(p, file_type=file_type, sample_type=stype, fs=48e3) as w:
        w.write(x)

    # raw and CSV carry no metadata, so the hint is how they are told; BLUE
    # and SigMF carry their own and must not need it.
    kw = {"sample_type": stype} if file_type in ("raw", "csv") else {}
    with Reader(p, **kw) as r:
        assert r.mode == "scalar"
        assert r.sample_type == stype
        # Asked BEFORE the read: for CSV this is the lazy whole-file scan,
        # and it counts one column per line only if it knows the mode.
        assert r.num_samples == len(x)
        y = r.read(len(x) * 2)

    assert len(y) == len(x)
    assert not y.imag.any(), "a scalar capture has no Q to read back"
    tol = {"f32": 1e-6, "f64": 1e-6, "i32": 1e-6, "i16": 1e-4, "i8": 2e-2}
    assert np.allclose(y.real, x.real, atol=tol[stype])


@pytest.mark.parametrize(
    "stype,mode_char", [("cf32", b"C"), ("f32", b"S"), ("i16", b"S")]
)
def test_blue_writes_the_mode_character_its_data_actually_has(
    tmp_path, stype, mode_char
):
    """HCB byte 52 is the format MODE, and it was the literal 'C' regardless.

    Read as bytes rather than through our own reader on purpose: a reader that
    shared the writer's assumption would agree with it and prove nothing.
    """
    p = tmp_path / "cap.blue"
    with Writer(p, file_type="blue", sample_type=stype, fs=48e3) as w:
        w.write(np.zeros(8, np.complex64))
    hcb = p.read_bytes()[:512]
    assert hcb[52:53] == mode_char
    assert hcb[53:54] == {"cf32": b"F", "f32": b"F", "i16": b"I"}[stype]


def test_sigmf_names_real_types_with_its_own_prefix(tmp_path):
    """SigMF spells a real type `rf32_le`, not `f32_le` -- its complex/real
    marker is its own vocabulary and does not match `--sample-type`. Emitting
    `cf32_le` for real data would be a sidecar that lies about its payload."""
    p = tmp_path / "cap.sigmf-data"
    with Writer(p, file_type="sigmf", sample_type="f32", fs=48e3) as w:
        w.write(np.zeros(8, np.complex64))
    doc = json.loads((tmp_path / "cap.sigmf-meta").read_text())
    assert doc["global"]["core:datatype"] == "rf32_le"


def test_a_real_capture_is_exactly_half_the_bytes(tmp_path):
    """The whole of what mode 'S' means, in one number."""
    x = np.zeros(100, np.complex64)
    sizes = {}
    for stype in ("cf32", "f32"):
        p = tmp_path / f"{stype}.bin"
        with Writer(p, file_type="raw", sample_type=stype, fs=48e3) as w:
            w.write(x)
        sizes[stype] = p.stat().st_size
    assert sizes["cf32"] == 800
    assert sizes["f32"] == 400


def test_a_sigmf_sidecar_naming_an_unknown_datatype_is_refused(tmp_path):
    """`core:datatype` is the only place a SigMF capture says what it holds,
    so a value we cannot decode has no fallback worth having -- reading it as
    the constructor's hint would be answering a question the file already
    answered, wrongly. Both halves of the code are checked: an unknown MODE
    letter and an unknown ELEMENT."""
    for dt in ("xf32_le", "cq99_le"):
        p = tmp_path / f"{dt}.sigmf-data"
        p.write_bytes(b"\x00" * 64)
        (tmp_path / f"{dt}.sigmf-meta").write_text(
            json.dumps(
                {"global": {"core:datatype": dt, "core:sample_rate": 1e6}}
            )
        )
        with pytest.raises((ValueError, RuntimeError, OSError)):
            Reader(p)
