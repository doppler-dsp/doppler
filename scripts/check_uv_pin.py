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

4. A makefile runs uv through ``$(UV)``, never a bare ``uv run``/``uv sync``
   (doppler#2369). ``UV=<the pinned uv>`` is the way out on a box whose uv
   is not the pin -- uv's own refusal and ``python-executable-check`` both
   name it -- and a bare spelling is a call it does not reach: ``make pyext
   UV=...`` still failed at its last step, a bare ``uv sync``. Lines that
   must stay bare for now are listed in ``scripts/.bare-uv-allow`` with
   their reason. The list is a ratchet: an entry must match exactly one
   logical makefile line, one that matches none fails, and with ``--base``
   one the list at the merge base did not hold is refused. Nor may a
   makefile run a tool OUTSIDE the lock -- ``uvx``, ``uv tool run``, or
   ``uv run --with`` (doppler#2371): it resolves the tool unpinned, so
   a gate's verdict can change with no commit here, and it does not enforce
   ``required-version`` either. The tool goes in the dev group with an
   exact pin and runs through ``$(DEV_RUN)``.

Files are every tracked file (``git ls-files``; every file under ``--root``
when it is not a checkout), less prose -- ``docs/``, ``*.md``, the changelog
-- and ``vendor/``, the lockfile, and this gate and its test, which spell out
the forms they refuse. Comment lines are skipped and backslash continuations
joined first.

The lock itself is checked by ``make lint-uv-lock``: the committed uv.lock
must be exactly what the pinned uv writes, which no amount of pin-checking
can establish.

Usage:  python3 scripts/check_uv_pin.py [--root DIR] [--print-version]
                                        [--base REF]
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

#: Rule 4's files and the bare form it refuses. `uv` must not be the tail of
#: a longer word or path (`$(UV)` is upper case, so it never matches), and a
#: shell substitution `$$(uv run ...)` is a bare call like any other. After
#: a word and a `-` (`foo-uv`) it is another name; after a lone `-` it is
#: make's ignore-errors prefix, and `-uv sync` or `@-uv run` runs a bare uv.
#: Global flags may come before the subcommand (`uv -q sync`,
#: `uv --project . run`), each with at most one value; backtracking leaves
#: the subcommand for the alternation.
_MAKEFILE = re.compile(r"(?:^|/)(?:GNU)?[Mm]akefile$|\.mk$")
_BARE_UV = re.compile(
    r"(?<![\w./])(?<!\w-)uv\s+(?:-\S+(?:\s+[^-\s]\S*)?\s+)*"
    r"(?:run|sync|lock|pip|build|venv|tool|export|tree|add|remove|cache"
    r"|python|version|init|publish|format)\b"
)
#: Rule 4's other half: a tool run outside the lock. Three forms, each with
#: make's `@`/`-` prefixes and uv's global flags, by uv's spellings: bare,
#: by path (`.venv/bin/uv`), or a make variable whose name ends in UV
#: (`$(UV)`, `${UV}`, `$(_PY_UV)`):
#:
#: - `uvx`, bare or by path, as a word of its own: not `uvx_helper`,
#:   `uvx-helper.sh` or `foo-uvx`;
#: - `uv tool run`, with flags before or after `tool`;
#: - `uv run` given `--with`/`-w`/`--with-requirements`/`--with-editable`
#:   among uv's OWN options, or `$(DEV_RUN)` given one: the package resolves
#:   outside the lock (`_run_with`). An option after the command is the
#:   command's (`pytest --with-coverage`), not uv's.
_ANY_UV = r"(?:\$[({]\w*UV[)}]|(?<![\w.])(?<!\w-)(?:[\w.$(){}/-]*/)?uv(?=\s))"
_FLAGS = r"(?:-\S+(?:\s+[^-\s]\S*)?\s+)*"
_UNLOCKED_TOOL = re.compile(
    r"(?<![\w.])(?<!\w-)uvx(?=\s|$)"
    rf"|{_ANY_UV}\s+{_FLAGS}tool\s+{_FLAGS}run\b"
)
#: Where `uv run`'s own options start: after `run`, or after `$(DEV_RUN)`
#: (`$(UV) run --group dev`).
_RUN = re.compile(rf"{_ANY_UV}\s+{_FLAGS}run(?=\s|$)|\$[({{]DEV_RUN[)}}]")
#: The `uv run` options that add a package from outside the lock.
_RUN_WITH = frozenset(
    {"--with", "-w", "--with-requirements", "--with-editable"}
)
#: The `uv run` options (its own and uv's global ones) that take a value, so
#: the token after one is that value, not the command. From the pinned uv's
#: `uv run --help`; test_uv_pin_gate.py derives the same set from that help
#: and fails when the pin's set moves.
RUN_VALUED = frozenset(
    {
        "--allow-insecure-host",
        "--cache-dir",
        "--color",
        "--config-file",
        "--config-setting",
        "--config-settings-package",
        "--default-index",
        "--directory",
        "--env-file",
        "--exclude-newer",
        "--exclude-newer-package",
        "--extra",
        "--extra-index-url",
        "--find-links",
        "--fork-strategy",
        "--group",
        "--index",
        "--index-strategy",
        "--index-url",
        "--keyring-provider",
        "--link-mode",
        "--no-binary-package",
        "--no-build-isolation-package",
        "--no-build-package",
        "--no-editable-package",
        "--no-extra",
        "--no-group",
        "--no-sources-package",
        "--only-group",
        "--package",
        "--prerelease",
        "--prerelease-package",
        "--project",
        "--python",
        "--python-platform",
        "--refresh-package",
        "--reinstall-package",
        "--resolution",
        "--upgrade-group",
        "--upgrade-package",
        "--with",
        "--with-editable",
        "--with-requirements",
        "-C",
        "-P",
        "-f",
        "-i",
        "-p",
        "-w",
    }
)


#: Value-taking spellings `uv run --help` does not show, so the test that
#: holds RUN_VALUED to the help cannot see them: hidden options and aliases,
#: read from uv 0.12.24's CLI source (crates/uv-cli/src/lib.rs).
RUN_VALUED_UNLISTED = frozenset(
    {
        "--python-preference",
        "--python-fetch",
        "--preview-features",
        "--preview-feature",
        "--max-recursion-depth",
        "--trusted-host",
        "--config-settings",
    }
)
#: The uv RUN_VALUED_UNLISTED was read from. The help cannot show a new
#: hidden option, so a test fails once the uv pin moves past this: re-read
#: the new uv's crates/uv-cli/src/lib.rs for hidden options and aliases that
#: take a value, update the set, then this version.
RUN_VALUED_UNLISTED_UV = "0.12.24"
_VALUED = RUN_VALUED | RUN_VALUED_UNLISTED


def _run_with(line: str) -> bool:
    """Whether a `uv run` in ``line`` adds a package with `--with` or its
    kin among uv's own options, which end at the command: the first token
    that is neither an option nor the value of one.

    A single-dash token longer than two characters is read a character at
    a time, as uv reads it: a cluster of short flags (``-qw X``) or a short
    option with its value attached (``-wX``).
    """
    for m in _RUN.finditer(line):
        toks = line[m.end() :].split()
        i = 0
        while i < len(toks) and toks[i].startswith("-"):
            tok = toks[i]
            name, eq, _ = tok.partition("=")
            if name in _RUN_WITH:
                return True
            if tok.startswith("--") or len(tok) <= 2:
                i += 2 if name in _VALUED and not eq else 1
                continue
            step = 1
            for j, ch in enumerate(tok[1:], 1):
                if "-" + ch in _RUN_WITH:
                    return True
                if "-" + ch in _VALUED:
                    # Its value is the rest of the token, or the next one.
                    step = 1 if j < len(tok) - 1 else 2
                    break
            i += step
    return False


def _unlocked_tool(line: str) -> bool:
    """Whether ``line`` runs a tool outside the lock (the three forms)."""
    return bool(_UNLOCKED_TOOL.search(line)) or _run_with(line)


#: Rule 4's allowlist, relative to the root: `<line text> -- <reason>`.
BARE_UV_ALLOW = "scripts/.bare-uv-allow"


def _parse_allow(text: str) -> list[tuple[int, str, str]]:
    """(line number, key, reason) per BARE_UV_ALLOW entry: the one reader of
    the format, for the list as it is and as the merge base had it. An entry
    with no ` -- <reason>` has an empty reason."""
    out = []
    for n, raw in enumerate(text.splitlines(), 1):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        key, _, why = line.partition(" -- ")
        out.append((n, key.strip(), why.strip()))
    return out


def read_bare_uv_allow(root: Path) -> tuple[dict[str, str], list[str]]:
    """({line text: reason}, problems) from BARE_UV_ALLOW.

    The list must EXIST, even with no entries. A missing list read as empty
    would let a change delete it -- passing, since an empty list gains
    nothing -- and a later change recreate it with new entries, which
    would pass too: the merge base has no list to be shrunk from. That is
    the #1976 hole; refusing the absence closes it."""
    path = root / BARE_UV_ALLOW
    allow: dict[str, str] = {}
    if not path.is_file():
        return allow, [
            f"{BARE_UV_ALLOW}: missing. It holds the makefile lines that may "
            "run a bare uv, and only shrinks; with no entries left it stays, "
            "empty but for its header"
        ]
    bad: list[str] = []
    for n, key, why in _parse_allow(path.read_text(encoding="utf-8")):
        if not why:
            bad.append(f"{BARE_UV_ALLOW}:{n}: no reason -- `{key[:60]}`")
            continue
        allow[key] = why
    return allow, bad


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


def _past_recipe_comment(physical: list[str], n: int, line: str) -> str:
    """``line`` less a leading recipe comment on its FIRST physical line.

    A recipe's ``@# ...`` is a shell comment, and the shell ends a comment
    at the newline: a ``\\`` at its end is part of the comment, not a
    continuation, so the next physical line runs. Only the first physical
    line is prose; the rest of the logical line is still checked.
    """
    first = physical[n - 1].rstrip()
    if not first.lstrip().startswith("@#"):
        return line
    if not first.endswith("\\"):
        return ""
    return line[len(first) :]


def _allow_keys(text: str) -> list[str]:
    """The entry keys of a BARE_UV_ALLOW text, through _parse_allow."""
    return [key for _, key, _ in _parse_allow(text)]


def added_allow_entries(root: Path, base: str, keys: list[str]) -> list[str]:
    """BARE_UV_ALLOW entries the list at the merge base with ``base`` lacks.

    The list only shrinks, so an entry it did not have is refused even with
    a reason: the line is spelled $(UV) instead. The read is
    _gitbase.added_since_base, the one ratchet read. Its ``since`` is the
    list itself, because the gate predates the list: the change that brings
    the list (doppler#2369) has no baseline and adds nothing. That is safe
    only because read_bare_uv_allow refuses a missing list: once main has
    it, no change can delete it and later recreate it as a new baseline.
    A tree that is not a checkout has no base; a base git cannot read
    fails closed.
    """
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    from _gitbase import BaseUnreadableError, added_since_base, in_git_repo

    if not in_git_repo(root):
        return []
    try:
        added = added_since_base(
            root, base, BARE_UV_ALLOW, keys, _allow_keys, since=BARE_UV_ALLOW
        )
    except BaseUnreadableError:
        return [
            f"{BARE_UV_ALLOW}: cannot resolve {base}, so an ADDED entry "
            "cannot be told from an old one. Fetch it: git fetch --no-tags "
            "--depth=1 origin +refs/heads/main:refs/remotes/origin/main"
        ]
    return [
        f"{BARE_UV_ALLOW}: `{k}` is not in the list at the merge base with "
        f"{base} -- the list only shrinks; spell the line $(UV) instead"
        for k in added
    ]


def bare_uv_offenders(
    root: Path, files: list[str], base: str | None = None
) -> list[str]:
    """``path:line: text`` for each makefile line running a bare uv, plus
    each BARE_UV_ALLOW entry that is unreasoned, matches no line or more
    than one, or (with ``base``) is new since the merge base."""
    allow, bad = read_bare_uv_allow(root)
    hits: dict[str, list[str]] = {k: [] for k in allow}
    for rel in files:
        if not _MAKEFILE.search(rel):
            continue
        text = (root / rel).read_text(encoding="utf-8")
        physical = text.splitlines()
        for n, line in _logical_lines(text):
            line = _past_recipe_comment(physical, n, line)
            if _unlocked_tool(line):
                bad.append(
                    f"{rel}:{n}: runs a tool outside the lock -- "
                    f"`{line.strip()[:70]}`; pin it in the dev group and run "
                    "it through $(DEV_RUN)"
                )
                continue
            if not _BARE_UV.search(line):
                continue
            allowed = [k for k in allow if k in line]
            for k in allowed:
                hits[k].append(f"{rel}:{n}")
            if allowed:
                continue
            bad.append(
                f"{rel}:{n}: bare uv -- `{line.strip()[:70]}`; "
                "spell it $(UV) so UV= reaches it"
            )
    for key in sorted(allow):
        if not hits[key]:
            bad.append(
                f"{BARE_UV_ALLOW}: `{key}` matches no makefile line -- its "
                "reason is gone; remove the entry"
            )
        elif len(hits[key]) > 1:
            bad.append(
                f"{BARE_UV_ALLOW}: `{key}` matches {len(hits[key])} lines "
                f"({', '.join(hits[key])}) -- an entry exempts exactly one; "
                "quote more of the line"
            )
    if base is not None:
        bad += added_allow_entries(root, base, list(allow))
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
    ap.add_argument(
        "--base",
        default=None,
        help=f"ref whose merge base holds the {BARE_UV_ALLOW} this one may "
        "only shrink from (rule 4); unset, that check is skipped",
    )
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
    bad += bare_uv_offenders(root, files, args.base)
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
