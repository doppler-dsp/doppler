#!/usr/bin/env python3
"""The lifecycle, object by object: which of the card's phases are present.

``docs/dev/contributing/adding-algorithms.md`` opens with a card: its
principles and a table of phases every object goes through. Each phase has
a gate or a named artifact, and no gate shows, for the objects a branch
touches, which phases are there. Two pull requests were cleared on green CI
with phases 6, 8, 9 and 10 incomplete (doppler#3140). This is that view,
and the gate on a new gap.

How it works
------------
**The page is the declaration.** The phase numbers and names are read from
the table between ``<!-- spine-card:start -->`` and ``<!-- spine-card:end
-->``. ``CHECKERS`` holds one function per phase and ``REVIEW_ONLY`` the
phases no file can answer, each with its reason. A phase on the card that
is in neither, or a name in either that the card does not list, fails the
run: one declaration, checked in both directions.

**The objects are derived.** They are the components of the jm manifest
(``just_makeit.components(load_manifest(root))``). A header directory with
no manifest (``mpsk``, ``conv``, ``rs``, ``util``: function primitives) and
a ``kind = "handle"`` module are not components and have no table here;
that rule is printed with every run. The objects a branch touches are the
paths of ``git diff --name-only <base>...HEAD`` mapped by ``touched()``.

**A row is answered by the gate that owns it.** Each checker calls the
script that already decides the question and adds a check only where
nothing does:

========== ==================== =========================================
phase      item                 answered by
========== ==================== =========================================
Why        design-page          added here: a ``docs/design/`` page names
                                the object (``_mentioned_tokens`` of
                                ``check_user_docs.py`` reads the page)
Declare    manifest, module     the jm manifest itself
Implement  header, source       added here: the two files exist
Pin        c-test               ``owner()`` of ``check_bench_coverage.py``
                                over ``native/tests/**/test_*.c``
Bind       class, fragment      ``public_classes()`` of
                                ``check_user_docs.py``; the fragment file
Instrument state                ``check_serializable.py`` and its two
                                lists
Explore    sweep                added here: ``native/validation/<obj>_*.c``
                                or a characterization subject, attributed
                                by ``owner()``
Certify    report, log-row      the globs and markers of
                                ``gen_validation_log.py``
User story guide, py-example,   ``gaps()`` of ``check_user_docs.py`` and
           gallery              its ratchet, ``docs/.user-docs-exempt``
User story c-example            added here (doppler#3137): a
                                ``native/examples/*.c`` includes the
                                object's header
User story c-bench, py-bench    ``owner()`` over ``native/benchmarks``;
                                ``names_used()`` of ``check_user_docs.py``
                                over the module's ``bench_*.py``
Land       changelog,           ``make changelog-check`` and
           issue-link           ``scripts/issue-link-check.sh``, run and
                                read
========== ==================== =========================================

**Four states.** An item is *present*, *missing*, *skipped* or *not
applicable*, and a row takes the worst of its items. Not applicable is a
rule, stated in the evidence cell: an object with no example has no
gallery decision to make, and Land is a fact about a branch, so an object
the branch does not touch has none.

**A gap is held, declared, or refused.** A missing item passes when one of
these is true, and fails the run otherwise:

- the gate that owns it already lists it (``docs/.user-docs-exempt``,
  ``scripts/.serializable-ignore``);
- ``scripts/.spine-gaps`` lists it, one ``<object> <phase>/<item>:
  <reason>`` a line;
- for Land only, a ``Spine-skip:`` trailer in one of the branch's own
  commits names the phase and the object.

``scripts/.spine-gaps`` only shrinks: a line whose gap is closed fails as
stale, and a line the merge base did not hold fails as added, unless a
``Spine-skip:`` trailer of the branch declares that phase for that object.
The trailer is the declaration and the line is its record. Both are
needed, because a trailer is read from ``git log <base>..HEAD`` and that
range is empty once the branch merges: the line is what keeps the declared
gap passing on ``main`` and on every later branch, and it carries the
issue number that says who owns it.

The trailer's form, one a line::

    Spine-skip: <phase> <object>: <reason> (#<issue>)

``<phase>`` is the card's name or number. The reason and the issue are
mandatory, and a line that starts ``Spine-skip:`` and does not parse is an
error, never ignored. Only the branch's OWN commits are read:
``issue-link-check.sh`` refuses a branch whose commit is a base commit
under a new hash, and when it does, no trailer of that branch counts.

**The principles have one source.** ``CLAUDE.md`` carries a copy of the
card's principles between ``<!-- spine-principles:start -->`` and
``<!-- spine-principles:end -->``. A copy that differs from the page fails,
as does a ``CLAUDE.md`` without the markers; ``--fix`` rewrites the copy
from the page, and ``make format`` runs it (``docs-relink``).

Output
------
The card's principles, then one Markdown table per touched object (every
object with ``--all``), on stdout: valid Markdown, so CI appends it to the
job summary (``--summary FILE``) and an author pastes it at the top of a
pull request. Failures go to stderr. Exit 0 when every gap is held or
declared, 1 otherwise.

Usage
-----
    python scripts/check_spine.py [--root DIR] [--base REF] [--all]
                                  [--summary FILE] GALLERY...
    python scripts/check_spine.py --fix
"""

from __future__ import annotations

import argparse
import os
import re
import subprocess
import sys
from pathlib import Path
from typing import TYPE_CHECKING, NamedTuple

