#!/usr/bin/env python3
"""A finding is cited by its key or its claim, never by its number.

A validation report numbers its findings by position (``F1`` for the first
``find()``), so a number goes stale the moment a finding before it is
dropped or reordered: #2056 dropped one PSD finding and left five
citations pointing at the wrong one, two of them outside the report
(#2059). Inside a report that is handled at render: prose cites
``R.ref(key)`` and ``Report._self_check`` refuses a typed number.
Everything OUTSIDE a report -- a C test's comment, a header, a design
page, an example, a validator's own comments -- has no render step, so
this gate holds it, in both directions:

1. **No typed number.** ``F1`` to ``F99`` as a whole word is refused.
   ``F32``/``F64`` are dtype names and pass. Cite the key, or say the
   claim in words.
2. **Every cited key exists.** A citation is the word *finding* then the
   key in backticks, as in ``finding `pfa_over_delivered``` (or a list:
   ``findings `pfa_over_delivered` and `unity_window```), and each key must
   be one some
   ``validate.py`` records with ``find("key", ...)``. A renamed or
   dropped finding would otherwise leave the citation pointing at
   nothing, which is the same defect as a stale number, one step later.
3. **A key names ONE finding.** A key recorded by two reports is refused:
   a citation outside a report resolves against every report at once, so
   a shared key would let it resolve through the wrong one.
4. **A validator's own ``ref``/``number`` keys exist in that file.** A
   ``number(key)`` runs only when figures are drawn, which no CI run does,
   so a stale one would otherwise surface only at the next regeneration.

The patterns are not declared here. They live once, in
``src/doppler/tests/_finding_keys.py``, which ``Report`` imports too: two
copies once disagreed on ``F32``. That module is stdlib-only, so this
gate loads it by path and still runs on the floor Python, without the
extension modules.

**Registration-free.** It scans every tracked text file (``git
ls-files``) and reads every tracked ``validate.py``, so a new test, page,
example or report is covered the moment it is committed.

**What it skips, and why.** Generated files, whose source is scanned or
renders its own citations: a ``results.md`` (its numbers come from
``render()``), the validation log built from them, ``docs/c-api`` (built
from the headers), ``*.pyi`` stubs, vendored code. ``CHANGELOG.md``,
because a released entry records what a report said at that release.
``docs/dev/issues.md`` and ``issue-tiers.toml``, which carry GitHub's
issue titles verbatim. And, for the typed-number rule only, the files
whose subject IS the numbering -- the report framework, its docs, its
tests and the report gates -- which have to write ``F1`` to explain it;
their key citations are still resolved.

Usage
-----
    python3 scripts/check_finding_citations.py           # exit 1 on a hit
    python3 scripts/check_finding_citations.py --root D  # scan another tree
"""

from __future__ import annotations

import argparse
import fnmatch
import importlib.util
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

#: The one declaration of the patterns, shared with `Report`.
_KEYS_MODULE = ROOT / "src" / "doppler" / "tests" / "_finding_keys.py"

#: The text files scanned.
SUFFIXES = ("*.c", "*.h", "*.py", "*.md", "*.toml", "*.cmake", "*.rs")

#: Skipped by both rules: generated, released history, GitHub's titles.
SKIP = (
    "*/results.md",
    "docs/dev/contributing/validation-log.md",
    "docs/c-api/*",
    "*.pyi",
    "vendor/*",
    "CHANGELOG.md",
    "docs/dev/issues.md",
    "docs/dev/issue-tiers.toml",
    # This gate's own test seeds typed numbers and unknown keys on purpose.
    "src/doppler/tests/test_finding_citations_gate.py",
)

#: Skipped by the typed-number rule only: the numbering's own framework,
#: which has to write ``F1`` to explain how numbers are assigned.
FRAMEWORK = (
    "src/doppler/tests/_finding_keys.py",
    "src/doppler/tests/_validation_common.py",
    "src/doppler/tests/test_validation_report.py",
    "docs/dev/contributing/validation.md",
    "scripts/gen_validation_log.py",
    "scripts/check_validation_reports.py",
    "scripts/check_finding_citations.py",
    "scripts/codemod_finding_keys.py",
)


def _patterns():
    """`_finding_keys`, loaded by path: importing `doppler` needs a build."""
    spec = importlib.util.spec_from_file_location(
        "_finding_keys", _KEYS_MODULE
    )
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def _skipped(path: str, globs: tuple[str, ...]) -> bool:
    return any(fnmatch.fnmatch(path, g) for g in globs)


def tracked(root: Path, *globs: str) -> list[str]:
    """Every tracked file matching `globs`, as git names it."""
    out = subprocess.run(
        ["git", "-C", str(root), "ls-files", "-z", "--", *globs],
        check=True,
        capture_output=True,
        text=True,
    ).stdout
    return [p for p in out.split("\0") if p]


