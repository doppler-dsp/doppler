"""`scripts/cov_skipped_objects.py`: a SKIPPED test leaves the coverage report.

`make coverage` pipes its list of instrumented test executables through the
script before handing them to llvm-cov. A test that skipped ran nothing, and
an instruction-set tier binary (`test_fir_chunk_avx512`) is the only object
mapping its tier's code -- so on a runner without the extension, keeping it
reported every line of that code as uncovered and failed the patch gate on
the runner's CPU (doppler#1934). Each case writes ctest's two records the
way ctest writes them and runs the script as the recipe does: candidates on
stdin, kept ones on stdout, what it dropped on stderr.
"""

from __future__ import annotations

import json
import subprocess
import sys
from typing import TYPE_CHECKING

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)
SCRIPT = REPO / "scripts" / "cov_skipped_objects.py"

# ctest --output-junit's shape for a skip: status="notrun" plus a <skipped>
# child carrying the return code (measured with cmake 4.2.3).
JUNIT = """<?xml version="1.0" encoding="UTF-8"?>
<testsuite name="(empty)" tests="4" failures="1" skipped="1">
  <testcase name="test_ran" classname="test_ran" status="run"/>
  <testcase name="test_tier" classname="test_tier" status="notrun">
    <skipped message="SKIP_RETURN_CODE=77"/>
    <system-out>SKIP: this CPU lacks the extension</system-out>
  </testcase>
  <testcase name="test_failed" classname="test_failed" status="fail">
    <failure message="Failed"/>
  </testcase>
  <testcase name="validate_x_check" status="run"/>
</testsuite>
"""


def _records(tmp_path: Path, junit: str = JUNIT) -> tuple[Path, Path]:
    """Write ctest's two records; the tier test's command is NOT its name."""
    bin_dir = tmp_path / "build" / "native"
    tests = {
        "test_ran": bin_dir / "test_ran",
        # add_test(NAME test_tier COMMAND tier_exe): the executable comes
        # from the command, never from the test's name.
        "test_tier": bin_dir / "tier_exe",
        "test_failed": bin_dir / "test_failed",
        "validate_x_check": bin_dir / "validate_x",
    }
    j = tmp_path / "ctest-junit.xml"
    j.write_text(junit, encoding="utf-8")
    t = tmp_path / "ctest-tests.json"
    t.write_text(
        json.dumps(
            {
                "kind": "ctestInfo",
                "tests": [
                    {"name": n, "command": [str(p), "--check"]}
                    for n, p in tests.items()
                ]
                + [{"name": "fixture_only"}],  # no command at all
            }
        ),
        encoding="utf-8",
    )
    return j, t


def _run(
    junit: Path, tests: Path, stdin: str, cwd: Path
) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [
            sys.executable,
            str(SCRIPT),
            "--junit",
            str(junit),
            "--tests",
            str(tests),
        ],
        input=stdin,
        capture_output=True,
        text=True,
        cwd=cwd,
    )


def test_drops_only_the_skipped_executable(tmp_path: Path) -> None:
    j, t = _records(tmp_path)
    # RELATIVE paths, as the recipe's `find $(COV_DIR)` prints them, against
    # ctest's ABSOLUTE commands: the comparison must resolve both.
    candidates = [
        "build/native/test_ran",
        "build/native/tier_exe",
        "build/native/test_failed",
        "build/native/validate_x",
    ]
    r = _run(j, t, "\n".join(candidates) + "\n", tmp_path)
    assert r.returncode == 0, r.stderr
    assert r.stdout.splitlines() == [
        "build/native/test_ran",
        "build/native/test_failed",  # a failure is not a skip
        "build/native/validate_x",
    ]
    assert "SKIPPED on this host" in r.stderr
    assert "tier_exe" in r.stderr


def test_no_skips_is_silent_and_keeps_everything(tmp_path: Path) -> None:
    j, t = _records(
        tmp_path,
        junit='<testsuite><testcase name="test_tier" status="run"/>'
        "</testsuite>",
    )
    r = _run(j, t, "build/native/tier_exe\n", tmp_path)
    assert r.returncode == 0, r.stderr
    assert r.stdout == "build/native/tier_exe\n"
    assert r.stderr == ""


def test_missing_record_fails_instead_of_passing_through(
    tmp_path: Path,
) -> None:
    # Passing every candidate through when ctest left no record would put
    # back the runner-dependent failure and look like it had worked.
    _, t = _records(tmp_path)
    r = _run(tmp_path / "absent.xml", t, "build/native/tier_exe\n", tmp_path)
    assert r.returncode == 2
    assert r.stdout == ""
    assert "cannot read ctest's records" in r.stderr


def test_docstring_examples_run() -> None:
    r = subprocess.run(
        [sys.executable, "-m", "doctest", str(SCRIPT)],
        capture_output=True,
        text=True,
    )
    assert r.returncode == 0, r.stdout + r.stderr