from _gitbase import (
    BaseUnreadableError,
    added_since_base,
    in_git_repo,
    resolve_base,
)
from _layout import C_PREFIX, HEADER_ROOT, PKG
from _pydocs import kinds
from check_bench_coverage import owner
from check_serializable import _is_serializable, _load_list
from check_user_docs import (
    EXEMPT as USER_DOCS_EXEMPT,
)
from check_user_docs import (
    _mentioned_tokens,
    example_names,
    names_used,
    parse_entries,
)
from check_user_docs import (
    gaps as user_docs_gaps,
)
from gen_validation_log import BLOB, END_MARKER, GLOBS, START_MARKER

if TYPE_CHECKING:
    from collections.abc import Callable, Iterable

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
PAGE = "docs/dev/contributing/adding-algorithms.md"
RATCHET = "scripts/.spine-gaps"
SELF = "scripts/check_spine.py"
CARD_START = "<!-- spine-card:start -->"
CARD_END = "<!-- spine-card:end -->"
PRINCIPLES_LEAD = "**The principles.**"
PHASES_LEAD = "**The phases.**"
#: The copy of the principles every session loads, and its markers.
COPY = "CLAUDE.md"
COPY_START = "<!-- spine-principles:start -->"
COPY_END = "<!-- spine-principles:end -->"
STATELESS = "scripts/.serializable-stateless"
STATE_IGNORE = "scripts/.serializable-ignore"
VALIDATION_LOG = "docs/dev/contributing/validation-log.md"
ISSUE_LINK = HERE / "issue-link-check.sh"

#: One card row: ``| 4 | **Pin** | ... | proven by |``.
CARD_ROW_RE = re.compile(r"^\|\s*(\d+)\s*\|\s*\*\*(.+?)\*\*\s*\|(.*)\|\s*$")
#: ``<object> <phase>/<item>: <reason>`` -- the shape of
#: ``docs/.user-docs-exempt``, with the rule split into phase and item.
ENTRY_RE = re.compile(r"^(\S+)\s+([a-z-]+)/([a-z-]+):\s*(\S.*)$")
#: A well-formed trailer. The phase may be two words ("User story"), so the
#: object is the last token before the first colon after the keyword.
SKIP_RE = re.compile(
    r"^Spine-skip:\s*(?P<phase>\S.*?)\s+(?P<object>\S+):\s*"
    r"(?P<reason>\S.*?)\s*\(#(?P<issue>\d+)\)\s*$"
)
#: Anything that set out to be one. Matched without case so ``spine-skip:``
#: is an error to fix rather than a line nothing reads.
SKIP_ATTEMPT_RE = re.compile(r"^\s*spine-skip\b", re.I)
INCLUDE_RE = re.compile(
    rf'^\s*#\s*include\s*[<"]{re.escape(PKG)}/([^/">]+)/', re.M
)

PRESENT, MISSING, SKIPPED, NA = "present", "missing", "skipped", "n/a"

#: What closes each gap, printed with the failure.
FIX = {
    "why/design-page": "write docs/design/<algo>.md and name the object's "
    "class or header in backticks",
    "declare/manifest": "declare it in objects/<obj>.toml",
    "declare/module": "list it in its modules/<mod>.toml `objects`",
    "implement/header": "write the header",
    "implement/source": "write the implementation",
    "pin/c-test": "write native/tests/<obj>/test_<obj>_core.c",
    "bind/class": "export its class from the package's __all__ "
    "(make jm-apply)",
    "bind/fragment": "render its binding (make jm-apply)",
    "instrument/state": 'set serializable = "true" and write the triplet, '
    f"or list it in {STATELESS} with the reason",
    "explore/sweep": "write native/validation/<obj>_*.c, or a "
    "characterization subject named for it",
    "certify/report": "certify it: docs/dev/contributing/validation.md",
    "certify/log-row": "make docs-relink",
    "user-story/guide": "name its class in backticks in a docs/guide/ page",
    "user-story/py-example": "use its class in a src/doppler/examples/ script",
    "user-story/gallery": "add its example to GALLERY_SCRIPTS, or give the "
    "reason in src/doppler/examples/.no-figure",
    "user-story/c-example": "write a native/examples/*.c that includes its "
    "header",
    "user-story/c-bench": "fill in native/benchmarks/<obj>/bench_<obj>_core.c",
    "user-story/py-bench": "use its class in a bench_*.py of its module",
    "land/changelog": "add changelog.d/<section>/<slug>.md",
    "land/issue-link": "add `Closes #N` or `No-issue:` to a commit",
}


class Item(NamedTuple):
    """One artifact a phase owes, and what was found.

    ``held`` names the list of the OWNING gate when that gate already
    carries this gap (``docs/.user-docs-exempt``), so the gap is not
    recorded a second time in ``scripts/.spine-gaps``.
    """

    name: str
    state: str
    evidence: str
    held: str = ""


class Phase(NamedTuple):
    """One row of the card."""

    number: int
    name: str
    proven_by: str

    @property
    def slug(self) -> str:
        """The name as a ratchet key spells it: ``user-story``."""
        return self.name.lower().replace(" ", "-")


class Skip(NamedTuple):
    """One parsed ``Spine-skip:`` trailer."""

    phase: str
    obj: str
    reason: str
    issue: str


