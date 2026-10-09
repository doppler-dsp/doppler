#!/usr/bin/env python3
"""Gate: every uv installer in the tree reads the one uv pin.

uv.lock's bytes depend on the uv that writes it. Bumping the project version
and re-locking changed 2 lines under uv 0.11.16 and 308 under 0.11.28: the
newer one drops dependency markers its resolution fork already implies. The
v0.65.0 release commit was the 308-line one, and nothing flagged it, because
nothing chose a uv version at all (doppler#1940).

The pin has one home, ``[tool.uv] required-version`` in ``pyproject.toml``.
uv enforces it itself: every project command on another version exits 2,
naming the ``uv self update`` that fixes it. This gate holds the places that
INSTALL uv to the same value:

1. The pin exists and is exact, ``==X.Y.Z``. A range is not a pin -- 0.11.16
   and 0.11.28 share a minor version and write different bytes. A root
   ``uv.toml`` carrying ``required-version`` would outrank it, for uv and for
   setup-uv alike, so one is refused.
2. ``astral-sh/setup-uv`` is used in exactly one place,
   ``.github/actions/setup-uv/action.yml``, and every step there passes
   ``version-file: pyproject.toml`` and no ``version``. Without an explicit
   file, setup-uv looks for the pin itself and, finding none, logs "Falling
   back to latest" and installs whatever released today. With one, a missing
   pin is an error.
3. Any other installer -- ``pip``/``pipx``/``uv tool install uv``, astral's
   ``install.sh`` or ``install.ps1``, a ``ghcr.io/astral-sh/uv`` image, a
   ``uv self update`` -- names the pin through a ``UV_VERSION`` variable, never
   a literal and never nothing. The Dockerfile ``ARG UV_VERSION`` takes no
   default, since a default is a second copy of the pin. This rule is the
   only thing holding some of them: a ``uvx``/``uv tool run`` does not
   enforce ``required-version`` (measured), and the manylinux release leg's
   uv exists to run ``uvx auditwheel repair``.

Files are every tracked file (``git ls-files``; every file under ``--root``
when it is not a checkout), less prose -- ``docs/``, ``*.md``, the changelog
-- and ``vendor/``, the lockfile, and this gate and its test, which spell out
the forms they refuse. Comment lines are skipped and backslash continuations
joined first.

The lock itself is checked by ``make lint-uv-lock``: the committed uv.lock
must be exactly what the pinned uv writes, which no amount of pin-checking
can establish.

Usage:  python3 scripts/check_uv_pin.py [--root DIR] [--print-version]
Exit 0 when the pin is exact and every installer reads it. With
``--print-version``, print the pinned version (the Makefile's UV_VERSION).
Runs on the floor Python: ``yaml`` is imported only by the check itself, and
the pin is read without ``tomllib``.
"""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

#: The one place setup-uv may be used.
COMPOSITE = ".github/actions/setup-uv/action.yml"
#: What every setup-uv step there must pass.
VERSION_FILE = "pyproject.toml"

#: Not scanned for installers: prose, foreign code, generated files, and the
#: two files whose job is to spell out the forms they refuse.
_SKIP_PREFIXES = ("vendor/", "docs/", "changelog.d/")
_SKIP_SUFFIXES = (".md", ".lock")
_SKIP_FILES = (
    "scripts/check_uv_pin.py",
    "src/doppler/tests/test_uv_pin_gate.py",
)

_EXACT = re.compile(r"==\d+\.\d+\.\d+")
#: A reference to the pin variable: ${UV_VERSION}, $(UV_VERSION),
#: $UV_VERSION, or a workflow expression naming uv_version.
_READS_PIN = re.compile(
    r"\$(?:\{UV_VERSION\b[^}]*\}|\(UV_VERSION\)|UV_VERSION\b)"
    r"|\$\{\{[^}]*\buv_version\b[^}]*\}\}"
)
#: Installer forms. Group 1 is the rest of the command.
_PKG_INSTALL = re.compile(
    r"\b(?:pip3?|pipx)\s+install\b(.*)|\buv\s+tool\s+install\b(.*)"
)
_SELF_UPDATE = re.compile(r"\buv\s+self\s+update\b(.*)")
_INSTALLER_URL = re.compile(r"astral\.sh/uv/(\S*?)install\.(?:sh|ps1)")
_UV_IMAGE = re.compile(r"ghcr\.io/astral-sh/uv(?::(\S+))?")
_ARG_DEFAULT = re.compile(r"^\s*ARG\s+UV_VERSION\s*=")
#: A package token that names uv: ``uv``, ``uv==1.2.3``, ``uv[x]>=1``.
_UV_TOKEN = re.compile(r"uv(?:\[[^\]]*\])?(?:[=<>!~].*)?")
#: Where one command ends and the next begins.
_END = re.compile(r"\s(?:\|\|?|&&|;)\s|\)\s*$")


