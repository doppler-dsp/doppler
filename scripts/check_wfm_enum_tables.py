#!/usr/bin/env python3
"""Gate: the waveform enum name tables have ONE C home, and it matches the
manifest.

`just-makeit.toml`'s `[[enum]]` blocks are declared to be the single source of
truth for every waveform string<->int mapping. Measured (doppler#760), that was
true of the *Python binding* only: jm renders `[[enum]]` into
`wfm_compose_ext.c`, and the C side kept its own tables -- twelve in
`native/src/app/wfmgen.c`, seven in `native/src/wfm/wfm_json.c`, three in
`native/inc/doppler/wfm/wfm_names.h` -- maintained by hand and by nobody's
gate.

Why that is worse than ordinary duplication: **list order IS the C enum
value**. A table that drifts does not fail to compile and does not raise; it
maps a flag to the WRONG waveform. The rot is not hypothetical --
`wfm_writer`'s copy of
`TYPE_NAMES` had already fallen to 8 entries with no "dsss", and `wfm_json.c`'s
`DATA_NAMES` listed `{"none","prbs"}` against `wfmgen.c`'s `{"prbs","none"}`,
which was harmless only because one file compared its index while the other
assigned it.

So this gate makes the SSOT claim a check rather than a comment:

1. **One C home.** Every enum name table lives in
`native/inc/doppler/wfm/wfm_names.h`.
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
7. **An enum bound by NAME has exactly one C enum, and the same names.** An
   `[[enum]]` that lists `enumerators` (gh-1450) makes jm write each C
   constant by name, so the compiler checks every name the manifest uses --
   but not the other way: a C value added with no Python name compiles and
   is unreachable from Python. So the C enum carries `SSOT: enum=X` (and
   nothing else), and its enumerator names equal `X.enumerators`, in order.
   Both directions fail closed: an annotation naming no `[[enum]]`, or one
   whose `[[enum]]` lists no `enumerators`, is an error, and so is an
   `[[enum]]` with `enumerators` that no annotated C enum carries -- unless
   every one of its names is a name of ONE annotated C enum, which makes it
   a view over that enum (`carrier_acq_window` offers three of
   `psd_window`'s four), never a source of names of its own. And the VALUES
   hold, because they are persisted (a Spectrogram blob stores its window
   and mode): they run consecutively in the manifest's order, and an
   enumerator that existed at the merge base with `--base` keeps its value
   (a shallow clone whose base will not resolve fails closed).
8. **A view's set, stated in the manifest and again in C, is held to
   agree.** A view names a subset of its parent for Python, but C takes the
   parent's type, so C must refuse the rest: a second statement of the same
   set (#2194). So the refusal carries `/* SSOT: view=X */` (or `//`) on the
   line directly above it, the view is a PREFIX of its parent (one
   comparison bounds it), and the comparison is `> LAST` or `>= NEXT` with
   the enumerator alone as its operand (`> LAST + 1` admits too much). A
   view with no such comparison, or two, an annotation naming no view, or
   any other comparison is an error that says what to write. It holds the
   comparison, not that the statement returns or runs first.

The annotation is a plain `/* ... */` comment -- doxygen reads only `/**` and
`/*!`, so it is invisible to the C API docs -- placed anywhere between the
previous declaration and the table it describes::

    /* SSOT: enum=wfm_type, count=N_TYPES */
    /* SSOT: enum=ftype,
       cenum=doppler/wfm_writer/wfm_writer_core.h:wfm_filetype_t */

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

from _gitbase import BaseUnreadableError, in_git_repo, resolve_base, show_at
from _layout import GLUE_FRAGMENT, INC_DIR, header

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
NAMES_H_REL = header("wfm/wfm_names.h")
MANIFEST_REL = "just-makeit.toml"

#: Hand-written C scanned for rule 1. jm owns the `*_ext*.c` bindings and
#: renders its own `_enum_*` tables into them from the same `[[enum]]` blocks,
#: so gating those for DUPLICATION would report the manifest's own output as a
#: duplicate. They are gated for AGREEMENT instead -- see rule 6, and the
#: reason the two rules had to be split.
SCAN_ROOTS = ("native/src", "native/inc")
GENERATED_RE = GLUE_FRAGMENT

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
#: Rule 8: `/* SSOT: view=X */` (or `// SSOT: view=X`) on the line above
#: the comparison that refuses past view X, in C under native/.
_VIEW_RE = re.compile(
    r"^[ \t]*(?P<lead>/\*|\*|//)?[ \t]*SSOT:[ \t]*view=(?P<view>\w+)",
    re.MULTILINE,
)
#: The refusal: `> LAST` or the equivalent `>= NEXT`, whose operand is the
#: enumerator ALONE (`> BH + 1` admits a value past the view, so it is no
#: refusal). `->` is a member access, never a `>`.
_BOUND_CMP_RE = re.compile(
    r"(?<![-=<>])(?P<op>>=?)(?!=)\s*(?P<name>[A-Za-z_]\w*)\s*(?=[)&|;?]|$)",
    re.MULTILINE,
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


#: An enum declaration, anonymous or typedef'd, in a header under INC_DIR.
_ENUM_DECL_RE = re.compile(
    r"(?:typedef\s+)?\benum\s*(?:\w+\s*)?\{(?P<body>[^}]*)\}\s*(?:\w+\s*)?;"
)
_COMMENT_RE = re.compile(r"/\*.*?\*/|//[^\n]*", re.DOTALL)


def _enumerators(body: str) -> list[tuple[str, int | None]]:
    """The (name, C value) pairs of an enum body, in source order.

    A value is the explicit integer, or one past the previous value as C
    counts it; None once an explicit value is not an integer literal.
    """
    out: list[tuple[str, int | None]] = []
    nxt: int | None = 0
    for part in _COMMENT_RE.sub("", body).split(","):
        if not part.strip():
            continue
        name, eq, val = part.partition("=")
        if eq:
            try:
                nxt = int(val.strip(), 0)
            except ValueError:
                nxt = None
        out.append((name.strip(), nxt))
        nxt = None if nxt is None else nxt + 1
    return out


def bound_enums(
    root: Path,
) -> tuple[list[tuple[str, str, list[tuple[str, int | None]]]], list[str]]:
    """Every header enum annotated `SSOT: enum=X`, and the malformed ones.

    The annotation is looked for between the previous `;` OUTSIDE a comment
    and the enum itself -- the "since the previous declaration" rule
    `_tables` uses -- so an enum cannot adopt an annotation written for a
    declaration above it. Returns ``(where, X, [(name, value), ...])`` and
    errors. Public: `check_named_args.py` reads the same enums to turn a
    literal back into its name, rather than keeping a copy of them.
    """
    found: list[tuple[str, str, list[tuple[str, int | None]]]] = []
    errs: list[str] = []
    for path in sorted((root / INC_DIR).rglob("*.h")):
        try:
            text = path.read_text(encoding="utf-8")
        except (UnicodeDecodeError, OSError):
            continue
        f, e = _bound_in_text(text, path.relative_to(root).as_posix())
        found += f
        errs += e
    return found, errs


def _bound_in_text(
    text: str, rel: str
) -> tuple[list[tuple[str, str, list[tuple[str, int | None]]]], list[str]]:
    """`bound_enums` for one header's text: the working tree's, or the merge
    base's when the values ratchet reads what an enumerator used to be."""
    found: list[tuple[str, str, list[tuple[str, int | None]]]] = []
    errs: list[str] = []
    if "SSOT:" not in text:
        return found, errs
    # Same offsets, comments blanked: a `;` inside prose ends nothing.
    masked = _COMMENT_RE.sub(lambda m: " " * len(m.group(0)), text)
    for m in _ENUM_DECL_RE.finditer(masked):
        start = masked.rfind(";", 0, m.start()) + 1
        specs = _SSOT_RE.findall(text[start : m.start()])
        if not specs:
            continue
        line = text.count("\n", 0, m.start()) + 1
        if len(specs) > 1:
            errs.append(
                f"{rel}:{line}: {len(specs)} `SSOT:` annotations above"
                " one enum; exactly one describes it."
            )
            continue
        fields = [f.strip() for f in specs[0].split(",") if f.strip()]
        keys = [f.partition("=")[0].strip() for f in fields]
        if keys != ["enum"]:
            errs.append(
                f"{rel}:{line}: a C enum's annotation takes `enum=` only"
                f" (got {specs[0].strip()!r}); its names are checked"
                " against that [[enum]]'s `enumerators`."
            )
            continue
        name = fields[0].partition("=")[2].strip()
        body = text[m.start("body") : m.end("body")]
        found.append((f"{rel}:{line}", name, _enumerators(body)))
    return found, errs


