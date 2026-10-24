"""A scene written as a SigMF capture carries its annotations, and Reader
returns them (doppler#2564, doppler#2565).

The reference is ``Composer.to_sigmf()``, called with the same sample type,
rate and centre frequency the Writer was given: ``Writer.annotate(scene)``
builds its rows through the same C builder, so the two must agree row for
row, and the frequency edges prove the Writer's ``fc`` is the one used.
"""

from __future__ import annotations

import gc
import json

import pytest

from doppler.wfm import Composer, Reader, Segment, Writer

FS = 1e6
FC = 915e6


@pytest.fixture
def scene():
    # Three tone bursts, each at its own frequency drawn from [90, 110] kHz,
    # so the rows come from the draws and not from the spec alone.
    return Composer(
        Segment(
            type="tone",
            fs=FS,
            freq=(9e4, 1.1e5),
            snr=20.0,
            seed=1,
            num_samples=1000,
            off_samples=500,
            repeats=3,
        )
    )


def _expected(scene):
    meta = json.loads(scene.to_sigmf(sample_type="cf32", fs=FS, fc=FC))
    return meta["annotations"]


def test_sigmf_round_trip(tmp_path, scene):
    """Written, then read back: the rows equal to_sigmf()'s."""
    path = tmp_path / "cap.sigmf-data"
    with Writer(
        path, file_type="sigmf", sample_type="cf32", fs=FS, fc=FC
    ) as w:
        w.annotate(scene)
        w.write(scene.compose())
    with Reader(path) as r:
        got = r.annotations
    assert got == _expected(scene)
    assert len(got) == 3
    lo = got[0]["core:freq_lower_edge"]
    assert FC + 8e4 < lo < FC + 1.2e5  # the Writer's fc reached the edges


def test_raw_sidecar_round_trip(tmp_path, scene):
    """A raw capture keeps the rows in the sidecar beside it."""
    path = tmp_path / "cap.cf32"
    with Writer(path, file_type="raw", sample_type="cf32", fs=FS, fc=FC) as w:
        w.annotate(scene)
        w.write(scene.compose())
    with Reader(path) as r:
        assert r.annotations == _expected(scene)


def test_no_scene_reads_empty(tmp_path, scene):
    """No scene, no rows; a BLUE capture has none either."""
    sig = tmp_path / "plain.sigmf-data"
    with Writer(sig, file_type="sigmf", sample_type="cf32", fs=FS) as w:
        w.write(scene.compose())
    with Reader(sig) as r:
        assert r.annotations == []
    blue = tmp_path / "plain.blue"
    with Writer(blue, file_type="blue", sample_type="cf32", fs=FS) as w:
        w.write(scene.compose())
    with Reader(blue) as r:
        assert r.annotations == []


def test_annotate_refuses_where_rows_would_be_dropped(tmp_path, scene):
    """BLUE has no annotations; a raw writer without a sidecar has nowhere
    to keep them. Both refuse rather than drop the rows at close."""
    with (
        Writer(tmp_path / "x.blue", file_type="blue", fs=FS) as w,
        pytest.raises(ValueError),
    ):
        w.annotate(scene)
    with (
        Writer(
            tmp_path / "x.cf32", file_type="raw", fs=FS, sidecar=False
        ) as w,
        pytest.raises(ValueError),
    ):
        w.annotate(scene)


def test_annotate_refuses_a_non_composer(tmp_path):
    """None reaches C as NULL and is refused there; any other non-Composer
    fails the capsule unwrap. That raises AttributeError today, like every
    capsule param (just-buildit/just-makeit#2355 asks for TypeError)."""
    with Writer(tmp_path / "y.sigmf-data", file_type="sigmf", fs=FS) as w:
        with pytest.raises(ValueError):
            w.annotate(None)
        with pytest.raises(AttributeError):
            w.annotate(object())


def test_writer_keeps_the_scene_alive(tmp_path):
    """The scene is read at close, so the Writer holds it: dropping the
    caller's last reference before close() must not free it."""
    path = tmp_path / "alive.sigmf-data"
    seg = Segment(type="tone", fs=FS, freq=1e5, snr=20.0, num_samples=100)
    c = Composer(seg)
    x = c.compose()
    expected = json.loads(c.to_sigmf(sample_type="cf32", fs=FS))["annotations"]
    w = Writer(path, file_type="sigmf", sample_type="cf32", fs=FS)
    w.annotate(c)
    del c
    gc.collect()
    w.write(x)
    w.close()
    with Reader(path) as r:
        assert r.annotations == expected


def test_closing_the_scene_before_the_writer_is_safe(tmp_path):
    """The usual `with` shape closes the scene first. The Writer still
    holds it, and reads its segments at close, so closing the Composer while
    it is lent must not free them under the Writer (jm gh-2346, the review's
    heap-use-after-free): the rows still come out right."""
    path = tmp_path / "closed.sigmf-data"
    w = Writer(path, file_type="sigmf", sample_type="cf32", fs=FS)
    seg = Segment(type="tone", fs=FS, freq=1e5, snr=20.0, num_samples=100)
    with Composer(seg) as c:
        w.annotate(c)
        w.write(c.compose())
        expected = json.loads(c.to_sigmf(sample_type="cf32", fs=FS))
    w.close()
    with Reader(path) as r:
        assert r.annotations == expected["annotations"]
