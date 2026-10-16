#!/usr/bin/env python3
"""The jm drift gate, plus one more question of the same run (doppler#2377).

`make drift-check` asks the pinned just-makeit whether a project's generated
glue matches its manifest: `just-makeit status --check`, whose exit code is
the verdict. This script runs that ONE status, as ``--check --json``, and
keeps jm's verdict as its own. It then asks one more question of the same
report.

**Is every vendored ``jm_*.h`` the pinned jm's own render?** jm writes its C
harness headers -- ``jm_bench.h``, ``jm_test.h``, ``jm_perf.h``, ``jm_simd.h``
-- once and never rewrites them (create-only). When a newer jm ships a fixed
one, ``status`` lists the old copy under ``outdated`` and counts it as
nothing. jm is right not to gate on the whole list: a project's Makefile,
``.gitignore`` and ``clib_common.h`` are create-only too, and edited on
purpose. A ``jm_*.h`` is not -- the content is jm's (``_createonly`` classes
each one ``JM``). When doppler's copy is behind, it is a stale vendor. That is
how ``jm_bench.h`` kept a 32-row cap for three releases after jm fixed it
(just-makeit#2188, doppler#2062), with ``status`` saying OUTDATED on every
run and nothing failing.

The comparison is jm's own: ``outdated`` diffs each file against the tree
``apply`` renders for this project at this pin, placeholders filled and
formatters run. Nothing here renders a template, so there is no second
implementation to drift from the first.

``--fix`` re-vendors: it deletes each outdated ``jm_*.h`` and runs
``just-makeit apply``, which writes a missing create-only file from the
pinned template. That is the one way jm offers to replace one, and it is
why this check can fail a CI run: there is a command that clears it.

Usage (from the project root; the Makefile is the driver)::

    python scripts/check_jm_status.py          # gate: exit 0, 1 or jm's
    python scripts/check_jm_status.py --fix    # re-vendor, then apply

On a failed status the text report is printed by a second run, so the
reader sees jm's own explanation. A passing run costs one status, as before.

Examples
--------
>>> vendored_outdated(
...     {
...         "outdated": [
...             {"path": "Makefile", "allowed": False},
...             {"path": "native/benchmarks/jm_bench.h", "allowed": False},
...             {"path": "native/inc/doppler/jm_perf.h", "allowed": True},
...         ]
...     }
... )
['native/benchmarks/jm_bench.h']
>>> vendored_outdated({"outdated": []})
[]
"""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path
from typing import Any

#: A jm harness header: content jm owns outright, never a project's edit.
VENDORED = re.compile(r"(^|/)jm_[a-z0-9_]+\.h$")


def _jm() -> list[str]:
    """The pinned jm's console script, beside the interpreter running us.

    `make` runs this under ``uv run`` after ``uv sync``, so the venv's
    ``just-makeit`` is the pin -- not whichever one PATH reaches first.
    """
    return [str(Path(sys.executable).parent / "just-makeit")]


def vendored_outdated(report: dict[str, Any]) -> list[str]:
    """Paths in *report*'s ``outdated`` list that are a ``jm_*.h``.

    An entry ``allowed`` by ``[project] status_allow`` counts too: allowing
    drift in a file jm owns would leave the stale copy this gate exists to
    catch, so the allowance is not honored here.
    """
    return sorted(
        e["path"]
        for e in report.get("outdated", [])
        if VENDORED.search(e["path"])
    )


def status(root: Path) -> tuple[int, dict[str, Any]]:
    """Run ``just-makeit status --check --json`` in *root*.

    Returns jm's exit code and its parsed report. Warnings go to stderr,
    untouched, so they read as they always have.
    """
    r = subprocess.run(
        [*_jm(), "status", "--check", "--json"],
        cwd=root,
        stdout=subprocess.PIPE,
        text=True,
    )
    try:
        report = json.loads(r.stdout)
    except ValueError:
        # No report at all: jm failed before it could write one. Its own
        # stderr already said why; never read that as an empty list.
        sys.stdout.write(r.stdout)
        return (r.returncode or 2), {}
    return r.returncode, report


def check(root: Path) -> int:
    """The gate: jm's drift verdict, then the vendored headers."""
    rc, report = status(root)
    if rc != 0:
        # Drift. Re-run for the human report, which --json replaces.
        subprocess.run([*_jm(), "status", "--check"], cwd=root)
        return rc
    stale = vendored_outdated(report)
    if stale:
        print(
            "check_jm_status: vendored jm header(s) behind the pinned "
            "just-makeit (doppler#2377):"
        )
        for p in stale:
            print(f"  {p}")
        print(
            "  These are jm's own files, so a difference is a stale copy, "
            "not an edit.\n"
            "  Re-vendor: make jm-revendor   (deletes each one, then "
            "`just-makeit apply` writes the pinned render)"
        )
        return 1
    print(
        f"check_jm_status: OK -- no drift; every vendored jm_*.h in "
        f"{root.resolve().name} is the pinned jm's render"
    )
    return 0


def fix(root: Path) -> int:
    """Re-vendor every outdated ``jm_*.h`` in *root* through ``apply``."""
    _, report = status(root)
    stale = vendored_outdated(report)
    for p in stale:
        (root / p).unlink()
        print(f"check_jm_status: re-vendoring {p}")
    if not stale:
        print("check_jm_status: nothing to re-vendor")
        return 0
    return subprocess.run([*_jm(), "apply"], cwd=root).returncode


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--fix", action="store_true", help="re-vendor")
    args = ap.parse_args(argv)
    root = Path.cwd()
    return fix(root) if args.fix else check(root)


if __name__ == "__main__":
    sys.exit(main())
