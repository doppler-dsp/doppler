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