class Tree(NamedTuple):
    """Everything the checkers read, gathered once for all objects."""

    root: Path
    manifest: dict[str, object]
    objects: list[str]
    names: set[str]
    module: dict[str, str]
    package: dict[str, str]
    classes: dict[str, dict[str, str]]
    user_gaps: set[tuple[str, str]]
    user_held: set[tuple[str, str]]
    examples: dict[str, set[str]]
    design: dict[str, set[str]]
    c_examples: dict[str, set[str]]
    c_tests: dict[str, list[str]]
    c_benches: dict[str, list[str]]
    py_benches: dict[str, dict[str, set[str]]]
    sweeps: dict[str, list[str]]
    reports: dict[str, str]
    log: str
    stateless: set[str]
    state_ignore: set[str]
    land: dict[str, Item]
    touched: set[str]


def _read(path: Path) -> str:
    return path.read_text(encoding="utf-8") if path.is_file() else ""


def read_card(root: Path) -> tuple[list[str], list[Phase]]:
    """The card's principles, verbatim, and its phases.

    The principles are the lines from the one starting ``**The
    principles.**`` up to, and not including, the one starting ``**The
    phases.**``, so what is printed above a report is the page's text and
    never a second copy of it.

    Raises
    ------
    SystemExit
        The page, either marker or the table is absent: there is no
        declaration to check anything against.
    """
    text = _read(root / PAGE)
    if CARD_START not in text or CARD_END not in text:
        raise SystemExit(f"spine-check: {PAGE} has no spine-card markers")
    card = text.split(CARD_START, 1)[1].split(CARD_END, 1)[0]
    lines = card.strip("\n").splitlines()
    principles: list[str] = []
    inside = False
    for line in lines:
        if line.startswith(PHASES_LEAD):
            break
        inside = inside or line.startswith(PRINCIPLES_LEAD)
        if inside:
            principles.append(line)
    phases = []
    for line in lines:
        m = CARD_ROW_RE.match(line)
        if m:
            proven = m[3].split("|")[-1].strip()
            phases.append(Phase(int(m[1]), m[2].strip(), proven))
    if not phases:
        raise SystemExit(f"spine-check: {PAGE}: the card has no phase rows")
    return "\n".join(principles).strip("\n").splitlines(), phases


def _copy_parts(text: str) -> tuple[str, str, str] | None:
    """``(before, copy, after)`` around the copy's markers, or None."""
    if COPY_START not in text or COPY_END not in text:
        return None
    before, rest = text.split(COPY_START, 1)
    copy, after = rest.split(COPY_END, 1)
    return before, copy, after


def copy_errors(
    root: Path, principles: list[str], *, markers_optional: bool = False
) -> list[str]:
    """Is ``CLAUDE.md``'s copy of the principles the page's text?

    ``CLAUDE.md`` at the root carries the principles between
    ``<!-- spine-principles:start -->`` and ``<!-- spine-principles:end
    -->`` so every session reads them. The lifecycle page is the source.
    The two are compared with leading and trailing blank lines stripped;
    a difference fails, and so does a ``CLAUDE.md`` with no markers,
    unless ``markers_optional``. A tree with no ``CLAUDE.md`` has no copy
    to hold.
    """
    path = root / COPY
    if not path.is_file():
        return []
    parts = _copy_parts(_read(path))
    if parts is None:
        if markers_optional:
            return []
        return [
            f"{COPY}: no {COPY_START} ... {COPY_END} markers; add them and "
            "run `make format`"
        ]
    if parts[1].strip("\n") != "\n".join(principles).strip("\n"):
        return [
            f"{COPY}: its copy of the principles differs from {PAGE}; "
            "run `make format` (the page is the source)"
        ]
    return []


def fix_copy(root: Path, principles: list[str]) -> bool:
    """Rewrite ``CLAUDE.md``'s copy from the page; True if it changed.

    Only the text between the markers is replaced. A file with no markers
    is left alone: where the copy goes is an author's decision, and
    ``copy_errors`` is what reports it.
    """
    path = root / COPY
    parts = _copy_parts(_read(path))
    if parts is None:
        return False
    body = "\n".join(principles).strip("\n")
    text = f"{parts[0]}{COPY_START}\n\n{body}\n\n{COPY_END}{parts[2]}"
    if text == _read(path):
        return False
    path.write_text(text, encoding="utf-8")
    return True


def _norm(name: str) -> str:
    return name.replace("_", "").lower()


def _classes(
    manifest: dict[str, object], obj: str, pkg: str, exported: dict[str, str]
) -> dict[str, str]:
    """``{dotted: bare}`` for the exported classes this object renders as.

    Its ``class_name`` and each view's, as the manifest declares them, or,
    when it declares none, the class of its package whose name is the
    object's with the underscores dropped (``acc_f32`` is ``AccF32``).
    Only classes its own package exports count, so a neighbour's class of
    the same name is not this object's. A class whose name starts with an
    underscore (``_SynthEngine``) is exported and not public: it is bound,
    and owes no user story of its own.
    """
    table = manifest.get(obj)
    declared: set[str] = set()
    if isinstance(table, dict):
        if isinstance(table.get("class_name"), str):
            declared.add(table["class_name"])
        for view in table.get("views") or []:
            if isinstance(view, dict) and view.get("class_name"):
                declared.add(str(view["class_name"]))
    return {
        dotted: bare
        for dotted, bare in exported.items()
        if dotted == f"{PKG}.{pkg}.{bare}"
        and (bare in declared or (not declared and _norm(bare) == _norm(obj)))
    }


def _by_owner(paths: Iterable[tuple[str, str]], names: set[str]) -> dict:
    """``{owner: [rel, ...]}`` for ``(stem, rel)`` pairs, by longest name."""
    out: dict[str, list[str]] = {}
    for stem, rel in paths:
        who = owner(stem, names)
        if who:
            out.setdefault(who, []).append(rel)
    return out


