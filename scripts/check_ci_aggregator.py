#!/usr/bin/env python3
"""Fail when a CI job does not feed the one required check, ``CI passed``.

``protect-main`` requires exactly one status check: ``CI passed``, the
``ci-passed`` job at the bottom of ``ci.yml``. That job is green only when
every job in its ``needs`` succeeded, so its ``needs`` list IS the merge gate.
A job that is not in it can fail on every pull request and block nothing.

That is not hypothetical as a shape. Until 2026-09-14 the ruleset required
four named checks, and a job's name was what made it binding; relaxing it to
the single aggregator moved that responsibility into a hand-kept YAML list
that nothing read. Adding a job and forgetting the list is exactly the kind
of mistake a green PR does not reveal -- the new job still shows red on the
PR page, it just stops mattering.

What is checked
---------------
- the aggregator job exists, is named ``CI passed`` (the string the ruleset
  requires), and runs ``if: always()`` -- without it a failed dependency
  SKIPS the aggregator, and a skipped required check does not block;
- every other job in the workflow is in its ``needs``;
- every entry in its ``needs`` names a job that exists.

Usage
-----
::

    python scripts/check_ci_aggregator.py            # .github/workflows/ci.yml
    python scripts/check_ci_aggregator.py FILE       # a named workflow
"""

from __future__ import annotations

import pathlib
import sys

import yaml

ROOT = pathlib.Path(__file__).resolve().parent.parent
WORKFLOW = ROOT / ".github" / "workflows" / "ci.yml"
AGGREGATOR = "ci-passed"
REQUIRED_NAME = "CI passed"


def check(path: pathlib.Path) -> list[str]:
    """Return every way ``path``'s aggregator fails to gate its jobs.

    Parameters
    ----------
    path : pathlib.Path
        A GitHub Actions workflow file.

    Returns
    -------
    list of str
        One human-readable problem per line; empty when the gate is sound.
    """
    try:
        doc = yaml.safe_load(path.read_text(encoding="utf-8"))
    except yaml.YAMLError as exc:
        return [f"{path}: not valid YAML: {exc}"]
    jobs = (doc or {}).get("jobs") or {}
    agg = jobs.get(AGGREGATOR)
    if agg is None:
        return [
            f"{path}: no `{AGGREGATOR}` job -- `{REQUIRED_NAME}` is the only "
            "check protect-main requires, so without it nothing gates a merge"
        ]

    problems: list[str] = []
    if agg.get("name") != REQUIRED_NAME:
        problems.append(
            f"{path}: `{AGGREGATOR}` is named {agg.get('name')!r}, but the "
            f"ruleset requires the check {REQUIRED_NAME!r} by that string"
        )
    if str(agg.get("if", "")).replace(" ", "") != "always()":
        problems.append(
            f"{path}: `{AGGREGATOR}` must run `if: always()` -- otherwise a "
            "failed dependency skips it, and a skipped check does not block"
        )

    needs = agg.get("needs") or []
    if isinstance(needs, str):
        needs = [needs]
    needs_set = set(needs)
    for job in sorted(set(jobs) - {AGGREGATOR} - needs_set):
        problems.append(
            f"{path}: job `{job}` is not in `{AGGREGATOR}`'s needs, so it "
            "gates no merge however red it goes"
        )
    for job in sorted(needs_set - set(jobs)):
        problems.append(
            f"{path}: `{AGGREGATOR}` needs `{job}`, which is not a job here"
        )
    return problems


def main(argv: list[str]) -> int:
    paths = [pathlib.Path(a) for a in argv] if argv else [WORKFLOW]
    problems = [p for path in paths for p in check(path)]
    for p in problems:
        print(f"ci-aggregator-check: {p}")
    if problems:
        return 1
    doc = yaml.safe_load(paths[0].read_text(encoding="utf-8")) or {}
    gated = len(doc.get("jobs") or {}) - 1  # every job but the aggregator
    print(
        f"ci-aggregator-check: OK -- all {gated} job(s) gate `{REQUIRED_NAME}`"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