def read_pin(root: Path) -> tuple[str | None, str | None]:
    """(``X.Y.Z``, None) for an exact pin, else (None, the problem).

    A line scan of the ``[tool.uv]`` table rather than ``tomllib``, which the
    floor Python (3.9) does not have; rule 1 is what makes the scan sound --
    a pin it cannot read is a pin the gate refuses.
    """
    text = (root / "pyproject.toml").read_text(encoding="utf-8")
    in_table, value = False, None
    for line in text.splitlines():
        s = line.strip()
        if s.startswith("["):
            in_table = s == "[tool.uv]"
            continue
        m = re.match(r'required-version\s*=\s*"([^"]*)"', s)
        if in_table and m:
            value = m.group(1)
    if value is None:
        return None, "pyproject.toml has no [tool.uv] required-version"
    if not _EXACT.fullmatch(value):
        return None, (
            f'pyproject.toml: required-version = "{value}" is not an exact '
            '"==X.Y.Z" pin; two uv versions inside a range can write '
            "different lock bytes"
        )
    return value[2:], None


def _files(root: Path) -> list[str]:
    """Tracked files when ``root`` is a checkout, every file otherwise."""
    if (root / ".git").exists():
        out = subprocess.run(
            ["git", "-C", str(root), "ls-files", "-z"],
            capture_output=True,
            text=True,
            check=True,
        ).stdout
        paths = [p for p in out.split("\0") if p]
    else:
        paths = [
            f.relative_to(root).as_posix()
            for f in root.rglob("*")
            if f.is_file() and ".git" not in f.parts
        ]
    return sorted(
        p
        for p in paths
        if not p.startswith(_SKIP_PREFIXES)
        and not p.endswith(_SKIP_SUFFIXES)
        and p not in _SKIP_FILES
    )


def _logical_lines(text: str) -> list[tuple[int, str]]:
    """Comment-free lines with backslash continuations joined.

    Each entry is (1-based number of the first physical line, joined text).
    """
    out: list[tuple[int, str]] = []
    buf, start = "", 0
    for n, raw in enumerate(text.splitlines(), 1):
        line = raw.rstrip()
        if not buf and line.lstrip().startswith("#"):
            continue
        if not buf:
            start = n
        if line.endswith("\\"):
            buf += line[:-1] + " "
            continue
        out.append((start, buf + line))
        buf = ""
    if buf:
        out.append((start, buf))
    return out


def _uv_tokens(args: str) -> list[str]:
    """The package tokens naming uv in one install command's arguments."""
    args = _END.split(args, maxsplit=1)[0]
    # Backslashes first: inside a double-quoted `bash -c "..."` a token is
    # spelled \"uv\" or uv==\$X, and the escapes would hide it.
    toks = [t.replace("\\", "").strip("'\";,") for t in args.split()]
    return [t for t in toks if _UV_TOKEN.fullmatch(t)]


