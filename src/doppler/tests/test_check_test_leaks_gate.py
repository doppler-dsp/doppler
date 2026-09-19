"""The test-leak gate, exercised over a seeded git repository.

`scripts/check_test_leaks.py` wraps every `make` test target: a PASSING run
must leave no file behind. It exists because three C tests left 67 files in
the build tree on every green run -- and dropped them into the repo root,
gitignored and invisible, whenever they were run by hand from there. Each
case builds a throwaway repo and runs the gate over a one-line command.
"""

from __future__ import annotations

import json
import subprocess
import sys
from typing import TYPE_CHECKING

import pytest

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)
SCRIPT = REPO / "scripts" / "check_test_leaks.py"


@pytest.fixture
def repo(tmp_path: Path) -> Path:
    root = tmp_path / "repo"
    root.mkdir()
    subprocess.run(["git", "init", "-q", str(root)], check=True)
    # As doppler ignores them: .benchmarks/ must be IGNORED to show up at all,
    # since git never lists an empty untracked directory.
    (root / ".gitignore").write_text(
        "*.csv\nbld/\n.benchmarks/\n", encoding="utf-8"
    )
    (root / "tracked.txt").write_text("v1\n", encoding="utf-8")
    subprocess.run(["git", "-C", str(root), "add", "-A"], check=True)
    return root


def _gate(root: Path, code: str, *pre: str) -> subprocess.CompletedProcess:
    """Run the gate over `python -c CODE`, with cwd = the seeded repo."""
    return subprocess.run(
        [
            sys.executable,
            str(SCRIPT),
            "--root",
            str(root),
            *pre,
            "--",
            sys.executable,
            "-c",
            code,
        ],
        cwd=root,
        capture_output=True,
        text=True,
    )


def test_a_clean_run_passes(repo: Path) -> None:
    r = _gate(repo, "pass")
    assert r.returncode == 0, r.stderr
    assert "OK" in r.stdout


def test_a_new_ignored_file_is_a_leak(repo: Path) -> None:
    r = _gate(repo, "open('out.csv', 'w').write('1')")
    assert r.returncode == 1
    assert "out.csv" in r.stderr


def test_a_rewritten_leftover_is_a_leak(repo: Path) -> None:
    # The state the tree was in: the file already exists from an earlier
    # run, so nothing is NEW afterwards. Only the write time gives it away.
    (repo / "stale.csv").write_text("old", encoding="utf-8")
    r = _gate(repo, "open('stale.csv', 'w').write('new')")
    assert r.returncode == 1
    assert "stale.csv" in r.stderr


def test_an_untouched_leftover_is_not_this_runs_leak(repo: Path) -> None:
    (repo / "stale.csv").write_text("old", encoding="utf-8")
    assert _gate(repo, "pass").returncode == 0


def test_a_rewritten_tracked_file_is_a_leak(repo: Path) -> None:
    r = _gate(repo, "open('tracked.txt', 'w').write('v2')")
    assert r.returncode == 1
    assert "tracked.txt" in r.stderr


def test_write_then_delete_passes(repo: Path) -> None:
    code = "import os; open('t.csv', 'w').write('1'); os.remove('t.csv')"
    assert _gate(repo, code).returncode == 0


@pytest.mark.parametrize(
    "path",
    [
        "__pycache__/m.pyc",
        ".pytest_cache/v",
        ".coverage",
        "coverage.xml",
        "bld/__pycache__/m.pyc",
    ],
)
def test_harness_caches_are_not_leaks(repo: Path, path: str) -> None:
    code = (
        f"import os; p = {path!r}; "
        "os.makedirs(os.path.dirname(p) or '.', exist_ok=True); "
        "open(p, 'w').write('1')"
    )
    r = _gate(repo, code)
    assert r.returncode == 0, r.stderr


def test_benchmark_storage_in_the_root_is_a_leak(repo: Path) -> None:
    # pytest-benchmark's default storage. It is configured under build/, so
    # a root .benchmarks/ means a pytest ran outside that configuration --
    # which the downstream example's did until this gate refused it.
    r = _gate(repo, "import os; os.mkdir('.benchmarks')")
    assert r.returncode == 1
    assert ".benchmarks" in r.stderr


def test_a_new_ignored_directory_is_walked(repo: Path) -> None:
    code = (
        "import os; os.makedirs('bld/x'); "
        "open('bld/x/out.bin', 'w').write('1')"
    )
    r = _gate(repo, code)
    assert r.returncode == 1
    assert "out.bin" in r.stderr


def test_a_failing_run_keeps_its_code_and_its_files(repo: Path) -> None:
    r = _gate(
        repo, "open('evidence.csv', 'w').write('1'); raise SystemExit(3)"
    )
    assert r.returncode == 3
    assert "FAIL" not in r.stderr
    assert (repo / "evidence.csv").exists()


def _fake_ctest(tmp_path: Path, workdirs: list[Path], body: str) -> list[str]:
    """A stand-in for ctest: lists `workdirs`, and otherwise runs `body`."""
    listing = {
        "tests": [
            {
                "name": f"t{i}",
                "properties": [{"name": "WORKING_DIRECTORY", "value": str(d)}],
            }
            for i, d in enumerate(workdirs)
        ]
    }
    script = tmp_path / "fake_ctest.py"
    script.write_text(
        "import sys\n"
        "if '--show-only=json-v1' in sys.argv:\n"
        f"    print({json.dumps(json.dumps(listing))})\n"
        "    raise SystemExit(0)\n"
        f"{body}\n",
        encoding="utf-8",
    )
    return [sys.executable, str(script)]


def _ctest_gate(repo: Path, cmd: list[str]) -> subprocess.CompletedProcess:
    return subprocess.run(
        [
            sys.executable,
            str(SCRIPT),
            "--root",
            str(repo),
            "--ctest",
            "--",
            *cmd,
        ],
        cwd=repo,
        capture_output=True,
        text=True,
    )


def test_ctest_working_dirs_are_walked(repo: Path, tmp_path: Path) -> None:
    wd = tmp_path / "build" / "mod"
    wd.mkdir(parents=True)
    (wd / "product.o").write_text("built before the run", encoding="utf-8")
    cmd = _fake_ctest(
        tmp_path, [wd], f"open({str(wd / 'cap.cf32')!r}, 'w').write('1')"
    )
    r = _ctest_gate(repo, cmd)
    assert r.returncode == 1
    assert "cap.cf32" in r.stderr
    assert "cwd of t0" in r.stderr
    assert "product.o" not in r.stderr


def test_ctest_testing_dir_is_ctests_own(repo: Path, tmp_path: Path) -> None:
    wd = tmp_path / "build"
    wd.mkdir()
    log = wd / "Testing" / "Temporary" / "LastTest.log"
    body = (
        f"import os; os.makedirs({str(log.parent)!r}); "
        f"open({str(log)!r}, 'w').write('1')"
    )
    r = _ctest_gate(repo, _fake_ctest(tmp_path, [wd], body))
    assert r.returncode == 0, r.stderr


def test_ctest_with_no_working_dirs_refuses(
    repo: Path, tmp_path: Path
) -> None:
    # A listing the gate cannot read must not become a clean report.
    r = _ctest_gate(repo, _fake_ctest(tmp_path, [], "pass"))
    assert r.returncode == 1
    assert "refusing" in r.stderr
