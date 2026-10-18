#!/usr/bin/env python3
"""Check bootstrap.toml's ``cmd`` escape hatches.

A ``cmd = [...]`` section replaces a package list with a command
``install-deps`` runs verbatim. Two things about one can go wrong without
anything else noticing:

1. **The TOML reader cuts it short.** just-bashit's reader ends a string at
   the first ``]`` and does not unescape a double quote (measured on v0.7.1,
   the CI image's pin, and the current release). A ``[ ... ]`` test or a
   ``[0-9]`` class in the shell line is therefore truncated before it runs.
   Python's ``tomllib`` reads the same file correctly, so nothing here would
   disagree with it -- the check is on the characters themselves: every
   element of every ``cmd`` must be free of ``]`` and ``"``.

2. **``clang-rt.apt`` swallows a real failure.** That command installs
   ``libclang-rt-<major>-dev`` and tolerates exactly one outcome, the
   package not existing on this release (22.04, whose clang bundles the
   runtime). It is run here against stubbed ``clang``, ``apt-get``,
   ``apt-cache`` and ``sudo`` in a directory that holds nothing else, once
   per case, and must exit 0 for the benign cases and non-zero for every
   failure. A failed ``apt-get update`` only warns, so a package the cached
   lists hold still installs; absence is claimed only after a successful
   update. A command that reports a failed install, missing clang, missing
   sudo, or a package missing from stale lists as "bundled" fails this.

Usage
-----
    python3 scripts/check_bootstrap_cmd.py
"""

from __future__ import annotations

import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

if sys.version_info >= (3, 11):
    import tomllib
else:  # pragma: no cover - the 3.9/3.10 floor
    import tomli as tomllib

ROOT = Path(__file__).resolve().parent.parent
BOOTSTRAP_TOML = ROOT / "bootstrap.toml"

# Characters just-bashit's TOML reader mishandles inside a string.
FORBIDDEN = ("]", '"')

STUBS = {
    "sudo": 'exec "$@"\n',
    "clang": "printf '%s\\n' \"$CLANG_OUT\"\n",
    "apt-get": (
        'case "$*" in\n'
        '  *update*) exit "${UPDATE_RC:-0}" ;;\n'
        '  *install*) echo "install $*"; exit "${INSTALL_RC:-0}" ;;\n'
        "esac\n"
    ),
    "apt-cache": 'exit "${CACHE_RC:-0}"\n',
}

# (name, stubs left out, environment, should succeed)
CASES = [
    ("package exists and installs", (), {"CLANG_OUT": "19.1.7"}, True),
    (
        "package absent on this release",
        (),
        {"CLANG_OUT": "14.0.0", "CACHE_RC": "100"},
        True,
    ),
    ("install fails", (), {"CLANG_OUT": "19.1.7", "INSTALL_RC": "100"}, False),
    (
        "update fails, cached lists hold it",
        (),
        {"CLANG_OUT": "19.1.7", "UPDATE_RC": "100"},
        True,
    ),
    (
        "update fails, package not in cached lists",
        (),
        {"CLANG_OUT": "19.1.7", "UPDATE_RC": "100", "CACHE_RC": "100"},
        False,
    ),
    (
        "update fails, install fails",
        (),
        {"CLANG_OUT": "19.1.7", "UPDATE_RC": "100", "INSTALL_RC": "100"},
        False,
    ),
    ("clang reports no version", (), {"CLANG_OUT": ""}, False),
    ("no clang", ("clang",), {}, False),
    ("no sudo when not root", ("sudo",), {"CLANG_OUT": "19.1.7"}, None),
]


def _forbidden_characters(doc: dict) -> list[str]:
    """Every ``cmd`` element holding a character the reader cuts on."""
    bad = []
    for group, managers in doc.items():
        if not isinstance(managers, dict):
            continue
        for manager, section in managers.items():
            if not isinstance(section, dict) or "cmd" not in section:
                continue
            for i, arg in enumerate(section["cmd"]):
                for ch in FORBIDDEN:
                    if ch in arg:
                        bad.append(
                            f"[{group}.{manager}] cmd[{i}] holds {ch!r}"
                        )
    return bad


def _run_case(cmd: list[str], omit: tuple, env: dict) -> int:
    """Run ``cmd`` with only the stubs (minus ``omit``) and sh/id on PATH."""
    with tempfile.TemporaryDirectory() as tmp:
        bin_dir = Path(tmp)
        for tool in ("sh", "id"):
            found = shutil.which(tool)
            assert found, f"{tool} is not on PATH"
            (bin_dir / tool).symlink_to(found)
        for name, body in STUBS.items():
            if name in omit:
                continue
            path = bin_dir / name
            path.write_text("#!/bin/sh\n" + body, encoding="utf-8")
            path.chmod(0o755)
        full_env = {"PATH": str(bin_dir), "HOME": tmp, **env}
        return subprocess.run(
            cmd, env=full_env, capture_output=True, check=False
        ).returncode


def main() -> int:
    doc = tomllib.loads(BOOTSTRAP_TOML.read_text(encoding="utf-8"))
    errors = _forbidden_characters(doc)

    cmd = doc.get("clang-rt", {}).get("apt", {}).get("cmd")
    if not cmd:
        errors.append("[clang-rt.apt] has no cmd to check")
    else:
        root = os.getuid() == 0
        for name, omit, env, ok in CASES:
            if ok is None:  # no sudo matters only to a non-root caller
                if root:
                    continue
                ok = False
            rc = _run_case(cmd, omit, env)
            if (rc == 0) != ok:
                want = "exit 0" if ok else "fail"
                errors.append(f"[clang-rt.apt] {name}: rc={rc}, want {want}")

    if errors:
        print("bootstrap-cmd-check: FAIL")
        for e in errors:
            print(f"  {e}")
        return 1
    print(
        f"bootstrap-cmd-check: every cmd is reader-safe; [clang-rt.apt] "
        f"passes {len(CASES)} stubbed case(s)"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
