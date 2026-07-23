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

Container-level behaviour lives in test_api_surface.py / test_compose.py; this
file is about the binding.
"""

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
    """Only BLUE has an extended header; other containers refuse.

    The variant codec (gh-554) surfaces a non-zero ``sink_fn`` return as a
    generic ``ValueError: add_keyword failed`` -- the C encoder rejects a
    non-BLUE writer.
    """
    with (
        Writer(tmp_path / "c.raw") as w,  # raw, not blue
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
    with Writer(tmp_path / "c.blue", file_type="blue") as w:
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
    with Writer(p, sample_type="ci16") as w:
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
    with Writer(tmp_path / "c.cf32") as w:
        assert not hasattr(w, "reset")
        with pytest.raises(AttributeError):
            w.reset()


def test_close_is_idempotent_and_destroy_agrees(tmp_path, scene):
    """`close()` survived the migration and stays idempotent."""
    w = Writer(tmp_path / "c.cf32")
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
        p = pathlib.Path(tempfile.mkdtemp()) / "big.blue"
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
