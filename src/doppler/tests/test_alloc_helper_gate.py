"""The allocation-helper gate, exercised over a seeded tree.

`scripts/check_alloc_helpers.py` exists because the rule it enforces spent a
month enforced by nothing. `native/inc/clib_common.h` gained `dp_xmalloc`,
`dp_xcalloc` and `dp_xnn` on 2026-07-21, three cores adopted them, and the
instruction to extend that to other cores as they were touched lived in a
memory file and in no gate. The check that found this asked a simpler
question than the rule does -- *does anything at all fail on a bare
`malloc`?* -- and the answer was no.

So the cases below are about the gate's own failure modes rather than about
allocation. Each one seeds a fake repo, because a gate that can only be
tested against the real tree cannot be sabotaged: you would have to break
doppler to check it, and nobody does that twice.

The ratchet has two directions and both are load-bearing. Growth is the
obvious one. Slack -- a count left high after its file improved -- is the one
that shipped broken elsewhere in this repo: `PY_HOLLOW_ALLOW` had no
staleness check, so an entry kept its waiver after the file it covered
started recording, and the next regression in that file would have passed.
"""

from __future__ import annotations

import subprocess
import sys
from typing import TYPE_CHECKING

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)
SCRIPT = REPO / "scripts" / "check_alloc_helpers.py"


def _seed(tmp_path: Path, files: dict[str, str], allow: str | None) -> Path:
    """A minimal tree shaped like the parts of doppler the gate scans."""
    for rel, body in files.items():
        p = tmp_path / rel
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(body, encoding="utf-8")
    a = tmp_path / "scripts" / ".alloc-helper-allow"
    a.parent.mkdir(parents=True, exist_ok=True)
    a.write_text(allow if allow is not None else "", encoding="utf-8")
    return a


def _run(tmp_path: Path, allow_path: Path, *extra: str):
    return subprocess.run(
        [
            sys.executable,
            str(SCRIPT),
            "--root",
            str(tmp_path),
            "--allow",
            str(allow_path),
            *extra,
        ],
        capture_output=True,
        text=True,
    )


CLEAN = "void f (void) { s->b = dp_xmalloc (n); }\n"
BARE = "void f (void) { s->b = malloc (n); }\n"


def test_a_helper_only_tree_passes(tmp_path: Path) -> None:
    """The state the gate is steering towards: no bare allocation at all."""
    a = _seed(tmp_path, {"native/src/x/x_core.c": CLEAN}, allow="")
    r = _run(tmp_path, a)
    assert r.returncode == 0, r.stdout
    assert "0 bare allocation" in r.stdout


def test_a_new_bare_allocation_fails(tmp_path: Path) -> None:
    """The defect the gate exists for, in a file nothing has allowed."""
    a = _seed(tmp_path, {"native/src/x/x_core.c": BARE}, allow="")
    r = _run(tmp_path, a)
    assert r.returncode == 1
    assert "not in the baseline" in r.stdout
    # the message has to name the line, or the gate reports a problem the
    # reader then has to go and find
    assert "native/src/x/x_core.c:1" in r.stdout
    assert "dp_xmalloc" in r.stdout


def test_one_more_than_the_baseline_fails(tmp_path: Path) -> None:
    """A ratcheted file may not GROW -- the common case, and the subtle one.

    The file is already allowed to allocate, so a reader adding one more
    would see a green gate under any check that asked only "is this file
    listed?".
    """
    a = _seed(
        tmp_path,
        {"native/src/x/x_core.c": BARE * 3},
        allow="native/src/x/x_core.c 2\n",
    )
    r = _run(tmp_path, a)
    assert r.returncode == 1
    assert "baseline allows 2" in r.stdout


def test_the_baseline_may_not_go_slack(tmp_path: Path) -> None:
    """A count left high after its file improved is a waiver with no reason."""
    a = _seed(
        tmp_path,
        {"native/src/x/x_core.c": BARE},
        allow="native/src/x/x_core.c 5\n",
    )
    r = _run(tmp_path, a)
    assert r.returncode == 1
    assert "gone slack" in r.stdout
    assert "has to come down" in r.stdout


def test_a_listed_file_that_vanished_fails(tmp_path: Path) -> None:
    """A deleted file's allowance must not sit there covering nothing."""
    a = _seed(tmp_path, {"native/src/x/x_core.c": CLEAN}, allow="gone/y.c 1\n")
    r = _run(tmp_path, a)
    assert r.returncode == 1
    assert "the file is gone" in r.stdout


def test_the_helpers_own_home_is_exempt(tmp_path: Path) -> None:
    """`dp_xmalloc` IS `dp_xnn (malloc (n))`; one file has to contain it."""
    a = _seed(
        tmp_path,
        {
            "native/inc/clib_common.h": "void *dp_xmalloc (size_t n)"
            " { return dp_xnn (malloc (n)); }\n"
        },
        allow="",
    )
    r = _run(tmp_path, a)
    assert r.returncode == 0, r.stdout


def test_generated_glue_is_out_of_scope(tmp_path: Path) -> None:
    """jm owns `_ext` files, and their NULL checks are reachable.

    A numpy output buffer is sized by the CALLER, so its allocation can fail
    on a length a caller chose and the generated code raises MemoryError.
    Gating that would be a gate against the generator.
    """
    a = _seed(
        tmp_path,
        {
            "native/src/coding/coding_ext.c": BARE,
            "native/src/coding/coding_ext_interleaver.c": BARE,
        },
        allow="",
    )
    r = _run(tmp_path, a)
    assert r.returncode == 0, r.stdout


