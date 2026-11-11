"""The release tag, for the scripts that pin documentation to it.

``check_repo_links.py`` (is every pinned link's target in the tag?) asks it
HERE, once (doppler#2857), so any other script that pins a link to the tag
asks the same question of the same tag. The answer has four
states, and CI is not allowed to be quiet about the bad one: see
:func:`tag_state`.
"""

from __future__ import annotations

import re
import subprocess
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from pathlib import Path


def version(root: Path) -> str:
    """The version a release bumps: ``pyproject.toml``'s."""
    text = (root / "pyproject.toml").read_text(encoding="utf-8")
    m = re.search(r'^version\s*=\s*"([^"]+)"', text, re.MULTILINE)
    if not m:
        raise SystemExit("release tag: no version in pyproject.toml")
    return m[1]


def git(root: Path, *args: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        ["git", *args],
        cwd=root,
        capture_output=True,
        text=True,
        check=False,
    )


def tag_state(root: Path, tag: str, ci: bool) -> tuple[str, str]:
    """``(state, message)``: ``present`` | ``unreleased`` | ``unchecked`` |
    ``error``.

    * ``present``: the tag is local, or was fetched. Every claim about it
      can be checked.
    * ``unreleased``: the remote has no such tag (the release PR, whose links
      name the tag about to be cut). Claims are checked against the working
      tree only, and the message says so.
    * ``error``: in CI, and the tag could neither be fetched nor proven absent.
      A skip here would make the check mean nothing where checkouts carry no
      tags, so it is a failure.
    * ``unchecked``: the same, on a developer machine (offline). A loud
      warning, not a failure.

    The tag is fetched at ``--depth=1`` only when this clone is already
    shallow: ``git fetch --depth=1`` on a FULL clone would turn it shallow and
    cut a developer's history.
    """
    if (
        git(root, "rev-parse", "-q", "--verify", f"refs/tags/{tag}").returncode
        == 0
    ):
        return "present", f"tag {tag} is local"
    shallow = (
        git(root, "rev-parse", "--is-shallow-repository").stdout.strip()
        == "true"
    )
    fetch = [
        "fetch",
        *(["--depth=1"] if shallow else []),
        "origin",
        "tag",
        tag,
    ]
    got = git(root, *fetch)
    if got.returncode == 0:
        return "present", f"tag {tag} fetched"
    ls = git(root, "ls-remote", "--exit-code", "--tags", "origin", tag)
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


def in_tag(root: Path, tag: str, path: str) -> bool:
    """Whether ``path`` exists at ``tag`` (a file or a directory)."""
    return (
        git(root, "cat-file", "-e", f"{tag}:{path.rstrip('/')}").returncode
        == 0
    )
