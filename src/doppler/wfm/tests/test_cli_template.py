"""`wfmgen json-template` emits a ready-to-edit spec that feeds back in.

The subcommand dumps a representative spec in the canonical `--from-file`
schema (to stdout, or to a named file). Its whole value is that it is valid by
construction, so these tests confirm it both *parses as JSON* and
*round-trips*: feeding the dumped template back through `--from-file` must
compose without
error and produce IQ.
"""

from __future__ import annotations

import json
import subprocess

from doppler.wfm import cli


def _bin() -> str:
    return cli._runnable()


def test_json_template_stdout_is_valid_json():
    p = subprocess.run(
        [_bin(), "json-template"],
        capture_output=True,
    )
    assert p.returncode == 0
    spec = json.loads(p.stdout)  # parses as JSON
    assert spec["version"] == "wfmgen-1"
    assert len(spec["segments"]) >= 1


def test_json_template_to_file(tmp_path):
    out = tmp_path / "tpl.json"
    p = subprocess.run(
        [_bin(), "json-template", str(out)],
        capture_output=True,
    )
    assert p.returncode == 0
    assert p.stdout == b""  # nothing on stdout when a path is given
    spec = json.loads(out.read_text())
    assert spec["version"] == "wfmgen-1"


def test_json_template_roundtrips_through_from_file(tmp_path):
    # Dump the template, then feed it straight back: must compose and emit
    # IQ
    # (proving the sum segment's noise model resolves without over-specifying).
    tpl = tmp_path / "tpl.json"
    iq = tmp_path / "out.cf32"
    subprocess.run([_bin(), "json-template", str(tpl)], check=True)
    p = subprocess.run(
        [_bin(), "--from-file", str(tpl), "--output", str(iq)],
        capture_output=True,
    )
    assert p.returncode == 0, p.stderr
    assert iq.stat().st_size > 0
