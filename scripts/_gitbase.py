"""A tracked file as the merge base had it: the one place that asks git.

A RATCHET -- a baseline that may only shrink -- has to know what the baseline
was before this branch touched it, and that is a question for git: read the
file at the merge base with the target branch and compare. Five gate scripts
each wrote that `git merge-base` + `git show` pair for themselves
(doppler#1838), and the copies had drifted on what the answers MEAN. This is
the read, once: :func:`resolve_base` (the merge base as a revision, for a
caller that needs several files at it), :func:`show_at` (one file at a
revision) and :func:`show_at_base` (the two together).

"Could git tell me?" has three answers, and they are not interchangeable:

- **not a git repository** -- :func:`in_git_repo` is False. A gate's own test
  harness runs against a synthetic tree with no history, which has nothing to
  have raised anything in.
- **a repository whose ref will not resolve** -- :class:`BaseUnreadableError`.
  This is the shallow clone: the baseline exists and could not be read.
- **a ref that resolves but never had the file** -- :func:`show_at` returns
  None. The file is new on this branch, so there is no earlier state to have
  grown from.

What a caller DOES with each is policy, and it stays the caller's. The
ratchets that guard a number (`check_alloc_helpers`, `check_warnings`,
`check_tests_ssot`) fail closed on an unreadable ref, because a ratchet that
cannot read its baseline has not been checked. `check_validation_times` and
`gen_jm_pin` treat it as "nothing to compare": a gate that fires on a clone's
shape teaches people to ignore it. `test_base_ref_reads.py` pins both.

>>> in_git_repo(__import__("pathlib").Path("/"))
False
"""

from __future__ import annotations

import subprocess
from typing import TYPE_CHECKING, TypeVar

if TYPE_CHECKING:
    from collections.abc import Callable, Iterable
    from pathlib import Path

T = TypeVar("T")


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


def resolve_base(root: Path, ref: str) -> str:
    """The merge base of HEAD and `ref`, as a commit SHA.

    The merge base rather than `ref` itself, so a branch that is behind the
    target is compared with where it BRANCHED, not with commits it has not
    pulled in: a gate whose verdict changes when you rebase is measuring the
    gap to `main`, and git already reports that. When no merge base exists
    (unrelated histories, a shallow clone that fetched only a tip) `ref` is
    returned as given, which is the only comparison left; it is a SHA, a
    branch or a tag, whatever the caller passed, and is usable wherever git
    takes a revision.

    Returned as a revision so a caller that needs more than one file at the
    base -- `git ls-tree`, `git diff`, several `git show` -- resolves it once.

    Raises
    ------
    BaseUnreadableError
        Neither a merge base nor `ref` itself resolves to a commit --
        typically a shallow clone that never fetched the target. Fetch it:
        ``git fetch --no-tags --depth=1 origin
        +refs/heads/main:refs/remotes/origin/main``.
    """
    mb = subprocess.run(
        ["git", "merge-base", "HEAD", ref],
        cwd=root,
        capture_output=True,
        text=True,
    )
    if mb.returncode == 0 and mb.stdout.strip():
        return mb.stdout.strip()
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
    return ref


def show_at(root: Path, rev: str, rel: str) -> str | None:
    """`rel` as of revision `rev`; None if `rev` never had the file."""
    show = subprocess.run(
        ["git", "show", f"{rev}:{rel}"],
        cwd=root,
        capture_output=True,
        text=True,
    )
    return show.stdout if show.returncode == 0 else None


def show_at_base(root: Path, ref: str, rel: str) -> str | None:
    """`rel` as of the merge base of HEAD and `ref`; None if absent there.

    Raises
    ------
    BaseUnreadableError
        See :func:`resolve_base`.
    """
    return show_at(root, resolve_base(root, ref), rel)


def added_since_base(
    root: Path,
    ref: str,
    rel: str,
    entries: Iterable[T],
    parse: Callable[[str], Iterable[T]],
    *,
    since: str,
) -> list[T]:
    """The ``entries`` a shrink-only list has GAINED since the merge base.

    The ratchet question, answered once: which of ``entries`` (the list as
    it is now) did ``rel`` not hold at the merge base with ``ref``?
    ``parse`` turns the file's text into its entries.

    **Absent at the base means EMPTY.** A list that is missing at the base,
    whether deleted, renamed or never committed, has held nothing, so every
    entry is added. Reading "absent" as "new, skip the check" turned the
    ratchet off for any branch that moved the list (#1976 review).

    The one exception is the change that introduces the ratchet itself.
    ``since`` names the gate's own file, and while THAT is absent at the
    base there is no baseline yet, so nothing counts as added. That lets
    the PR that brings a gate also bring its first list.

    Raises
    ------
    BaseUnreadableError
        See :func:`resolve_base`. A ratchet that cannot read its baseline
        has not been checked, so callers fail closed.
    """
    if show_at_base(root, ref, since) is None:
        return []
    then = show_at_base(root, ref, rel)
    before = set(parse(then)) if then is not None else set()
    return [e for e in entries if e not in before]
