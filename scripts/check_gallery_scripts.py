#!/usr/bin/env python3
"""Every committed gallery plot has a script that ``make gallery`` runs.

``make gallery`` used to hold two lists for one thing: ``GALLERY_SCRIPTS``
(the scripts it runs, and what ``release-freshness-check`` reads) and a
hand-kept list of PNG names to move into ``docs/assets/``. They disagreed.
``plan_background_demo.png`` was moved on every run while its script was
never run, so the plot went stale where no gate could see it, and v0.59.0
shipped a picture of a defect #1639 had already fixed (#1644).

This script is now the one reader of both directions:

``--outputs SCRIPT...``
    Print the PNG each script writes. ``make gallery`` moves exactly these,
    so the move list is derived from ``GALLERY_SCRIPTS`` and cannot disagree
    with it.

``SCRIPT...`` (the check, on ``make lint``)
    Fail when a listed script is missing or names no PNG (it would run and
    its plot would be lost), and when an example under
    ``src/doppler/examples/`` names a PNG that is committed under
    ``docs/assets/`` but is not in ``GALLERY_SCRIPTS`` -- the committed plot
    then has no target that re-renders it. That second direction is the one
    no derivation can close, because the missing script is the one nobody
    wrote down.

A script's plot is every quoted ``"<name>.png"`` literal in its source --
``savefig("x.png")``, ``out_path="x.png"``, ``main(out="x.png")`` all
qualify. Quoted only: a usage line such as ``[out.png]`` is prose.

Existing orphans are a RATCHET in ``scripts/.gallery-orphans-allow``: one
``<script path> | <reason>`` per line. The list may only shrink -- an entry
whose script is now listed, or whose plot is no longer committed, fails the
gate until it is deleted.

Usage
-----
``make gallery-scripts-check`` -- ``GALLERY_SCRIPTS`` is passed in rather
than restated here: a second list is one that can disagree with the first.
"""

from __future__ import annotations

import argparse
import pathlib
import re

ROOT = pathlib.Path(__file__).resolve().parent.parent
EXAMPLES = "src/doppler/examples"
ASSETS = "docs/assets"
ALLOW = ROOT / "scripts" / ".gallery-orphans-allow"

#: A quoted PNG file name: the plot a script writes by default.
PNG_RE = re.compile(r"""["']([A-Za-z0-9][A-Za-z0-9_.-]*\.png)["']""")


def plots(path: pathlib.Path) -> list[str]:
    """The PNG names *path* writes, in order of first mention, unique."""
    seen: dict[str, None] = {}
    for m in PNG_RE.finditer(path.read_text(encoding="utf-8")):
        seen.setdefault(m.group(1), None)
    return list(seen)


def read_allow(path: pathlib.Path) -> tuple[dict[str, str], list[str]]:
    """``({script: reason}, [malformed lines])`` from the ratchet file.

    A line with no reason is malformed, not an entry: the reason is the
    point, so the file cannot waive anything by accident.
    """
    entries: dict[str, str] = {}
    bad: list[str] = []
    if not path.is_file():
        return entries, bad
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        script, sep, reason = line.partition("|")
        if not sep or not reason.strip():
            bad.append(line)
            continue
        entries[script.strip()] = reason.strip()
    return entries, bad


def check(repo: pathlib.Path, listed: list[str], allow: pathlib.Path) -> int:
    rc = 0
    for s in listed:
        p = repo / s
        if not p.is_file():
            rc = 1
            print(f"gallery-scripts: {s} is in GALLERY_SCRIPTS but missing")
        elif not plots(p):
            rc = 1
            print(
                f"gallery-scripts: {s} is in GALLERY_SCRIPTS but names no "
                '"<name>.png" -- `make gallery` would run it and move nothing'
            )

    committed = {p.name for p in (repo / ASSETS).glob("*.png")}
    orphans: dict[str, list[str]] = {}
    for ex in sorted((repo / EXAMPLES).glob("*.py")):
        rel = ex.relative_to(repo).as_posix()
        if rel in listed:
            continue
        mine = [png for png in plots(ex) if png in committed]
        if mine:
            orphans[rel] = mine

    entries, bad = read_allow(allow)
    for line in bad:
        rc = 1
        print(f"gallery-scripts: allowlist line has no reason: {line!r}")

    new = {s: pngs for s, pngs in orphans.items() if s not in entries}
    if new:
        rc = 1
        print(
            f"gallery-scripts: {len(new)} example(s) name a plot committed "
            f"under {ASSETS}/ but are not in GALLERY_SCRIPTS:"
        )
        for s, pngs in new.items():
            print(f"    {s}  ->  {', '.join(pngs)}")
        print(
            "  Nothing re-renders that plot, so it goes stale unseen and the\n"
            "  release freshness gate cannot see it either (#1644). Add the\n"
            "  script to GALLERY_SCRIPTS in the Makefile."
        )

    stale = sorted(set(entries) - set(orphans))
    if stale:
        rc = 1
        print(
            "gallery-scripts: the allowlist is a ratchet and these entries "
            "no longer name an orphan -- delete them:"
        )
        for s in stale:
            print(f"    {s}")

    if rc == 0:
        print(
            f"gallery-scripts: OK -- {len(listed)} scripts, "
            f"{len(entries)} allowlisted orphan(s)"
        )
    return rc


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "--repo",
        default=str(ROOT),
        help="repo root (a test points this at its fixture)",
    )
    ap.add_argument("--allow", default=str(ALLOW), help="ratchet file")
    ap.add_argument(
        "--outputs",
        action="store_true",
        help="print each script's PNG names instead of checking",
    )
    ap.add_argument(
        "scripts", nargs="*", help="the Makefile's GALLERY_SCRIPTS"
    )
    args = ap.parse_args(argv)
    repo = pathlib.Path(args.repo)

    if args.outputs:
        for s in args.scripts:
            for png in plots(repo / s):
                print(png)
        return 0
    return check(repo, args.scripts, pathlib.Path(args.allow))


if __name__ == "__main__":
    raise SystemExit(main())
