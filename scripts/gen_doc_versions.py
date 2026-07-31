#!/usr/bin/env python3
"""Stamp the release version into marked doc regions from pyproject.toml.

``pyproject.toml`` is where a release actually bumps the version (``make
bump-version`` writes it there, plus ``CMakeLists.txt``, ``Cargo.toml``
and ``uv.lock``). Docs that must show a concrete version -- a download
URL, an unpack command -- had no way to say so without hand-typing it,
which is precisely what ``scripts/check_version_strings.py`` exists to
forbid, and for good reason: a hand-typed version is stale at the next
tag.

Telling the reader to resolve it themselves is not the answer either. A
new user pasting

    VER=$(curl -fsSL .../releases/latest | sed -n 's/..."tag_name"...//p')

is being handed a shell pipeline to work around our documentation
problem, before they have installed anything.

So the version is GENERATED into the page, exactly like ``README.md``'s
body (``gen_readme.py``) and the per-distro install scripts
(``gen_install_scripts.py``). The page shows a clean, copy-pasteable
command; ``--check`` fails CI the moment it drifts from
``pyproject.toml``; and ``make docs-relink`` fixes it. Rot becomes a red
gate instead of a stale page.

Regions are delimited exactly like the other generators::

    <!-- doc-version:start -->
    ```bash
    curl -fsSL -O https://.../releases/download/v0.39.0/doppler-0.39.0-linux-x86_64.tar.gz
    ```
    <!-- doc-version:end -->

Inside a region every semver-looking token is rewritten to the current
version, so a region needs no placeholder syntax and stays readable and
runnable as committed. Outside a region nothing is touched --
``check_version_strings.py`` still forbids hand-typed versions
everywhere else, and skips these regions so the two gates cannot
contradict each other.

Usage
-----
    python scripts/gen_doc_versions.py --write   # stamp the regions
    python scripts/gen_doc_versions.py --check   # exit 1 on any drift
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

START = "<!-- doc-version:start -->"
END = "<!-- doc-version:end -->"

# A semver-ish token: 1.2.3, optionally with a pre-release suffix. Bounded
# on both sides so 0.39.0 is not found inside 10.39.05 or 0.39.01.
VERSION_TOKEN = re.compile(
    r"(?<![0-9.])\d+\.\d+\.\d+(?:[A-Za-z0-9.]*)?(?![0-9.])"
)

REGION = re.compile(
    rf"({re.escape(START)}\n)(.*?)(\n?{re.escape(END)})",
    re.DOTALL,
)


def current_version() -> str:
    """The version a release bumps -- pyproject.toml is the source."""
    text = (ROOT / "pyproject.toml").read_text(encoding="utf-8")
    m = re.search(r'^version\s*=\s*"([^"]+)"', text, re.MULTILINE)
    if not m:
        raise SystemExit("gen_doc_versions: no version in pyproject.toml?")
    return m.group(1)


def pages() -> list[Path]:
    """Every hand-owned markdown page that could carry a region.

    Cheap to scan and self-maintaining: adding a region to a new page
    needs no edit here. Build trees are skipped -- an example's build
    directory can contain copies of its own README.
    """
    out: list[Path] = [ROOT / "README.md"]
    for root in ("docs", "examples"):
        for page in sorted((ROOT / root).rglob("*.md")):
            if "build" in page.relative_to(ROOT).parts:
                continue
            out.append(page)
    return [p for p in out if p.is_file()]


def render(text: str, version: str) -> str:
    """Rewrite every version token inside every marked region."""

    def one(m: re.Match[str]) -> str:
        head, body, tail = m.group(1), m.group(2), m.group(3)
        return head + VERSION_TOKEN.sub(version, body) + tail

    return REGION.sub(one, text)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    mode = ap.add_mutually_exclusive_group(required=True)
    mode.add_argument("--write", action="store_true", help="stamp regions")
    mode.add_argument("--check", action="store_true", help="fail on drift")
    args = ap.parse_args()

    version = current_version()
    stale: list[str] = []
    written: list[str] = []

    for page in pages():
        text = page.read_text(encoding="utf-8")
        if START not in text:
            continue
        new = render(text, version)
        if new == text:
            continue
        rel = page.relative_to(ROOT)
        if args.write:
            page.write_text(new, encoding="utf-8")
            written.append(str(rel))
        else:
            stale.append(str(rel))

    if args.check and stale:
        print(
            f"gen_doc_versions: these pages show a version other than "
            f"{version} inside a doc-version region -- run `make "
            f"docs-relink`:",
            file=sys.stderr,
        )
        for rel in stale:
            print(f"  {rel}", file=sys.stderr)
        return 1

    if args.write:
        print(
            f"gen_doc_versions: {len(written)} page(s) stamped with "
            f"{version}"
            + ("".join(f"\n  {r}" for r in written) if written else "")
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
