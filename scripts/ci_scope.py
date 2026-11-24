#!/usr/bin/env python3
"""How much of CI one run owes: ``full``, ``minimal`` or ``skip``.

ci.yml runs on every push, not only on pull requests and ``main``. A branch
with no pull request used to meet CI for the first time when the PR opened,
weeks in, so every red it carried was found at once, late. This is the one
place that decides what a given run runs.

====== ===================================================================
scope  when
====== ===================================================================
full   ``pull_request``, ``schedule``, ``workflow_dispatch``, and a push to
       ``main`` or ``develop`` (which keeps its own tree-tested skip, see
       ``changes``).
skip   a push to another ref whose sha is the head of an open pull request:
       that PR's own run is the gate, and a second full run of the same
       commit would only be a second bill.
minimal a push to any other ref: the jobs the PR run would not catch
       cheaply, the rest left to it (docs/dev/ci.md, "What runs where").
====== ===================================================================

The sha is compared, not just the branch name: a pull request from a FORK
whose branch is called like ours must not silence our branch's run, and a
push that lands after the PR's last head (a force-push racing the PR event)
is not the commit the PR run is testing.

Inputs are environment variables so a test drives it with anything, and
``OPEN_PR_HEADS`` (the open PRs' head shas for this branch, space
separated) is the only thing it asks GitHub for, through ``gh`` in main().
Standard library only: it runs on the bare runner.

>>> scope("pull_request", "refs/pull/7/merge", "abc", ())
'full'
>>> scope("push", "refs/heads/main", "abc", ())
'full'
>>> scope("push", "refs/heads/fix/x", "abc", ())
'minimal'
>>> scope("push", "refs/heads/fix/x", "abc", ("abc",))
'skip'
>>> scope("push", "refs/heads/fix/x", "def", ("abc",))
'minimal'
"""

from __future__ import annotations

import json
import os
import subprocess
import sys

#: Refs whose pushes run in full (``main`` keeps today's tree-tested skip).
LONG_LIVED = ("refs/heads/main", "refs/heads/develop")


def scope(
    event: str, ref: str, sha: str, open_pr_heads: tuple[str, ...]
) -> str:
    """``full``, ``minimal`` or ``skip`` for one run."""
    if event != "push" or ref in LONG_LIVED:
        return "full"
    if sha in open_pr_heads:
        return "skip"
    return "minimal"


#: The Linux OS legs each scope builds, as the JSON the matrix reads with
#: fromJSON. A push without a PR builds the newer image alone; every other run
#: builds both glibcs.
LINUX_OS = {
    "full": ["ubuntu-22.04", "ubuntu-24.04"],
    "minimal": ["ubuntu-24.04"],
    "skip": ["ubuntu-22.04", "ubuntu-24.04"],
}


def linux_os(result: str) -> str:
    """The Linux OS legs for ``result`` (``full`` | ``minimal`` | ``skip``),
    as a compact JSON array. A ``skip`` run builds nothing (every job is
    skipped), so its list is never read; it is the full list so an unknown
    scope fails safe toward building more, not less.

    >>> linux_os("minimal")
    '["ubuntu-24.04"]'
    >>> linux_os("full")
    '["ubuntu-22.04","ubuntu-24.04"]'
    """
    return json.dumps(
        LINUX_OS.get(result, LINUX_OS["full"]), separators=(",", ":")
    )


def _open_pr_heads(repo: str, branch: str) -> tuple[str, ...]:
    """The head shas of the open pull requests whose head branch is
    ``branch``, by ``gh`` (``GH_TOKEN`` is the job's, ``pull-requests:
    read``). A ``gh`` failure is raised, not read as "no PR": a scope that
    guessed ``minimal`` would run a duplicate, one that guessed ``skip``
    would run nothing, and either is worse than a red scope job."""
    out = subprocess.run(
        [
            "gh",
            "pr",
            "list",
            "--repo",
            repo,
            "--head",
            branch,
            "--state",
            "open",
            "--json",
            "headRefOid",
        ],
        check=True,
        capture_output=True,
        text=True,
    ).stdout
    return tuple(p["headRefOid"] for p in json.loads(out))


def main() -> int:
    event = os.environ["EVENT"]
    ref = os.environ["REF"]
    sha = os.environ["SHA"]
    heads: tuple[str, ...] = ()
    if event == "push" and ref not in LONG_LIVED:
        heads = _open_pr_heads(os.environ["REPO"], os.environ["BRANCH"])
    result = scope(event, ref, sha, heads)
    print(f"scope={result} (event={event} ref={ref} sha={sha[:9]})")
    with open(os.environ["GITHUB_OUTPUT"], "a", encoding="utf-8") as fh:
        fh.write(f"scope={result}\n")
        fh.write(f"linux_os={linux_os(result)}\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
