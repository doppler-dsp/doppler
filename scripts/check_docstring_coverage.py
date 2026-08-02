#!/usr/bin/env python3
"""Docstring-coverage meter — the burn-down gate for doppler's public API docs.

Every public Python surface should be fully documented, written once as
Doxygen in the C ``_core.h`` headers and flowed to Python by ``jm`` (see
just-makeit's ``docstring-derivation.md``). This script measures how far we
are from 100% and, in ``--check`` mode, ratchets: a module's incomplete count
may go **down** on a PR but never **up**.

Two faces are scored independently, because ``jm`` currently derives them at
different richness:

* **stub** — the numpy docstrings rendered into ``src/doppler/**/*.pyi``. This
  is the face IDEs, type-checkers, and the mkdocstrings site read (statically,
  via griffe). Scored by parsing the committed stubs — no build required, so
  this face runs anywhere (e.g. the docs CI job).
* **runtime** — the ``__doc__`` visible from ``help()`` on the built
  extension. Scored by importing ``doppler`` and introspecting; skipped with a
  note if the package is not importable (not built).

A callable's signature *expectation* (which params must be documented, whether
a Returns section is required) is derived once from the authoritative ``.pyi``
signature and applied to **both** faces, so the two scores are directly
comparable.

Coverage — one bar
------------------
A surface is *covered* (:func:`covered`, the gate ``--check`` ratchets) when it
carries the best docstring achievable for its kind:

* anything a human authors — real/built-in methods, classes, module functions
  and docstrings — must be strict FULL (below);
* jm's generated glue (the ``state_bytes``/``get_state``/``set_state`` triplet,
  ``destroy``/``close``, the ``__enter__``/``__exit__`` pair, and the
  ``*_max_out`` capacity accessors) is scored on jm's prose **without** the
  example a real method needs — you cannot ``>>>`` a generic object's
  ``state_bytes`` or capacity. jm owns those docstrings (gh-647), so they are
  documented-by-jm, not a human authoring target.

The single number reaches 100% once the authoring is done and jm documents the
remaining glue — no separate aspirational meter, because covered *is* the goal.

Classification (the strict per-surface verdict for authored surfaces)
---------------------------------------------------------------------
FULL
    Summary line, a ``Parameters`` entry for every signature parameter, a
    ``Returns`` section when the callable returns a value, and at least one
    ``>>>`` doctest. Properties need only a non-empty docstring (they have no
    params/return to document).
PARTIAL
    A summary plus at least one section, but missing a required one.
STUB
    No docstring, or a bare summary with no sections at all.

Usage
-----
    python scripts/check_docstring_coverage.py            # report the table
    python scripts/check_docstring_coverage.py --check    # ratchet vs baseline
    python scripts/check_docstring_coverage.py --update-baseline
    python scripts/check_docstring_coverage.py --json     # per-callable dump

Intentional exclusions go in ``docs/.docstring-coverage-ignore`` (one
fully-qualified name per line, ``#`` comments allowed).
"""

from __future__ import annotations

import argparse
import ast
import glob
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PKG_DIR = os.path.join(ROOT, "src", "doppler")
BASELINE_FILE = os.path.join(ROOT, "docs", ".docstring-coverage-baseline")
IGNORE_FILE = os.path.join(ROOT, "docs", ".docstring-coverage-ignore")

# Application / entry-point packages with no importable object API. Mirrors
# _pydocs.SKIP_TOP so this meter and check_api_docs agree on "what is public".
SKIP_TOP = {"cli", "specan", "tests", "benchmarks"}

# Generated glue jm emits for its own machinery — the lifecycle/serialization
# triplet, the context-manager pair, and the `*_max_out` capacity accessors.
# jm owns their docstrings end to end (gh-647), so they are not a human
# authoring target and are scored WITHOUT the example a real DSP method needs
# (you cannot `>>>` a generic object's state_bytes / capacity). They still
# count toward 100% — just as "documented by jm", not "authored by us".
BOILERPLATE = {
    "state_bytes",
    "get_state",
    "set_state",
    "destroy",
    "close",
    "__enter__",
    "__exit__",
}


def _is_boilerplate(name: str) -> bool:
    """A generated-glue method: the lifecycle set or a capacity accessor."""
    return name in BOILERPLATE or name.endswith("_max_out")


