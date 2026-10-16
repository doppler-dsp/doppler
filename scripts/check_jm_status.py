#!/usr/bin/env python3
"""The jm drift gate, plus one more question of the same run (doppler#2377).

`make drift-check` asks the pinned just-makeit whether a project's generated
glue matches its manifest: `just-makeit status --check`, whose exit code is
the verdict. This script runs that ONE status, as ``--check --json``, and
keeps jm's verdict as its own. It then asks one more question of the same
report.

**Is every ``jm_*.h`` doppler commits the pinned jm's own render?** jm writes
its C harness headers -- ``jm_bench.h``, ``jm_test.h``, ``jm_perf.h``,
``jm_simd.h`` -- once and never rewrites them (create-only). When a newer jm
ships a fixed one, ``status`` lists the old copy under ``outdated`` and counts
it as nothing. jm is right not to gate on the whole list: every create-only
file is the project's to edit once written -- a Makefile, ``.gitignore``,
``clib_common.h``, and the harness headers too (jm's ``_createonly`` says so of
``jm_test.h``: create-only "precisely so a project may extend it").

Gating the ``jm_*.h`` headers is doppler's POLICY, not jm's classification:
doppler never edits one it commits, so a fix jm ships reaches it by
re-vendoring, and a copy that differs from the pinned render is a stale
vendor, never an edit.
That is how ``jm_bench.h`` kept a 32-row cap for three releases after jm
fixed it (just-makeit#2188, doppler#2062), with ``status`` saying OUTDATED on
every run and nothing failing. A header doppler needs to extend gets its own
file beside jm's, which this gate does not see.

**Only the headers git tracks.** The root project DECLINES ``jm_test.h``: it
is gitignored and ``status_allow``-ed missing, because doppler's C tests run
on ``dp_test.h``. ``jm apply`` still writes one into every checkout that runs
it, and jm compares whatever is on disk. Gating that file would turn every
developer's ``make lint`` red on a file in no diff the day jm's template
moves, while CI's fresh checkout has none and stays green -- and ``apply``
could not clear it. So the report is filtered through ``git ls-files``
(`tracked`); the downstream example, which commits its ``jm_test.h``, is
still gated on it.

The comparison is jm's own: ``outdated`` diffs each file against the tree
``apply`` renders for this project at this pin, placeholders filled and
formatters run. Nothing here renders a template, so there is no second
implementation to drift from the first.

``--fix`` re-vendors: it deletes each outdated, tracked ``jm_*.h`` and runs
``just-makeit apply``, which writes a missing create-only file from the
pinned template. That is the one way jm offers to replace one, and it is
why this check can fail a CI run: there is a command that clears it. It
refuses a header with uncommitted changes, which deleting would lose.
just-makeit#2285 would have ``apply`` refresh a JM harness header only when
generated code needs a symbol the header lacks; a fix like the 32-row cap
adds none, so ``--fix`` stays until jm can replace a stale header itself.

Usage (from the project root; the Makefile is the driver)::

    python scripts/check_jm_status.py          # gate: exit 0, 1 or jm's
    python scripts/check_jm_status.py --fix    # re-vendor, then apply

On a failed status the text report is printed by a second run, so the
reader sees jm's own explanation. A passing run costs one status, as before.

Examples
--------
An entry ``status_allow`` allows still counts (see `vendored_outdated`):

>>> vendored_outdated(
...     {
...         "outdated": [
...             {"path": "Makefile", "allowed": False},
...             {"path": "native/benchmarks/jm_bench.h", "allowed": False},
...             {"path": "native/inc/doppler/jm_perf.h", "allowed": True},
...         ]
...     }
... )
['native/benchmarks/jm_bench.h', 'native/inc/doppler/jm_perf.h']
>>> vendored_outdated({"outdated": []})
[]
"""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
from pathlib import Path
from typing import Any

#: A jm harness header. One doppler commits it keeps verbatim (policy, not
#: jm's); one it declines is untracked and never gated (see `tracked`).
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
    drift in a header doppler keeps verbatim would leave the stale copy
    this gate exists to catch, so the allowance is not honored here.

    The list must be THERE. A report without it -- a jm that renamed the
    key -- raises ``KeyError`` rather than reading as an empty list, which
    would pass every run from then on: #2377 again.
    """
    return sorted(
        e["path"] for e in report["outdated"] if VENDORED.search(e["path"])
    )


def _git(root: Path, *args: str) -> str:
    """Run ``git *args`` in *root* and return its stdout.

    A failure raises: a gate that cannot ask git which headers are committed
    must not read the answer as "none". Git finds the repository from
    *root*, as a person running the command would: an inherited ``GIT_*``
    variable is dropped. ``git rebase -x 'make lint'`` in a linked worktree
    exports ``GIT_DIR``, and with it set git takes the downstream example's
    directory for the top of the work tree, so it lists only paths the root
    also tracks there: the example's ``jm_test.h`` went ungated.
    """
    env = {k: v for k, v in os.environ.items() if not k.startswith("GIT_")}
    return subprocess.run(
        ["git", *args],
        cwd=root,
        env=env,
        stdout=subprocess.PIPE,
        text=True,
        check=True,
    ).stdout


def tracked(root: Path, paths: list[str]) -> list[str]:
    """The members of *paths* (relative to *root*) that git tracks.

    jm reports a create-only file that is on disk, committed or not, and
    ``apply`` writes the root's declined ``jm_test.h`` into every checkout
    that runs it. Only a committed header is doppler's copy to keep current.
    """
    if not paths:
        return []
    have = set(_git(root, "ls-files", "-z", "--", *paths).split("\0"))
    return [p for p in paths if p in have]


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
    if "outdated" not in report:
        print(
            "check_jm_status: jm's --json report has no `outdated` list, "
            "so the vendored headers cannot be checked. Read the pinned "
            "jm's report shape and update this script (doppler#2377)."
        )
        return 2
    stale = tracked(root, vendored_outdated(report))
    if stale:
        print(
            "check_jm_status: vendored jm header(s) behind the pinned "
            "just-makeit (doppler#2377):"
        )
        for p in stale:
            print(f"  {p}")
        print(
            "  doppler keeps the jm headers it commits verbatim, so a "
            "difference is a stale copy, not an edit.\n"
            "  Re-vendor: make jm-revendor   (deletes each one, then "
            "`just-makeit apply` writes the pinned render)"
        )
        return 1
    print(
        f"check_jm_status: OK -- no drift; every committed jm_*.h in "
        f"{root.resolve().name} is the pinned jm's render"
    )
    return 0


def fix(root: Path) -> int:
    """Re-vendor every outdated, tracked ``jm_*.h`` in *root* (``apply``).

    It needs jm's report to know what is stale, so a status that wrote
    none, or one without its ``outdated`` list, fails rather than reading
    as "nothing to re-vendor". A header with uncommitted changes is refused
    before anything in *root* is deleted: re-vendoring would lose them
    silently. (``make jm-revendor`` runs this once per project, so a refusal
    in the second leaves the first re-vendored.)
    """
    rc, report = status(root)
    if "outdated" not in report:
        print("check_jm_status: no report from jm; nothing was changed")
        return rc or 2
    stale = tracked(root, vendored_outdated(report))
    edited = _git(root, "status", "--porcelain", "--", *stale) if stale else ""
    if edited:
        print(
            "check_jm_status: uncommitted changes to a header it would "
            "re-vendor; commit or discard them first (nothing was changed):"
        )
        sys.stdout.write(edited)
        return 1
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
