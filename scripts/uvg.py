#!/usr/bin/env python3
"""Run a uv command for the project, refusing to REBUILD a mismatched .venv.

Every `uv` call the Makefile makes goes through this (`UV`, `override UV =
python3 scripts/uvg.py $(UV_BIN)`), on every platform. It refuses one thing:
a NESTED make (MAKELEVEL > 0) asking a sync-capable uv command (run,
sync, venv) to rebuild a .venv that is on another Python than the request
names.

Why (#3305): `uv run` removes and recreates a .venv whose Python disagrees with
the one it is asked for, and a suite that is still importing from that venv
loses its packages mid-run. A top-level make may rebuild (that is how a venv is
made or moved); a nested make may not, and says so in one line:

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


def _call(uv_bin: str, args: list[str]) -> int:
    """Run uv, or refuse in one line when the OS cannot start it.

    A UV_BIN that is not an executable the OS can start (a `#!/bin/sh` file
    with no .exe on Windows, a file with no exec bit on POSIX) makes Popen
    raise OSError. That is a refusal of the request, not a crash, so it is
    one line and exit 2 rather than a traceback (#3305).
    """
    try:
        return subprocess.call([uv_bin, *args])
    except OSError as e:
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
    nested = int(os.environ.get("MAKELEVEL", "0") or 0) > 0
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