def _is_record_class(cls: ast.ClassDef) -> bool:
    """A ``single = true`` result record — ``class X(tuple[...])``.

    jm emits one of these for a by-value metric record (gh-646:
    ``ToneMetrics``/``NPRMetrics``/…). It is not user-constructable — a method
    returns it — so it can carry no ``>>>`` example; it is documented by its
    ``record_doc`` (the class summary) plus an ``Attributes`` table whose
    entries are the field properties, and those fields are counted separately
    as properties. So the class itself is scored WITHOUT the example, exactly
    like generated glue: fully documented *for what it is*. Detected by a
    ``tuple[...]`` base, which is the fixed-length-tuple subclass jm generates.
    """
    for base in cls.bases:
        node = base.value if isinstance(base, ast.Subscript) else base
        name = (
            node.id
            if isinstance(node, ast.Name)
            else getattr(node, "attr", None)
        )
        if name == "tuple":
            return True
    return False


FULL, PARTIAL, STUB = "FULL", "PARTIAL", "STUB"

# Raw Doxygen tags that must never survive into a rendered docstring. jm is
# meant to strip inline tags and drop block tags; when it fails (plan item F1)
# the literal tag leaks. Zero tolerance — any hit fails the gate.
_TAG_LEAK_RE = re.compile(
    r"@(?:c|p|a|e|b|ref|note|see|warning|retval|sa|li)\b"
)


# --------------------------------------------------------------------------- #
# Classification
# --------------------------------------------------------------------------- #
def _param_documented(text: str, name: str) -> bool:
    """True if ``name`` is a numpy ``Parameters`` entry (``name :``)."""
    return re.search(rf"(?m)^\s*{re.escape(name)}\s*:", text) is not None


def classify(
    text: str | None, params: list[str], has_return: bool, is_property: bool
) -> str:
    """Score one docstring against a signature expectation."""
    if not text or not text.strip():
        return STUB
    if is_property:
        # Properties have no params/return; any real docstring is enough.
        return FULL
    has_params = "Parameters" in text
    has_returns = "Returns" in text
    has_example = ">>>" in text
    all_params = all(_param_documented(text, p) for p in params)

    complete = has_example
    if params:
        complete = complete and has_params and all_params
    if has_return:
        complete = complete and has_returns
    if complete:
        return FULL
    if not (has_params or has_returns or has_example):
        return STUB
    return PARTIAL


def covered(
    text: str | None,
    params: list[str],
    has_return: bool,
    is_property: bool,
    boilerplate: bool,
) -> bool:
    """Does ``text`` meet the CURRENT (achievable-today) bar — the gate?

    The strict FULL bar (see :func:`classify`) requires a ``>>>`` example. That
    is right for a real DSP method, but jm's generated glue
    (``state_bytes``/``get_state``/``set_state``/``destroy``/``__enter__``/
    ``__exit__``) carries a complete numpy docstring with **no** example by
    design (gh-647: you cannot ``>>>`` a generic object's ``state_bytes``
    without constructing it). Such a method is fully documented *for what it
    is*, yet can never reach strict FULL. So glue is scored on the FULL shape
    **minus** the example, and everything else on strict FULL — a single gate
    that reaches 100% once authoring is done and jm documents the glue.
    """
    if is_property:
        return bool(text and text.strip())
    if not boilerplate:
        return classify(text, params, has_return, is_property) == FULL
    if not text or not text.strip():
        return False
    ok = True
    if params:
        ok = (
            ok
            and "Parameters" in text
            and all(_param_documented(text, p) for p in params)
        )
    if has_return:
        ok = ok and "Returns" in text
    return ok


def tag_leaks(text: str | None) -> list[str]:
    """Raw Doxygen tags surviving in a rendered docstring (should be none)."""
    if not text:
        return []
    return _TAG_LEAK_RE.findall(text)


# --------------------------------------------------------------------------- #
# Stub-face parsing (static, no import)
# --------------------------------------------------------------------------- #
# jm injects an optional ``out=`` output buffer on block/variable_output
# methods. Its contract is uniform library-wide (a pre-allocated buffer of the
# output dtype, or None to allocate — gated by ``test_out_param_dtype.py``), jm
# does not render it into the ``.pyi``, and the derivation convention
# (just-makeit#666) treats jm-injected plumbing as optional to document. So it
# is not a *documentable* parameter for coverage — requiring it would make FULL
# unreachable for every block method through the header. Excluded only when it
# is optional (has a default); a hypothetical required ``out`` still counts.
_OPTIONAL_PLUMBING = frozenset({"out"})