def test_the_helper_call_itself_is_not_a_hit(tmp_path: Path) -> None:
    """`dp_xmalloc (` contains `malloc (` and must not match it.

    Without the look-behind, adopting the helper would RAISE a file's count
    and the gate would fight the fix it exists to encourage.
    """
    body = "a = dp_xmalloc (n);\nb = dp_xcalloc (n, s);\n"
    a = _seed(tmp_path, {"native/src/x/x_core.c": body}, allow="")
    r = _run(tmp_path, a)
    assert r.returncode == 0, r.stdout


def test_tests_and_benchmarks_are_out_of_scope(tmp_path: Path) -> None:
    """An oracle allocates freely; a crash there is a test failure."""
    a = _seed(
        tmp_path,
        {"native/tests/test_x_core.c": BARE * 4},
        allow="",
    )
    r = _run(tmp_path, a)
    assert r.returncode == 0, r.stdout


def test_update_baseline_records_what_is_there(tmp_path: Path) -> None:
    """The escape hatch, and that it writes a ratchet a reader can follow."""
    a = _seed(tmp_path, {"native/src/x/x_core.c": BARE * 2}, allow="")
    r = _run(tmp_path, a, "--update-baseline")
    assert r.returncode == 0, r.stdout
    assert "native/src/x/x_core.c 2" in a.read_text()
    assert "MAY ONLY SHRINK" in a.read_text()
    # and the tree it just recorded now passes
    assert _run(tmp_path, a).returncode == 0


def test_the_real_repo_passes_its_own_ratchet() -> None:
    """The gate, applied to doppler itself -- the artifact, not a fixture.

    `--base HEAD` deliberately neuters the RAISE check here. Whether a count
    went up without a reason is a question about a BRANCH against main, and
    it already has an execution home: `make lint-alloc-helpers`, which the
    `alloc-helpers` pre-commit hook dispatches to at the default
    `--base origin/main`, under the `pre-commit (lint + format)` job -- the
    one that checks out at `fetch-depth: 0`. This test asks the other -- do
    the counts in the tree match the baseline -- and it runs in the Python
    job, whose shallow checkout has no `origin/main` to read. Left at the
    default it would not test the ratchet; it would test the fetch depth.
    """
    r = subprocess.run(
        [sys.executable, str(SCRIPT), "--base", "HEAD"],
        capture_output=True,
        text=True,
    )
    assert r.returncode == 0, r.stdout + r.stderr


# --------------------------------------------------------------------------- #
# The RAISE check, and the two ways "no baseline" differ                      #
# --------------------------------------------------------------------------- #
def _git(tmp_path: Path, *args: str) -> None:
    subprocess.run(
        ["git", *args], cwd=tmp_path, capture_output=True, check=True
    )


def test_a_raise_without_a_reason_fails(tmp_path: Path) -> None:
    """The whole point: a count may go up, but not silently.

    The gate cannot see a raise from the working tree alone -- the allow file
    IS the baseline, so `2` and a raised `1` are the same bytes. It reads the
    file at the base ref to tell them apart.
    """
    a = _seed(
        tmp_path,
        {"native/src/x/x_core.c": BARE},
        allow="native/src/x/x_core.c 1\n",
    )
    _git(tmp_path, "init", "-q", "-b", "main")
    _git(tmp_path, "add", "-A")
    _git(
        tmp_path,
        "-c",
        "user.email=t@t",
        "-c",
        "user.name=t",
        "commit",
        "-qm",
        "base",
    )
    _git(tmp_path, "branch", "-f", "origin/main", "main")

    # Two bare allocations now, and the baseline raised to match -- silently.
    (tmp_path / "native/src/x/x_core.c").write_text(
        BARE + BARE, encoding="utf-8"
    )
    a.write_text("native/src/x/x_core.c 2\n", encoding="utf-8")
    r = _run(tmp_path, a, "--base", "origin/main")
    assert r.returncode == 1, r.stdout
    assert "went UP with no reason" in r.stdout

    # The same raise, explained on the line, is the sanctioned path.
    a.write_text(
        "native/src/x/x_core.c 2  # caller-sized, failure handled\n",
        encoding="utf-8",
    )
    r = _run(tmp_path, a, "--base", "origin/main")
    assert r.returncode == 0, r.stdout


def test_a_repo_whose_base_ref_is_missing_fails(tmp_path: Path) -> None:
    """A shallow clone must not read as "nothing was raised".

    This is the case the not-a-repo escape below must NOT swallow: the
    baseline exists, it simply could not be read, and a ratchet that cannot
    read its baseline has not passed.
    """
    a = _seed(tmp_path, {"native/src/x/x_core.c": CLEAN}, allow="")
    _git(tmp_path, "init", "-q", "-b", "main")
    _git(tmp_path, "add", "-A")
    _git(
        tmp_path,
        "-c",
        "user.email=t@t",
        "-c",
        "user.name=t",
        "commit",
        "-qm",
        "base",
    )
    r = _run(tmp_path, a, "--base", "origin/does-not-exist")
    assert r.returncode == 1, r.stdout
    assert "cannot read" in r.stdout


def test_a_tree_that_is_not_a_repo_skips_the_raise_check(
    tmp_path: Path,
) -> None:
    """...while a non-repo tree has no history to have raised anything in.

    Every other test here seeds exactly that, so getting this wrong took the
    whole suite red rather than one case -- which is how it was caught.
    """
    a = _seed(tmp_path, {"native/src/x/x_core.c": CLEAN}, allow="")
    r = _run(tmp_path, a, "--base", "origin/main")
    assert r.returncode == 0, r.stdout
