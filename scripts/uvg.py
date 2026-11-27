#!/usr/bin/env python3
"""Run a uv command for the project, refusing to REBUILD a mismatched .venv.

Every `uv` call the Makefile makes goes through this (`UV`, `override UV =
python3 scripts/uvg.py $(UV_BIN)`), on every platform. It refuses one thing:
a NESTED make (MAKELEVEL > 1) asking a sync-capable uv command (run,
sync, venv) to rebuild a .venv that is on another Python than the request
names. GNU make exports MAKELEVEL one above the make that runs the recipe, so
a top-level make's recipe sees 1; only a make run by a make (2 or more) is
nested.

`uv run` removes and recreates a .venv whose Python disagrees with the one it
is asked for, and a suite that is still importing from that venv loses its
packages mid-run. A top-level make may rebuild (that is how a venv is made or
moved); a nested make may not, and says so in one line:

    venv_guard: the .venv is on Python 3.14; this request is 3.12 (from
    .python-version). A nested make will not rebuild it. Run the same make
    command at top level, or set UV_PYTHON=3.14 to match the venv.

`uv run --no-sync` never syncs, so it is always allowed (the interpreter probe
uses it). Everything else runs the command unchanged.

Why Python and not a shell script: on the Windows runner `bash` on PATH is the
WSL launcher in System32, so a process that spawns `bash` (the test-leak
wrapper does) cannot run a shell script. `python3` is what the Makefile already
runs on every platform, and every spawner here can start it by name.

Usage: python3 scripts/uvg.py <uv> [args...]
"""

from __future__ import annotations

import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

#: The uv subcommands that can create, sync or rebuild the project .venv.
SYNCING = frozenset({"run", "sync", "venv"})


def _request() -> tuple[str, str]:
    """The Python the request names, and where it came from ('' if none)."""
    want = os.environ.get("UV_PYTHON", "")
    if want:
        return want, "UV_PYTHON"
    pin = Path(".python-version")
    if pin.is_file():
        words = pin.read_text(encoding="utf-8").split()
        if words:
            return words[0], ".python-version"
    return "", ""


def _venv_version() -> str:
    """The venv's major.minor, read from pyvenv.cfg ('' when there is none)."""
    cfg = Path(".venv") / "pyvenv.cfg"
    if not cfg.is_file():
        return ""
    m = re.search(
        r"^version_info = (\d+\.\d+)", cfg.read_text(encoding="utf-8"), re.M
    )
    return m.group(1) if m else ""


def _windows_script_shell(uv_bin: str, err: OSError) -> str | None:
    """The sh that runs a `#!` UV_BIN on Windows, or None (refuse instead).

    On Windows CreateProcess starts only images, so a `#!/bin/sh` file fails
    with WinError 193 (ERROR_BAD_EXE_FORMAT). make runs UV_BIN through its
    SHELL, /bin/sh, which is Git's sh there; this runs the same file through
    the sh on PATH. Only that error, and only a file that opens with `#!`:
    anything else stays a refusal. POSIX never reaches this, because a file
    with no exec bit is refused there as make's shell would refuse it.
    """
    if os.name != "nt" or getattr(err, "winerror", None) != 193:
        return None
    try:
        with open(uv_bin, "rb") as f:
            if f.read(2) != b"#!":
                return None
    except OSError:
        return None
    return shutil.which("sh")


def _call(uv_bin: str, args: list[str]) -> int:
    """Run uv, or refuse in one line when the OS cannot start it.

    A UV_BIN that the OS cannot start makes Popen raise OSError. That is a
    refusal of the request, not a crash: one line and exit 2, never a
    traceback. The one start that is not refused is `_windows_script_shell`'s.
    """
    try:
        return subprocess.call([uv_bin, *args])
    except OSError as e:
        sh = _windows_script_shell(uv_bin, e)
        if sh is not None:
            return subprocess.call([sh, uv_bin, *args])
        print(
            f"uvg: cannot start {uv_bin!r} ({e.strerror}). Point UV_BIN at "
            "a uv that runs.",
            file=sys.stderr,
        )
        return 2


def main(argv: list[str]) -> int:
    if not argv:
        print("usage: uvg.py <uv> [args...]", file=sys.stderr)
        return 2
    uv_bin, args = argv[0], argv[1:]
    if not args or args[0] not in SYNCING or "--no-sync" in args:
        return _call(uv_bin, args)
    want, source = _request()
    nested = int(os.environ.get("MAKELEVEL", "0") or 0) > 1
    have = _venv_version()
    if want and have and nested:
        req = ".".join(want.split(".")[:2])
        if have != req:
            print(
                f"venv_guard: the .venv is on Python {have}; this request is "
                f"{req} (from {source}). A nested make will not rebuild it. "
                "Run the same make command at top level, or set "
                f"UV_PYTHON={have} to match the venv.",
                file=sys.stderr,
            )
            return 2
    return _call(uv_bin, args)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
