"""`docs/specan/frames.json` still matches what the pipeline produces.

The committed frames are what the static docs player shows, and they are a
projection of the specan source -- the Python app AND the C core. They had
already drifted once without anything noticing: the file was recorded at 1601
display bins while the code produced 801, and the divergence arrived through
the C side, which `specan-check` was not watching at the time.

`specan-check` is a *path* gate -- it fails when something under the watched
directories changed and `frames.json` did not. That answers "did somebody
re-record", not "do the frames match the code", and it cannot answer the
second: it runs in a job with no build. This test can, because the Python
suite already builds the extension, and re-recording 120 frames costs 0.25 s.

What is compared, and why not simply the bytes: the recording is seeded now
(doppler#1367), so two runs on ONE machine are byte-identical -- but the `db`
values come out of the C FFT, whose SIMD tier differs across the
architectures CI builds on, and the frames are rounded to 0.1 dB, so a level
sitting on a rounding boundary could tip either way. Byte-equality would turn
that into a flake. So the GEOMETRY -- the fields that actually drifted -- is
compared exactly, and the levels get a tolerance wide enough for a rounding
boundary and far tighter than any real change.
"""

from __future__ import annotations

import json

import numpy as np
import pytest

from doppler.specan.record_demo import record
from doppler.tests._repo import repo_root

FRAMES = repo_root(__file__) / "docs" / "specan" / "frames.json"

#: One 0.1 dB rounding step, doubled for the two roundings in play. The
#: bin-count drift this guards against moved the array LENGTH, and a real
#: level change moves dB, not hundredths of one.
TOL_DB = 0.2

#: Compared exactly -- these are the fields that silently drifted before.
GEOMETRY = ("fft_size", "fs_out", "center_freq", "span", "rbw")


@pytest.fixture(scope="module")
def committed() -> list[dict]:
    return json.loads(FRAMES.read_text(encoding="utf-8"))


@pytest.fixture(scope="module")
def fresh(committed: list[dict]) -> list[dict]:
    """Re-record with the same knobs `make record-demo` uses."""
    return record(n_frames=len(committed), fft_size=512)


def test_the_committed_file_ends_in_a_newline() -> None:
    """Else `end-of-file-fixer` blocks the commit after a re-record."""
    assert FRAMES.read_bytes().endswith(b"\n")


def test_frame_count_matches(committed: list[dict], fresh: list[dict]) -> None:
    assert len(fresh) == len(committed)


@pytest.mark.parametrize("field", GEOMETRY)
def test_geometry_is_exact(
    committed: list[dict], fresh: list[dict], field: str
) -> None:
    """A bin count or span that moved is the drift this file exists for."""
    assert {f[field] for f in fresh} == {f[field] for f in committed}


def test_every_frame_has_the_committed_bin_count(
    committed: list[dict], fresh: list[dict]
) -> None:
    assert [len(f["db"]) for f in fresh] == [len(f["db"]) for f in committed]


def test_levels_match_within_tolerance(
    committed: list[dict], fresh: list[dict]
) -> None:
    """Seeded, so this is the same noise realization -- not just the same
    distribution. A mismatch means the pipeline changed."""
    a = np.array([f["db"] for f in committed], dtype=float)
    b = np.array([f["db"] for f in fresh], dtype=float)
    worst = np.abs(a - b).max()
    assert worst <= TOL_DB, (
        f"recorded frames differ from the pipeline by {worst:.2f} dB "
        f"(tolerance {TOL_DB} dB) — run 'make record-demo'"
    )


def test_the_recording_is_reproducible() -> None:
    """The seed is what lets every assertion above mean anything."""
    a = record(n_frames=3, fft_size=512)
    b = record(n_frames=3, fft_size=512)
    assert a == b
