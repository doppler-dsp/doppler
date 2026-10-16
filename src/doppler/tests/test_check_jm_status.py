"""`check_jm_status.py`: jm's drift verdict plus stale vendored headers.

The script runs one `just-makeit status --check --json` and must (a) keep
jm's exit code as the drift verdict, (b) fail when a `jm_*.h` is in the
report's `outdated` list -- jm's own file, so a difference is a stale copy
(#2377) -- and (c) leave every other create-only file advisory, as jm does.
These tests feed it jm's report shape through a fake `subprocess.run`, so
they hold without a jm run; `make drift-check` exercises the real one.
"""

from __future__ import annotations

import importlib.util
import json
import subprocess
from typing import TYPE_CHECKING, Any

import pytest

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

SCRIPT = repo_root(__file__) / "scripts" / "check_jm_status.py"


def _load():
    spec = importlib.util.spec_from_file_location("check_jm_status", SCRIPT)
    assert spec and spec.loader
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def _fake(monkeypatch, mod, rc: int, outdated: list[str], calls: list):
    """jm answers with *rc* and an `outdated` list of *outdated* paths."""
    report = {
        "entries": [],
        "outdated": [{"path": p, "allowed": False} for p in outdated],
    }

    def run(cmd, **kw: Any):
        calls.append(cmd)
        out = json.dumps(report) if "--json" in cmd else ""
        return subprocess.CompletedProcess(cmd, rc, stdout=out)

    monkeypatch.setattr(mod.subprocess, "run", run)


def test_a_stale_vendored_header_fails(monkeypatch, tmp_path: Path, capsys):
    mod = _load()
    calls: list = []
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
    _fake(monkeypatch, mod, 1, [], calls)
    monkeypatch.chdir(tmp_path)
    assert mod.main([]) == 1
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
    hdr = tmp_path / "native" / "benchmarks" / "jm_bench.h"
    hdr.parent.mkdir(parents=True)
    hdr.write_text("stale\n", encoding="utf-8")
    (tmp_path / "Makefile").write_text("mine\n", encoding="utf-8")
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