def _params_of(fn: ast.FunctionDef) -> list[str]:
    """Documentable parameter names of a def (drops self/cls, *args, **kw,
    and the jm-injected optional ``out=`` plumbing buffer)."""
    a = fn.args
    pos = [*a.posonlyargs, *a.args]
    optional = {p.arg for p in pos[len(pos) - len(a.defaults) :]}
    optional |= {
        p.arg for p, d in zip(a.kwonlyargs, a.kw_defaults) if d is not None
    }
    names = [p.arg for p in (*a.posonlyargs, *a.args, *a.kwonlyargs)]
    return [
        n
        for n in names
        if n not in ("self", "cls")
        and not (n in _OPTIONAL_PLUMBING and n in optional)
    ]


def _has_return(fn: ast.FunctionDef) -> bool:
    r = fn.returns
    if r is None:
        return False
    # ``-> None`` annotates a no-return callable.
    return not (isinstance(r, ast.Constant) and r.value is None)


def _is_property(fn: ast.FunctionDef) -> bool:
    for dec in fn.decorator_list:
        name = (
            dec.id
            if isinstance(dec, ast.Name)
            else (dec.attr if isinstance(dec, ast.Attribute) else None)
        )
        if name in ("property", "cached_property"):
            return True
        # A setter/getter overload: fold into the property, don't double-count.
        if isinstance(dec, ast.Attribute) and dec.attr in ("setter", "getter"):
            return True
    return False


class Sym:
    """One measured callable, its expectation, and both face-texts."""

    def __init__(
        self,
        qual: str,
        kind: str,
        params: list[str],
        has_return: bool,
        is_property: bool,
        stub_doc: str | None,
        record: bool = False,
    ):
        self.qual = qual
        self.kind = kind  # class | method | property | function | module
        self.params = params
        self.has_return = has_return
        self.is_property = is_property
        self.stub_doc = stub_doc
        self.runtime_doc: str | None = None
        self.runtime_seen = False  # was the runtime object found?
        self.boilerplate = _is_boilerplate(qual.rsplit(".", 1)[-1])
        # A single=true structseq record class: documented like glue (no
        # example possible), scored on the same no-example bar.
        self.record = record

    def status(self, face: str) -> str:
        """Strict FULL/PARTIAL/STUB — the END-STATE (ideal) bar."""
        text = self.stub_doc if face == "stub" else self.runtime_doc
        return classify(text, self.params, self.has_return, self.is_property)

    def covered(self, face: str) -> bool:
        """Meets the CURRENT (achievable-today) bar — the gate."""
        text = self.stub_doc if face == "stub" else self.runtime_doc
        return covered(
            text,
            self.params,
            self.has_return,
            self.is_property,
            self.boilerplate or self.record,
        )

    def leaks(self, face: str) -> list[str]:
        return tag_leaks(self.stub_doc if face == "stub" else self.runtime_doc)


def parse_stub_face() -> dict[str, list[Sym]]:
    """``{module_dotted: [Sym, ...]}`` from every committed .pyi + __init__."""
    out: dict[str, list[Sym]] = {}
    for entry in sorted(os.listdir(PKG_DIR)):
        d = os.path.join(PKG_DIR, entry)
        if entry in SKIP_TOP or entry.startswith("_") or not os.path.isdir(d):
            continue
        init = os.path.join(d, "__init__.py")
        if not os.path.isfile(init):
            continue
        mod = f"doppler.{entry}"
        syms: list[Sym] = []

        # Module docstring lives on __init__.py (a re-export shim today).
        with open(init, encoding="utf-8") as fh:
            init_tree = ast.parse(fh.read())
        syms.append(
            Sym(mod, "module", [], False, False, ast.get_docstring(init_tree))
        )

        for pyi in sorted(glob.glob(os.path.join(d, "*.pyi"))):
            with open(pyi, encoding="utf-8") as fh:
                tree = ast.parse(fh.read())
            for node in tree.body:
                if isinstance(node, ast.ClassDef) and not node.name.startswith(
                    "_"
                ):
                    syms.extend(_class_syms(mod, node))
                elif isinstance(
                    node, ast.FunctionDef
                ) and not node.name.startswith("_"):
                    syms.append(
                        Sym(
                            f"{mod}.{node.name}",
                            "function",
                            _params_of(node),
                            _has_return(node),
                            False,
                            ast.get_docstring(node),
                        )
                    )
        out[mod] = syms
    return out