def touched(paths: Iterable[str], objects: list[str], names: set[str]) -> set:
    """The objects a set of changed paths belongs to.

    A path under ``native/`` or ``src/`` belongs to the object one of its
    directories is named for (``native/inc/doppler/<obj>/``,
    ``native/src/<obj>/``, ``native/tests/<obj>/``,
    ``native/benchmarks/<obj>/``, ``src/doppler/<mod>/tests/validation/
    <obj>/``), else to the object its filename is attributed to by
    ``owner()`` once a ``test_``, ``bench_`` or ``<mod>_ext_`` prefix is
    taken off (``native/validation/<obj>_*.c``, ``<mod>_ext_<obj>.c``,
    ``bench_<obj>.py``). ``objects/<obj>.toml`` belongs to its object.
    A module's own files (``modules/<mod>.toml``, ``__init__.py``) name no
    one object and touch none.
    """
    objs = set(objects)
    out: set[str] = set()
    for rel in paths:
        parts = rel.split("/")
        stem = parts[-1].rsplit(".", 1)[0]
        if parts[0] == "objects" and len(parts) == 2:
            out |= {stem} & objs
            continue
        if parts[0] not in ("native", "src"):
            continue
        dirs = [p for p in parts[:-1] if p in objs]
        if dirs:
            out.add(dirs[-1])
            continue
        stem = re.sub(r"^(?:test_|bench_|.*?_ext_)", "", stem)
        who = owner(stem, names)
        if who in objs:
            out.add(who)
    return out


def _run(cmd: list[str], root: Path, **env: str) -> tuple[int, str]:
    """Run a gate at ``root``; its exit code and last line of output.

    The ``make`` variables of an outer run are dropped, so a gate started
    from ``make lint`` reads its own arguments and not the outer command
    line's flags.
    """
    keep = {
        k: v
        for k, v in os.environ.items()
        if k not in ("MAKEFLAGS", "MFLAGS", "MAKELEVEL")
    }
    try:
        done = subprocess.run(
            cmd,
            cwd=root,
            env={**keep, **env},
            capture_output=True,
            text=True,
            check=False,
        )
    except OSError as e:
        return 127, str(e)
    lines = [s for s in (done.stdout + done.stderr).splitlines() if s.strip()]
    return done.returncode, "\n".join(lines)


def land_gates(root: Path, base: str) -> tuple[dict[str, Item], bool]:
    """Ask the two Land gates about this branch; is the branch its own?

    ``scripts/issue-link-check.sh`` and ``make changelog-check`` are run at
    ``root`` against ``base`` and their verdicts read: exit 0 is present,
    and the gate's own closing line is the evidence. The second value is
    False when ``issue-link-check`` reports a base commit under a new
    hash, in which case the branch's declarations are not its own.
    """
    out: dict[str, Item] = {}
    rc, text = _run(["sh", str(ISSUE_LINK)], root, ISSUE_BASE=base)
    own = "is a base commit" not in text
    said = [s for s in text.splitlines() if "issue-link-check:" in s]
    line = said[0] if said else "issue-link-check.sh: no verdict line"
    out["issue-link"] = Item(
        "issue-link", PRESENT if rc == 0 else MISSING, line.strip()
    )
    if (root / "Makefile").is_file():
        asks = ["changelog-check", f"CHANGELOG_BASE={base}"]
        rc, text = _run(
            ["make", "-s", "--no-print-directory", *asks],
            root,
        )
        said = [s for s in text.splitlines() if "changelog-check:" in s]
        line = said[-1] if said else "make changelog-check: no verdict line"
        out["changelog"] = Item(
            "changelog", PRESENT if rc == 0 else MISSING, line.strip()
        )
    else:
        out["changelog"] = Item(
            "changelog", NA, "no Makefile at the root to ask"
        )
    return out, own


