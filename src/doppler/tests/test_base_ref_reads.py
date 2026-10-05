"""Every ratchet's read of "the file as the merge base had it".

A ratchet needs to know what its baseline was before the branch touched it, and
that is a question for git. Five gate scripts each wrote the `git merge-base`
+ `git show` pair for themselves, and they had drifted on what the three
different answers mean (doppler#1838):

- **not a git repository** -- the check does not apply;
- **a ref that will not resolve** -- a shallow clone that never fetched it;
- **a ref that resolves but never had the file** -- the file is new here.

They are now one primitive, `scripts/_gitbase.py`, and the scripts that read a
baseline call it. What stays per-script is the POLICY: `check_alloc_helpers`
and `check_warnings` fail closed on an unreadable ref ("a ratchet that cannot
read its baseline has not passed"), while `check_validation_times` and
`gen_jm_pin` treat one as "nothing to compare" -- a gate that fires on a
clone's shape teaches people to ignore it. These tests pin both halves: the
primitive's three answers, and each script's reading of them, so unifying the
read did not quietly unify the policy.

The cases that matter most are the ones where the merge base and the tip of
the target branch DIFFER. A branch that is merely BEHIND must be compared with
where it branched, not with commits it has not pulled in: a gate whose verdict
changes when you rebase is measuring the gap to `main`, and git already
reports that (measured twice on one branch, in `check_tests_ssot.ratchet`).
"""

from __future__ import annotations

import importlib.util
import subprocess
import sys
from typing import TYPE_CHECKING

import pytest

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path
    from types import ModuleType

REPO = repo_root(__file__)
SCRIPTS = REPO / "scripts"


def _git(root: Path, *args: str) -> str:
    done = subprocess.run(
        ["git", "-c", "user.email=t@t", "-c", "user.name=t", *args],
        cwd=root,
        check=True,
        capture_output=True,
        text=True,
    )
    return done.stdout.strip()


def _commit(root: Path, rel: str, text: str, msg: str) -> None:
    p = root / rel
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(text, encoding="utf-8")
    _git(root, "add", "-A")
    _git(root, "commit", "-q", "-m", msg)


def _repo(root: Path) -> None:
    _git(root, "init", "-q", "-b", "main")


def _load(name: str, monkeypatch: pytest.MonkeyPatch) -> ModuleType:
    """A script as a module, with scripts/ importable as it is when run."""
    monkeypatch.syspath_prepend(str(SCRIPTS))
    spec = importlib.util.spec_from_file_location(
        f"_t_{name}", SCRIPTS / f"{name}.py"
    )
    assert spec is not None
    assert spec.loader is not None
    mod = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = mod
    spec.loader.exec_module(mod)
    return mod


@pytest.fixture
def gitbase(monkeypatch: pytest.MonkeyPatch) -> ModuleType:
    return _load("_gitbase", monkeypatch)


# ---------------------------------------------------------------------------
# The primitive: three answers, and the merge base rather than the tip.
# ---------------------------------------------------------------------------


def test_not_a_repo_is_its_own_answer(
    tmp_path: Path, gitbase: ModuleType
) -> None:
    """A synthetic tree has no history, so there is nothing to compare."""
    assert gitbase.in_git_repo(tmp_path) is False


def test_a_resolvable_ref_and_a_present_file_return_the_text(
    tmp_path: Path, gitbase: ModuleType
) -> None:
    _repo(tmp_path)
    _commit(tmp_path, "f.txt", "one\n", "c1")
    assert gitbase.in_git_repo(tmp_path) is True
    assert gitbase.show_at_base(tmp_path, "HEAD", "f.txt") == "one\n"


def test_a_ref_that_resolves_but_never_had_the_file_is_none(
    tmp_path: Path, gitbase: ModuleType
) -> None:
    """The file is NEW on this branch: nothing earlier to have grown from."""
    _repo(tmp_path)
    _commit(tmp_path, "other.txt", "x\n", "c1")
    assert gitbase.show_at_base(tmp_path, "HEAD", "f.txt") is None


def test_a_ref_that_will_not_resolve_raises(
    tmp_path: Path, gitbase: ModuleType
) -> None:
    """A shallow clone that never fetched the target has not been checked."""
    _repo(tmp_path)
    _commit(tmp_path, "f.txt", "one\n", "c1")
    with pytest.raises(gitbase.BaseUnreadableError):
        gitbase.show_at_base(tmp_path, "origin/nope", "f.txt")
    with pytest.raises(gitbase.BaseUnreadableError):
        gitbase.resolve_base(tmp_path, "origin/nope")


