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

3. **``nats`` installs something other than its pin.** That group downloads
   the nats-server release binary, checked against the release's SHA-256
   for the machine's arch (#2712). One command per Linux manager, so first
   the copies must be IDENTICAL, and the version and each arch's hash must
   appear exactly once in it. Then it is run against a stubbed ``curl`` and
   ``uname`` with ``NATS_BIN_DIR`` in a scratch directory: a tarball that
   is a real nats-server archive but not the pinned bytes, an unpinned
   arch and a failed download must all fail and install nothing; an older
   version already there must still download; the pinned version already
   there must succeed without a download.

Usage
-----
    python3 scripts/check_bootstrap_cmd.py
"""

from __future__ import annotations

import os
import re
import shutil
import subprocess
import sys
import tarfile
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


# The nats cmd's stubs: `uname -m` reports $UNAME_M; `curl` records that it
# ran, then copies $FIXTURE to its -o path or fails with $CURL_RC.
NATS_STUBS = {
    "uname": "printf '%s\\n' \"$UNAME_M\"\n",
    "curl": (
        'touch "$MARK"\n'
        '[ -n "${CURL_RC:-}" ] && exit "$CURL_RC"\n'
        "while [ $# -gt 0 ]; do\n"
        '  [ "$1" = -o ] && { cp "$FIXTURE" "$2"; exit 0; }\n'
        "  shift\n"
        "done\n"
        "exit 2\n"
    ),
}

# (name, environment, the installed binary's version or None, should
# succeed, should download)
NATS_CASES = [
    (
        "a tarball that is not the pinned bytes",
        {"UNAME_M": "x86_64"},
        None,
        False,
        True,
    ),
    ("an unpinned arch", {"UNAME_M": "riscv64"}, None, False, False),
    (
        "the download fails",
        {"UNAME_M": "x86_64", "CURL_RC": "22"},
        None,
        False,
        True,
    ),
    (
        "an older version installed",
        {"UNAME_M": "aarch64", "CURL_RC": "22"},
        "2.9.0",
        False,
        True,
    ),
    (
        "the pinned version installed",
        {"UNAME_M": "x86_64"},
        "PIN",
        True,
        False,
    ),
]


def _nats_copies(doc: dict) -> tuple[list[str] | None, list[str]]:
    """The one nats cmd, and why the copies or the pin are not one."""
    errors = []
    cmds = {m: sec.get("cmd") for m, sec in doc.get("nats", {}).items()}
    if not cmds.get("apt"):
        return None, ["[nats.apt] has no cmd to check"]
    for manager, cmd in sorted(cmds.items()):
        if cmd != cmds["apt"]:
            errors.append(f"[nats.{manager}] cmd differs from [nats.apt]")
    line = " ".join(cmds["apt"])
    for what, pattern in (
        ("version", r"\bv=[0-9][0-9.]*;"),
        ("x86_64 hash", r"x86_64\) a=amd64; h=[0-9a-f]{64};"),
        ("aarch64 hash", r"aarch64\) a=arm64; h=[0-9a-f]{64};"),
    ):
        n = len(re.findall(pattern, line))
        if n != 1:
            errors.append(f"[nats] the {what} appears {n} times, want 1")
    return cmds["apt"], errors


def _run_nats_case(cmd, version, env, installed):
    """Run the nats cmd against stubs; return (rc, downloaded, binary)."""
    with tempfile.TemporaryDirectory() as tmp:
        t = Path(tmp)
        stubs, out = t / "stubs", t / "out"
        stubs.mkdir()
        out.mkdir()
        for name, body in NATS_STUBS.items():
            path = stubs / name
            path.write_text("#!/bin/sh\n" + body, encoding="utf-8")
            path.chmod(0o755)
        if installed:
            v = version if installed == "PIN" else installed
            fake = out / "nats-server"
            fake.write_text(f"#!/bin/sh\necho nats-server: v{v}\n")
            fake.chmod(0o755)
        # A real archive in the release's layout, so a command that skipped
        # the hash check would install it: only the check can refuse it.
        arch = "arm64" if env.get("UNAME_M") == "aarch64" else "amd64"
        pkg = f"nats-server-v{version}-linux-{arch}"
        (t / pkg).mkdir()
        (t / pkg / "nats-server").write_text("#!/bin/sh\necho impostor\n")
        with tarfile.open(t / "fixture.tar.gz", "w:gz") as tar:
            tar.add(t / pkg, arcname=pkg)
        full_env = {
            "PATH": f"{stubs}:/usr/bin:/bin",
            "HOME": tmp,
            "NATS_BIN_DIR": str(out),
            "FIXTURE": str(t / "fixture.tar.gz"),
            "MARK": str(t / "downloaded"),
            **env,
        }
        rc = subprocess.run(
            cmd, env=full_env, capture_output=True, check=False
        ).returncode
        binary = out / "nats-server"
        impostor = binary.exists() and "impostor" in binary.read_text()
        return rc, (t / "downloaded").exists(), impostor


def _check_nats(doc: dict) -> list[str]:
    cmd, errors = _nats_copies(doc)
    if cmd is None:
        return errors
    m = re.search(r"\bv=([0-9][0-9.]*);", " ".join(cmd))
    version = m.group(1) if m else "0"
    for name, env, installed, ok, downloads in NATS_CASES:
        rc, downloaded, impostor = _run_nats_case(cmd, version, env, installed)
        if (rc == 0) != ok:
            want = "exit 0" if ok else "fail"
            errors.append(f"[nats] {name}: rc={rc}, want {want}")
        if downloaded != downloads:
            want = "a download" if downloads else "no download"
            errors.append(f"[nats] {name}: want {want}")
        if impostor:
            errors.append(f"[nats] {name}: installed an unpinned binary")
    return errors


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

    errors += _check_nats(doc)

    if errors:
        print("bootstrap-cmd-check: FAIL")
        for e in errors:
            print(f"  {e}")
        return 1
    print(
        f"bootstrap-cmd-check: every cmd is reader-safe; [clang-rt.apt] "
        f"passes {len(CASES)} stubbed case(s); the [nats.*] copies are one "
        f"pin and pass {len(NATS_CASES)}"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