def survey(
    root: Path, gallery: set[str], land: dict[str, Item], changed: set[str]
) -> Tree:
    """Read the tree once: the manifest, and every file a checker asks of.

    Raises
    ------
    SystemExit
        The manifest is unreadable or names no component: every table
        would be empty and the run would pass by checking nothing.
    """
    from just_makeit import components, load_manifest

    try:
        manifest = load_manifest(root)
    except (OSError, ValueError) as e:
        raise SystemExit(f"spine-check: the manifest: {e}") from e
    objects = sorted(components(manifest))
    if not objects:
        raise SystemExit("spine-check: the manifest names no components")
    inc = root / HEADER_ROOT
    names = set(objects) | (
        {p.name for p in inc.iterdir() if p.is_dir()}
        if inc.is_dir()
        else set()
    )
    mods = manifest.get("module") or {}
    module = {
        o: m
        for m, v in mods.items()
        if isinstance(v, dict)
        for o in v.get("objects", [])
    }
    # A module renders into the package its `package` key names, else
    # into one of its own name (`wfm_reader` is part of `doppler.wfm`).
    package = {o: str(mods[m].get("package") or m) for o, m in module.items()}
    exported = {
        f"{mod}.{sym}": sym
        for mod, syms in kinds(str(root / "src" / PKG)).items()
        for sym, kind in syms.items()
        if kind == "class"
    }
    classes = {
        o: _classes(manifest, o, package.get(o, ""), exported) for o in objects
    }

    user_gaps, _errors = user_docs_gaps(root, gallery)
    user_held = {
        (k, r)
        for k, r, _ in parse_entries(_read(root / USER_DOCS_EXEMPT))
        if r
    }

    def rel(p: Path) -> str:
        return p.relative_to(root).as_posix()

    design = {
        rel(p): _mentioned_tokens(str(p))
        | {"file:" + p.stem.replace("-", "_")}
        for p in sorted((root / "docs" / "design").glob("*.md"))
    }
    c_examples = {
        rel(p): set(INCLUDE_RE.findall(_read(p)))
        for p in sorted((root / "native" / "examples").glob("*.c"))
    }
    c_tests = _by_owner(
        (
            (p.name[len("test_") : -len(".c")], rel(p))
            for p in sorted((root / "native" / "tests").rglob("test_*.c"))
        ),
        names,
    )
    c_benches = _by_owner(
        (
            (p.name[len("bench_") : -len(".c")], rel(p))
            for p in sorted(
                (root / "native" / "benchmarks").rglob("bench_*.c")
            )
        ),
        names,
    )
    src = root / "src" / PKG
    py_benches: dict[str, dict[str, set[str]]] = {}
    for p in sorted(src.glob("*/benchmarks/bench_*.py")):
        py_benches.setdefault(p.parents[1].name, {})[rel(p)] = names_used(p)
    sweeps = _by_owner(
        [
            (p.stem, rel(p))
            for p in sorted((root / "native" / "validation").glob("*.c"))
        ]
        + [
            (p.name, rel(p) + "/")
            for p in sorted(src.glob("*/tests/characterization/*"))
            if p.is_dir() and not p.name.startswith("_")
        ],
        names,
    )
    reports = {
        p.parent.name: rel(p) for g in GLOBS for p in sorted(src.glob(g))
    }
    log = _read(root / VALIDATION_LOG)
    if START_MARKER in log and END_MARKER in log:
        log = log.split(START_MARKER, 1)[1].split(END_MARKER, 1)[0]
    else:
        log = ""
    return Tree(
        root=root,
        manifest=manifest,
        objects=objects,
        names=names,
        module=module,
        package=package,
        classes=classes,
        user_gaps=user_gaps,
        user_held=user_held,
        examples=example_names(root),
        design=design,
        c_examples=c_examples,
        c_tests=c_tests,
        c_benches=c_benches,
        py_benches=py_benches,
        sweeps=sweeps,
        reports=reports,
        log=log,
        stateless=_load_list(root / STATELESS),
        state_ignore=_load_list(root / STATE_IGNORE),
        land=land,
        touched=touched(changed, objects, names),
    )


def _file(t: Tree, name: str, rel: str) -> Item:
    """Present when ``rel`` is a file under the root."""
    if (t.root / rel).is_file():
        return Item(name, PRESENT, f"`{rel}`")
    return Item(name, MISSING, f"no `{rel}`")


def _first(name: str, found: list[str], what: str) -> Item:
    """Present when anything was found; the first is the evidence."""
    if not found:
        return Item(name, MISSING, f"no {what}")
    more = f" (+{len(found) - 1})" if len(found) > 1 else ""
    return Item(name, PRESENT, f"`{found[0]}`{more}")


def check_why(t: Tree, obj: str) -> list[Item]:
    """A ``docs/design/`` page names the object.

    Named means: the page is called after it (``docs/design/<obj>*.md``,
    by ``owner()``), or a backtick span or link text on it holds one of
    its classes, its header (``<obj>_core``) or one of its C symbols
    (``dp_<obj>_...``). The bare object name is not enough: ``delay``,
    ``frame`` and ``lo`` are words a page uses for other things.
    """
    bare = set(t.classes[obj].values())
    prefix = f"{C_PREFIX}_{obj}_" if C_PREFIX else f"{obj}_"
    pages = []
    for page, tokens in t.design.items():
        stems = {s[5:] for s in tokens if s.startswith("file:")}
        if (
            any(owner(s, t.names) == obj for s in stems)
            or bare & tokens
            or f"{obj}_core" in tokens
            or any(tok.startswith(prefix) for tok in tokens)
        ):
            pages.append(page)
    return [_first("design-page", pages, "docs/design/ page names it")]


def check_declare(t: Tree, obj: str) -> list[Item]:
    """Its manifest fragment exists and a module lists it."""
    mod = t.module.get(obj)
    return [
        _file(t, "manifest", f"objects/{obj}.toml"),
        Item("module", PRESENT, f"`modules/{mod}.toml`")
        if mod
        else Item("module", MISSING, "no module lists it"),
    ]


def check_implement(t: Tree, obj: str) -> list[Item]:
    """The public header and the implementation exist.

    An object the manifest declares ``header_only`` has no ``_core.c`` to
    find: its header is the implementation.
    """
    table = t.manifest.get(obj)
    header_only = (
        isinstance(table, dict) and table.get("header_only") == "true"
    )
    return [
        _file(t, "header", f"{HEADER_ROOT}/{obj}/{obj}_core.h"),
        Item("source", NA, "`header_only` in its manifest: no `_core.c`")
        if header_only
        else _file(t, "source", f"native/src/{obj}/{obj}_core.c"),
    ]


def check_pin(t: Tree, obj: str) -> list[Item]:
    """A C test file is attributed to it."""
    return [_first("c-test", t.c_tests.get(obj, []), "C test file")]


def check_bind(t: Tree, obj: str) -> list[Item]:
    """Its class is exported and its binding fragment is rendered."""
    mod = t.module.get(obj, "")
    return [
        _first(
            "class",
            sorted(t.classes[obj]),
            f"class of it in {PKG}.{t.package.get(obj, mod)}.__all__",
        ),
        _file(t, "fragment", f"native/src/{mod}/{mod}_ext_{obj}.c"),
    ]


