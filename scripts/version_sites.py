#!/usr/bin/env python3
"""One declaration of every file that states doppler's release version.

There used to be two lists — ``VERSION_PROBES`` (how to READ each site) and
``BUMP_VERSION_CMD`` (how to WRITE each site) — and they differed by more than
a filename. Each site had a bespoke ``grep | sed`` to read it and a bespoke
``sed -i`` to write it, so the two spellings of one site could disagree about
which line they meant, and a site present in one list and absent from the other
was invisible.

They did disagree. ``BUMP_VERSION_CMD`` stripped the pre-release suffix on the
way into ``CMakeLists.txt`` and ``Cargo.toml`` (CMake and Cargo reject
``1.2.3rc1``) while ``VERSION_PROBES`` read back whatever was there and
``version-check`` demanded every site match. Measured on 2026-08-24::

    $ make bump-version VERSION=0.44.0rc1
    $ make version-check
    ERROR: CMakeLists.txt has 0.44.0, but pyproject.toml has 0.44.0rc1

A bump that cannot pass its own check, in the exact step ``release.yml`` runs
against the tag. The write rule and the read rule were each locally reasonable
and jointly impossible, which is what two lists buy you.

So: one table, one reader, one writer.

Pre-releases
------------
Refused, deliberately, rather than half-supported. ``standard.mk`` is vendored
verbatim and its ``version-check`` requires every probe to return the same
string and to equal ``VERSION``; CMake and Cargo cannot hold the suffix. There
is no spelling of the site table that satisfies both, so supporting
pre-releases needs a change in ``standard.mk`` (a per-site expected transform,
just-buildit/just-buildit.github.io#30) — not a local workaround that produces
an untaggable tree.
doppler has cut **zero** pre-releases in its history, so nothing is lost today
and the trap is gone.

Usage
-----
    python3 scripts/version_sites.py --read <label>   # one site's version
    python3 scripts/version_sites.py --write X.Y.Z    # rewrite every site
    python3 scripts/version_sites.py --labels         # declared labels
    python3 scripts/version_sites.py --check-literals # one literal per file

``--write`` verifies afterwards that **every declared site** reads back the
value it was given, so a site the Makefile forgets to probe is still covered.
"""

from __future__ import annotations

import argparse
import re
import sys
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

#: `X.Y.Z` and nothing else — see "Pre-releases" above.
RELEASE_RE = re.compile(r"^\d+\.\d+\.\d+$")


@dataclass(frozen=True)
class Site:
    """One file stating the release version.

    Attributes
    ----------
    label : str
        What `version-check` prints. Also the `--read` key.
    path : str
        Repo-relative.
    pattern : str
        Matches the whole line; group 1 is the version and is the ONLY part
        rewritten, so quoting and surrounding syntax are preserved by
        construction rather than by each site's sed getting it right.
    table : str | None
        A TOML table to scope the search to. ``bootstrap.toml`` and
        ``just-makeit.toml`` both carry a `[project] version`, and
        just-makeit.toml's very next line is `jm_version` — a DIFFERENT number
        with its own SSOT in scripts/gen_jm_pin.py. Scoping is what keeps one
        pin from eating the other.
    """

    label: str
    path: str
    pattern: str
    table: str | None = None


SITES: tuple[Site, ...] = (
    Site(
        "pyproject.toml", "pyproject.toml", r'^version = "([^"]+)"', "project"
    ),
    Site(
        "CMakeLists.txt",
        "CMakeLists.txt",
        r"^project\(doppler VERSION ([0-9.]+)",
    ),
    Site(
        "Cargo.toml", "ffi/rust/Cargo.toml", r'^version = "([^"]+)"', "package"
    ),
    Site(
        "bootstrap.toml", "bootstrap.toml", r'^version = "([^"]+)"', "project"
    ),
    Site(
        "just-makeit.toml",
        "just-makeit.toml",
        r'^version = "([^"]+)"',
        "project",
    ),
    # The overlay port builds the checkout it lives in, so its version IS
    # this tree's version -- vcpkg just needs it as a literal.
    Site(
        "vcpkg.json",
        "packaging/vcpkg/ports/doppler/vcpkg.json",
        r'^  "version": "([^"]+)",',
    ),
)


class SiteError(RuntimeError):
    """A site could not be read or written as declared."""


def _window(lines: list[str], site: Site) -> tuple[int, int]:
    """The half-open line range to search, honouring `site.table`.

    A table runs from its header to the next one, so this is the same
    range-scoping the old per-site seds spelled out individually — once.
    """
    if site.table is None:
        return 0, len(lines)
    header = f"[{site.table}]"
    start = next(
        (i for i, ln in enumerate(lines) if ln.strip() == header), None
    )
    if start is None:
        raise SiteError(f"{site.path}: no [{site.table}] table")
    end = next(
        (
            j
            for j in range(start + 1, len(lines))
            if lines[j].lstrip().startswith("[")
        ),
        len(lines),
    )
    return start + 1, end


