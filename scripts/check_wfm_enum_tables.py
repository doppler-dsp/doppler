#!/usr/bin/env python3
"""Gate: the waveform enum name tables have ONE C home, and it matches the
manifest.

`just-makeit.toml`'s `[[enum]]` blocks are declared to be the single source of
truth for every waveform string<->int mapping. Measured (doppler#760), that was
true of the *Python binding* only: jm renders `[[enum]]` into
`wfm_compose_ext.c`, and the C side kept its own tables -- twelve in
`native/src/app/wfmgen.c`, seven in `native/src/wfm/wfm_json.c`, three in
`native/inc/wfm/wfm_names.h` -- maintained by hand and by nobody's gate.

Why that is worse than ordinary duplication: **list order IS the C enum
value**. A table that drifts does not fail to compile and does not raise; it
maps a flag to the WRONG waveform. The rot is not hypothetical --
`wfm_writer`'s copy of
`TYPE_NAMES` had already fallen to 8 entries with no "dsss", and `wfm_json.c`'s
`DATA_NAMES` listed `{"none","prbs"}` against `wfmgen.c`'s `{"prbs","none"}`,
which was harmless only because one file compared its index while the other
assigned it.

So this gate makes the SSOT claim a check rather than a comment:

1. **One C home.** Every enum name table lives in `native/inc/wfm/wfm_names.h`.
   No other hand-written C file may declare a table with the same contents.
2. **The header matches the manifest.** Each table's strings equal its
   `[[enum]]` `values`, in order.
3. **Fail-closed on a new table.** Every table in the header must carry an
   `SSOT:` annotation naming the `[[enum]]` that owns it. A table added without
   one is an error, not a silent pass.
4. **The C enums that fix the indices agree.** Where a table names a `cenum=`,
   that C enum's Nth enumerator must have the explicit value N.
5. **The `count=` macros match** the tables they size.
6. **jm's own `_enum_*` tables match their `[[enum]]`.** Rule 1 skips the
   binding files because jm renders their tables from the manifest -- but a
   *sacred* `<module>_ext_<obj>.c` fragment is reconciled member by member and
   never re-rendered, so a new enum value reaches the manifest, the C enum and
   the `.pyi` while that table stays short. The getter then indexes past its
   own NULL. Fail-closed: a table matching no `[[enum]]` is an error too.

The annotation is a plain `/* ... */` comment -- doxygen reads only `/**` and
`/*!`, so it is invisible to the C API docs -- placed anywhere between the
previous declaration and the table it describes::

    /* SSOT: enum=wfm_type, count=N_TYPES */
    /* SSOT: enum=ftype, cenum=wfm_writer/wfm_writer_core.h:wfm_filetype_t */

It lives beside the table rather than in a list here on purpose: a mapping kept
in a second file is a second thing to update, and the table it describes is
exactly what someone editing it is looking at.

Usage:  python3 scripts/check_wfm_enum_tables.py [--root DIR]
Exit 0 when the header, the manifest and the C enums all agree and no other
hand-written C file re-declares a table.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

# `tomllib` is 3.11+. Every OTHER gate under scripts/ that a test in
# src/doppler/tests/ drives is stdlib-only and so runs on the floor Python,
# because those tests invoke the script with `sys.executable` -- which, in the
# `Python 3.9`/`3.10` CI matrix jobs, IS 3.9/3.10. This one reads the manifest,
# so it needs a TOML parser, and a bare `import tomllib` failed both those jobs
# with ModuleNotFoundError while passing every 3.11+ job and every local run.
# `tomli` is the same parser under its pre-stdlib name, marker-gated in the dev
# group so it installs only where the stdlib lacks one.
if sys.version_info >= (3, 11):
    import tomllib
else:  # pragma: no cover - exercised by the 3.9/3.10 CI matrix jobs
    import tomli as tomllib

#: Where the one C home lives, and the manifest that owns its contents. Both
#: are relative to `--root` so the gate can be exercised over a seeded tree --
#: a gate that can only run against the real tree cannot be sabotaged.
NAMES_H_REL = "native/inc/wfm/wfm_names.h"
MANIFEST_REL = "just-makeit.toml"

#: Hand-written C scanned for rule 1. jm owns the `*_ext*.c` bindings and
#: renders its own `_enum_*` tables into them from the same `[[enum]]` blocks,
#: so gating those for DUPLICATION would report the manifest's own output as a
#: duplicate. They are gated for AGREEMENT instead -- see rule 6, and the
#: reason the two rules had to be split.
SCAN_ROOTS = ("native/src", "native/inc")
GENERATED_RE = re.compile(r"_ext(_[a-z0-9_]+)?\.c$")

#: jm's own tables, in the binding files rule 1 skips. `_enum_stype` is the
#: bare form and `_enum_Reader_t0_source` the per-property one; both end in
#: the manifest name that owns them.
_JM_TABLE_RE = re.compile(
    r"static\s+const\s+char\s*\*\s*const\s+_enum_(\w+)\s*\[[^\]]*\]"
    r"\s*=\s*\{(?P<body>[^}]*)\}"
)

_TABLE_RE = re.compile(
    r"static\s+const\s+char\s*\*\s*const\s+(\w+)\s*\[[^\]]*\]\s*=\s*\{"
    r"(?P<body>[^}]*)\}"
)
_STR_RE = re.compile(r'"((?:[^"\\]|\\.)*)"')
_DEFINE_RE = re.compile(r"^\s*#\s*define\s+(\w+)\s+(\d+)\s*$", re.MULTILINE)
#: An annotation must OPEN its line (bare, or just inside `/*` or after the
#: `*` of a continuation). Prose that merely mentions the marker mid-sentence
#: -- as this header's own banner does when it explains the convention -- is
#: then not mistaken for one.
_SSOT_RE = re.compile(
    r"^[ \t]*(?:/\*|\*)?[ \t]*SSOT:[ \t]*(?P<spec>[^*\n]+)", re.MULTILINE
)


class Table:
    """One `static const char *const X[]` and the annotation that owns it."""

    def __init__(self, name: str, values: list[str], spec: str | None) -> None:
        self.name = name
        self.values = values
        self.enum: str | None = None
        self.count: str | None = None
        self.cenum: str | None = None
        self.error: str | None = None
        if spec is None:
            self.error = (
                f"{name}[] carries no `SSOT:` annotation. Add one naming the"
                " [[enum]] that owns it, e.g."
                f" `/* SSOT: enum=<manifest name> */` directly above it."
            )
            return
        for field in (f.strip() for f in spec.split(",")):
            if not field:
                continue
            key, _, val = field.partition("=")
            key, val = key.strip(), val.strip()
            if key == "enum":
                self.enum = val
            elif key == "count":
                self.count = val
            elif key == "cenum":
                self.cenum = val
            else:
                self.error = (
                    f"{name}[]: unknown SSOT field {key!r}"
                    " (expected enum=, count= or cenum=)."
                )
                return
        if not self.enum:
            self.error = f"{name}[]: SSOT annotation names no `enum=`."


def _tables(text: str) -> list[Table]:
    """Every table in `text`, each paired with its preceding `SSOT:` spec.

    The annotation is looked for in the span since the PREVIOUS declaration
    ended, not in "the comment above". Anchoring on the previous statement is
    what stops a newly inserted table from silently adopting the annotation
    written for the one below it.
    """
    out: list[Table] = []
    prev_end = 0
    for m in _TABLE_RE.finditer(text):
        region = text[prev_end : m.start()]
        specs = _SSOT_RE.findall(region)
        spec = specs[-1] if specs else None
        t = Table(m.group(1), _STR_RE.findall(m.group("body")), spec)
        if len(specs) > 1:
            t.error = (
                f"{m.group(1)}[]: {len(specs)} `SSOT:` annotations since the"
                " previous declaration; exactly one describes a table."
            )
        out.append(t)
        prev_end = m.end()
    return out


def _c_enum(path: Path, typedef: str) -> list[tuple[str, int | None]] | None:
    """The enumerators of `typedef enum {...} <typedef>;`, in source order.

    Returns None when the typedef is not found, which the caller reports --
    a gate that silently skips a check it cannot perform is the "absent output
    is not a pass" failure this repo has already been bitten by.
    """
    m = re.search(
        rf"typedef\s+enum\s*\{{(?P<body>[^}}]*)\}}\s*{re.escape(typedef)}\s*;",
        path.read_text(encoding="utf-8"),
        re.DOTALL,
    )
    if not m:
        return None
    body = re.sub(r"/\*.*?\*/", "", m.group("body"), flags=re.DOTALL)
    body = re.sub(r"//[^\n]*", "", body)
    out: list[tuple[str, int | None]] = []
    for part in body.split(","):
        part = part.strip()
        if not part:
            continue
        if "=" in part:
            nm, _, val = part.partition("=")
            try:
                out.append((nm.strip(), int(val.strip(), 0)))
            except ValueError:
                out.append((nm.strip(), None))
        else:
            out.append((part, None))
    return out


def _scan_files(root: Path, names_h: Path) -> list[Path]:
    out: list[Path] = []
    for rel in SCAN_ROOTS:
        base = root / rel
        if not base.is_dir():
            continue
        for p in sorted(base.rglob("*")):
            if p.suffix not in (".c", ".h") or p == names_h:
                continue
            if GENERATED_RE.search(p.name):
                continue
            out.append(p)
    return out


def check(root: Path) -> list[str]:
    """Every problem found, as ready-to-print lines."""
    errs: list[str] = []
    names_h = root / NAMES_H_REL
    manifest = root / MANIFEST_REL
    if not names_h.is_file():
        return [f"{NAMES_H_REL}: missing — there is no one C home to check."]
    if not manifest.is_file():
        return [
            f"{MANIFEST_REL}: missing — nothing to check the header against."
        ]

    header = names_h.read_text(encoding="utf-8")
    tables = _tables(header)
    if not tables:
        return [
            f"{NAMES_H_REL}: no name tables found. Either the header was"
            " emptied or this gate stopped recognising its shape; both are"
            " failures, because a check that matches nothing reports a clean"
            " tree."
        ]
    defines = {
        m.group(1): int(m.group(2)) for m in _DEFINE_RE.finditer(header)
    }
    data = tomllib.loads(manifest.read_text(encoding="utf-8"))
    enums = {e["name"]: list(e["values"]) for e in data.get("enum", [])}

    for t in tables:
        if t.error:
            errs.append(f"{NAMES_H_REL}: {t.error}")
            continue
        want = enums.get(t.enum or "")
        if want is None:
            errs.append(
                f"{MANIFEST_REL}: no [[enum]] named '{t.enum}' (claimed by"
                f" {t.name}[]). Declare it, or fix the SSOT annotation."
            )
        elif t.values != want:
            errs.append(
                f"{t.name}[] != [[enum]] {t.enum}\n"
                f"      header:   {t.values}\n"
                f"      manifest: {want}"
            )
        if t.count is not None:
            n = defines.get(t.count)
            if n is None:
                errs.append(
                    f"{NAMES_H_REL}: {t.name}[] claims count macro"
                    f" {t.count}, which is not defined."
                )
            elif n != len(t.values):
                errs.append(
                    f"{NAMES_H_REL}: {t.count} is {n}, but {t.name}[] has"
                    f" {len(t.values)} entries."
                )
        if t.cenum is not None:
            rel, _, typedef = t.cenum.partition(":")
            path = root / "native" / "inc" / rel
            if not typedef or not path.is_file():
                errs.append(
                    f"{t.name}[]: cenum={t.cenum} does not resolve to a header"
                    " under native/inc (expected `<path>:<typedef>`)."
                )
                continue
            members = _c_enum(path, typedef)
            if members is None:
                errs.append(
                    f"{rel}: no `typedef enum {{...}} {typedef};` found"
                    f" (claimed by {t.name}[]). The ordering {t.name}[] relies"
                    " on is then checked by nothing."
                )
                continue
            if len(members) != len(t.values):
                errs.append(
                    f"{typedef} has {len(members)} enumerator(s) but"
                    f" {t.name}[] has {len(t.values)} entries."
                )
            for i, (nm, val) in enumerate(members):
                if val is not None and val != i:
                    errs.append(
                        f"{typedef}: {nm} = {val}, but it is at index {i};"
                        f" {t.name}[] is indexed by position."
                    )

    # Rule 1 -- no second C home. A verbatim re-declaration is the regression
    # this gate exists to catch, and it is what the compiler cannot see.
    by_values = {tuple(t.values): t.name for t in tables if not t.error}
    for path in _scan_files(root, names_h):
        try:
            text = path.read_text(encoding="utf-8")
        except (UnicodeDecodeError, OSError):
            continue
        for local in _tables(text):
            owner = by_values.get(tuple(local.values))
            if owner is None:
                continue
            rel = path.relative_to(root).as_posix()
            errs.append(
                f"{rel}: {local.name}[] re-declares {owner}[] from"
                f" wfm/wfm_names.h. Include that header and use {owner}."
            )

    # Rule 6 -- jm's OWN tables still have to agree with the manifest.
    #
    # The comment on GENERATED_RE used to be the whole argument for skipping
    # these: jm renders them from the same [[enum]], so they cannot drift.
    # That is true of a file jm re-renders and FALSE of a sacred
    # `<module>_ext_<obj>.c` fragment, which `jm apply` reconciles member by
    # member and never re-renders. Measured 2026-09-03: adding a third value
    # to the `t0_source` enum updated the manifest, the C enum and the `.pyi`,
    # and left `_enum_Reader_t0_source[]` two entries long -- so the getter
    # indexed one past its own NULL terminator for the new value, which is not
    # a wrong string but a read of whatever follows the array.
    #
    # Fail-closed: a table whose name matches no [[enum]] is an error, because
    # the alternative is a table this gate silently does not check. All 23 in
    # the tree resolve today.
    for path in sorted((root / "native" / "src").rglob("*.c")):
        if not GENERATED_RE.search(path.name):
            continue
        try:
            text = path.read_text(encoding="utf-8")
        except (UnicodeDecodeError, OSError):
            continue
        rel = path.relative_to(root).as_posix()
        for m in _JM_TABLE_RE.finditer(text):
            name = m.group(1)
            values = _STR_RE.findall(m.group("body"))
            # Longest suffix wins: `_enum_Reader_fs_source` must resolve to
            # `fs_source` and not to a shorter name that happens to end it.
            owned = sorted(
                (e for e in enums if name == e or name.endswith("_" + e)),
                key=len,
                reverse=True,
            )
            if not owned:
                errs.append(
                    f"{rel}: _enum_{name}[] matches no [[enum]] in"
                    f" {MANIFEST_REL}, so nothing checks it. Name the table"
                    " after the manifest enum that owns it."
                )
                continue
            want = enums[owned[0]]
            if values != want:
                errs.append(
                    f"{rel}: _enum_{name}[] is {values}, but [[enum]]"
                    f" {owned[0]} is {want}. A sacred fragment is reconciled"
                    " member by member and this table is NOT re-rendered:"
                    " edit it by hand to match."
                )
    return errs


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "--root",
        type=Path,
        default=Path(__file__).resolve().parent.parent,
        help="tree to check (default: the repository root)",
    )
    args = ap.parse_args()

    errs = check(args.root.resolve())
    if errs:
        print(f"wfm-enum-tables: {len(errs)} problem(s).")
        for e in errs:
            print(f"  {e}")
        print(
            "\nList order IS the C enum value: a table that drifts maps a flag"
            " to the\nwrong waveform rather than failing. One home"
            " (native/inc/wfm/wfm_names.h),\none declaration"
            " (just-makeit.toml [[enum]]). See doppler#760."
        )
        return 1

    n = len(_tables((args.root / NAMES_H_REL).read_text(encoding="utf-8")))
    print(
        f"wfm-enum-tables: OK — {n} table(s) match their [[enum]],"
        " 0 second homes"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
