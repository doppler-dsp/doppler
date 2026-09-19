#!/usr/bin/env python3
"""Gate: a `push:` trigger says whether it wants tags.

GitHub evaluates a push trigger's ``paths:`` filter for BRANCH pushes only --
"path filters are not evaluated for pushes of tags". So a workflow whose
``push:`` names neither ``branches`` nor ``tags`` runs on every tag push,
whatever its paths say. `ci-image.yml` was that shape: it rebuilds and
publishes the CI image when ``Dockerfile.ci`` or ``bootstrap.toml`` change,
and on the v0.51.0 release tag it spent 19.4 minutes doing so for a tree
that could not have changed either.

The rule: every ``push:`` trigger declares one of ``branches``,
``branches-ignore``, ``tags`` or ``tags-ignore``. Listing branches alone is
how GitHub says "not tags"; listing tags alone says "only tags"
(release.yml). Either is a decision; the absence of both is the accident.

Usage:  python3 scripts/check_workflow_tag_triggers.py [--root DIR]
Exit 0 when every workflow's push trigger is explicit about tags.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

import yaml

ROOT = Path(__file__).resolve().parent.parent
KEYS = ("branches", "branches-ignore", "tags", "tags-ignore")


def _on(doc: dict) -> object:
    # PyYAML reads a bare `on:` key as the boolean True.
    return doc.get("on", doc.get(True))


def offenders(root: Path) -> tuple[list[str], int]:
    """(workflow files whose push fires on tags by accident, files read)."""
    bad: list[str] = []
    files = sorted((root / ".github" / "workflows").glob("*.y*ml"))
    for f in files:
        doc = yaml.safe_load(f.read_text(encoding="utf-8")) or {}
        on = _on(doc)
        if isinstance(on, str):
            on = {on: None}
        elif isinstance(on, list):
            on = dict.fromkeys(on)
        if not isinstance(on, dict) or "push" not in on:
            continue
        push = on["push"] or {}
        if not any(k in push for k in KEYS):
            bad.append(f.relative_to(root).as_posix())
    return bad, len(files)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--root", type=Path, default=ROOT)
    root = ap.parse_args().root.resolve()
    bad, n = offenders(root)
    if n == 0:
        print(
            "workflow-tag-triggers: FAIL — no workflow files found; this "
            "gate has not run, so it has not passed"
        )
        return 1
    if bad:
        print(
            "workflow-tag-triggers: FAIL — a push trigger that names "
            "neither branches nor tags runs on EVERY tag push (GitHub "
            "ignores paths: for tags):"
        )
        for b in bad:
            print(f"  {b}")
        print(
            "  Add `branches: ['**']` to keep branch pushes and drop "
            "tags, or `tags:` if the workflow is for tags."
        )
        return 1
    print(
        f"workflow-tag-triggers: OK — {n} workflow(s), every push trigger "
        "is explicit about tags"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