def check_instrument(t: Tree, obj: str) -> list[Item]:
    """It has declared its state stance, as ``check_serializable.py`` asks.

    Telemetry probes are not derived: no repository-wide rule says which
    object owes one (doppler#2478), so that half stays with review.
    """
    if _is_serializable(_read(t.root / "objects" / f"{obj}.toml")):
        return [Item("state", PRESENT, f"`objects/{obj}.toml`: serializable")]
    if obj in t.stateless:
        return [Item("state", PRESENT, f"stateless, by `{STATELESS}`")]
    held = STATE_IGNORE if obj in t.state_ignore else ""
    return [Item("state", MISSING, "no state stance declared", held)]


def check_explore(t: Tree, obj: str) -> list[Item]:
    """A C validator or a characterization subject is named for it."""
    return [
        _first(
            "sweep",
            t.sweeps.get(obj, []),
            f"`native/validation/{obj}_*.c` or characterization subject",
        )
    ]


def check_certify(t: Tree, obj: str) -> list[Item]:
    """It has a validation report, and the validation log links it."""
    report = t.reports.get(obj)
    if not report:
        return [
            Item("report", MISSING, f"no `tests/validation/{obj}/results.md`"),
            Item("log-row", NA, "no report to list"),
        ]
    row = f"({BLOB}/{report})" in t.log
    return [
        Item("report", PRESENT, f"`{report}`"),
        Item("log-row", PRESENT, f"`{VALIDATION_LOG}`")
        if row
        else Item("log-row", MISSING, f"no row in `{VALIDATION_LOG}`"),
    ]


def check_user_story(t: Tree, obj: str) -> list[Item]:
    """Guide, examples in both languages, gallery, benchmarks in both.

    The guide mention, the Python example and the gallery decision are
    ``check_user_docs.py``'s verdicts, per class, and a gap it already
    lists is held there. An object with no public class has none of the
    three to owe, and one with no example has no gallery decision.
    """
    classes = {
        k: v for k, v in t.classes[obj].items() if not v.startswith("_")
    }
    bare = set(classes.values())
    mod = t.package.get(obj, "")
    items: list[Item] = []
    for rule, name in (("guide", "guide"), ("example", "py-example")):
        if not classes:
            items.append(Item(name, NA, "no public class to owe one"))
            continue
        lacking = sorted(k for k in classes if (k, rule) in t.user_gaps)
        if not lacking:
            items.append(Item(name, PRESENT, "`make user-docs-check`"))
            continue
        held = all((k, rule) in t.user_held for k in lacking)
        items.append(
            Item(
                name,
                MISSING,
                f"no {rule} for {', '.join(f'`{k}`' for k in lacking)}",
                USER_DOCS_EXEMPT if held else "",
            )
        )
    scripts = sorted(s for s, used in t.examples.items() if used & bare)
    undecided = [s for s in scripts if (s, "gallery") in t.user_gaps]
    if not scripts:
        items.append(Item("gallery", NA, "no example to decide for"))
    elif undecided:
        held = all((s, "gallery") in t.user_held for s in undecided)
        items.append(
            Item(
                "gallery",
                MISSING,
                f"no decision for `{undecided[0]}`",
                USER_DOCS_EXEMPT if held else "",
            )
        )
    else:
        items.append(Item("gallery", PRESENT, f"`{scripts[0]}` is decided"))
    items.append(
        _first(
            "c-example",
            sorted(p for p, inc in t.c_examples.items() if obj in inc),
            f"`native/examples/*.c` includes `{PKG}/{obj}/`",
        )
    )
    items.append(_first("c-bench", t.c_benches.get(obj, []), "C benchmark"))
    if not classes:
        items.append(Item("py-bench", NA, "no public class to measure"))
    else:
        items.append(
            _first(
                "py-bench",
                sorted(
                    p
                    for p, used in t.py_benches.get(mod, {}).items()
                    if used & bare
                ),
                f"`src/{PKG}/{mod}/benchmarks/bench_*.py` uses its class",
            )
        )
    return items


def check_land(t: Tree, obj: str) -> list[Item]:
    """The branch that touches it has a changelog entry and an issue line.

    Land is a fact about a branch, read from the two gates that own it.
    An object the branch does not touch, or a tree with no commits ahead
    of the base, has no Land row to answer.
    """
    if not t.land:
        why = "no commits ahead of the base"
        return [Item("changelog", NA, why), Item("issue-link", NA, why)]
    if obj not in t.touched:
        why = "the branch does not touch it"
        return [Item("changelog", NA, why), Item("issue-link", NA, why)]
    return [t.land["changelog"], t.land["issue-link"]]


#: One checker per phase, keyed by the card's name for it, lower case.
CHECKERS: dict[str, Callable[[Tree, str], list[Item]]] = {
    "why": check_why,
    "declare": check_declare,
    "implement": check_implement,
    "pin": check_pin,
    "bind": check_bind,
    "instrument": check_instrument,
    "explore": check_explore,
    "certify": check_certify,
    "user story": check_user_story,
    "land": check_land,
}

#: Phases of the card that no file can answer, and why. Empty: every phase
#: on the card has an artifact a checker finds.
REVIEW_ONLY: dict[str, str] = {}


