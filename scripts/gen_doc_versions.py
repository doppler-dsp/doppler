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
runnable as committed. That includes anything shaped like one, so keep a
region tight around the command that carries the version. A token followed by
a file extension keeps it (``v0.79.0.tar.gz``), and a four-part number such as
a dotted-quad address (``127.0.0.1``) is not a version and is left alone.
One two-part token is stamped too: the Debian runtime package embeds its ABI
series in its name (``libdoppler-dsp0.62_0.62.0_amd64.deb``), and ``0.62`` is
the version's major.minor. Outside a region nothing is touched --
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

# A semver-ish token: 1.2.3, optionally with a pre-release suffix (1.2.3rc1,
# 1.2.3.dev4). Bounded on both sides so 0.39.0 is not found inside 10.39.05 or
# 0.39.01, or 0.39.0.1.
#
# The suffix is letters and digits with NO dot, plus the PEP 440 dotted forms
# (.devN, .postN). It used to be a greedy [A-Za-z0-9.]*, which took ``.tar.gz``
# for part of the version: the natural URL
# ``.../archive/refs/tags/v0.79.0.tar.gz`` was rewritten to ``.../v0.79.0`` on
# the first stamp (doppler#2856). Ending the token before any other
# ``.<letter>`` means no list of file extensions to keep in step: ``.tar.gz``,
# ``.zip``, ``.deb`` and a sentence-final period are all simply not part of it.
VERSION_TOKEN = re.compile(
    r"(?<![0-9.])\d+\.\d+\.\d+"
    r"(?:[A-Za-z][A-Za-z0-9]*)?"
    r"(?:\.(?:dev|post)\d+)*"
    r"(?!\d|\.\d)"
)

# The ABI series a Debian runtime package carries in its NAME
# (libdoppler-dsp0.62): the version's major.minor, anchored to that prefix so
# no other two-part number is ever rewritten.
SERIES_TOKEN = re.compile(r"(?<=libdoppler-dsp)\d+\.\d+(?![0-9.])")

# A link to a file of THIS repository at a release tag. Unlike a version in a
# command, it appears in running prose and in tables, where a marked region
# would be a wrapper around every link, so the pin is stamped WHEREVER it
# appears: write ``.../blob/v0.1.0/path`` once and each release rewrites it
# (doppler#2857). The reader of a deployed page is then shown the code they
# installed, not whatever ``main`` has become. ``scripts/check_repo_links.py``
# refuses the unpinned ``.../blob/main`` form in the docs, so a link cannot
# go unstamped by being written the other way.
LINK_PIN = re.compile(
    r"(https://github\.com/doppler-dsp/doppler/(?:blob|tree)/)"
    r"v\d+\.\d+\.\d+(?=/)"
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
    """Every hand-owned page or example build file that could carry a region.

    Cheap to scan and self-maintaining: adding a region to a new page
    needs no edit here. Build trees are skipped -- an example's build
    directory can contain copies of its own README.

    An example project's ``CMakeLists.txt`` is scanned too: its
    ``find_package(doppler X.Y.Z REQUIRED)`` floor sits in a region (the
    markers inside ``#`` comments), so each release re-stamps it and a
    stale install fails at configure instead of at its first ``#include``
    (doppler#1583).
    """
    out: list[Path] = [ROOT / "README.md"]
    for root in ("docs", "examples", "example-projects"):
        for page in sorted((ROOT / root).rglob("*.md")):
            if "build" in page.relative_to(ROOT).parts:
                continue
            out.append(page)
    out.extend(example_cmakelists())
    return [p for p in out if p.is_file()]


def example_cmakelists() -> list[Path]:
    """Every example project's own CMakeLists.txt, build trees skipped."""
    return [
        p
        for p in sorted((ROOT / "example-projects").rglob("CMakeLists.txt"))
        if "build" not in p.relative_to(ROOT).parts
    ]


# An example's `find_package(doppler ...)`. It must sit in a region: a floor
# typed by hand rots, and no floor at all is how a stale 0.56.0 install was
# accepted and failed later as a missing header (doppler#1583).
FIND_DOPPLER = re.compile(r"find_package\s*\(\s*doppler\b")


def unfloored(text: str) -> list[int]:
    """Line numbers of `find_package(doppler` calls outside every region."""
    spans = [m.span() for m in REGION.finditer(text)]
    return [
        text.count("\n", 0, m.start()) + 1
        for m in FIND_DOPPLER.finditer(text)
        if not any(a <= m.start() < b for a, b in spans)
    ]


def render(text: str, version: str, pin_links: bool = True) -> str:
    """Rewrite every version token inside every marked region, and (unless
    ``pin_links`` is False) every repository link pinned to a release tag
    (:data:`LINK_PIN`) anywhere.

    ``pin_links`` is False for a frozen page under ``archive/``: it records
    what a release looked like, so its links must keep naming THAT release. A
    live page that needs a historical link names a commit instead
    (``.../blob/<40-hex sha>/path``), which this never matches.
    """
    if pin_links:
        text = LINK_PIN.sub(rf"\g<1>v{version}", text)

    def one(m: re.Match[str]) -> str:
        head, body, tail = m.group(1), m.group(2), m.group(3)
        series = ".".join(version.split(".")[:2])
        body = SERIES_TOKEN.sub(series, VERSION_TOKEN.sub(version, body))
        return head + body + tail

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
        new = render(text, version, "archive" not in page.parts)
        if new == text:
            continue
        rel = page.relative_to(ROOT)
        if args.write:
            page.write_text(new, encoding="utf-8")
            written.append(str(rel))
        else:
            stale.append(str(rel))

    bare = [
        f"{p.relative_to(ROOT)}:{line}"
        for p in example_cmakelists()
        for line in unfloored(p.read_text(encoding="utf-8"))
    ]
    if bare:
        print(
            "gen_doc_versions: an example's find_package(doppler) is outside "
            "a doc-version region, so it accepts any installed version "
            "(doppler#1583). Wrap it:\n"
            f"  # {START}\n"
            f"  find_package(doppler {version} REQUIRED)\n"
            f"  # {END}",
            file=sys.stderr,
        )
        for where in bare:
            print(f"  {where}", file=sys.stderr)
        return 1

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
