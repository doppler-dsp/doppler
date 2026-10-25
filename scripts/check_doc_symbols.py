#!/usr/bin/env python3
"""Gate: a C symbol named in the docs is a C symbol the tree defines.

`check_doc_paths.py` holds the file half of a citation and
`check_doc_sections.py` the section half. This holds the third: a page that
names `dp_frame_crc_ok()` or `dp_acc_f32_state_t` is telling the reader what
to call, and nothing checked that the name still exists. The 2026-10-25 docs
audit (#2580) found about twenty that did not -- types renamed by the
`*_state_t` convention, functions deleted with the API they belonged to, a
`dp_cf64_t` that never existed -- each of them in a page every docs gate
passed.

The snippet gates do not cover this. A fence they compile catches a wrong
name, but a fence marked `skip=` is not compiled, and prose is not a fence at
all. `docs/examples/streaming.md` declared `dp_cf64_t *data` inside a skipped
fence; the type had never existed, and the skip hid it.

**What is checked.** Every `dp_*` / `DP_*` identifier inside a code span or a
fence on a hand-written page under `docs/`, against the identifiers in the
comment-stripped sources git tracks under `native/`, `ffi/`, `vendor/` and
the CMake files. Comments are stripped because a comment naming a deleted
function is exactly what keeps a stale name looking alive. A symbol a test or
validation helper defines counts: the design pages document those helpers on
purpose.

**What is not.** `docs/c-api/` is mkdoxy output from the headers, so it
cannot name what the headers do not. A page under an `archive/` directory is
a record of how things were, and a name that has since gone is history there,
not a defect -- the same reason `check_doc_paths.py` leaves released
changelog entries alone. A file name (`dp_state_pyhelp.h`) is
`check_doc_paths.py`'s, and a placeholder (`dp_<obj>_create`, `dp_mytype_t`,
`dp_foo_create`) names no symbol at all.

**A page may name a symbol that is gone on purpose** -- "the cell flavor,
deleted with the pool", "there is no `dp_source_t`". Those are listed in
EXEMPT as `page symbol`, and the list may only SHRINK: an entry whose symbol
now exists, or whose page no longer names it, is itself a failure, so the
exemptions cannot outlive their reason.

Nothing is registered: the pages are globbed and the symbols come out of the
tree, so a new page or a new name is covered by existing.

Usage:  python3 scripts/check_doc_symbols.py [--root DIR]
Exit 0 when every C symbol a page names exists or is listed in EXEMPT.
"""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

#: Pages naming a symbol that is gone on purpose, one `page symbol` per line.
EXEMPT = Path("scripts") / ".doc-symbol-exempt"

#: Where the symbols are defined. Tests and validation are under native/.
SYMBOL_DIRS = ("native/", "ffi/", "vendor/", "cmake/")
SYMBOL_FILE = re.compile(r"\.(?:c|h|in|rs|cmake)$|(?:^|/)CMakeLists\.txt$")

#: A `dp_`/`DP_` identifier. Not when it is part of a longer word, a file
#: name (`dp_state_pyhelp.h`) or a pattern (`dp_*`, `DP_<X>`).
_SYMBOL = re.compile(
    r"(?<![A-Za-z0-9_<])((?:dp|DP)_[A-Za-z0-9_]*[A-Za-z0-9])"
    r"(?![A-Za-z0-9_*<>{]|\.[A-Za-z])"
)

#: An underscore-separated word that marks a name as a template, not a
#: symbol: `dp_mytype_create`, `dp_foo_create`.
PLACEHOLDER_WORDS = frozenset({"foo", "bar", "baz", "mytype", "myobj"})

_FENCE = re.compile(r"^[ \t]*(```|~~~).*?^[ \t]*\1", re.S | re.M)
_SPAN = re.compile(r"`([^`\n]+)`")
_BLOCK_COMMENT = re.compile(r"/\*.*?\*/", re.S)
_LINE_COMMENT = re.compile(r"//[^\n]*")


