#!/usr/bin/env python3
"""The keyword names a binding accepts must be the ones its stub publishes.

A CPython binding declares its keywords in a `_kwlist`, and the `.pyi` stub
declares them again in a `def`. Nothing compared the two, so they could
disagree indefinitely — and one pair did, for years:
`DelayCf64.ptr` accepted `n=` while `delay.pyi` published `count=`
(doppler#619). A caller following the stub got a `TypeError`, and a type
checker validated the call the interpreter then refused.

**`jm status --check` cannot see this class.** The kwlist lives in the
wrapper body of a sacred `_ext_<obj>.c` fragment, and jm says so in its own
output — *"never re-renders a wrapper body — that part is yours. Not counted
as drift."* Measured before writing this: renaming the kwarg back to `n`
leaves `make drift-check` reporting exactly what it reported before, and
passing. So this is not a gap jm will close; the two faces are ours to
compare.

Registration-free by construction. It discovers `native/src/*/*_ext_*.c`,
pairs each `<Class>Obj_<method>` with the `def` of the same name under
`class <Class>` in that module's stub, and compares the keyword names in
order. A new object is covered the moment its fragment exists.

## What it deliberately does not check

Only methods that have BOTH a `_kwlist` and a stub `def` are compared. A
positional-only binding (`PyArg_ParseTuple`, no kwlist) publishes no keyword
names, so there is nothing to disagree about.

Everything skipped is COUNTED AND PRINTED rather than passed over in
silence — a gate whose skip list grows quietly is the failure this repo
names elsewhere as indistinguishable from a gate passing.

## The ratchet

`KNOWN` carries the mismatches that existed when this gate was written, and
it MAY ONLY SHRINK — an entry that stops mismatching is a failure, so the
list cannot rot in the other direction either.

Both entries are the same shape and neither is ours to fix in a manifest:
a hand-written `out=` on a method whose params are SCALAR. jm offers `out=`
for the void-arg generator shape and for an array input, and not for this
one — verified by regenerating `delay_ext_delay.c` from the manifest and
reading what came out (`{ "x", NULL }`, no `out`). So the capability is real,
hand-written, and invisible to the manifest the stub is rendered from.
Filed upstream; doppler#922 tracks removing these two.
"""

from __future__ import annotations

import pathlib
import re
import sys

REPO = pathlib.Path(__file__).resolve().parent.parent

#: `<Class>Obj_<method> (` at the start of a line — GNU style puts the
#: declarator on its own line, so this is the whole definition header.
_FUNC = re.compile(r"^(?P<cls>\w+)Obj_(?P<meth>\w+)\s*\(", re.MULTILINE)

#: `static char *_kwlist[] = { "a", "b", NULL };`, possibly wrapped.
_KWLIST = re.compile(r"_kwlist\s*\[\s*\]\s*=\s*\{(?P<body>.*?)\}", re.DOTALL)

_STR = re.compile(r'"([^"]*)"')

#: Mismatches that predate this gate. SHRINK ONLY — see "The ratchet" above.
#: Each maps ``(module, Class, method)`` to the keywords the BINDING accepts,
#: so a change to either face fails rather than silently re-baselining.
KNOWN: dict[tuple[str, str, str], list[str]] = {
    # `DelayCf64.push_ptr` came OFF this list on the jm 0.64.1 bump: gh-1079
    # gave the all-scalar `variable_output` shape a generated `out=`, so the
    # stub now publishes the keyword the hand-written binding already took and
    # the two agree on their own. `Farrow.delay` stays -- it is an array
    # beside a scalar, which `_outbuf.why_not` excludes (gh-412), so jm
    # generates no `out=` there and the hand-written one is still unmatched.
    ("resample", "Farrow", "delay"): ["x", "mu", "out"],
}


def c_kwlists(path: pathlib.Path) -> dict[tuple[str, str], list[str]]:
    """Map ``(Class, method) -> keyword names`` for one fragment."""
    text = path.read_text(encoding="utf-8")
    hits = list(_FUNC.finditer(text))
    out: dict[tuple[str, str], list[str]] = {}
    for i, m in enumerate(hits):
        end = hits[i + 1].start() if i + 1 < len(hits) else len(text)
        body = text[m.start() : end]
        kw = _KWLIST.search(body)
        if kw is None:
            continue
        names = _STR.findall(kw.group("body"))
        out[(m.group("cls"), m.group("meth"))] = names
    return out