def _class_syms(mod: str, cls: ast.ClassDef) -> list[Sym]:
    """The class itself + its methods/properties (init folds in)."""
    init = next(
        (
            n
            for n in cls.body
            if isinstance(n, ast.FunctionDef) and n.name == "__init__"
        ),
        None,
    )
    init_params = _params_of(init) if init else []
    out = [
        Sym(
            f"{mod}.{cls.name}",
            "class",
            init_params,
            False,
            False,
            ast.get_docstring(cls),
            record=_is_record_class(cls),
        )
    ]
    for node in cls.body:
        if not isinstance(node, ast.FunctionDef):
            continue
        if node.name == "__init__" or (
            node.name.startswith("_")
            and node.name not in ("__enter__", "__exit__")
        ):
            continue
        is_prop = _is_property(node)
        out.append(
            Sym(
                f"{mod}.{cls.name}.{node.name}",
                "property" if is_prop else "method",
                _params_of(node),
                _has_return(node),
                is_prop,
                ast.get_docstring(node),
            )
        )
    # De-dup property getter/setter overloads by qualname (keep first w/ doc).
    seen: dict[str, Sym] = {}
    for s in out:
        if s.qual not in seen or (not seen[s.qual].stub_doc and s.stub_doc):
            seen[s.qual] = s
    return list(seen.values())


# --------------------------------------------------------------------------- #
# Runtime-face introspection (needs a build)
# --------------------------------------------------------------------------- #
def attach_runtime_face(mods: dict[str, list[Sym]]) -> bool:
    """Fill ``runtime_doc`` by importing doppler; False if not built."""
    try:
        import importlib

        importlib.import_module("doppler")
    except Exception:
        return False
    for mod, syms in mods.items():
        try:
            m = importlib.import_module(mod)
        except Exception:
            continue
        for s in syms:
            parts = s.qual.split(".")[2:]  # strip "doppler.<mod>."
            try:
                if s.kind == "module":
                    s.runtime_doc, s.runtime_seen = m.__doc__, True
                elif len(parts) == 1:
                    obj = getattr(m, parts[0])
                    s.runtime_doc, s.runtime_seen = obj.__doc__, True
                else:  # class member
                    cls = getattr(m, parts[0])
                    member = getattr(cls, parts[1])
                    s.runtime_doc, s.runtime_seen = member.__doc__, True
            except AttributeError:
                s.runtime_seen = False
    return True


# --------------------------------------------------------------------------- #
# Reporting / ratchet
# --------------------------------------------------------------------------- #
def load_ignore() -> set[str]:
    if not os.path.exists(IGNORE_FILE):
        return set()
    out = set()
    with open(IGNORE_FILE, encoding="utf-8") as fh:
        for line in fh:
            line = line.split("#", 1)[0].strip()
            if line:
                out.add(line)
    return out


def incomplete_counts(syms: list[Sym], face: str, ignore: set[str]) -> int:
    """Callables not yet COVERED on ``face`` — the CURRENT-bar gate count.

    "Covered" is the achievable-today bar (:func:`covered`): strict FULL for
    real surfaces, FULL-minus-example for generated glue. This is what the
    ratchet enforces (may only DROP), and it can reach 0.
    """
    n = 0
    for s in syms:
        if s.qual in ignore:
            continue
        if face == "runtime" and not s.runtime_seen:
            continue  # can't score what the build didn't expose
        if not s.covered(face):
            n += 1
    return n


def all_leaks(mods: dict[str, list[Sym]], have_runtime: bool) -> list[str]:
    hits = []
    for syms in mods.values():
        for s in syms:
            for tag in s.leaks("stub"):
                hits.append(f"{s.qual} [stub] {tag}")
            if have_runtime:
                for tag in s.leaks("runtime"):
                    hits.append(f"{s.qual} [runtime] {tag}")
    return hits