def _check_bound_values(
    root: Path, base: str, bound: list[tuple[str, str, list]]
) -> list[str]:
    """Rule 7's values: what a bound enumerator reads, now and since the base.

    The values are persisted -- the Spectrogram blob stores its window and
    mode, `AccTrace.mode` returns the int -- so a renumbered window re-reads
    every saved blob as another one, and names alone cannot see it. Two
    checks, because each misses what the other catches:

    - **In the tree** (no git): every value is an integer C can read here,
      and the values run consecutively in the manifest's order. That refuses
      `KAISER = 5` and a duplicate, on any clone.
    - **Since the merge base** with `base`: an enumerator that existed there
      keeps its value. That refuses a renumber made TOGETHER with a matching
      reorder of the manifest, which the tree alone reads as consistent.

    A tree that is not a git repository has no base to compare (a seeded
    test tree); a repository whose base will not resolve -- a shallow clone
    -- fails CLOSED, naming the ref, because a ratchet that cannot read its
    baseline has not been checked (scripts/_gitbase.py).
    """
    errs: list[str] = []
    for where, name, pairs in bound:
        vals = [v for _, v in pairs]
        if any(v is None for v in vals):
            errs.append(
                f"{where}: [[enum]] {name}'s C enum has a value this gate"
                " cannot read as an integer; a bound enum's values are"
                " persisted, so each must be a plain integer."
            )
        elif vals != list(range(vals[0], vals[0] + len(vals))):
            errs.append(
                f"{where}: [[enum]] {name}'s values {vals} do not run"
                " consecutively in the manifest's order (a duplicate, or a"
                " renumbered enumerator)."
            )
    if not in_git_repo(root):
        return errs
    try:
        rev = resolve_base(root, base)
    except BaseUnreadableError:
        return [
            *errs,
            f"the values ratchet cannot read its baseline: `{base}` does not"
            " resolve here (a shallow clone?). Fetch it, e.g. `git fetch"
            " --no-tags origin +refs/heads/main:refs/remotes/origin/main`,"
            " or pass --base.",
        ]
    for where, name, pairs in bound:
        rel = where.rpartition(":")[0]
        then = show_at(root, rev, rel)
        if then is None:
            continue
        old = {
            n: v
            for _, x, ps in _bound_in_text(then, rel)[0]
            if x == name
            for n, v in ps
        }
        for n, v in pairs:
            if n in old and old[n] is not None and old[n] != v:
                errs.append(
                    f"{where}: {n} was {old[n]} at the merge base with"
                    f" {base} and is {v} now. Its value is persisted, so it"
                    " may not change: add a new enumerator instead."
                )
    return errs


