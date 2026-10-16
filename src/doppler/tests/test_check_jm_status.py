"""`check_jm_status.py`: jm's drift verdict plus stale vendored headers.

The script runs one `just-makeit status --check --json` and must (a) keep
jm's exit code as the drift verdict, (b) fail when a `jm_*.h` is in the
report's `outdated` list and committed -- jm's own file, so a difference is
a stale copy (#2377) -- and (c) leave every other create-only file, and an
untracked header, advisory. These tests feed it jm's report shape through a
fake `subprocess.run` inside a real git repo, so they hold without a jm run;
`make drift-check` exercises the real one.
"""

from __future__ import annotations

import doctest
import importlib.util
import json
import subprocess
from typing import TYPE_CHECKING, Any

import pytest

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

ROOT = repo_root(__file__)
SCRIPT = ROOT / "scripts" / "check_jm_status.py"
#: The real one: `_fake` replaces the module's, and git must still run.
_RUN = subprocess.run


def _repo(
    root: Path, committed: list[str], untracked: list[str] | None = None
) -> None:
    """A git repo at *root* with *committed* files committed and
    *untracked* files written beside them, each holding "stale"."""
    git = ["git", "-c", "user.name=t", "-c", "user.email=t@t", "-C", str(root)]
    _RUN(["git", "init", "-q", str(root)], check=True)
    for rel in [*committed, *(untracked or [])]:
        f = root / rel
        f.parent.mkdir(parents=True, exist_ok=True)
        f.write_text("stale\n", encoding="utf-8")
    # As at the real root, where .gitignore holds the declined jm_test.h:
    # an untracked header is ignored, not merely unstaged.
    (root / ".gitignore").write_text(
        "".join(f"{p}\n" for p in untracked or []), encoding="utf-8"
    )
    if committed:
        _RUN([*git, "add", "--", *committed], check=True)
        _RUN([*git, "commit", "-qm", "init"], check=True)


def _load():
    spec = importlib.util.spec_from_file_location("check_jm_status", SCRIPT)
    assert spec and spec.loader
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def _fake(
    monkeypatch,
    mod,
    rc: int,
    outdated: list[str] | None,
    calls: list,
    allowed: bool = False,
):
    """jm answers with *rc* and an `outdated` list of *outdated* paths
    (None: a report with no `outdated` key at all)."""
    report: dict[str, Any] = {"entries": []}
    if outdated is not None:
        report["outdated"] = [
            {"path": p, "allowed": allowed} for p in outdated
        ]

    def run(cmd, **kw: Any):
        if cmd[0] == "git":
            return _RUN(cmd, **kw)
        calls.append(cmd)
        out = json.dumps(report) if "--json" in cmd else ""
        return subprocess.CompletedProcess(cmd, rc, stdout=out)

    monkeypatch.setattr(mod.subprocess, "run", run)


def test_a_stale_vendored_header_fails(monkeypatch, tmp_path: Path, capsys):
    mod = _load()
    calls: list = []
    _repo(tmp_path, ["native/benchmarks/jm_bench.h"])
    _fake(
        monkeypatch,
        mod,
        0,
        ["Makefile", "native/benchmarks/jm_bench.h"],
        calls,
    )
    monkeypatch.chdir(tmp_path)
    assert mod.main([]) == 1
    out = capsys.readouterr().out
    assert "native/benchmarks/jm_bench.h" in out
    assert "make jm-revendor" in out
    assert "Makefile" not in out.split("doppler#2377")[1]


def test_other_outdated_files_stay_advisory(
    monkeypatch, tmp_path: Path, capsys
):
    mod = _load()
    calls: list = []
    _fake(
        monkeypatch, mod, 0, ["Makefile", "native/inc/x/clib_common.h"], calls
    )
    monkeypatch.chdir(tmp_path)
    assert mod.main([]) == 0
    assert "OK" in capsys.readouterr().out


def test_jm_drift_is_the_verdict_and_the_text_report_is_shown(
    monkeypatch, tmp_path: Path
):
    mod = _load()
    calls: list = []
    # 3, not 1: the gate's own refusal is 1, so only jm's code passing
    # through unchanged returns this.
    _fake(monkeypatch, mod, 3, [], calls)
    monkeypatch.chdir(tmp_path)
    assert mod.main([]) == 3
    # One JSON run for the verdict, then the human report for the reader.
    assert calls[0][-3:] == ["status", "--check", "--json"]
    assert calls[1][-2:] == ["status", "--check"]


def test_no_report_is_a_failure_not_an_empty_list(monkeypatch, tmp_path: Path):
    mod = _load()

    def run(cmd, **kw: Any):
        return subprocess.CompletedProcess(cmd, 0, stdout="Traceback ...")

    monkeypatch.setattr(mod.subprocess, "run", run)
    monkeypatch.chdir(tmp_path)
    assert mod.main([]) != 0


def test_fix_deletes_the_stale_header_then_applies(
    monkeypatch, tmp_path: Path
):
    mod = _load()
    calls: list = []
    _repo(tmp_path, ["native/benchmarks/jm_bench.h", "Makefile"])
    hdr = tmp_path / "native" / "benchmarks" / "jm_bench.h"
    _fake(
        monkeypatch,
        mod,
        0,
        ["Makefile", "native/benchmarks/jm_bench.h"],
        calls,
    )
    monkeypatch.chdir(tmp_path)
    assert mod.main(["--fix"]) == 0
    assert not hdr.exists()
    assert (tmp_path / "Makefile").exists()
    assert calls[-1][-1] == "apply"


