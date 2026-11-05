#!/usr/bin/env python3
"""Gate: the docs point at the RELEASE the reader installed, not at ``main``.

A newcomer reads the deployed docs and installs a release. Every link into the
repository that says ``blob/main`` shows them code that may not match what they
have (doppler#2857 counted 642 such links and none pinned to a tag). So:

1. **No ``blob/main`` or ``tree/main`` link, and no ``git clone`` without
   ``--branch``, in the docs** -- discovered over every page, not registered --
   except on pages listed in ``docs/.main-links-allowed``, which are pages
   about ``main`` itself (the contributing pages). The pinned form is
   ``.../blob/v0.79.0/path``; ``scripts/gen_doc_versions.py`` re-stamps it on
   every release, so write it once and it follows the version.
2. **A pinned link must point at a file that exists.** In the working tree
   always; at the tag too, because a link to a file added since the last
   release would 404 for the reader until the next one.

The tag check is **fail-closed where it matters**. Skipping it when the tag is
not at hand would make it a gate that checks nothing in CI, where checkouts
carry no tags. So when the tag is not local this script fetches it, and:

* it is there (local or fetched): every target is checked against it;
* it does not exist on the remote: that version is not released yet (the
  release PR, whose links name the tag about to be cut). Targets are checked
  against the working tree only, and the output says so;
* in CI and the answer cannot be had (no network, no remote): FAIL;
* on a developer machine, offline: a loud message, then pass.

Usage
-----
    python scripts/check_repo_links.py
"""

from __future__ import annotations

import fnmatch
import os
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
ALLOWED = "docs/.main-links-allowed"
REPO = r"https://github\.com/doppler-dsp/doppler"

MAIN_LINK = re.compile(rf"{REPO}/(?:blob|tree)/main\b")
PINNED = re.compile(rf"{REPO}/(?:blob|tree)/v\d+\.\d+\.\d+/([^)\s\"'>#`]+)")
CLONE = re.compile(r"git\s+clone\b[^\n]*doppler-dsp/doppler")


def version(root: Path = ROOT) -> str:
    text = (root / "pyproject.toml").read_text(encoding="utf-8")
    m = re.search(r'^version\s*=\s*"([^"]+)"', text, re.MULTILINE)
    if not m:
        raise SystemExit("check_repo_links: no version in pyproject.toml")
    return m[1]


def pages(root: Path = ROOT) -> list[Path]:
    """Every markdown page that can reach a reader, generated ones included.

    ``docs/c-api`` (Doxygen's output) and ``archive/`` (frozen) are left out;
    build trees under the examples are not pages.
    """
    out = [root / "README.md"]
    for top in ("docs", "examples", "example-projects"):
        for p in sorted((root / top).rglob("*.md")):
            parts = p.relative_to(root).parts
            if "build" in parts or "archive" in parts:
                continue
            if parts[:2] == ("docs", "c-api"):
                continue
            out.append(p)
    return [p for p in out if p.is_file()]


def allowed(root: Path) -> list[str]:
    """Globs of pages that may link ``main``, from ``docs/.main-links-allowed``
    (``<glob>  <why>`` per line; the reason is mandatory)."""
    f = root / ALLOWED
    if not f.exists():
        return []
    out: list[str] = []
    for line in f.read_text(encoding="utf-8").splitlines():
        s = line.strip()
        if not s or s.startswith("#"):
            continue
        glob, _, why = s.partition(" ")
        if not why.strip():
            raise SystemExit(f"{ALLOWED}: '{glob}' needs a reason")
        out.append(glob)
    return out


def unpinned(root: Path) -> list[str]:
    """``blob/main`` links and ``git clone`` lines on pages that may not."""
    ok = allowed(root)
    out: list[str] = []
    for page in pages(root):
        rel = page.relative_to(root).as_posix()
        if any(fnmatch.fnmatch(rel, g) for g in ok):
            continue
        for n, line in enumerate(
            page.read_text(encoding="utf-8").splitlines(), start=1
        ):
            if MAIN_LINK.search(line):
                out.append(
                    f"{rel}:{n}: links the repository at `main`; pin it to "
                    "the release (`.../blob/v<version>/...`) or, if the page "
                    f"is about main, list it in {ALLOWED}"
                )
            if CLONE.search(line) and not re.search(r"--branch|\s-b\s", line):
                out.append(
                    f"{rel}:{n}: `git clone` of the repository without "
                    "`--branch v<version>`: a reader gets main, not the "
                    "release they installed"
                )
    return out


