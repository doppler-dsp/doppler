"""`wfmgen` accepts ``LO:HI`` ranges on its numeric flags.

A scalar flag (``--freq 1e5``) is that number; a ``LO:HI`` flag
(``--freq 1e5:2e5``) marks the field as ranged, drawn uniformly per repeat.
The fully-resolved ``--record`` capture stores the *range* (a ``[lo, hi]``
array), not a sampled value, so the run replays byte-for-byte. These tests
drive the CLI end to end: every numeric flag in its ranged form must parse,
produce a finite run, and round-trip the range into the record.
"""

from __future__ import annotations

import json
import subprocess
from typing import TYPE_CHECKING

import numpy as np

from doppler.measure import ToneMeasure
from doppler.wfm import cli

if TYPE_CHECKING:
    from pathlib import Path


def _bin() -> str:
    return cli._runnable()


def _record(tmp_path: Path, *flags: str) -> dict:
    """Run wfmgen with *flags* + ``--output /dev/null --record FILE``."""
    rec = tmp_path / "record.json"
    p = subprocess.run(
        [_bin(), *flags, "--output", "/dev/null", "--record", str(rec)],
        capture_output=True,
    )
    assert p.returncode == 0, p.stderr.decode()
    return json.loads(rec.read_text())


def test_freq_range_records_as_array(tmp_path):
    spec = _record(
        tmp_path, "--type", "tone", "--freq", "1e5:2e5", "--count", "64"
    )
    seg = spec["segments"][0]
    assert seg["freq"] == [1e5, 2e5]


def test_all_numeric_flags_ranged(tmp_path):
    """One chirp invocation exercising every ranged flag arm + parse_range."""
    spec = _record(
        tmp_path,
        "--type",
        "chirp",
        "--fs",
        "1e6",
        "--freq",
        "1e5:2e5",
        "--f-end",
        "3e5:4e5",
        "--snr",
        "10:20",
        "--count",
        "64:128",
        "--off",
        "4:8",
        "--level",
        "-12:-3",
    )
    seg = spec["segments"][0]
    assert seg["freq"] == [1e5, 2e5]
    assert seg["f_end"] == [3e5, 4e5]
    assert seg["snr"] == [10.0, 20.0]
    assert seg["num_samples"] == [64, 128]
    assert seg["off_samples"] == [4, 8]
    assert seg["level"] == [-12.0, -3.0]


def test_scalar_flags_still_scalar(tmp_path):
    """A bare number stays a scalar — ranges are strictly opt-in via ``:``."""
    spec = _record(
        tmp_path, "--type", "tone", "--freq", "1e5", "--count", "64"
    )
    seg = spec["segments"][0]
    assert seg["freq"] == 1e5
    assert not isinstance(seg["freq"], list)


def test_seed_advance_flag_accepted(tmp_path):
    """The ``--seed-advance`` choice flag parses and the run completes (the
    mode is a no-op on a finite single pass and is not serialised into the
    record, so this just asserts the CLI accepts it)."""
    spec = _record(
        tmp_path, "--type", "tone", "--seed-advance", "noise", "--count", "64"
    )
    assert spec["version"] == 1


def _sigmf(tmp_path, *flags):
    """Run wfmgen writing a SigMF pair; return (samples, meta)."""
    out = tmp_path / "cap.sigmf-data"
    p = subprocess.run(
        [_bin(), *flags, "--file-type", "sigmf", "--output", str(out)],
        capture_output=True,
    )
    assert p.returncode == 0, p.stderr.decode()
    data = tmp_path / "cap.sigmf-data.sigmf-data"
    meta = tmp_path / "cap.sigmf-data.sigmf-meta"
    return np.fromfile(data, dtype=np.complex64), json.loads(meta.read_text())


def test_sigmf_annotation_reports_the_drawn_value_not_the_range_lo(tmp_path):
    """The sidecar must describe the capture, measured against the capture.

    Every annotation used to carry the range's ``lo`` for frequency and SNR
    while its ``sample_start``/``sample_count`` were exact -- authoritative
    about *when* and wrong about *what*, which is the half nobody audits
    (doppler#1086: up to 1224 Hz and 6.0 dB out). So this measures each
    annotation's own window with our own tone meter rather than re-deriving
    the draw, because a hash-vs-hash check agrees with itself while both
    sides are wrong.
    """
    fs = 1e6
    lo, hi = 11200.0, 12800.0
    samples, meta = _sigmf(
        tmp_path,
        "--type",
        "tone",
        "--fs",
        f"{fs:g}",
        "--freq",
        f"{lo:g}:{hi:g}",
        "--snr",
        "8:14",
        "--count",
        "4096",
        "--repeats",
        "3",
    )
    anns = meta["annotations"]
    assert len(anns) == 3, anns

    seen_f, seen_s = [], []
    for a in anns:
        start, n = int(a["core:sample_start"]), int(a["core:sample_count"])
        block = samples[start : start + n]
        r = ToneMeasure(n=len(block), fs=fs).analyze_complex(block)

        said_f = float(a["core:freq_lower_edge"])
        said_s = float(a["wfmgen:snr"])
        seen_f.append(said_f)
        seen_s.append(said_s)

        # One FFT bin of slack on frequency, 0.5 dB on SNR: the annotation
        # must be the drawn value, and `lo` is 638-1224 Hz away at this
        # geometry -- far outside either.
        assert abs(said_f - r.fund_freq) < fs / len(block), (
            f"annotation says {said_f:.0f} Hz, samples say {r.fund_freq:.0f}"
        )
        assert abs(said_s - r.snr) < 0.5, (
            f"annotation says {said_s:.1f} dB, samples say {r.snr:.1f}"
        )
        assert lo <= said_f <= hi

    # Reading `lo` back would make all three identical AND right at the
    # bound. Both are asserted, so the regression cannot pass by luck on a
    # scene whose draws happen to land close together.
    assert len(set(seen_f)) == 3, f"draws did not vary: {seen_f}"
    assert min(seen_f) > lo, f"an annotation sat on the range's lo: {seen_f}"
    assert len(set(seen_s)) == 3, f"snr draws did not vary: {seen_s}"


def test_sigmf_annotation_carries_the_drawn_level(tmp_path):
    """`level` is drawn and had no annotation key at all until #1086."""
    _, meta = _sigmf(
        tmp_path,
        "--type",
        "tone",
        "--fs",
        "1e6",
        "--level",
        "-20:-5",
        "--count",
        "1024",
        "--repeats",
        "3",
    )
    levels = [float(a["wfmgen:level_db"]) for a in meta["annotations"]]
    assert len(levels) == 3
    assert all(-20.0 <= v <= -5.0 for v in levels), levels
    assert len(set(levels)) == 3, f"level draws did not vary: {levels}"
