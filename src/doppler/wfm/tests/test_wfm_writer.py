"""Smoke tests for the `Writer` object binding itself.

jm scaffolds a skipped `test_create()` here -- a Writer has no seedable
constructor, since it needs a path. These replace it with the equivalent checks
against a real capture, covering what the handle -> object migration had to
preserve and the two behaviours that are hand-written in the sacred fragment:

  * an ``os.PathLike`` constructor argument            (jm gh-515)
  * ``track_clipping()`` keeping its default argument
  * ``close()`` reporting a failed final flush, which ``destroy()`` cannot
  * ``reset()`` refusing rather than silently doing nothing

Container-level behaviour lives in test_api_surface.py / test_compose.py; this
file is about the binding.
"""

import numpy as np
import pytest

from doppler.wfm import Composer, Reader, Segment, Writer


@pytest.fixture
def scene():
    return Composer([Segment("qpsk", sps=8, num_samples=1024)]).compose()


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


def test_reset_refuses(tmp_path):
    """A writer cannot be reset; build a new one for a new capture."""
    with (
        Writer(tmp_path / "c.cf32") as w,
        pytest.raises(NotImplementedError, match="construct a new Writer"),
    ):
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