def _check_bound_enums(root: Path, data: dict) -> list[str]:
    """Rule 7: an [[enum]] bound by name and its one C enum agree."""
    bound, errs = bound_enums(root)
    by_name = {e["name"]: e for e in data.get("enum", [])}
    carried: dict[str, list[str]] = {}
    for where, name, pairs in bound:
        names = [n for n, _ in pairs]
        carried.setdefault(name, []).append(where)
        spec = by_name.get(name)
        if spec is None:
            errs.append(
                f"{where}: `SSOT: enum={name}` names no [[enum]] in"
                f" {MANIFEST_REL}."
            )
            continue
        want = spec.get("enumerators")
        if want is None:
            errs.append(
                f"{where}: [[enum]] {name} lists no `enumerators`, so jm"
                " numbers its names by position and nothing ties them to this"
                " C enum. Bind them by name (gh-1450)."
            )
            continue
        if names != list(want):
            errs.append(
                f"{where}: the C enum's names != [[enum]] {name}"
                f" enumerators\n      header:   {names}\n"
                f"      manifest: {list(want)}"
            )
    views = _views(data, bound)
    for name, spec in by_name.items():
        if "enumerators" not in spec:
            continue
        homes = carried.get(name, [])
        if len(homes) == 1:
            continue
        # A view: every name already belongs to one bound C enum, so the
        # compiler checks them and none is a name of its own (rule 8 holds
        # its C refusal to it).
        if not homes and name in views:
            continue
        errs.append(
            f"{MANIFEST_REL}: [[enum]] {name} lists `enumerators`, so"
            f" exactly one C enum must carry `SSOT: enum={name}` (or every"
            " name must belong to one that does); found"
            f" {len(homes)}{': ' + ', '.join(homes) if homes else ''}."
        )
    return errs


