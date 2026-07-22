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


def test_close_is_idempotent_and_aliases_destroy(capture):
    """`close()` survived the migration; `destroy()` is jm's spelling."""
    p, _ = capture
    r = Reader(p)
    r.close()
    r.close()  # idempotent
    r.destroy()  # the generated name, same effect