def _behind_main(root: Path) -> tuple[str, str]:
    """main moved on after the branch was cut; returns (merge base, tip)."""
    _repo(root)
    _commit(root, "f.txt", "base\n", "base")
    _git(root, "branch", "feature")
    _commit(root, "f.txt", "main moved on\n", "main ahead")
    tip = _git(root, "rev-parse", "main")
    _git(root, "switch", "-q", "feature")
    return _git(root, "rev-parse", "feature"), tip


def test_the_merge_base_is_used_not_the_tip(
    tmp_path: Path, gitbase: ModuleType
) -> None:
    """The branch is behind: compare with where it branched.

    Compared with the TIP of main, every branch cut before someone else's
    change would be asked to account for a change it did not make.
    """
    base, tip = _behind_main(tmp_path)
    assert gitbase.resolve_base(tmp_path, "main") == base != tip
    assert gitbase.show_at_base(tmp_path, "main", "f.txt") == "base\n"


def test_no_merge_base_falls_back_to_the_ref_itself(
    tmp_path: Path, gitbase: ModuleType
) -> None:
    """Unrelated histories: the only comparison left is the ref as given."""
    _repo(tmp_path)
    _commit(tmp_path, "f.txt", "mine\n", "mine")
    _git(tmp_path, "checkout", "-q", "--orphan", "other")
    _git(tmp_path, "rm", "-rfq", ".")
    _commit(tmp_path, "f.txt", "theirs\n", "theirs")
    _git(tmp_path, "switch", "-q", "main")
    assert gitbase.show_at_base(tmp_path, "other", "f.txt") == "theirs\n"


# ---------------------------------------------------------------------------
# check_validation_times: an unreadable baseline is "nothing to compare".
# ---------------------------------------------------------------------------


@pytest.fixture
def vt(monkeypatch: pytest.MonkeyPatch) -> ModuleType:
    return _load("check_validation_times", monkeypatch)


