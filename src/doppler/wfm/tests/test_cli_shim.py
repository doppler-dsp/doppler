"""The ``wfmgen`` console script hands the C binary's exit status back.

The shim claims argv, stdio and exit status pass straight through. On POSIX
``os.execv`` makes that true by construction; on Windows ``os.execv`` spawns
the target and exits the caller at once, so the shim reported 0 for a failed
run and left a ``--continuous`` writer orphaned (it filled a CI runner's
disk). These pin the contract on the real binary, and pin the Windows branch
on any platform.
"""

from __future__ import annotations

import subprocess
import sys

from doppler.wfm import cli


def _shim(*args: str) -> subprocess.CompletedProcess[bytes]:
    code = "import sys; from doppler.wfm import cli; sys.exit(cli.main())"
    return subprocess.run(
        [sys.executable, "-c", code, *args], capture_output=True
    )


def test_a_failing_run_fails_through_the_shim():
    p = _shim("--no-such-flag")
    assert p.returncode != 0
    assert b"unknown option" in p.stderr


def test_a_good_run_passes_its_output_through_the_shim():
    p = _shim("--type", "tone", "--count", "16")
    assert p.returncode == 0
    assert len(p.stdout) == 16 * 8  # cf32: 8 bytes per sample


def test_windows_runs_the_binary_as_a_child(monkeypatch):
    calls = []
    monkeypatch.setattr(cli.sys, "platform", "win32")
    monkeypatch.setattr(cli, "_runnable", lambda: "wfmgen.exe")
    monkeypatch.setattr(cli.sys, "argv", ["wfmgen", "--count", "4"])
    monkeypatch.setattr(
        cli.subprocess, "call", lambda argv: calls.append(argv) or 3
    )

    def no_exec(*_a):
        raise AssertionError("os.execv does not replace a Windows process")

    monkeypatch.setattr(cli.os, "execv", no_exec)
    assert cli.main() == 3
    assert calls == [["wfmgen.exe", "--count", "4"]]