def _locate(site: Site) -> tuple[list[str], int, re.Match[str]]:
    """Find the single line stating the version. Exactly one, or raise.

    "Exactly one" is the point. A pattern that stops matching returns nothing
    to compare and a write that silently changes nothing; a pattern that
    matches twice means the file grew a second claim. Both are reported here
    rather than becoming a wrong number downstream.
    """
    path = ROOT / site.path
    if not path.is_file():
        raise SiteError(f"{site.path}: no such file")
    lines = path.read_text(encoding="utf-8").splitlines(keepends=True)
    lo, hi = _window(lines, site)
    rx = re.compile(site.pattern)
    hits = [(i, m) for i in range(lo, hi) if (m := rx.match(lines[i]))]
    where = f"{site.path}" + (f" [{site.table}]" if site.table else "")
    if not hits:
        raise SiteError(f"{where}: nothing matches {site.pattern!r}")
    if len(hits) > 1:
        rows = ", ".join(str(i + 1) for i, _ in hits)
        raise SiteError(f"{where}: {site.pattern!r} matches lines {rows}")
    ((idx, match),) = hits
    return lines, idx, match


def read(label: str) -> str:
    return _locate(_site(label))[2].group(1)


def write(version: str) -> None:
    """Set every declared site, then read them all back."""
    if not RELEASE_RE.match(version):
        raise SiteError(
            f"{version!r} is not X.Y.Z. Pre-release versions are not "
            f"supported: CMake and Cargo reject the suffix, and "
            f"standard.mk's version-check requires every site to match, so "
            f"a pre-release bump produces a tree that cannot pass its own "
            f"release gate. See this file's docstring."
        )
    for site in SITES:
        lines, idx, match = _locate(site)
        line = lines[idx]
        s, e = match.span(1)
        lines[idx] = line[:s] + version + line[e:]
        (ROOT / site.path).write_text("".join(lines), encoding="utf-8")

    # Read back, because a write that matched the wrong line still "worked".
    # This covers EVERY declared site, so one the Makefile forgets to probe is
    # still verified here.
    for site in SITES:
        got = read(site.label)
        if got != version:
            raise SiteError(
                f"{site.label}: wrote {version} but reads back {got}"
            )
        print(f"  {site.label:<24} {version}")


def stray_literals() -> list[str]:
    """Every place a site FILE states the current version a second time.

    A site file may say the version exactly once -- on the declared line. A
    second literal is a claim nothing bumps, and it has a precise cost:
    standard.mk's `ci-changes` decides "this PR is a version bump alone" by
    substituting old -> new across each changed file and requiring the result
    to equal HEAD. A stale literal elsewhere in the file breaks that equality,
    so the release PR is classified `src=true` and the whole matrix runs --
    at EVERY release, because the literal is stale again the moment it is
    fixed by hand. The v0.53.0 release PR hit exactly this: a comment in
    CMakeLists.txt spelled the SONAME chain as `libdoppler.so.0.52.0`.

    It fails closed (the slow path is the safe one), which is why nothing
    caught it before a release did. This is the per-PR check.

    Returns
    -------
    list[str]
        One `path:line: text` per stray literal; empty when clean.
    """
    out = []
    for path in sorted({s.path for s in SITES}):
        declared = {
            _locate(s)[1] for s in SITES if s.path == path
        }  # 0-based indices of the lines that ARE the declaration
        version = read(next(s.label for s in SITES if s.path == path))
        # Not part of a longer number (10.52.0, 0.52.01, 1.0.52.0) -- but a
        # bare leading dot is fine and is the case that matters:
        # `libdoppler.so.0.52.0` is preceded by ".so.", not by a digit.
        rx = re.compile(
            r"(?<!\d)(?<!\d\.)" + re.escape(version) + r"(?!\.?\d)"
        )
        lines = (ROOT / path).read_text(encoding="utf-8").splitlines()
        for i, line in enumerate(lines):
            if i not in declared and rx.search(line):
                out.append(f"{path}:{i + 1}: {line.strip()}")
    return out


def _site(label: str) -> Site:
    for s in SITES:
        if s.label == label:
            return s
    known = ", ".join(s.label for s in SITES)
    raise SiteError(f"unknown site {label!r}; declared: {known}")


def main() -> int:
    ap = argparse.ArgumentParser(description="Read or write the version sites")
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument("--read", metavar="LABEL")
    g.add_argument("--write", metavar="X.Y.Z")
    g.add_argument("--labels", action="store_true")
    g.add_argument(
        "--check-literals",
        action="store_true",
        help="fail if a site file states the version anywhere else",
    )
    args = ap.parse_args()
    try:
        if args.labels:
            print(" ".join(s.label for s in SITES))
        elif args.check_literals:
            stray = stray_literals()
            if stray:
                print(
                    "version_sites: a version-site file states the version "
                    "outside its declared line:\n  "
                    + "\n  ".join(stray)
                    + "\n  Nothing bumps these, and one stale literal makes "
                    "`ci-changes` classify every release PR as src=true\n"
                    "  (the full matrix). Write X.Y.Z, or derive it.",
                    file=sys.stderr,
                )
                return 1
            print(
                f"version_sites: OK -- {len({s.path for s in SITES})} site "
                "file(s), each states the version once"
            )
        elif args.read:
            print(read(args.read))
        else:
            write(args.write)
    except SiteError as e:
        print(f"version_sites: {e}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