@pytest.mark.parametrize(
    "path, vendored",
    [
        ("native/benchmarks/jm_bench.h", True),
        ("native/tests/jm_test.h", True),
        ("native/inc/doppler/jm_perf.h", True),
        ("native/inc/doppler/jm_simd.h", True),
        ("native/inc/doppler/clib_common.h", False),
        ("Makefile", False),
        ("native/inc/doppler/not_jm_bench.h", False),
    ],
)
def test_which_paths_are_vendored(path: str, vendored: bool):
    mod = _load()
    got = mod.vendored_outdated({"outdated": [{"path": path}]})
    assert got == ([path] if vendored else [])


def test_the_module_examples_hold():
    """The docstring's examples are part of the contract; nothing else
    runs doctests under scripts/."""
    mod = _load()
    got = doctest.testmod(mod)
    assert got.attempted > 0 and got.failed == 0


def test_an_allowed_stale_header_still_fails(
    monkeypatch, tmp_path: Path, capsys
):
    """``status_allow`` cannot excuse a header doppler keeps verbatim."""
    mod = _load()
    calls: list = []
    _repo(tmp_path, ["native/inc/doppler/jm_perf.h"])
    _fake(
        monkeypatch,
        mod,
        0,
        ["native/inc/doppler/jm_perf.h"],
        calls,
        allowed=True,
    )
    monkeypatch.chdir(tmp_path)
    assert mod.main([]) == 1
    assert "native/inc/doppler/jm_perf.h" in capsys.readouterr().out


@pytest.mark.parametrize("argv", [[], ["--fix"]])
def test_a_report_without_its_outdated_list_fails(
    monkeypatch, tmp_path: Path, argv: list[str]
):
    """A jm that renamed the key must not read as "nothing outdated"."""
    mod = _load()
    calls: list = []
    _fake(monkeypatch, mod, 0, None, calls)
    monkeypatch.chdir(tmp_path)
    assert mod.main(argv) == 2
    assert not any(c[-1:] == ["apply"] for c in calls)


def test_vendored_outdated_refuses_a_report_without_the_list():
    """Its own guard, not only its callers': a missing key is an error."""
    with pytest.raises(KeyError):
        _load().vendored_outdated({"entries": []})


@pytest.mark.parametrize("argv", [[], ["--fix"]])
def test_an_untracked_header_is_not_gated(
    monkeypatch, tmp_path: Path, capsys, argv: list[str]
):
    """The root's declined jm_test.h: `jm apply` writes it into a checkout,
    jm reports it OUTDATED once its template moves, and it is in no diff.
    Neither the gate nor --fix may act on it; the committed header beside
    it still fails."""
    mod = _load()
    calls: list = []
    _repo(tmp_path, [], ["native/tests/jm_test.h"])
    _fake(monkeypatch, mod, 0, ["native/tests/jm_test.h"], calls)
    monkeypatch.chdir(tmp_path)
    assert mod.main(argv) == 0
    assert (tmp_path / "native" / "tests" / "jm_test.h").exists()
    assert not any(c[-1:] == ["apply"] for c in calls)
    assert "jm_test.h" not in capsys.readouterr().out


def test_fix_refuses_a_header_with_uncommitted_edits(
    monkeypatch, tmp_path: Path, capsys
):
    """Deleting it would lose the edit; nothing is deleted or applied."""
    mod = _load()
    calls: list = []
    _repo(tmp_path, ["native/benchmarks/jm_bench.h"])
    hdr = tmp_path / "native" / "benchmarks" / "jm_bench.h"
    hdr.write_text("an edit\n", encoding="utf-8")
    _fake(monkeypatch, mod, 0, ["native/benchmarks/jm_bench.h"], calls)
    monkeypatch.chdir(tmp_path)
    assert mod.main(["--fix"]) == 1
    assert hdr.read_text(encoding="utf-8") == "an edit\n"
    assert not any(c[-1:] == ["apply"] for c in calls)
    assert "native/benchmarks/jm_bench.h" in capsys.readouterr().out


@pytest.mark.parametrize(
    "project, spec",
    [
        # The root's own headers, not the example's: each is classified
        # against its own project.
        (".", [".", ":(exclude)examples/downstream-jm"]),
        ("examples/downstream-jm", ["."]),
    ],
)
def test_jm_still_compares_every_committed_header(
    project: str, spec: list[str]
):
    """The gate passes on an empty `outdated` list, so it cannot tell "all
    current" from "jm stopped comparing these". jm compares a create-only
    file only while its classifier calls it jm's (`is_versioned`); a jm
    release that reclassified one of these would leave the gate green for
    good. Ask the pinned jm directly, for every header each project
    commits."""
    # Imported, not importorskip'd: a jm that moved this module would
    # otherwise skip on every leg, and nothing fails a run on a skip.
    from just_makeit import _createonly as createonly

    proj = ROOT / project
    out = _RUN(
        ["git", "-C", str(proj), "ls-files", "-z", "--", *spec],
        capture_output=True,
        text=True,
        check=True,
    ).stdout
    mod = _load()
    headers = [p for p in out.split("\0") if mod.VENDORED.search(p)]
    assert headers, f"no committed jm_*.h under {project}"
    for rel in headers:
        assert createonly.is_versioned(rel, proj), rel


def test_an_inherited_git_dir_is_ignored(monkeypatch, tmp_path: Path):
    """`git rebase -x 'make lint'` in a linked worktree exports GIT_DIR.
    With it set, git reads another index from the project's directory, so
    a committed header it does not list there would pass stale. Here the
    other repository is empty, so the header vanishes outright."""
    mod = _load()
    proj = tmp_path / "proj"
    _repo(proj, ["native/benchmarks/jm_bench.h"])
    other = tmp_path / "other"
    _RUN(["git", "init", "-q", str(other)], check=True)
    monkeypatch.setenv("GIT_DIR", str(other / ".git"))
    paths = ["native/benchmarks/jm_bench.h"]
    assert mod.tracked(proj, paths) == paths
