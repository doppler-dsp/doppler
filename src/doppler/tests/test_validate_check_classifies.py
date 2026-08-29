"""The validate-check gate, exercised over fake validators.

`scripts/check_validation_reports.py` exists because the classification it
does used to be a shell loop inside `make validate-check`, where any
non-zero exit read as STALE. That named two failures the same and answered
both with `make validate` -- and the common one was the other one: in a
fresh worktree `make build` does not build the Python extensions, so every
validator dies importing `doppler.*` and all 21 reports read "stale".
Re-rendering a report cannot fix an import (doppler#1074).

The cases below are about the gate's own failure modes rather than about
any object's report. Each seeds fake validators, because a gate that can
only be tested against the real tree cannot be sabotaged: you would have to
break every validation report to check it, and nobody does that twice.

The distinction is load-bearing in BOTH directions. A crash reported as
STALE sends the reader to the wrong remedy. A stale report reported as an
error would hide a real drift behind "could not run" -- so each case
asserts the other classification is *absent*, not merely that its own is
present.
"""

from __future__ import annotations

import subprocess
import sys
from typing import TYPE_CHECKING

from doppler.tests._repo import repo_root
from doppler.tests._validation_common import EXIT_STALE

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)
SCRIPT = REPO / "scripts" / "validate_check.py"

#: A validator whose committed report matches a fresh run.
_FRESH = "import sys\nprint('  up to date: results.md')\nsys.exit(0)\n"

#: One whose report has drifted: the marker, a diff, and EXIT_STALE.
_STALE = (
    "import sys\n"
    "print('  ok so far')\n"
    "print('  STALE: results.md no longer matches its generator')\n"
    "print('    -old limit line')\n"
    "print('    +new limit line')\n"
    f"sys.exit({EXIT_STALE})\n"
)

#: One that never decided. This is the shape a missing extension takes.
_BROKEN = (
    "raise ModuleNotFoundError(\n"
    "    \"No module named 'doppler.util.util'\"\n"
    ")\n"
)


def _seed(tmp_path: Path, bodies: dict[str, str]) -> list[str]:
    paths = []
    for name, body in bodies.items():
        p = tmp_path / name
        p.write_text(body, encoding="utf-8")
        paths.append(str(p))
    return paths


def _run(validators: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(SCRIPT), *validators],
        capture_output=True,
        text=True,
    )


def test_all_fresh_passes(tmp_path: Path) -> None:
    r = _run(_seed(tmp_path, {"a.py": _FRESH, "b.py": _FRESH}))
    assert r.returncode == 0, r.stdout
    assert "2 report(s) up to date" in r.stdout


def test_a_stale_report_is_stale_and_says_make_validate(
    tmp_path: Path,
) -> None:
    r = _run(_seed(tmp_path, {"a.py": _FRESH, "b.py": _STALE}))
    assert r.returncode == 1
    assert "STALE — " in r.stdout
    assert "run 'make validate'" in r.stdout
    # The diff is the diagnosis; a filename alone costs a round trip.
    assert "+new limit line" in r.stdout
    # And it must NOT be called an error, or a real drift hides behind
    # "could not run" and nobody re-renders the report.
    assert "ERROR — " not in r.stdout
    assert "make pyext" not in r.stdout


def test_a_crashed_validator_is_an_error_not_stale(tmp_path: Path) -> None:
    """The whole point of doppler#1074."""
    r = _run(_seed(tmp_path, {"a.py": _FRESH, "b.py": _BROKEN}))
    assert r.returncode == 1
    assert "ERROR — " in r.stdout
    # The traceback IS the diagnosis -- the old sed replay discarded it.
    assert "ModuleNotFoundError" in r.stdout
    # It must name the remedy that works, and not the one that cannot.
    assert "make pyext" in r.stdout
    assert "STALE — " not in r.stdout
    assert "run 'make validate'" not in r.stdout


def test_both_kinds_are_reported_separately(tmp_path: Path) -> None:
    """One of each: neither classification may swallow the other."""
    r = _run(
        _seed(tmp_path, {"a.py": _STALE, "b.py": _BROKEN, "c.py": _FRESH})
    )
    assert r.returncode == 1
    assert "1 stale" in r.stdout
    assert "1 could not run" in r.stdout


def test_no_validators_is_a_usage_error_not_a_pass(tmp_path: Path) -> None:
    """An empty list must not read as "everything is up to date".

    The old loop iterated `$(VALIDATORS)` and printed OK for an empty
    expansion -- a gate that checks nothing reports success, which is the
    failure mode a typo'd variable produces.
    """
    r = _run([])
    assert r.returncode == 2
    assert "OK" not in r.stdout