def _views(
    data: dict, bound: list[tuple[str, str, list[tuple[str, int | None]]]]
) -> dict[str, tuple[str, list[tuple[str, int | None]], list[str]]]:
    """Every view: an [[enum]] with `enumerators`, no C enum of its own, and
    every name a name of ONE bound C enum. ``{view: (parent, parent's
    (name, value) pairs, the view's names)}`` -- rule 7's definition, in one
    place for rule 7, rule 8 and `enum_names`."""
    homes = {name for _, name, _ in bound}
    out: dict[str, tuple[str, list[tuple[str, int | None]], list[str]]] = {}
    for spec in data.get("enum", []):
        name, want = spec.get("name"), spec.get("enumerators")
        if not want or name in homes:
            continue
        for _, parent, pairs in bound:
            if set(want) <= {n for n, _ in pairs}:
                out[name] = (parent, pairs, list(want))
                break
    return out


def enum_names(root: Path) -> tuple[dict[str, dict[int, str]], list[str]]:
    """value -> name for every [[enum]] a C enum binds, views included: a
    view's are its parent's, restricted to the view, so a value outside it
    has no name. Public: `check_named_args.py` names a literal by it."""
    bound, errs = bound_enums(root)
    out = {
        enum: {v: n for n, v in pairs if v is not None}
        for _, enum, pairs in bound
    }
    manifest = root / MANIFEST_REL
    if manifest.is_file():
        data = tomllib.loads(manifest.read_text(encoding="utf-8"))
        for view, (_, pairs, names) in _views(data, bound).items():
            out[view] = {
                v: n for n, v in pairs if n in names and v is not None
            }
    return out, errs


