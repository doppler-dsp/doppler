#!/usr/bin/env python3
"""Constructor arguments must be optional on both faces, or neither.

A jm object declaration has three faces: the manifest that declares it, the
``.pyi`` stub that type checkers and the docs site read, and the C binding that
actually enforces the call. This gate checks the last two agree about which
constructor arguments may be **omitted**.

They can disagree. jm renders a stub parameter as ``x: T = ...`` — omittable —
while the generated ``PyArg_ParseTupleAndKeywords`` format string places it
before the ``|``, making it mandatory. Nothing else catches that: the stub
parses, the extension compiles, the tests pass because they pass every
argument, and the only symptom is that following the published signature raises
``TypeError``. A type checker will bless the failing call, because the stub is
the only thing it can see.

That is the argument-shape twin of just-makeit's gh-815, which added a checker
for the same divergence on the *return* shape. Filed upstream as
`just-makeit#823 <https://github.com/just-buildit/just-makeit/issues/823>`_;
until it lands, this gate is doppler's own detector, in the same spirit as
``check_doc_face_parity.py``.

Compare by NAME, never by position
----------------------------------
The stub's parameter order and the binding's ``kwlist`` order can differ — jm
reports that separately as kwargs-drift, and ``carrier_acq`` currently has it.
A positional comparison therefore names the *wrong* parameter: the first
version of this check reported ``sample_rate_hz``, which is accepted at
runtime, while the genuinely broken ``psd_template`` went unmentioned. The two
are different bugs with different fixes, so this reads the ``kwlist`` and
matches on names.

Static by construction
----------------------
Nothing is imported and no object is constructed, so classes whose constructors
need live resources (a file, a socket, a clock) are covered exactly like the
rest. The inputs are the committed ``.pyi`` and the committed ``_ext_*.c``,
which is also what CI ships.

Usage
-----
.. code-block:: sh

    python scripts/check_init_param_optionality.py     # report, exit 1 on any
    python scripts/check_init_param_optionality.py -v  # also list what passed
"""

from __future__ import annotations

import argparse
import ast
import pathlib
import re
import sys

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parent
SRC = ROOT / "src" / "doppler"
NATIVE = ROOT / "native" / "src"
IGNORE = HERE / ".init-param-optionality-ignore"


def _load_ignore() -> set[str]:
    """Read the burn-down list of accepted divergences.

    Returns
    -------
    set of str
        ``Class.param`` entries. Blank lines and ``#`` comments are skipped.
    """
    if not IGNORE.exists():
        return set()
    out = set()
    for line in IGNORE.read_text(encoding="utf-8").splitlines():
        line = line.split("#", 1)[0].strip()
        if line:
            out.add(line)
    return out


def _fmt_counts(fmt: str) -> tuple[int, int]:
    """Split a PyArg format string into (total, required) argument counts.

    Parameters
    ----------
    fmt : str
        The format string, e.g. ``"O|dddKs"``.

    Returns
    -------
    tuple of (int, int)
        How many Python arguments the call accepts in total, and how many of
        those precede the ``|`` and are therefore mandatory.

    Notes
    -----
    ``O&`` consumes one Python argument but two C varargs, so it collapses to a
    single ``O`` first; ``$`` (keyword-only marker) and a stray ``&`` carry no
    argument of their own.
    """
    f = re.sub(r"[$&]", "", fmt.replace("O&", "O"))
    required, sep, optional = f.partition("|")
    return len(required) + len(optional), len(required) if sep else len(f)