#: `    def name(` inside a class body, capturing through the closing paren.
_DEF = re.compile(
    r"^(?P<indent>[ \t]+)def[ \t]+(?P<name>\w+)[ \t]*\((?P<args>.*?)\)[ \t]*"
    r"(->.*?)?:",
    re.MULTILINE | re.DOTALL,
)
_CLASS = re.compile(r"^class[ \t]+(?P<name>\w+)", re.MULTILINE)


def stub_defs(path: pathlib.Path) -> dict[tuple[str, str], list[str]]:
    """Map ``(Class, method) -> parameter names`` for one stub."""
    text = path.read_text(encoding="utf-8")
    bounds = [(m.start(), m.group("name")) for m in _CLASS.finditer(text)]
    out: dict[tuple[str, str], list[str]] = {}
    for i, (start, cls) in enumerate(bounds):
        end = bounds[i + 1][0] if i + 1 < len(bounds) else len(text)
        for d in _DEF.finditer(text[start:end]):
            args = d.group("args")
            names: list[str] = []
            depth = 0
            cur = ""
            for ch in args:
                if ch in "[({":
                    depth += 1
                elif ch in "])}":
                    depth -= 1
                if ch == "," and depth == 0:
                    names.append(cur)
                    cur = ""
                else:
                    cur += ch
            names.append(cur)
            clean = []
            for raw in names:
                nm = raw.split(":")[0].split("=")[0].strip()
                if nm and nm not in ("self", "*", "/", "cls"):
                    clean.append(nm.lstrip("*"))
            out[(cls, d.group("name"))] = clean
    return out


def main() -> int:
    frags = sorted((REPO / "native" / "src").glob("*/*_ext_*.c"))
    if not frags:
        print("check_kwarg_parity: no *_ext_*.c fragments found — this gate")
        print("  has not run, so it has not passed.")
        return 1

    bad: list[str] = []
    checked = 0
    ratcheted = 0
    no_stub_file: set[str] = set()
    no_stub_member: list[str] = []

    for frag in frags:
        module = frag.parent.name
        stub = REPO / "src" / "doppler" / module / f"{module}.pyi"
        kwl = c_kwlists(frag)
        if not kwl:
            continue
        if not stub.is_file():
            no_stub_file.add(module)
            continue
        defs = stub_defs(stub)
        for (cls, meth), names in sorted(kwl.items()):
            key = (cls, "__init__" if meth == "init" else meth)
            if key not in defs:
                no_stub_member.append(f"{module}:{cls}.{key[1]}")
                continue
            checked += 1
            want = defs[key]
            allowed = KNOWN.get((module, cls, key[1]))
            if allowed is not None:
                if names == want:
                    bad.append(
                        f"  {module}:{cls}.{key[1]} is in KNOWN but now "
                        f"AGREES — delete the entry. The ratchet may only "
                        f"shrink, and an entry that no longer describes "
                        f"anything is the same rot in the other direction."
                    )
                elif names != allowed:
                    bad.append(
                        f"  {module}:{cls}.{key[1]} mismatches DIFFERENTLY "
                        f"than KNOWN records.\n"
                        f"    binding accepts {names}\n"
                        f"    KNOWN records   {allowed}\n"
                        f"    Fix it, or update the entry deliberately."
                    )
                else:
                    ratcheted += 1
                continue
            if names != want:
                bad.append(
                    f"  {frag.relative_to(REPO)}\n"
                    f"    {cls}.{key[1]} binding accepts {names}\n"
                    f"    {stub.relative_to(REPO)} publishes  {want}"
                )

    if bad:
        print("check_kwarg_parity: a stub publishes keywords the binding")
        print("  does not accept. A caller following the stub gets a")
        print("  TypeError, and a type checker validates the call that")
        print("  fails (doppler#619).\n")
        print("\n".join(bad))
        print(
            f"\n  {len(bad)} mismatch(es) over {checked} compared method(s)."
        )
        return 1

    extra = f", {ratcheted} ratcheted (doppler#922)" if ratcheted else ""
    if no_stub_file:
        extra += f", {len(no_stub_file)} module(s) with no stub"
    if no_stub_member:
        extra += f", {len(no_stub_member)} member(s) absent from a stub"
    print(
        f"check_kwarg_parity: OK — {checked} keyword list(s) agree with the "
        f"stub that publishes them{extra}"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