def _git(root: Path, *args: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        ["git", *args],
        cwd=root,
        capture_output=True,
        text=True,
        check=False,
    )


def tag_state(root: Path, tag: str, ci: bool) -> tuple[str, str]:
    """``(state, message)``: ``present`` | ``unreleased`` | ``unchecked`` |
    ``error``. See the module docstring for what each means."""
    if (
        _git(
            root, "rev-parse", "-q", "--verify", f"refs/tags/{tag}"
        ).returncode
        == 0
    ):
        return "present", f"tag {tag} is local"
    got = _git(root, "fetch", "--depth=1", "origin", "tag", tag)
    if got.returncode == 0:
        return "present", f"tag {tag} fetched"
    ls = _git(root, "ls-remote", "--exit-code", "--tags", "origin", tag)
    if ls.returncode == 2:
        return "unreleased", (
            f"{tag} does not exist on the remote: not released yet, so "
            "links are checked against the working tree only"
        )
    why = (got.stderr or ls.stderr).strip().splitlines()[-1:] or ["no detail"]
    if ci:
        return "error", (
            f"in CI the tag {tag} could not be fetched and its absence could "
            f"not be proven ({why[0]}): the pinned links are unchecked, which "
            "is a failure, not a pass"
        )
    return "unchecked", (
        f"WARNING: tag {tag} is not available here ({why[0]}); pinned links "
        "were checked against the working tree only. CI checks them against "
        "the tag."
    )


def pinned_targets(root: Path) -> dict[str, list[str]]:
    """``{repo-relative path: [page:line, ...]}`` for every pinned link."""
    out: dict[str, list[str]] = {}
    for page in pages(root):
        rel = page.relative_to(root).as_posix()
        for n, line in enumerate(
            page.read_text(encoding="utf-8").splitlines(), start=1
        ):
            for m in PINNED.finditer(line):
                out.setdefault(m[1].rstrip("/"), []).append(f"{rel}:{n}")
    return out


def dead_links(root: Path, tag: str, state: str) -> list[str]:
    out: list[str] = []
    for path, where in sorted(pinned_targets(root).items()):
        if not (root / path).exists():
            out.append(
                f"{where[0]}: pinned link to {path}, which does not exist "
                "in the working tree"
                + (f" (+{len(where) - 1} more)" if len(where) > 1 else "")
            )
        elif (
            state == "present"
            and _git(root, "cat-file", "-e", f"{tag}:{path}").returncode != 0
        ):
            out.append(
                f"{where[0]}: pinned link to {path}, which is not in {tag} "
                "(added since the release, so the link would 404 for a "
                "reader until the next one)"
                + (f" (+{len(where) - 1} more)" if len(where) > 1 else "")
            )
    return out


def run(root: Path, ci: bool) -> tuple[int, str]:
    """``(exit status, report)`` for the tree at ``root``."""
    tag = f"v{version(root)}"
    errors = unpinned(root)
    state, message = tag_state(root, tag, ci)
    if state == "error":
        errors.append(message)
    else:
        errors += dead_links(root, tag, state)
    n = sum(len(v) for v in pinned_targets(root).values())
    if errors:
        return 1, "check_repo_links: FAILED\n" + "\n".join(
            f"  {e}" for e in errors
        )
    return 0, f"check_repo_links: OK -- {n} pinned link(s); {message}"


def main() -> int:
    rc, report = run(ROOT, bool(os.environ.get("CI")))
    print(report, file=sys.stderr if rc else sys.stdout)
    return rc


if __name__ == "__main__":
    raise SystemExit(main())
