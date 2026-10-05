"""A tracked file as the merge base had it: the one place that asks git.

A RATCHET -- a baseline that may only shrink -- has to know what the baseline
was before this branch touched it, and that is a question for git: read the
file at the merge base with the target branch and compare. Five gate scripts
each wrote that `git merge-base` + `git show` pair for themselves
(`check_alloc_helpers.py`, `check_validation_times.py`, `gen_jm_pin.py`,
`check_tests_ssot.py` twice), and they had already drifted: one treats an
unreadable ref as a failure, another quietly falls back to the ref itself.
A ratchet that cannot read its baseline has not been checked, so the answer
to "could git tell me?" is one of three things, and they are not
interchangeable:

- **not a git repository** -- :func:`in_git_repo` is False. The raise check
  does not APPLY: a gate's own test harness runs it against a synthetic tree
  with no history, which has nothing to have raised anything in.
- **a repository whose ref will not resolve** --
  :class:`BaseUnreadableError`. This is the shallow clone, and it IS a failure:
  the baseline exists and could not be read, so "nothing grew" would be a
  guess.
- **a ref that resolves but never had the file** -- :func:`show_at_base`
  returns None. The file is new on this branch, so there is no earlier state
  to have grown from; the caller decides whether that is fine (a ratchet
  being introduced) or not.

>>> in_git_repo(__import__("pathlib").Path("/"))
False
"""

from __future__ import annotations

import subprocess
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from pathlib import Path


class BaseUnreadableError(Exception):
    """The ref cannot be resolved in a repository that has one.

    Raised rather than returned so no caller can read it as "unchanged".
    """


def in_git_repo(root: Path) -> bool:
    """Is `root` inside a git work tree at all?"""
    return (
        subprocess.run(
            ["git", "rev-parse", "--git-dir"],
            cwd=root,
            capture_output=True,
            text=True,
        ).returncode
        == 0
    )


def show_at_base(root: Path, ref: str, rel: str) -> str | None:
    """`rel` as of the merge base of HEAD and `ref`; None if absent there.

    The merge base rather than `ref` itself, so a branch that is behind the
    target is compared with where it BRANCHED, not with commits it has not
    pulled in. When no merge base exists (unrelated histories) the ref is
    used as given, which is the only comparison left.

    Raises
    ------
    BaseUnreadableError
        `ref` does not resolve to a commit -- typically a shallow clone that
        never fetched it. Fetch it:
        ``git fetch --no-tags --depth=1 origin
        +refs/heads/main:refs/remotes/origin/main``.
    """
    if (
        subprocess.run(
            ["git", "rev-parse", "--verify", "--quiet", f"{ref}^{{commit}}"],
            cwd=root,
            capture_output=True,
            text=True,
        ).returncode
        != 0
    ):
        raise BaseUnreadableError(ref)
    mb = subprocess.run(
        ["git", "merge-base", "HEAD", ref],
        cwd=root,
        capture_output=True,
        text=True,
    )
    base = (
        mb.stdout.strip() if mb.returncode == 0 and mb.stdout.strip() else ref
    )
    show = subprocess.run(
        ["git", "show", f"{base}:{rel}"],
        cwd=root,
        capture_output=True,
        text=True,
    )
    return show.stdout if show.returncode == 0 else None