def _tracked(root: Path) -> list[str]:
    out = subprocess.run(
        ["git", "ls-files"],
        cwd=root,
        capture_output=True,
        text=True,
        check=True,
    ).stdout
    return out.split()


def defined_symbols(files: list[str], root: Path) -> set[str]:
    """Every `dp_`/`DP_` identifier in the comment-stripped sources."""
    found: set[str] = set()
    for f in files:
        top_cmake = f == "CMakeLists.txt"
        if not (f.startswith(SYMBOL_DIRS) or top_cmake):
            continue
        if not SYMBOL_FILE.search(f):
            continue
        text = (root / f).read_text(encoding="utf-8", errors="replace")
        text = _LINE_COMMENT.sub("", _BLOCK_COMMENT.sub(" ", text))
        found.update(re.findall(r"\b(?:dp|DP)_\w+\b", text))
    return found


def pages(files: list[str]) -> list[str]:
    """Hand-written pages: not generated C API, not an archived record."""
    return sorted(
        f
        for f in files
        if f.startswith("docs/")
        and f.endswith(".md")
        and not f.startswith("docs/c-api/")
        and "archive" not in Path(f).parts
    )


def named_symbols(text: str) -> set[str]:
    """The `dp_*` names in a page's fences and code spans."""
    fences = [m.group(0) for m in _FENCE.finditer(text)]
    prose = _FENCE.sub("", text)
    spans = fences + _SPAN.findall(prose)
    names = {m.group(1) for s in spans for m in _SYMBOL.finditer(s)}
    return {
        n for n in names if not PLACEHOLDER_WORDS & set(n.lower().split("_"))
    }


def read_exempt(root: Path) -> set[tuple[str, str]]:
    out: set[tuple[str, str]] = set()
    path = root / EXEMPT
    if not path.exists():
        return out
    for line in path.read_text(encoding="utf-8").splitlines():
        line = line.split("#", 1)[0].strip()
        if line:
            page, symbol = line.split()
            out.add((page, symbol))
    return out


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "--root",
        type=Path,
        default=ROOT,
        help="tree to scan; the gate's own test points this at a seeded one",
    )
    root = ap.parse_args(argv).root.resolve()

    files = _tracked(root)
    defined = defined_symbols(files, root)
    scanned = pages(files)
    if not defined or not scanned:
        print(
            "check_doc_symbols: FAIL — found "
            f"{len(defined)} symbol(s) and {len(scanned)} page(s); a scan "
            "over nothing did not run, so it has not passed",
            file=sys.stderr,
        )
        return 1

    exempt = read_exempt(root)
    failures: list[str] = []
    named: set[tuple[str, str]] = set()
    for page in scanned:
        text = (root / page).read_text(encoding="utf-8", errors="replace")
        for sym in named_symbols(text):
            named.add((page, sym))
            if sym in defined or (page, sym) in exempt:
                continue
            failures.append(
                f"{page}: names `{sym}`, which nothing in native/, ffi/, "
                "vendor/ or the CMake files defines. Use the current name; "
                "if the page names it BECAUSE it is gone, add "
                f"`{page} {sym}` to {EXEMPT.as_posix()}."
            )
    for page, sym in sorted(exempt):
        if sym in defined:
            failures.append(
                f"{EXEMPT.as_posix()}: `{sym}` exists now; delete "
                f"`{page} {sym}` (the list only shrinks)."
            )
        elif (page, sym) not in named:
            failures.append(
                f"{EXEMPT.as_posix()}: {page} no longer names `{sym}`; "
                f"delete `{page} {sym}` (the list only shrinks)."
            )

    if failures:
        print("check_doc_symbols: FAIL\n", file=sys.stderr)
        for f in sorted(failures):
            print(f"  - {f}\n", file=sys.stderr)
        return 1

    print(
        f"check_doc_symbols: OK — {len(named)} C symbol name(s) across "
        f"{len(scanned)} page(s) resolve against {len(defined)} defined; "
        f"{len(exempt)} exempt"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