def _bindings() -> dict[str, tuple[int, int, str, list[str], str]]:
    """Collect every generated constructor's enforced call signature.

    Returns
    -------
    dict
        Class name -> ``(n_total, n_required, fmt, kwlist_names, c_file)``.
        The class name is taken from jm's ``<Class>Obj_init`` symbol, which is
        the Python-visible name with an ``Obj`` suffix.
    """
    out: dict[str, tuple[int, int, str, list[str], str]] = {}
    for c in sorted(NATIVE.rglob("*_ext_*.c")):
        text = c.read_text(encoding="utf-8")
        for m in re.finditer(r"^(\w+)Obj_init\s*\(", text, re.M):
            tail = text[m.end() : m.end() + 4000]
            fm = re.search(
                r"PyArg_ParseTupleAndKeywords\s*\(\s*args\s*,\s*kwds?\s*,"
                r'\s*"([^"]*)"',
                tail,
            )
            km = re.search(r"kwlist\[\]\s*=\s*\{(.*?)\}", tail, re.S)
            if not (fm and km):
                continue
            names = re.findall(r'"([^"]+)"', km.group(1))
            total, req = _fmt_counts(fm.group(1))
            out[m.group(1)] = (total, req, fm.group(1), names, str(c))
    return out


def main() -> int:
    """Run the gate.

    Returns
    -------
    int
        0 when every constructor agrees across both faces and the burn-down
        list is free of stale entries; 1 otherwise.
    """
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "-v",
        "--verbose",
        action="store_true",
        help="also print the constructors that were compared and passed",
    )
    args = ap.parse_args()

    binding = _bindings()
    if not binding:
        print(
            "check_init_param_optionality: found no generated constructors "
            "at all — the extraction is broken, not the tree",
            file=sys.stderr,
        )
        return 1

    ignore = _load_ignore()
    findings: list[tuple[str, str, list[str], str, str]] = []
    seen: set[str] = set()
    checked = 0

    for pyi in sorted(SRC.rglob("*.pyi")):
        tree = ast.parse(pyi.read_text(encoding="utf-8"), str(pyi))
        for node in ast.walk(tree):
            if not isinstance(node, ast.ClassDef):
                continue
            entry = binding.get(node.name)
            if entry is None:
                continue
            init = next(
                (
                    n
                    for n in node.body
                    if isinstance(n, ast.FunctionDef) and n.name == "__init__"
                ),
                None,
            )
            if init is None:
                continue
            params = [a.arg for a in init.args.args if a.arg != "self"]
            n_default = len(init.args.defaults)
            stub_optional = set(params[len(params) - n_default :])
            total, required, fmt, names, cfile = entry
            # A different arity means the two faces disagree about something
            # else entirely; that is not this gate's question to answer.
            if len(params) != total or len(names) != total:
                continue
            checked += 1
            bad = sorted(stub_optional & set(names[:required]))
            seen.update(f"{node.name}.{p}" for p in bad)
            bad = [p for p in bad if f"{node.name}.{p}" not in ignore]
            if bad:
                findings.append((str(pyi), node.name, bad, fmt, cfile))

    # An exemption nobody re-checks is not a decision, it is a blind spot: an
    # entry that no longer diverges must be deleted so the list keeps meaning
    # what it says.
    stale = sorted(ignore - seen)

    for pyi, cls, bad, fmt, cfile in findings:
        rel = pathlib.Path(pyi).relative_to(ROOT)
        print(f"  ! {cls}.{', '.join(bad)}")
        print(f"      {rel} gives a default; the extension requires it")
        print(f"      format {fmt!r} in {pathlib.Path(cfile).name}")

    if stale:
        print("\nStale entries in .init-param-optionality-ignore:\n")
        for s in stale:
            print(f"  ~ {s}  (no longer diverges — delete it)")

    if findings or stale:
        print(
            "\nA constructor argument the stub marks omittable but the "
            "binding requires:\nfollowing the published signature raises "
            "TypeError, and a type checker\ncannot see it. Fix the "
            "declaration, or add `Class.param` to\nscripts/"
            ".init-param-optionality-ignore with the reason it must wait.",
            file=sys.stderr,
        )
        return 1

    extra = f", {len(ignore)} accepted" if ignore else ""
    print(
        f"Init-param optionality: OK — {checked} constructor(s) agree "
        f"across both faces{extra}"
    )
    if args.verbose:
        for name in sorted(binding):
            print(f"    {name}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