def build_report(
    mods: dict[str, list[Sym]], have_runtime: bool, ignore: set[str]
) -> dict:
    rows = {}
    for mod, syms in sorted(mods.items()):
        active = [s for s in syms if s.qual not in ignore]
        row = {
            "stub": {FULL: 0, PARTIAL: 0, STUB: 0},
            "runtime": {FULL: 0, PARTIAL: 0, STUB: 0, "unseen": 0},
            "stub_incomplete": incomplete_counts(syms, "stub", ignore),
            "runtime_incomplete": incomplete_counts(syms, "runtime", ignore)
            if have_runtime
            else None,
            "total": len(active),
        }
        for s in active:
            row["stub"][s.status("stub")] += 1
            if have_runtime:
                if not s.runtime_seen:
                    row["runtime"]["unseen"] += 1
                else:
                    row["runtime"][s.status("runtime")] += 1
        rows[mod] = row
    return rows


def print_table(rows: dict, have_runtime: bool) -> None:
    print(
        f"{'module':<14} {'total':>6} "
        f"{'stub F/P/S':>16} {'stub✗':>6}  "
        f"{'runtime F/P/S':>16} {'rt✗':>6}"
    )
    print("-" * 74)
    tot = {"total": 0, "s_inc": 0, "r_inc": 0}
    for mod, r in rows.items():
        s = r["stub"]
        stubcell = f"{s[FULL]}/{s[PARTIAL]}/{s[STUB]}"
        if have_runtime:
            rt = r["runtime"]
            rtcell = f"{rt[FULL]}/{rt[PARTIAL]}/{rt[STUB]}"
            rt_inc = f"{r['runtime_incomplete']}"
            tot["r_inc"] += r["runtime_incomplete"]
        else:
            rtcell, rt_inc = "(not built)", "-"
        print(
            f"{mod:<14} {r['total']:>6} {stubcell:>16} "
            f"{r['stub_incomplete']:>6}  {rtcell:>16} {rt_inc:>6}"
        )
        tot["total"] += r["total"]
        tot["s_inc"] += r["stub_incomplete"]
    print("-" * 74)
    r_inc = tot["r_inc"] if have_runtime else "-"
    print(
        f"{'TOTAL':<14} {tot['total']:>6} "
        f"{'stub✗=' + str(tot['s_inc']):>23}  "
        f"{'rt✗=' + str(r_inc):>23}"
    )
    if not have_runtime:
        print(
            "\nNote: doppler not importable — runtime (help()) face "
            "skipped. Build + re-run to score it."
        )


def print_coverage(mods: dict, ignore: set[str]) -> None:
    """The single coverage line — the gate, which can reach 100%.

    A surface is covered (:func:`covered`) when it carries the best docstring
    achievable for its kind: strict FULL for anything a human authors, and
    jm's generated prose (no example) for boilerplate glue. It reaches 100%
    once the authoring is done and jm documents the remaining glue.
    """
    total = cov = 0
    for syms in mods.values():
        for s in syms:
            if s.qual in ignore:
                continue
            total += 1
            cov += s.covered("stub")
    if not total:
        return
    print()
    print(
        f"Coverage: {cov}/{total} = {100 * cov / total:.1f}%  "
        f"(incomplete {total - cov}; ratchets to 0). "
        f"Boilerplate glue is scored on jm's prose (no example)."
    )


def write_baseline(rows: dict, have_runtime: bool, n_leaks: int) -> None:
    lines = [
        "# docstring-coverage ratchet baseline — regenerate with",
        "#   python scripts/check_docstring_coverage.py --update-baseline",
        "# columns: <module> stub=<incomplete> runtime=<incomplete|->",
        "# 'incomplete' = PARTIAL + STUB (not yet FULL). May only DROP.",
        "# __leaks__ = raw Doxygen tags surviving in rendered docs; "
        "target 0, may only DROP.",
        "",
        f"__leaks__ count={n_leaks}",
        "",
    ]
    for mod, r in sorted(rows.items()):
        rt = r["runtime_incomplete"] if have_runtime else "-"
        lines.append(f"{mod} stub={r['stub_incomplete']} runtime={rt}")
    with open(BASELINE_FILE, "w", encoding="utf-8") as fh:
        fh.write("\n".join(lines) + "\n")