def _point_budget_at(
    vt: ModuleType, root: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    monkeypatch.setattr(
        vt, "BUDGET", root / "scripts" / ".validation-time-budget"
    )


def test_vt_reads_the_budget_at_the_merge_base(
    tmp_path: Path, vt: ModuleType, monkeypatch: pytest.MonkeyPatch
) -> None:
    _repo(tmp_path)
    _commit(
        tmp_path, "scripts/.validation-time-budget", "t_a  1.5  # why\n", "b"
    )
    _point_budget_at(vt, tmp_path, monkeypatch)
    assert vt.base_budget(tmp_path, "HEAD") == {"t_a": (1.5, "why")}


def test_vt_uses_the_merge_base_when_the_branch_is_behind(
    tmp_path: Path, vt: ModuleType, monkeypatch: pytest.MonkeyPatch
) -> None:
    _repo(tmp_path)
    _commit(tmp_path, "scripts/.validation-time-budget", "t_a  1.0\n", "base")
    _git(tmp_path, "branch", "feature")
    _commit(tmp_path, "scripts/.validation-time-budget", "t_a  9.0\n", "ahead")
    _git(tmp_path, "switch", "-q", "feature")
    _point_budget_at(vt, tmp_path, monkeypatch)
    assert vt.base_budget(tmp_path, "main") == {"t_a": (1.0, "")}


@pytest.mark.parametrize("why", ["not-a-repo", "unresolvable", "absent"])
def test_vt_cannot_tell_is_none_not_a_failure(
    tmp_path: Path,
    vt: ModuleType,
    monkeypatch: pytest.MonkeyPatch,
    why: str,
) -> None:
    """Every way of not knowing the baseline is None -- the policy.

    This gate's docstring is explicit that firing on a clone's shape teaches
    people to ignore it; the alloc and warnings gates choose the opposite,
    and unifying the READ must not have unified that choice.
    """
    if why != "not-a-repo":
        _repo(tmp_path)
        _commit(tmp_path, "other.txt", "x\n", "c1")
    _point_budget_at(vt, tmp_path, monkeypatch)
    ref = "origin/nope" if why == "unresolvable" else "HEAD"
    assert vt.base_budget(tmp_path, ref) is None


# ---------------------------------------------------------------------------
# gen_jm_pin: the pin at the merge base, None when git cannot say.
# ---------------------------------------------------------------------------


@pytest.fixture
def jp(monkeypatch: pytest.MonkeyPatch) -> ModuleType:
    return _load("gen_jm_pin", monkeypatch)


def _point_pin_at(
    jp: ModuleType, root: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    monkeypatch.setattr(jp, "ROOT", root)
    monkeypatch.setattr(jp, "SSOT", root / "just-makeit.toml")


def _pin(version: str) -> str:
    return f'[project]\njm_version = "{version}"\n'


def test_jp_reads_the_pin_at_the_merge_base_not_the_tip(
    tmp_path: Path, jp: ModuleType, monkeypatch: pytest.MonkeyPatch
) -> None:
    """Behind main means the pin has not MOVED on this branch.

    Compared with the tip, every branch cut before someone else's bump would
    be asked to announce a bump it did not make.
    """
    _repo(tmp_path)
    _commit(tmp_path, "just-makeit.toml", _pin("0.1.0"), "base")
    _git(tmp_path, "branch", "feature")
    _commit(tmp_path, "just-makeit.toml", _pin("0.2.0"), "main bumped it")
    _git(tmp_path, "switch", "-q", "feature")
    _point_pin_at(jp, tmp_path, monkeypatch)
    assert jp.base_pin("main") == "0.1.0"


@pytest.mark.parametrize("why", ["not-a-repo", "unresolvable", "absent"])
def test_jp_cannot_say_is_none(
    tmp_path: Path,
    jp: ModuleType,
    monkeypatch: pytest.MonkeyPatch,
    why: str,
) -> None:
    if why != "not-a-repo":
        _repo(tmp_path)
        _commit(tmp_path, "other.txt", "x\n", "c1")
    _point_pin_at(jp, tmp_path, monkeypatch)
    ref = "origin/nope" if why == "unresolvable" else "HEAD"
    assert jp.base_pin(ref) is None


def test_jp_merge_base_resolves_or_is_none(
    tmp_path: Path, jp: ModuleType, monkeypatch: pytest.MonkeyPatch
) -> None:
    """`branch_added_text` diffs against this: None must mean cannot-say."""
    base, _tip = _behind_main(tmp_path)
    _point_pin_at(jp, tmp_path, monkeypatch)
    assert jp.merge_base("main") == base
    assert jp.merge_base("origin/nope") is None


# ---------------------------------------------------------------------------
# check_tests_ssot.ratchet: an unreadable ref is a FAILURE, never a skip.
# ---------------------------------------------------------------------------


@pytest.fixture
def ts(monkeypatch: pytest.MonkeyPatch) -> ModuleType:
    return _load("check_tests_ssot", monkeypatch)


def _point_tests_at(
    ts: ModuleType, root: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    monkeypatch.setattr(ts, "ROOT", root)
    monkeypatch.setattr(ts, "TESTS", root / "native" / "tests")
    monkeypatch.setattr(
        ts, "IGNORE", root / "native" / "tests" / ".assertion-ratchet-ignore"
    )


ASSERT2 = "int main (void) { DP_CHECK (1); DP_CHECK (2); return 0; }\n"
ASSERT1 = "int main (void) { DP_CHECK (1); return 0; }\n"


def test_ts_flags_a_lost_assertion_against_the_merge_base(
    tmp_path: Path, ts: ModuleType, monkeypatch: pytest.MonkeyPatch
) -> None:
    _repo(tmp_path)
    _commit(tmp_path, "native/tests/test_a.c", ASSERT2, "base")
    (tmp_path / "native/tests/test_a.c").write_text(ASSERT1)
    _point_tests_at(ts, tmp_path, monkeypatch)
    bad = ts.ratchet("HEAD")
    assert len(bad) == 1
    assert "test_a.c: 2 assertions" in bad[0]
    assert "(-1)" in bad[0]


def test_ts_a_branch_behind_main_is_not_charged_for_main(
    tmp_path: Path, ts: ModuleType, monkeypatch: pytest.MonkeyPatch
) -> None:
    """Main ADDED assertions after the branch was cut; the branch lost none."""
    _repo(tmp_path)
    _commit(tmp_path, "native/tests/test_a.c", ASSERT1, "base")
    _git(tmp_path, "branch", "feature")
    _commit(tmp_path, "native/tests/test_a.c", ASSERT2, "main added one")
    _git(tmp_path, "switch", "-q", "feature")
    _point_tests_at(ts, tmp_path, monkeypatch)
    assert ts.ratchet("main") == []


def test_ts_an_unresolvable_ref_is_a_failure_not_a_skip(
    tmp_path: Path, ts: ModuleType, monkeypatch: pytest.MonkeyPatch
) -> None:
    """A ratchet that cannot read its baseline has not passed.

    Raised, not returned empty: an empty list is how this gate says "nothing
    lost", and a shallow clone has not shown that.
    """
    _repo(tmp_path)
    _commit(tmp_path, "native/tests/test_a.c", ASSERT2, "base")
    _point_tests_at(ts, tmp_path, monkeypatch)
    with pytest.raises(LookupError, match="does not resolve"):
        ts.ratchet("origin/nope")
