#!/usr/bin/env python3
"""Every C test claim has a negative control, and adoption only grows.

A test that cannot fail is a comment. The audit of the objects landing for
the last release found tests that passed against a receiver that did nothing,
and three reject tests refused for a reason other than the one they named
(#847). ``dp_test.h`` has the scope that asks for the other half of an
assertion -- ``DP_CLAIM`` opens a claim, ``DP_CONTROL`` evaluates the same
assertion on a named wrong case that must fail, ``DP_REFUSAL`` is a refusal
with its minimal pair -- and this is its gate.

What it holds, from the source text alone (no build, no run):

1. **A claim has a control.** Between one ``DP_CLAIM`` and the next (or the
   end of the file) there is at least one ``DP_CONTROL`` or ``DP_REFUSAL``.
   A claim whose controls are made in a helper the text cannot see is listed
   in ``scripts/.claims-no-control`` as ``<file>:<claim id>`` with a reason.
2. **No control outside a claim**, and no claim id used twice in a file.
3. **Adoption only grows.** Every C test (a ``native/tests`` file that ends in
   ``DP_TEST_END``/``DP_TEST_EMIT_END``) either opens a claim or is listed in
   ``scripts/.claims-unadopted``. The list is a ratchet: a new test may not be
   added to it (so a new object's test arrives with its claims), and a line
   that names a file that now has claims, or no file, is stale and fails
   (delete it: the list can only shrink).

What it does not hold: that a control's expression is the same assertion as
its check's (review, and the derived mutants of layer 3), and a claim whose
checks the text cannot see. The count of controls per claim at run time is
the test binary's own record (``DP_CLAIMS_OUT``), which the validators read.

Usage
-----
    python scripts/check_claims.py           # the gate (exit 1 on a finding)
    python scripts/check_claims.py --list    # adoption status, per file
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TESTS = "native/tests"
UNADOPTED = "scripts/.claims-unadopted"
NO_CONTROL = "scripts/.claims-no-control"

# A file that must break the rules on purpose (the macros' own test) says so,
# with a reason, in its first lines; the status listing shows it as exempt.
EXEMPT = re.compile(r"claims-gate: exempt \((.+?)\)")
CLAIM = re.compile(r'\bDP_CLAIM\s*\(\s*"([^"]+)"')
CONTROL = re.compile(r"\bDP_(?:CONTROL|REFUSAL)\s*\(")
END = re.compile(r"\bDP_TEST_(?:EMIT_)?END\s*\(")


def read_list(path: Path) -> dict[str, str]:
    """A shrink-only list: ``entry  # reason`` per line, ``#`` comments."""
    out: dict[str, str] = {}
    if not path.exists():
        return out
    for line in path.read_text(encoding="utf-8").splitlines():
        body, _, reason = line.partition("#")
        body = body.strip()
        if body:
            out[body] = reason.strip()
    return out


def test_files(root: Path) -> list[Path]:
    """The C tests: files under native/tests that end in the epilogue."""
    found = []
    for p in sorted((root / TESTS).rglob("*.c")):
        text = p.read_text(encoding="utf-8", errors="replace")
        if END.search(text):
            found.append(p)
    return found


def strip_comments(text: str) -> str:
    """Source with C comments blanked, so a macro named in prose is not one."""
    text = re.sub(
        r"/\*.*?\*/", lambda m: " " * len(m.group(0)), text, flags=re.S
    )
    return re.sub(r"//[^\n]*", "", text)


def claims_in(text: str) -> list[tuple[str, int]]:
    """Each claim's id and the number of controls before the next claim."""
    code = strip_comments(text)
    marks = [(m.start(), m.group(1)) for m in CLAIM.finditer(code)]
    out = []
    for i, (at, cid) in enumerate(marks):
        end = marks[i + 1][0] if i + 1 < len(marks) else len(code)
        out.append((cid, len(CONTROL.findall(code[at:end]))))
    return out


def check(root: Path) -> tuple[list[str], list[tuple[str, str]]]:
    """Return (findings, status rows) for the tree at ``root``."""
    errors: list[str] = []
    rows: list[tuple[str, str]] = []
    unadopted = read_list(root / UNADOPTED)
    no_control = read_list(root / NO_CONTROL)
    seen_unadopted: set[str] = set()
    used_no_control: set[str] = set()

    for p in test_files(root):
        rel = p.relative_to(root).as_posix()
        text = p.read_text(encoding="utf-8", errors="replace")
        exempt = EXEMPT.search(text[:2000])
        if exempt:
            rows.append((rel, f"exempt: {exempt.group(1)}"))
            continue
        code = strip_comments(text)
        claims = claims_in(text)
        if not claims:
            rows.append((rel, "unadopted"))
            seen_unadopted.add(rel)
            if rel not in unadopted:
                errors.append(
                    f"{rel}: opens no DP_CLAIM and is not on {UNADOPTED}. "
                    "A new test arrives with its claims and a control for "
                    "each; the list is shrink-only and a new file may not "
                    "join it."
                )
            continue
        rows.append((rel, f"adopted, {len(claims)} claims"))
        if rel in unadopted:
            errors.append(
                f"{UNADOPTED}: '{rel}' now opens claims; delete the line "
                "(the list can only shrink)."
            )
        first = CLAIM.search(code)
        before = code[: first.start()] if first else code
        if CONTROL.search(before):
            errors.append(
                f"{rel}: a DP_CONTROL/DP_REFUSAL before any DP_CLAIM"
            )
        ids = [c for c, _ in claims]
        for cid in sorted({c for c in ids if ids.count(c) > 1}):
            errors.append(f"{rel}: claim id '{cid}' is used twice")
        for cid, n in claims:
            key = f"{rel}:{cid}"
            if n == 0:
                used_no_control.add(key)
                if key not in no_control:
                    errors.append(
                        f"{rel}: claim '{cid}' has no DP_CONTROL or "
                        "DP_REFUSAL: add the same assertion on a named wrong "
                        "case that must fail, or the minimal pair of its "
                        f"refusal ({NO_CONTROL} holds only what the text "
                        "cannot see, with a reason)."
                    )
            elif key in no_control:
                errors.append(
                    f"{NO_CONTROL}: '{key}' now has a control; "
                    "delete the line."
                )

    for entry in unadopted:
        if entry not in seen_unadopted and not (root / entry).exists():
            errors.append(f"{UNADOPTED}: '{entry}' names no file; delete it.")
    for entry, reason in no_control.items():
        if (
            entry not in used_no_control
            and not any(e == entry for e in used_no_control)
            and not any(r.startswith(entry.split(":")[0]) for r, _ in rows)
        ):
            errors.append(f"{NO_CONTROL}: '{entry}' names no file; delete it.")
        if not reason:
            errors.append(f"{NO_CONTROL}: '{entry}' gives no reason.")
    return errors, rows


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=(__doc__ or "").split("\n\n")[0])
    ap.add_argument("--root", type=Path, default=ROOT)
    ap.add_argument("--list", action="store_true", help="adoption per file")
    a = ap.parse_args(argv)
    errors, rows = check(a.root.resolve())
    if a.list:
        for rel, state in rows:
            print(f"{state:>24}  {rel}")
    adopted = sum(1 for _, s in rows if s.startswith("adopted"))
    if errors:
        print(f"check_claims: {len(errors)} finding(s)")
        for e in errors:
            print(f"  {e}")
        return 1
    print(f"check_claims: OK ({adopted} of {len(rows)} C tests adopted)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
