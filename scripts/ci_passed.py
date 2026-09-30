#!/usr/bin/env python3
"""The verdict of ci.yml's ``CI passed``, the one required check.

Every job in the aggregator's ``needs`` must have SUCCEEDED, with exactly two
exceptions, and both need the ``changes`` job itself to have succeeded:

- **a version bump alone.** A job in ``SKIPPABLE`` may be SKIPPED when
  ``changes`` classified the diff as a bump (``make ci-changes`` said
  ``src=false``). A release commit re-tests nothing its parent passed.
- **a pull_request run.** A job in ``HEAVY`` may be SKIPPED when ``changes``
  said ``full=false``. A PR gets the fast gates; the full matrix runs in the
  merge queue (merge_group) and on push, where ``full=true`` and a skipped
  heavy job is red. An unset ``full`` counts as a full run.

Those are the ONLY ways a skip is green:

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
``NEEDS`` is ``toJSON(needs)``; ``SKIPPABLE`` and ``HEAVY`` are
space-separated job ids.
Standard library only: it runs on the bare runner.
"""

from __future__ import annotations

import json
import os
import sys


def verdict(
    needs: dict, skippable: set[str], heavy: frozenset[str] = frozenset()
) -> tuple[bool, list[str]]:
    """``(green, report lines)`` for one set of ``needs`` results."""
    changes = needs.get("changes", {})
    outputs = changes.get("outputs") or {}
    ran = changes.get("result") == "success"
    src = outputs.get("src", "")
    fast = ran and src == "false"
    # Only an explicit "false" is a pull_request run; anything else is full.
    light = ran and outputs.get("full", "") == "false"
    lines: list[str] = []
    green = True
    for job, info in sorted(needs.items()):
        result = info.get("result", "")
        if result == "success":
            continue
        if result == "skipped" and fast and job in skippable:
            lines.append(f"  skipped (version bump alone): {job}")
            continue
        if result == "skipped" and light and job in heavy:
            lines.append(f"  skipped (heavy; runs in the merge queue): {job}")
            continue
        green = False
        if result == "skipped":
            if job in heavy and not light:
                why = (
                    "it is heavy and this is a full run "
                    f"(full={outputs.get('full') or 'unset'}), so it must run"
                )
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
    elif light and any(
        needs.get(j, {}).get("result") == "skipped" for j in heavy
    ):
        lines.append(
            "CI passed: the fast gates of a pull_request -- the heavy jobs "
            "run in the merge queue, and every gate that ran is green."
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
    heavy = frozenset(os.environ.get("HEAVY", "").split())
    green, lines = verdict(needs, skippable, heavy)
    print("\n".join(lines))
    return 0 if green else 1


if __name__ == "__main__":
    sys.exit(main())
