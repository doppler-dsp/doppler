#!/usr/bin/env python3
"""A Python-only validation run exits 0; the full run still needs C harnesses.

``make validate`` runs every Python validator and then ``validate-c``, whose
guard refuses an empty harness list: the glob over ``native/validation/*.c``
finding nothing means the tree is broken. Before #2048 the only way to
regenerate one Python-only object was ``make validate VALIDATORS=<it>
VALIDATORS_C=``, and the guard refused that too, so the correct use of the
target exited 2 (#2033, #2047). The guard cannot tell "no C harness wanted"
from "no C harness found". So the Python half became its own target,
``validate-py``, and the guard stays for the run that wants both.

This holds that by running the REAL targets with stub validators, each of
which prints a sentinel; one then exits 0 and the other exits 1. Four
cases, because an exit status is only evidence if both values are tested:

1. ``make validate-py VALIDATORS=<passing stub>`` must exit 0 having run
   it, on this tree, where harness sources ARE tracked. ``BUILD_DIR``
   points at an empty directory, so a run that reached ``validate-c``
   would find every harness unbuilt and fail.
2. ``make validate-py VALIDATORS=<failing stub>`` must exit non-zero. A
   loop that drops a validator's status turns a crash into a regenerated
   report.
3. ``make validate-py VALIDATORS=`` must refuse. A loop over nothing exits
   0 having done nothing, which is the same lie as case 2.
4. ``make validate VALIDATORS=<passing stub>`` on a tree whose glob finds
   no harness must run the stub, then exit non-zero with ``validate-c``'s
   refusal. The empty glob is made by pointing ``GIT_INDEX_FILE`` at a file
   that does not exist: ``VALIDATORS_C`` is derived from ``git ls-files``,
   so the real derivation runs and comes back empty, with no copy of the
   tree.

No real validator and no C harness runs. The stubs are the only Python
that runs, through the ``uv run`` the recipe uses, so nothing is built
beyond the venv ``make lint`` already syncs. Plain python3: the standard
library only.

Usage
-----
    python3 scripts/check_validate_spellings.py   # exit 1 on any failure
"""

from __future__ import annotations

import os
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
NAME = "validate-spellings-check"

#: What every stub prints, so a run that exits as expected without having
#: run it is caught.
SENTINEL = f"{NAME}: stub validator ran"

#: The start of each target's refusal of an empty list.
PY_GUARD = "validate-py: no validators"
C_GUARD = "validate-c: no harness sources found"


def _make(
    target: str, validators: str, build: Path, **env: str
) -> subprocess.CompletedProcess[str]:
    """Run ``make <target>`` for real, with the given validators.

    Parameters
    ----------
    target : str
        ``validate-py`` or ``validate``.
    validators : str
        The value for ``VALIDATORS`` on make's command line; empty to test
        the refusal.
    build : Path
        An empty directory, passed as ``BUILD_DIR``, so that no C harness
        is ever found built and none can run.
    **env : str
        Added to the environment, e.g. ``GIT_INDEX_FILE``.

    Returns
    -------
    subprocess.CompletedProcess
        The finished run, its output decoded as UTF-8: the recipes print
        "--" and the C half's summary prints an arrow.

    Notes
    -----
    Make's own variables are dropped from the environment first. Run under
    ``make lint``, make exports its command line to children through
    MAKEFLAGS, and a ``VALIDATORS_C=`` there would reach these runs.
    """
    drop = {"MAKEFLAGS", "MFLAGS", "MAKELEVEL"}
    clean = {k: v for k, v in os.environ.items() if k not in drop}
    return subprocess.run(
        [
            "make",
            "--no-print-directory",
            target,
            f"VALIDATORS={validators}",
            f"BUILD_DIR={build}",
        ],
        cwd=ROOT,
        env=clean | env,
        capture_output=True,
        text=True,
        encoding="utf-8",
        check=False,
    )


def _fail(why: str, r: subprocess.CompletedProcess[str]) -> int:
    """Print which promise broke and the run that broke it; return 1."""
    print(f"{NAME}: {why} (exit {r.returncode})")
    for line in (r.stdout + r.stderr).splitlines():
        print(f"    {line}")
    return 1


def main() -> int:
    """Run the four cases in order and stop at the first that fails.

    Returns
    -------
    int
        0 when every case holds, 1 at the first that does not.
    """
    with tempfile.TemporaryDirectory() as d:
        tmp = Path(d)
        build = tmp / "build"
        build.mkdir()
        passes = tmp / "passes" / "validate.py"
        fails = tmp / "fails" / "validate.py"
        for stub, status in ((passes, 0), (fails, 1)):
            stub.parent.mkdir()
            stub.write_text(
                f"print({SENTINEL!r})\nraise SystemExit({status})\n",
                encoding="utf-8",
            )
        no_index = {"GIT_INDEX_FILE": str(tmp / "no-such-index")}

        # (the promise, target, VALIDATORS, extra env, exits 0?, must print)
        cases = [
            (
                "`make validate-py VALIDATORS=<one>` must run that validator "
                "and exit 0 on a tree with C harnesses",
                "validate-py",
                str(passes),
                {},
                True,
                [SENTINEL],
            ),
            (
                "`make validate-py` must exit non-zero when a validator fails",
                "validate-py",
                str(fails),
                {},
                False,
                [SENTINEL],
            ),
            (
                "`make validate-py` must refuse an empty validator list",
                "validate-py",
                "",
                {},
                False,
                [PY_GUARD],
            ),
            (
                "`make validate` must run the Python half, then refuse a "
                "tree whose harness glob finds none",
                "validate",
                str(passes),
                no_index,
                False,
                [SENTINEL, C_GUARD],
            ),
        ]
        for why, target, validators, env, exits_0, want in cases:
            r = _make(target, validators, build, **env)
            out = r.stdout + r.stderr
            held = (r.returncode == 0) == exits_0
            if not held or any(w not in out for w in want):
                return _fail(why, r)

    print(
        f"{NAME}: OK -- validate-py exits 0 on a passing validator and "
        "non-zero on a failing one or none; validate refuses an empty "
        "harness glob"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
