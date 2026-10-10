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

This holds both directions by running the REAL targets with a stub validator
that prints a sentinel and exits 0:

1. ``make validate-py VALIDATORS=<stub>`` on this tree, where harness sources
   ARE tracked, must exit 0 having run the stub. ``BUILD_DIR`` points at an
   empty directory, so a run that reached ``validate-c`` would find every
   harness unbuilt and fail.
2. ``make validate VALIDATORS=<stub>`` on a tree whose glob finds none must
   run the stub and then exit non-zero with the guard's message. The empty
   glob is made by pointing ``GIT_INDEX_FILE`` at a file that does not
   exist: ``VALIDATORS_C`` is derived from ``git ls-files``, so the real
   derivation runs and comes back empty, with no copy of the tree.

Nothing here builds anything or runs a real validator. The stub is the only
Python that runs, and the empty ``BUILD_DIR`` keeps every C harness out of
reach. Plain python3: the standard library only.

Usage
-----
    python3 scripts/check_validate_spellings.py   # exit 1 on either failure
"""

from __future__ import annotations

import os
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
NAME = "validate-spellings-check"

#: What the stub prints, so a run that exits 0 without running it is caught.
SENTINEL = f"{NAME}: stub validator ran"

#: The start of validate-c's refusal, the one line this gate depends on.
GUARD = "validate-c: no harness sources found"


def _make(
    target: str, tmp: Path, **env: str
) -> subprocess.CompletedProcess[str]:
    """Run ``make <target>`` for real, with the stub and an empty BUILD_DIR.

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
            f"VALIDATORS={tmp / 'validate.py'}",
            f"BUILD_DIR={tmp / 'build'}",
        ],
        cwd=ROOT,
        env=clean | env,
        capture_output=True,
        text=True,
        check=False,
    )


def _fail(why: str, r: subprocess.CompletedProcess[str]) -> int:
    print(f"{NAME}: {why} (exit {r.returncode})")
    for line in (r.stdout + r.stderr).splitlines():
        print(f"    {line}")
    return 1


def main() -> int:
    with tempfile.TemporaryDirectory() as d:
        tmp = Path(d)
        (tmp / "build").mkdir()
        (tmp / "validate.py").write_text(
            f"print({SENTINEL!r})\n", encoding="utf-8"
        )

        py = _make("validate-py", tmp)
        if py.returncode != 0 or SENTINEL not in py.stdout:
            return _fail(
                "`make validate-py VALIDATORS=<one>` must run that validator "
                "and exit 0 on a tree with C harnesses",
                py,
            )

        full = _make(
            "validate", tmp, GIT_INDEX_FILE=str(tmp / "no-such-index")
        )
        out = full.stdout + full.stderr
        if full.returncode == 0 or GUARD not in out:
            return _fail(
                "`make validate` must refuse a tree whose harness glob finds "
                "none",
                full,
            )
        if SENTINEL not in full.stdout:
            return _fail(
                "`make validate` must run the Python half before the C half",
                full,
            )

    print(
        f"{NAME}: OK -- validate-py ran Python alone (exit 0); validate "
        f"refused an empty harness glob (exit {full.returncode})"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