def installer_offenders(root: Path, files: list[str]) -> list[str]:
    """``path:line: what`` for each uv installer that does not read the pin."""
    bad: list[str] = []
    for rel in files:
        try:
            text = (root / rel).read_text(encoding="utf-8")
        except (UnicodeDecodeError, OSError):
            continue
        for n, line in _logical_lines(text):
            where = f"{rel}:{n}"
            if _ARG_DEFAULT.match(line):
                bad.append(
                    f"{where}: ARG UV_VERSION has a default -- a second copy "
                    "of the pin; pass it as a build-arg instead"
                )
            for m in _PKG_INSTALL.finditer(line):
                args = m.group(1) if m.group(1) is not None else m.group(2)
                for tok in _uv_tokens(args):
                    if not (tok.startswith("uv==") and _READS_PIN.search(tok)):
                        bad.append(f"{where}: installs `{tok}`")
            for m in _SELF_UPDATE.finditer(line):
                if not _READS_PIN.search(_END.split(m.group(1), 1)[0]):
                    bad.append(f"{where}: `uv self update` without the pin")
            for m in _INSTALLER_URL.finditer(line):
                if not _READS_PIN.search(m.group(1)):
                    bad.append(f"{where}: installer {m.group(0)}")
            for m in _UV_IMAGE.finditer(line):
                if not (m.group(1) and _READS_PIN.search(m.group(1))):
                    bad.append(f"{where}: image {m.group(0)}")
    return bad


def _steps_using_setup_uv(doc: object) -> list[dict]:
    """Every mapping in ``doc`` whose ``uses`` is astral-sh/setup-uv."""
    found: list[dict] = []
    if isinstance(doc, dict):
        uses = doc.get("uses")
        if isinstance(uses, str) and uses.startswith("astral-sh/setup-uv@"):
            found.append(doc)
        for v in doc.values():
            found += _steps_using_setup_uv(v)
    elif isinstance(doc, list):
        for v in doc:
            found += _steps_using_setup_uv(v)
    return found


def setup_uv_offenders(root: Path, files: list[str]) -> tuple[list[str], int]:
    """(problems with setup-uv steps, steps in the composite action)."""
    import yaml  # the check needs it; --print-version must not

    bad: list[str] = []
    in_composite = 0
    for rel in files:
        if not rel.startswith(".github/") or not rel.endswith(
            (".yml", ".yaml")
        ):
            continue
        doc = yaml.safe_load((root / rel).read_text(encoding="utf-8"))
        for step in _steps_using_setup_uv(doc):
            name = step.get("name") or step["uses"]
            if rel != COMPOSITE:
                bad.append(
                    f"{rel}: step '{name}' uses {step['uses']} directly; use "
                    "./.github/actions/setup-uv, the one place it is pinned"
                )
                continue
            in_composite += 1
            given = step.get("with") or {}
            if given.get("version-file") != VERSION_FILE:
                bad.append(
                    f"{rel}: step '{name}' lacks `version-file: "
                    f"{VERSION_FILE}`; without it a missing pin installs the "
                    "latest uv"
                )
            if "version" in given:
                bad.append(
                    f"{rel}: step '{name}' passes `version:`, a second copy "
                    "of the pin"
                )
    if COMPOSITE in files and in_composite == 0:
        bad.append(f"{COMPOSITE}: installs no uv at all")
    return bad, in_composite


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--root", type=Path, default=ROOT)
    ap.add_argument("--print-version", action="store_true")
    args = ap.parse_args()
    root = args.root.resolve()

    pin, problem = read_pin(root)
    if args.print_version:
        if pin is None:
            print(f"check_uv_pin: {problem}", file=sys.stderr)
            return 1
        print(pin)
        return 0

    bad = [problem] if problem else []
    uv_toml = root / "uv.toml"
    if uv_toml.is_file() and re.search(
        r"(?m)^\s*required-version\s*=", uv_toml.read_text(encoding="utf-8")
    ):
        bad.append(
            "uv.toml: required-version here outranks pyproject.toml's -- "
            "a second declaration of the pin"
        )
    files = _files(root)
    if not files:
        print("check_uv_pin: no files in scope -- nothing was checked")
        return 1
    setup_bad, n_steps = setup_uv_offenders(root, files)
    bad += setup_bad + installer_offenders(root, files)
    for b in bad:
        print(f"check_uv_pin: {b}")
    if bad:
        print(
            "\n  pyproject.toml's [tool.uv] required-version is the one uv "
            "pin (doppler#1940).\n  setup-uv reads it through "
            f"{COMPOSITE}; any other installer\n  takes it as UV_VERSION "
            "(`make -s print-uv-version`)."
        )
        return 1
    print(
        f"check_uv_pin: OK -- uv {pin}; {len(files)} file(s), "
        f"{n_steps} setup-uv step(s), every installer reads the pin"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
