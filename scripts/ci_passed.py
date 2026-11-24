#!/usr/bin/env python3
"""The verdict of ci.yml's ``CI passed``, the one required check.

Every job in the aggregator's ``needs`` must have SUCCEEDED, with exactly four
exceptions. The first two need the ``changes`` job itself to have succeeded,
the last two the ``scope`` job (scripts/ci_scope.py):

- **nothing untested.** A job in ``SKIPPABLE`` may be SKIPPED when
  ``changes`` said ``src=false``: a version bump alone (``make
  ci-changes``), or a push whose tree already passed as its PR (``make
  ci-tree-tested``).
- **nothing but docs.** A job in ``CODE_ONLY`` -- one docs cannot break --
  may be SKIPPED when ``changes`` said ``code=false`` (``make ci-docs``; it
  is also false whenever ``src`` is). Only an explicit ``"false"`` counts.

- **a push without a pull request.** A job in ``PR_ONLY`` may be SKIPPED when
  ``scope`` said ``minimal``: CI runs on every push, but only the cheap,
  telling jobs, and the rest wait for the PR's own full run.
- **a push the PR already covers.** When ``scope`` said ``skip`` (the pushed
  sha is the head of an open PR) every job but the failures may be skipped:
  the PR's run is the gate, and this one only reports that.

Those are the ONLY ways a skip is green:

- ``failure`` or ``cancelled``: always red. A job killed at its timeout ends
  ``cancelled``, and treating that as a pass is how a gate stops gating.
- a skip when ``src`` is anything but ``"false"`` (``true``, or empty because
  ``changes`` never ran): red, as it always was.
- a skip of a job in neither list: red even on a bump. The cheap gates
  that check the files a bump DOES change -- lint, manifest drift, the CI
  image pin (its hash covers bootstrap.toml) -- are not in the list, so a
  lint failure on a release PR still blocks it. just-makeit's aggregator
  exits 0 outright on ``src=false``; this one does not.

Inputs are environment variables, so a test drives it with any results:
``NEEDS`` is ``toJSON(needs)``; ``SKIPPABLE``, ``CODE_ONLY`` and ``PR_ONLY``
are space-separated job ids.

A ``failure`` or ``cancelled`` is red under every scope: ``scope`` changes
what may be skipped, never what may fail.
Standard library only: it runs on the bare runner.
"""

from __future__ import annotations

import json
import os
import sys


def verdict(
    needs: dict,
    skippable: set[str],
    code_only: frozenset[str] = frozenset(),
    pr_only: frozenset[str] = frozenset(),
) -> tuple[bool, list[str]]:
    """``(green, report lines)`` for one set of ``needs`` results."""
    scope_job = needs.get("scope", {})
    scope = (
        (scope_job.get("outputs") or {}).get("scope", "")
        if scope_job.get("result") == "success"
        else ""
    )
    minimal = scope == "minimal"
    covered = scope == "skip"
    changes = needs.get("changes", {})
    outputs = changes.get("outputs") or {}
    ran = changes.get("result") == "success"
    src = outputs.get("src", "")
    fast = ran and src == "false"
    docs_only = ran and outputs.get("code", "") == "false"
    lines: list[str] = []
    green = True
    for job, info in sorted(needs.items()):
        result = info.get("result", "")
        if result == "success":
            continue
        if result == "skipped" and fast and job in skippable:
            lines.append(f"  skipped (nothing untested): {job}")
            continue
        if result == "skipped" and docs_only and job in code_only:
            lines.append(f"  skipped (docs cannot break it): {job}")
            continue
        if result == "skipped" and minimal and job in pr_only:
            lines.append(f"  skipped (left to the PR run): {job}")
            continue
        if result == "skipped" and covered:
            lines.append(f"  skipped (the PR run is the gate): {job}")
            continue
        green = False
        if result == "skipped":
            if job in code_only and not docs_only:
                why = (
                    "docs cannot break it, but code changed "
                    f"(code={outputs.get('code') or 'unset'})"
                )
            elif minimal:
                why = "it is not one a push without a PR may skip"
            elif fast:
                why = "it is not one the fast path may skip"
            else:
                why = f"no bump-only classification (src={src or 'unset'})"
            lines.append(f"::error::{job} was skipped: {why}")
        else:
            lines.append(f"::error::{job} ended '{result}'")
    if not green:
        lines.append(
            "::error::a required CI job did not succeed — CI not green"
        )
    elif covered:
        lines.append(
            "CI passed: this commit is the head of an open pull request; "
            "that run is the gate, and this one ran nothing."
        )
    elif minimal:
        lines.append(
            "CI passed: a push without a pull request ran the minimal set "
            "-- the rest waits for the PR's full run."
        )
    elif fast:
        lines.append(
            "CI passed: nothing untested (a bump, or a tree its PR tested) "
            "-- the matrix was skipped, every gate that ran is green."
        )
    elif docs_only:
        lines.append(
            "CI passed: docs only -- the jobs docs cannot break were "
            "skipped, every gate that ran is green."
        )
    else:
        lines.append("All required CI jobs succeeded.")
    return green, lines


def main() -> int:
    needs = json.loads(os.environ["NEEDS"])
    skippable = set(os.environ.get("SKIPPABLE", "").split())
    code_only = frozenset(os.environ.get("CODE_ONLY", "").split())
    pr_only = frozenset(os.environ.get("PR_ONLY", "").split())
    green, lines = verdict(needs, skippable, code_only, pr_only)
    print("\n".join(lines))
    return 0 if green else 1


if __name__ == "__main__":
    sys.exit(main())