def card_errors(phases: list[Phase]) -> list[str]:
    """The card and the checkers, compared in both directions."""
    errors = []
    listed = {p.name.lower() for p in phases}
    for p in phases:
        key = p.name.lower()
        if key not in CHECKERS and key not in REVIEW_ONLY:
            errors.append(
                f"{PAGE}: phase {p.number} '{p.name}' has no checker in "
                f"{SELF} and no reason in REVIEW_ONLY"
            )
        if key in CHECKERS and key in REVIEW_ONLY:
            errors.append(
                f"{SELF}: '{p.name}' is in CHECKERS and in REVIEW_ONLY"
            )
    for key in sorted((set(CHECKERS) | set(REVIEW_ONLY)) - listed):
        errors.append(
            f"{SELF}: a checker for '{key}', which the card in {PAGE} "
            "does not list"
        )
    return errors


def parse_skips(
    messages: str, phases: list[Phase], objects: list[str]
) -> tuple[list[Skip], list[str]]:
    """Every ``Spine-skip:`` trailer in the messages, and every bad one.

    A line that starts ``Spine-skip`` (in any case) and does not match
    the form, or names a phase the card does not have or an object the
    manifest does not, is returned as an error with the line quoted.
    """
    by_name = {p.name.lower(): p for p in phases}
    by_name.update({p.slug: p for p in phases})
    by_name.update({str(p.number): p for p in phases})
    skips: list[Skip] = []
    errors: list[str] = []
    for line in messages.splitlines():
        if not SKIP_ATTEMPT_RE.match(line):
            continue
        m = SKIP_RE.match(line.strip())
        if not m:
            errors.append(
                f"malformed trailer: '{line.strip()}' -- the form is "
                "`Spine-skip: <phase> <object>: <reason> (#<issue>)`"
            )
            continue
        phase = by_name.get(m["phase"].lower())
        if phase is None:
            errors.append(
                f"'{line.strip()}': '{m['phase']}' is no phase of the card"
            )
        elif m["object"] not in objects:
            errors.append(
                f"'{line.strip()}': '{m['object']}' is no object of the "
                "manifest"
            )
        else:
            skips.append(
                Skip(phase.slug, m["object"], m["reason"], m["issue"])
            )
    return skips, errors


def parse_ratchet(text: str) -> tuple[dict[tuple[str, str, str], str], list]:
    """``{(object, phase, item): reason}`` and the lines that do not parse."""
    entries: dict[tuple[str, str, str], str] = {}
    bad: list[str] = []
    for line in text.splitlines():
        s = line.strip()
        if not s or s.startswith("#"):
            continue
        m = ENTRY_RE.match(s)
        if m:
            entries[(m[1], m[2], m[3])] = m[4]
        else:
            bad.append(s)
    return entries, bad


class Verdict(NamedTuple):
    """Every object's rows, and what fails the run."""

    rows: dict[str, list[tuple[Phase, str, str]]]
    errors: list[str]
    held: int


def judge(
    t: Tree,
    phases: list[Phase],
    ratchet: dict[tuple[str, str, str], str],
    grown: set[tuple[str, str, str]],
    skips: list[Skip],
) -> Verdict:
    """Decide every item of every object, and build each object's rows.

    A missing item is, in this order: held by its owning gate's list;
    recorded in the ratchet (and, when its line is new on this branch,
    declared by a trailer, which reads as skipped); declared by a trailer
    with no line yet (Land needs none, any other item is told the line to
    add); or new and undeclared, which fails. Ratchet lines that matched
    no gap are stale and fail.
    """
    skipped = {(s.obj, s.phase): s for s in skips}
    errors: list[str] = []
    rows: dict[str, list[tuple[Phase, str, str]]] = {}
    used: set[tuple[str, str, str]] = set()
    held_count = 0
    for obj in t.objects:
        rows[obj] = []
        for p in phases:
            key = p.name.lower()
            if key in REVIEW_ONLY:
                rows[obj].append((p, NA, f"review only: {REVIEW_ONLY[key]}"))
                continue
            if key not in CHECKERS:
                rows[obj].append((p, MISSING, "no checker"))
                continue
            items = CHECKERS[key](t, obj)
            states: list[str] = []
            cells: list[str] = []
            for it in items:
                k = (obj, p.slug, it.name)
                state, note = it.state, it.evidence
                skip = skipped.get((obj, p.slug))
                line = f"{obj} {p.slug}/{it.name}"
                fix = FIX[f"{p.slug}/{it.name}"]
                declare = (
                    f"declare it in a commit: `Spine-skip: {p.name} {obj}: "
                    "<reason> (#<issue>)`"
                )
                if skip:
                    said = f"skipped: {skip.reason} (#{skip.issue})"
                if it.state != MISSING:
                    pass
                elif it.held:
                    held_count += 1
                    note += f" (held by `{it.held}`)"
                elif k in ratchet and k not in grown:
                    used.add(k)
                    held_count += 1
                    note += f" (held by `{RATCHET}`: {ratchet[k]})"
                elif k in ratchet and skip:
                    used.add(k)
                    held_count += 1
                    state, note = SKIPPED, said
                elif k in ratchet:
                    used.add(k)
                    errors.append(
                        f"{RATCHET}: '{line}' is new; the list only "
                        f"shrinks. Do the work ({fix}), or {declare}"
                    )
                elif skip and p.slug == "land":
                    state, note = SKIPPED, said
                elif skip:
                    state, note = SKIPPED, said
                    errors.append(
                        f"{line}: declared by Spine-skip and not recorded; "
                        f"add to {RATCHET}: `{line}: {skip.reason} "
                        f"(#{skip.issue})`"
                    )
                else:
                    errors.append(
                        f"{line}: {it.evidence} -- {fix}; or {declare}"
                    )
                states.append(state)
                cells.append(note if len(items) == 1 else f"{it.name}: {note}")
            worst = next(
                (s for s in (MISSING, SKIPPED, PRESENT) if s in states), NA
            )
            rows[obj].append((p, worst, "; ".join(cells)))
    for k in sorted(set(ratchet) - used):
        errors.append(
            f"{RATCHET}: '{k[0]} {k[1]}/{k[2]}' is stale (done, held "
            "elsewhere, or names nothing); delete the line"
        )
    return Verdict(rows, errors, held_count)


