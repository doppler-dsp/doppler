#!/usr/bin/env python3
"""Gate: ``make test-all`` runs every test target CI runs.

``test-all`` is the local "run everything" a contributor reaches for before
calling a branch ready. It ran ``test test-examples test-python
test-examples-python`` while CI also ran ``test-snippets``, ``test-rust``,
``test-stubs``, ``test-api-docs`` and ``test-sweep``. So a branch could pass
``test-all`` and fail CI. #2886 did, twice: a stale ``.c-header-code-ignore``
entry and a broken import that only the doc-fence gates collect.

The two lists were kept by hand, in two files. This gate makes CI's list the
source and ``TEST_ALL_DEPS`` answer to it, in both directions:

1. Every ``make test`` / ``make test-<x>`` that a workflow under
   ``.github/workflows/`` runs is in ``TEST_ALL_DEPS``, or has a line in
   ``scripts/.test-all-exempt`` saying why it cannot run locally.
2. Every exempt line names a target CI still runs and ``TEST_ALL_DEPS`` still
   leaves out, so the list only shrinks: a line whose reason is gone fails.

A target chosen from a matrix -- ``make ${{ matrix.san.target }}`` -- is
resolved against that job's ``strategy.matrix`` (its axis lists and
``include`` entries), so each leg's target counts as one CI runs. The
sanitizer job is that shape, and reading only literal text missed all three
of its targets (#2912).

Exempt lines are ``<target> | <reason>``; ``#`` comments and blank lines are
ignored. A line with no reason fails.

Usage::

    python scripts/check_test_all_covers_ci.py            # the gate
    python scripts/check_test_all_covers_ci.py --root DIR # another tree

Examples
--------
>>> sorted(ci_targets("run: make -s test-python TEST_PATHS=x\\nmake test"))
['test', 'test-python']
>>> job = {
...     "strategy": {"matrix": {"san": [{"target": "test-asan"}]}},
...     "steps": [{"run": "make ${{ matrix.san.target }}"}],
... }
>>> sorted(matrix_targets({"jobs": {"j": job}}))
['test-asan']
>>> sorted(test_all_deps("TEST_ALL_DEPS = test \\\\\\n    test-python\\n"))
['test', 'test-python']
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path
from typing import Any

import yaml

#: A `make` call naming a test target, with any `-s`/`-k` style flags first.
MAKE_TEST = re.compile(r"\bmake\s+(?:-[A-Za-z-]+\s+)*(test(?:-[a-z0-9-]+)?)\b")
#: A `make` call whose target is a matrix expression, e.g.
#: `make ${{ matrix.san.target }}`; group 1 is the dotted path after `matrix.`.
MAKE_MATRIX = re.compile(
    r"\bmake\s+(?:-[A-Za-z-]+\s+)*\$\{\{\s*matrix\.([\w.-]+)\s*\}\}"
)
#: What a test target looks like, for a value read out of a matrix.
TEST_TARGET = re.compile(r"test(?:-[a-z0-9-]+)?")
#: The one assignment of the dependency list (doppler overrides standard.mk's
#: `?=` default with a plain `=`).
DEPS = re.compile(r"^TEST_ALL_DEPS\s*[:?]?=\s*(.*)$", re.M)


def ci_targets(text: str) -> set[str]:
    """Every test target a workflow's text runs through make."""
    return set(MAKE_TEST.findall(text))


def _matrix_values(matrix: dict[str, Any], path: list[str]) -> set[str]:
    """Every value ``matrix.<path>`` takes across a job's legs.

    The first path element names an axis (a list) or an ``include`` key;
    the rest index into each entry, so ``san.target`` reads ``target`` from
    every item of the ``san`` list.
    """
    axis, rest = path[0], path[1:]
    entries = list(matrix.get(axis) or [])
    entries += [i[axis] for i in matrix.get("include") or [] if axis in i]
    found: set[str] = set()
    for entry in entries:
        for key in rest:
            entry = entry.get(key) if isinstance(entry, dict) else None
        if isinstance(entry, str):
            found.add(entry)
    return found


def matrix_targets(workflow: dict[str, Any]) -> set[str]:
    """Test targets a parsed workflow runs as ``make ${{ matrix.<path> }}``."""
    found: set[str] = set()
    for job in (workflow.get("jobs") or {}).values():
        matrix = (job.get("strategy") or {}).get("matrix") or {}
        if not isinstance(matrix, dict):
            continue
        for step in job.get("steps") or []:
            for path in MAKE_MATRIX.findall(str(step.get("run", ""))):
                found |= _matrix_values(matrix, path.split("."))
    return {t for t in found if TEST_TARGET.fullmatch(t)}


def test_all_deps(makefile: str) -> set[str]:
    """``TEST_ALL_DEPS``'s targets, from the last assignment in the file."""
    # Join `\`-continued lines first: the list wraps at 79 columns.
    found = DEPS.findall(re.sub(r"\\\n", " ", makefile))
    if not found:
        sys.exit("check_test_all_covers_ci: no TEST_ALL_DEPS in the Makefile")
    return set(found[-1].split())


def read_exempt(path: Path) -> tuple[dict[str, str], list[str]]:
    """``{target: reason}`` from the exempt list, and its malformed lines."""
    held: dict[str, str] = {}
    bad: list[str] = []
    if not path.exists():
        return held, bad
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        target, _, reason = (p.strip() for p in line.partition("|"))
        if not reason:
            bad.append(f"'{target}' has no reason")
        held[target] = reason
    return held, bad


def check(root: Path) -> list[str]:
    """Every failure, as a line naming what to change."""
    ci: set[str] = set()
    for wf in sorted((root / ".github" / "workflows").glob("*.y*ml")):
        text = wf.read_text(encoding="utf-8")
        ci |= ci_targets(text)
        ci |= matrix_targets(yaml.safe_load(text) or {})
    deps = test_all_deps((root / "Makefile").read_text(encoding="utf-8"))
    exempt, errors = read_exempt(root / "scripts" / ".test-all-exempt")
    for t in sorted(ci - deps - set(exempt)):
        errors.append(
            f"CI runs `make {t}` and `make test-all` does not: add it to "
            "TEST_ALL_DEPS, or give it a line in scripts/.test-all-exempt "
            "saying why it cannot run locally"
        )
    for t in sorted(exempt):
        if t not in ci:
            errors.append(f"exempt '{t}' is not run by CI: delete its line")
        elif t in deps:
            errors.append(f"exempt '{t}' is in TEST_ALL_DEPS: delete its line")
    return errors


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--root", type=Path, default=Path(__file__).parent.parent)
    errors = check(ap.parse_args(argv).root)
    for e in errors:
        print(f"check_test_all_covers_ci: {e}")
    if not errors:
        print(
            "check_test_all_covers_ci: OK -- test-all runs every test "
            "target CI runs, or says why not"
        )
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