def read_baseline() -> dict[str, dict[str, int | None]]:
    out: dict[str, dict[str, int | None]] = {}
    if not os.path.exists(BASELINE_FILE):
        return out
    with open(BASELINE_FILE, encoding="utf-8") as fh:
        for line in fh:
            line = line.split("#", 1)[0].strip()
            if not line:
                continue
            mod, *kvs = line.split()
            rec: dict[str, int | None] = {}
            for kv in kvs:
                k, v = kv.split("=")
                rec[k] = None if v == "-" else int(v)
            out[mod] = rec
    return out


# --------------------------------------------------------------------------- #
# Entry point
# --------------------------------------------------------------------------- #
def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "--check",
        action="store_true",
        help="ratchet against the committed baseline; exit 1 on "
        "any regression or tag leak",
    )
    ap.add_argument(
        "--update-baseline",
        action="store_true",
        help="rewrite the baseline from the current tree",
    )
    ap.add_argument(
        "--json", action="store_true", help="dump per-callable status as JSON"
    )
    args = ap.parse_args()

    ignore = load_ignore()
    mods = parse_stub_face()
    have_runtime = attach_runtime_face(mods)

    if args.json:
        dump = []
        for syms in mods.values():
            for s in syms:
                if s.qual in ignore:
                    continue
                dump.append(
                    {
                        "qual": s.qual,
                        "kind": s.kind,
                        "boilerplate": s.boilerplate,
                        "stub": s.status("stub"),
                        "runtime": (
                            s.status("runtime") if s.runtime_seen else "UNSEEN"
                        )
                        if have_runtime
                        else None,
                    }
                )
        print(json.dumps(dump, indent=2))
        return 0

    rows = build_report(mods, have_runtime, ignore)
    leaks = all_leaks(mods, have_runtime)

    if args.update_baseline:
        write_baseline(rows, have_runtime, len(leaks))
        print(
            f"Wrote baseline: {os.path.relpath(BASELINE_FILE, ROOT)} "
            f"({len(leaks)} known tag leak(s))"
        )
        return 0

    print_table(rows, have_runtime)
    print_coverage(mods, ignore)

    if not args.check:
        if leaks:
            print(f"\n{len(leaks)} raw Doxygen tag leak(s):", file=sys.stderr)
            for h in leaks:
                print(f"  {h}", file=sys.stderr)
        return 0

    # Ratchet: incomplete counts and leak count may only DROP, never rise.
    baseline = read_baseline()
    if not baseline:
        print(
            "\nNo baseline committed. Run --update-baseline first.",
            file=sys.stderr,
        )
        return 1
    regressions = []
    for mod, r in rows.items():
        base = baseline.get(mod)
        if base is None:
            regressions.append(f"{mod}: new module not in baseline")
            continue
        if r["stub_incomplete"] > (base.get("stub") or 0):
            regressions.append(
                f"{mod}: stub incomplete {base.get('stub')} -> "
                f"{r['stub_incomplete']} (must not increase)"
            )
        if (
            have_runtime
            and base.get("runtime") is not None
            and r["runtime_incomplete"] > base["runtime"]
        ):
            regressions.append(
                f"{mod}: runtime incomplete {base['runtime']} -> "
                f"{r['runtime_incomplete']} (must not increase)"
            )

    leak_base = (baseline.get("__leaks__") or {}).get("count")
    ok = True
    if regressions:
        ok = False
        print("\nDocstring coverage regressed:", file=sys.stderr)
        for r in regressions:
            print(f"  {r}", file=sys.stderr)
    if leak_base is not None and len(leaks) > leak_base:
        ok = False
        print(
            f"\nTag leaks rose {leak_base} -> {len(leaks)} "
            "(must not increase):",
            file=sys.stderr,
        )
        for h in leaks:
            print(f"  {h}", file=sys.stderr)
    elif leaks:
        # Pre-existing, baselined leaks — surfaced but not a regression.
        print(f"\n{len(leaks)} known tag leak(s) still present (target 0):")
        for h in leaks:
            print(f"  {h}")
    if ok:
        print("\nDocstring coverage: OK — no regression.")
        return 0
    print(
        "\nLower the counts (document the surface) or, if a symbol is "
        "intentionally exempt, add it to "
        f"{os.path.relpath(IGNORE_FILE, ROOT)}. After improving, refresh "
        "with --update-baseline.",
        file=sys.stderr,
    )
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