def render(
    principles: list[str], v: Verdict, show: list[str], t: Tree, note: str
) -> str:
    """The report: the principles, then one table per object in ``show``."""
    out = ["## Lifecycle spine", ""]
    out += [*principles, ""]
    out += [
        f"One table per object: a component of the jm manifest "
        f"({len(t.objects)} today). A header directory with no manifest "
        "and a handle module are not components and have no table. "
        f"{note}",
        "",
    ]
    for obj in show:
        mod = t.module.get(obj, "-")
        cls = ", ".join(f"`{c}`" for c in sorted(t.classes[obj].values()))
        out += [f"### `{obj}` ({PKG}.{mod}{': ' + cls if cls else ''})", ""]
        out += ["| # | phase | state | evidence | proven by |"]
        out += ["| --- | --- | --- | --- | --- |"]
        for p, state, cell in v.rows[obj]:
            mark = f"**{state}**" if state in (MISSING, SKIPPED) else state
            cell = cell.replace("|", "\\|")
            out.append(
                f"| {p.number} | {p.name} | {mark} | {cell} | {p.proven_by} |"
            )
        out.append("")
    return "\n".join(out)


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=(__doc__ or "").split("\n\n")[0])
    ap.add_argument("--root", type=Path, default=ROOT)
    ap.add_argument("--base", default="origin/main")
    ap.add_argument(
        "--all", action="store_true", help="a table for every object"
    )
    ap.add_argument(
        "--summary", type=Path, help="also append the report to this file"
    )
    ap.add_argument(
        "--fix",
        action="store_true",
        help=f"rewrite {COPY}'s copy of the principles from the page, "
        "and do nothing else",
    )
    ap.add_argument(
        "--copy-markers-optional",
        action="store_true",
        help=f"pass a {COPY} that has no spine-principles markers",
    )
    ap.add_argument("gallery", nargs="*", help="the GALLERY_SCRIPTS paths")
    a = ap.parse_args(argv)
    root = a.root.resolve()

    principles, phases = read_card(root)
    if a.fix:
        if fix_copy(root, principles):
            print(f"spine-check: rewrote the principles in {COPY}")
        return 0
    errors = card_errors(phases)
    errors += copy_errors(
        root, principles, markers_optional=a.copy_markers_optional
    )

    # The branch: what it changed, what its own commits declare, and what
    # the two Land gates say of it. None of it applies outside a
    # repository, which is a gate's test running on a bare tree.
    changed: set[str] = set()
    messages = ""
    land: dict[str, Item] = {}
    note = "No repository: the branch half is inert."
    git = in_git_repo(root)
    if git:
        try:
            rev = resolve_base(root, a.base)
        except BaseUnreadableError:
            errors.append(
                f"cannot resolve {a.base}; the branch and the ratchet are "
                "unchecked (CI needs fetch-depth: 0)"
            )
            git = False
    if git:
        diff = subprocess.run(
            ["git", "diff", "--name-only", f"{rev}..HEAD"],
            cwd=root,
            capture_output=True,
            text=True,
            check=False,
        )
        changed = set(diff.stdout.splitlines())
        if changed:
            land, own = land_gates(root, a.base)
            if own:
                messages = subprocess.run(
                    ["git", "log", "--format=%B", f"{rev}..HEAD"],
                    cwd=root,
                    capture_output=True,
                    text=True,
                    check=False,
                ).stdout
            else:
                errors.append(
                    "a commit of this branch is a base commit under a new "
                    "hash (make issue-link-check says which); no Spine-skip "
                    "of the branch is read until its commits are its own"
                )
            note = f"The branch is measured against `{a.base}`."
        else:
            note = f"No commits ahead of `{a.base}`: the branch half is inert."

    t = survey(root, {Path(g).name for g in a.gallery}, land, changed)
    skips, skip_errors = parse_skips(messages, phases, t.objects)
    errors += skip_errors

    ratchet, bad = parse_ratchet(_read(root / RATCHET))
    errors += [f"{RATCHET}: malformed line: {s}" for s in bad]
    grown: set[tuple[str, str, str]] = set()
    if git:
        try:
            grown = set(
                added_since_base(
                    root,
                    a.base,
                    RATCHET,
                    sorted(ratchet),
                    lambda text: parse_ratchet(text)[0],
                    since=SELF,
                )
            )
        except BaseUnreadableError:
            errors.append(
                f"cannot read {RATCHET} at {a.base}; the ratchet is unchecked"
            )

    v = judge(t, phases, ratchet, grown, skips)
    errors += v.errors

    show = t.objects if a.all else sorted(t.touched)
    report = render(principles, v, show, t, note)
    if not show:
        report += "\nThe branch touches no object.\n"
    print(report)
    if a.summary:
        with open(a.summary, "a", encoding="utf-8") as fh:
            fh.write(report + "\n")

    if errors:
        print("spine-check: FAILED", file=sys.stderr)
        for e in errors:
            print(f"  {e}", file=sys.stderr)
        return 1
    print(
        f"spine-check: OK -- {len(show)} object(s) shown of "
        f"{len(t.objects)}; {v.held} gap(s) held, {len(ratchet)} of them "
        f"in {RATCHET}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