def _check_view_bounds(
    root: Path,
    data: dict,
    bound: list[tuple[str, str, list[tuple[str, int | None]]]],
) -> list[str]:
    """Rule 8: a view's set, stated twice, is held to agree.

    A view names a subset of its parent's values for Python; C, which takes
    the parent's type, has to refuse the rest, and that refusal states the
    same set a second time. So the refusal carries `SSOT: view=X` on the
    line above it, the view is a PREFIX of its parent (one comparison bounds
    it), and the comparison is `> LAST` or `>= NEXT` with the enumerator
    alone as its operand. Adding a value to the view without moving the
    refusal, or the reverse, is then red. It checks the comparison, not that
    the statement returns or runs first: review and the C test hold that.
    """
    views = _views(data, bound)
    marks: dict[str, list[tuple[str, tuple[str, str] | None]]] = {
        v: [] for v in views
    }
    errs: list[str] = []
    for top in SCAN_ROOTS:
        for path in sorted((root / top).rglob("*.[ch]")):
            try:
                text = path.read_text(encoding="utf-8")
            except (UnicodeDecodeError, OSError):
                continue
            rel = path.relative_to(root).as_posix()
            for m in _VIEW_RE.finditer(text):
                where = f"{rel}:{text.count(chr(10), 0, m.start()) + 1}"
                view = m.group("view")
                if view not in views:
                    errs.append(
                        f"{where}: `SSOT: view={view}` names no view (an"
                        " [[enum]] whose names all belong to one bound C"
                        " enum, with none of its own)."
                    )
                    continue
                # The comment ends at the line's end (`//`) or at its `*/`;
                # the refusal is the next non-blank line, nothing further.
                if m.group("lead") == "//":
                    after = text.find("\n", m.end())
                else:
                    after = text.find("*/", m.end())
                    after = after + 2 if after >= 0 else m.end()
                after = after if after >= 0 else len(text)
                while after < len(text) and text[after] in " \t\n":
                    after += 1
                eol = text.find("\n", after)
                eol = len(text) if eol < 0 else eol
                cmp_ = _BOUND_CMP_RE.search(text, after, eol)
                marks[view].append(
                    (where, (cmp_["op"], cmp_["name"]) if cmp_ else None)
                )
    for view, (parent, pairs, names) in views.items():
        found = marks[view]
        parent_names = [n for n, _ in pairs]
        last = names[-1]
        nxt = (
            parent_names[len(names)]
            if len(names) < len(parent_names)
            else None
        )
        write = f"`> {last}`" + (f" (or `>= {nxt}`)" if nxt else "")
        if not found:
            errs.append(
                f"{MANIFEST_REL}: view [[enum]] {view} has no C refusal."
                f" C takes {parent}, so put `/* SSOT: view={view} */` on the"
                f" line above the comparison that refuses past the view:"
                f" `if (<arg> > {last})`"
                + (f" (or `>= {nxt}`)." if nxt else ".")
            )
            continue
        if len(found) > 1:
            errs.append(
                f"{MANIFEST_REL}: view [[enum]] {view} has {len(found)}"
                f" `SSOT: view={view}` annotations"
                f" ({', '.join(w for w, _ in found)}); exactly one refusal"
                " states its bound."
            )
            continue
        where, cmp_ = found[0]
        if names != parent_names[: len(names)]:
            errs.append(
                f"{where}: view {view} ({names}) is not a prefix of {parent}"
                f" ({parent_names}), so one comparison cannot bound it."
            )
            continue
        if cmp_ is None:
            errs.append(
                f"{where}: the line after `SSOT: view={view}` is not a"
                f" refusal of the form {write}, with the enumerator alone as"
                " its operand."
            )
            continue
        op, name = cmp_
        if (op, name) in ((">", last), (">=", nxt)):
            continue
        admitted = (
            parent_names[: parent_names.index(name) + (op == ">")]
            if name in parent_names
            else None
        )
        errs.append(
            f"{where}: the refusal `{op} {name}` admits"
            f" {admitted if admitted is not None else 'an unknown set'},"
            f" but view {view} is {names}: write {write}."
        )
    return errs


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


def check(root: Path, base: str = "origin/main") -> list[str]:
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
            path = root / INC_DIR / rel
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

    # Rule 7 -- an [[enum]] bound by name has one C enum with its names.
    errs += _check_bound_enums(root, data)
    bound = bound_enums(root)[0]
    errs += _check_bound_values(root, base, bound)
    # Rule 8 -- a view's set, stated twice, is held to agree.
    errs += _check_view_bounds(root, data, bound)
    return errs


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "--root",
        type=Path,
        default=Path(__file__).resolve().parent.parent,
        help="tree to check (default: the repository root)",
    )
    ap.add_argument(
        "--base",
        default="origin/main",
        help="ref whose merge base holds the enumerator values a bound enum"
        " may not change (rule 7)",
    )
    args = ap.parse_args()

    errs = check(args.root.resolve(), args.base)
    if errs:
        print(f"wfm-enum-tables: {len(errs)} problem(s).")
        for e in errs:
            print(f"  {e}")
        print(
            "\nList order IS the C enum value: a table that drifts maps a flag"
            " to the\nwrong waveform rather than failing. One home"
            " (native/inc/doppler/wfm/wfm_names.h),\none declaration"
            " (just-makeit.toml [[enum]]). See doppler#760."
        )
        return 1

    root = args.root.resolve()
    n = len(_tables((root / NAMES_H_REL).read_text(encoding="utf-8")))
    bound = bound_enums(root)[0]
    data = tomllib.loads((root / MANIFEST_REL).read_text(encoding="utf-8"))
    v = len(_views(data, bound))
    print(
        f"wfm-enum-tables: OK — {n} table(s) match their [[enum]],"
        f" {len(bound)} C enum(s) bound by name, {v} view(s) bounded in C,"
        " 0 second homes"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
