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
2. **No example is run by its repository path.** ``python
   src/doppler/examples/x.py`` works only in a clone; the wheel ships the same
   script as ``python -m doppler.examples.x`` (the same allow-list applies).
3. **A pinned link must point at a file that exists.** In the working tree
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

import os
import re
import sys
from pathlib import Path

from _docs_pages import exempt_globs, is_exempt
from _docs_pages import pages as _pages
from _release_tag import in_tag, tag_state
from _release_tag import version as _version

ROOT = Path(__file__).resolve().parent.parent
ALLOWED = "docs/.main-links-allowed"
REPO = r"https://github\.com/doppler-dsp/doppler"

MAIN_LINK = re.compile(rf"{REPO}/(?:blob|tree)/main\b")
PINNED = re.compile(rf"{REPO}/(?:blob|tree)/v\d+\.\d+\.\d+/([^)\s\"'>#`]+)")
CLONE = re.compile(r"git\s+clone\b[^\n]*doppler-dsp/doppler")
# An example run by its repository path exists only in a clone; the wheel ships
# the same scripts as `python -m doppler.examples.<name>` (doppler#2857).
RUN_BY_PATH = re.compile(
    r"\bpython(?:3(?:\.\d+)?)?\s+(?:-u\s+)?(?:\./)?src/doppler/examples/"
)


def version(root: Path = ROOT) -> str:
    return _version(root)


def pages(root: Path = ROOT) -> list[Path]:
    """Every markdown page that can reach a reader, generated ones included
    (``docs/c-api`` and ``archive/`` are left out; see ``_docs_pages``)."""
    return _pages(root, ("docs", "examples", "example-projects"), readme=True)


def allowed(root: Path) -> list[str]:
    """Globs of pages that may link ``main``, from ``docs/.main-links-allowed``
    (``<glob>  <why>`` per line; the reason is mandatory)."""
    return exempt_globs(root, ALLOWED)


def unpinned(root: Path) -> list[str]:
    """``blob/main`` links and ``git clone`` lines on pages that may not."""
    ok = allowed(root)
    out: list[str] = []
    for page in pages(root):
        rel = page.relative_to(root).as_posix()
        if is_exempt(rel, ok):
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
            if RUN_BY_PATH.search(line):
                out.append(
                    f"{rel}:{n}: runs an example by its repository path, "
                    "which exists only in a clone; use `python -m "
                    "doppler.examples.<name>` (the wheel ships them)"
                )
            if CLONE.search(line) and not re.search(r"--branch|\s-b\s", line):
                out.append(
                    f"{rel}:{n}: `git clone` of the repository without "
                    "`--branch v<version>`: a reader gets main, not the "
                    "release they installed"
                )
    return out


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
        elif state == "present" and not in_tag(root, tag, path):
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
