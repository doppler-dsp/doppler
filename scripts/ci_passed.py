#!/usr/bin/env python3
"""The verdict of ci.yml's ``CI passed``, the one required check.

Every job in the aggregator's ``needs`` must have SUCCEEDED, with exactly one
exception: a job listed in ``SKIPPABLE`` may be SKIPPED, and only when the
``changes`` job itself succeeded and classified the diff as a version bump
alone (``make ci-changes`` said ``src=false``). That is the fast path -- a
release commit re-tests nothing its parent already passed -- and it is the
ONLY way a skip is green:

- ``failure`` or ``cancelled``: always red. A job killed at its timeout ends
  ``cancelled``, and treating that as a pass is how a gate stops gating.
- a skip when ``src`` is anything but ``"false"`` (``true``, or empty because
  ``changes`` never ran): red, as it always was.
- a skip of a job NOT in ``SKIPPABLE``: red even on a bump. The cheap gates
  that check the files a bump DOES change -- lint, manifest drift, the CI
  image pin (its hash covers bootstrap.toml) -- are not in the list, so a
  lint failure on a release PR still blocks it. just-makeit's aggregator
  exits 0 outright on ``src=false``; this one does not.

Inputs are environment variables, so a test drives it with any results:
``NEEDS`` is ``toJSON(needs)``; ``SKIPPABLE`` is space-separated job ids.
Standard library only: it runs on the bare runner.
"""

from __future__ import annotations

import json
import os
import sys


def verdict(needs: dict, skippable: set[str]) -> tuple[bool, list[str]]:
    """``(green, report lines)`` for one set of ``needs`` results."""
    changes = needs.get("changes", {})
    src = (changes.get("outputs") or {}).get("src", "")
    fast = changes.get("result") == "success" and src == "false"
    lines: list[str] = []
    green = True
    for job, info in sorted(needs.items()):
        result = info.get("result", "")
        if result == "success":
            continue
        if result == "skipped" and fast and job in skippable:
            lines.append(f"  skipped (version bump alone): {job}")
            continue
        green = False
        if result == "skipped":
            why = (
                "it is not one the fast path may skip"
                if fast
                else f"no bump-only classification (src={src or 'unset'})"
            )
            lines.append(f"::error::{job} was skipped: {why}")
        else:
            lines.append(f"::error::{job} ended '{result}'")
    if not green:
        lines.append(
            "::error::a required CI job did not succeed — CI not green"
        )
    elif fast:
        lines.append(
            "CI passed: a version bump alone -- the matrix was skipped, "
            "every gate that ran is green."
        )
    else:
        lines.append("All required CI jobs succeeded.")
    return green, lines


def main() -> int:
    needs = json.loads(os.environ["NEEDS"])
    skippable = set(os.environ.get("SKIPPABLE", "").split())
    green, lines = verdict(needs, skippable)
    print("\n".join(lines))
    return 0 if green else 1


if __name__ == "__main__":
    sys.exit(main())