def scan_validators(root: Path, pat):
    """Every recorded key, the keys two reports share, and dangling refs.

    Returns ``(keys, shared, dangling)``: the set of keys some tracked
    ``validate.py``, or a module beside it, records with ``find()``;
    ``{key: [validate.py, ...]}`` for a key recorded by more than one
    report; and ``(file:line, key)`` for each literal ``ref``/``number``
    key its own report does not record.
    """
    owners: dict[str, list[str]] = {}
    dangling: list[tuple[str, str]] = []
    # A report is its validator's FOLDER: validate.py may render part of
    # the report from a sibling module (the correlator's renders
    # 2-D detector flavor's part from detector.py, #2752 step 3.3), so the keys
    # one module records and another cites are the same report's. Every
    # tracked .py beside a validate.py is read with it.
    every_py = tracked(root, "*.py")
    for rel in tracked(root, "*validate.py"):
        folder = str(Path(rel).parent)
        mods = [r for r in every_py if str(Path(r).parent) == folder]
        srcs = {r: (root / r).read_text("utf-8") for r in mods}
        mine = {k for src in srcs.values() for k in pat.recorded_keys(src)}
        for key in mine:
            owners.setdefault(key, []).append(rel)
        for r, src in srcs.items():
            for line, key in pat.referenced_keys(src):
                if key not in mine:
                    dangling.append((f"{r}:{line}", key))
    shared = {k: v for k, v in owners.items() if len(v) > 1}
    return set(owners), shared, dangling


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    # A gate that can only run against the real tree cannot be sabotaged
    # without breaking doppler, so its test points it at a seeded one.
    ap.add_argument("--root", type=Path, default=ROOT)
    root = ap.parse_args(argv[1:]).root.resolve()
    pat = _patterns()

    keys, shared, dangling = scan_validators(root, pat)
    files, typed, unknown, unread = 0, [], [], []
    for rel in tracked(root, *SUFFIXES):
        if _skipped(rel, SKIP):
            continue
        # A file it cannot read is not a clean file: skipping it silently
        # is how a cp1252-written test page once passed on Windows.
        try:
            text = (root / rel).read_text(encoding="utf-8")
        except (OSError, UnicodeDecodeError) as e:
            unread.append((rel, type(e).__name__))
            continue
        files += 1
        lines = text.splitlines()
        if not _skipped(rel, FRAMEWORK):
            for n, line in enumerate(lines, 1):
                for m in pat.TYPED_F.finditer(line):
                    typed.append((f"{rel}:{n}", m.group(0), line.strip()))
        for m in pat.CITE.finditer(text):
            n = text.count("\n", 0, m.start()) + 1
            for key in pat.cited_keys(m.group(0)):
                if key not in keys:
                    unknown.append((f"{rel}:{n}", key))

    # A scan that read nothing is broken, not clean, and so is a key set
    # that is empty: every citation would then be refused, or with the
    # check inverted, none.
    if files == 0:
        print("check_finding_citations: read no file -- the scan is broken")
        return 1
    if not keys:
        print(
            "check_finding_citations: no validate.py records a key -- "
            "the key scan is broken"
        )
        return 1
    # Every message is ASCII: a Windows console's code page (cp1252) prints
    # an em-dash as a replacement character.
    if typed:
        print("check_finding_citations: a finding is cited by number -- FAIL")
        for where, tag, line in typed:
            print(f"  {where}: {tag}  | {line[:70]}")
        print(
            "\n  Findings are numbered by position, so a number re-points"
            "\n  when one before it is dropped (#2059). Cite the finding's"
            "\n  key as finding `pfa_over_delivered`, using the key its"
            "\n  validate.py records, or say the claim in words."
        )
    if unknown:
        print(
            "check_finding_citations: a cited finding does not exist -- FAIL"
        )
        for where, key in unknown:
            print(f"  {where}: finding `{key}`")
        print(
            "\n  No validate.py records these keys with find(). A finding"
            "\n  was renamed or dropped and its citation left behind: cite"
            "\n  the key it has now, or say the claim in words."
        )
    if shared:
        print("check_finding_citations: a key names two findings -- FAIL")
        for key, owners in sorted(shared.items()):
            print(f"  `{key}`: {', '.join(owners)}")
        print(
            "\n  A citation outside a report resolves against every report,"
            "\n  so a shared key can resolve through the wrong one. Rename"
            "\n  one of them (prefix it with its object)."
        )
    if dangling:
        print(
            "check_finding_citations: a validator cites a key it never "
            "records -- FAIL"
        )
        for where, key in dangling:
            print(f"  {where}: `{key}`")
        print(
            "\n  ref() is refused at render, but number() runs only when"
            "\n  figures are drawn: record the key with find(), or cite the"
            "\n  one it has now."
        )
    if unread:
        print("check_finding_citations: a scanned file cannot be read -- FAIL")
        for rel, why in unread:
            print(f"  {rel}: {why} (scanned files are UTF-8)")
    if typed or unknown or shared or dangling or unread:
        return 1
    print(
        f"check_finding_citations: OK -- {files} file(s), none typed; "
        f"every citation resolves against {len(keys)} key(s)"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
