#!/usr/bin/env python3
"""Fail when a doc names a `make` target that does not exist.

The Makefile got stricter and better-gated during the standard adoption
(doppler-dsp/doppler#555) -- and the prose describing it did not move with
it. That is the RFC's own failure mode one level up: the tooling stopped
drifting, and the documentation kept teaching the previous way, silently,
because nothing checks prose.

It is not hypothetical. This gate was written after finding three live
cases in one scan: ``make bench-baseline`` and ``make bench-check`` in
``docs/dev/contributing/benchmarking.md`` (renamed to ``bench-save`` /
``bench-compare``
by the port itself), and ``make install`` in ``CONTRIBUTING.md``,
advertised as "System install" for a target that has no rule anywhere.

Prose cannot be gated. Target *names* in prose can, which is the part that
actually breaks a reader: a renamed target turns a documented command into
``No rule to make target``, and a deleted one turns it into silence.

What counts as a reference
--------------------------
Only a **backticked** ``make <target>`` or a line inside a fenced block
that starts with ``make <target>``. Ordinary English -- "make changes",
"make sure", "standard make targets" -- is left alone on purpose: a
checker that flags prose is a checker that gets switched off.

Targets are read from make's own database (``make -rpn``), not scraped
from the Makefile, so the ``lint-<tool>`` rules that ``standard.mk``
stamps out with ``$(eval)`` are seen too -- they appear as literal text in
no file at all.

Usage
-----
    python scripts/check_doc_targets.py    # exit 1 on any missing target
"""

from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# Generated or vendored trees: their contents are owned elsewhere.
SKIP_PARTS = {"c-api", "archive", "build", "site", ".venv", "vendor"}

# `make foo` inside backticks, anywhere.
BACKTICKED = re.compile(r"`make ([a-z][a-z0-9-]*)")
# `make foo` at the start of a line, but ONLY inside a fenced block. Scanning
# the whole document for this instead was the first version, and prose caught
# it immediately: "the receiver's code loop has to / make up" wraps so that a
# sentence begins with `make `. One false positive of that kind is enough to
# get a checker disabled, which costs more than the drift it was catching.
FENCE_LINE = re.compile(r"^\s*(?:\$ )?make ([a-z][a-z0-9-]*)", re.M)
FENCE = re.compile(r"^```.*?^```", re.M | re.S)


def real_targets() -> set[str]:
    """Every target make knows about, from its own database."""
    proc = subprocess.run(
        ["make", "-rpn", "--no-print-directory"],
        cwd=ROOT,
        capture_output=True,
        text=True,
    )
    # A Makefile that fails to parse yields an empty database, and then
    # every documented target reads as missing -- true, but it names the
    # symptom instead of the cause.
    if proc.returncode != 0:
        raise SystemExit(
            "check_doc_targets: `make -rpn` failed, so there is no target "
            "list to check against:\n" + (proc.stderr or "(no stderr)")
        )
    return set(
        re.findall(r"^([a-zA-Z0-9_][a-zA-Z0-9_.-]*):", proc.stdout, re.M)
    )


def pages() -> list[Path]:
    out = [ROOT / "README.md", ROOT / "CONTRIBUTING.md"]
    for root in ("docs", "examples", "example-projects"):
        for page in sorted((ROOT / root).rglob("*.md")):
            if SKIP_PARTS.intersection(page.relative_to(ROOT).parts):
                continue
            out.append(page)
    return [p for p in out if p.is_file()]


def main() -> int:
    real = real_targets()
    hits: list[str] = []

    for page in pages():
        text = page.read_text(encoding="utf-8")
        seen: set[tuple[str, int]] = set()

        # (pattern, text, offset) — fences are scanned in place so reported
        # line numbers stay absolute.
        scans = [(BACKTICKED, text, 0)]
        scans += [
            (FENCE_LINE, m.group(0), m.start()) for m in FENCE.finditer(text)
        ]

        for pat, blob, offset in scans:
            for m in pat.finditer(blob):
                name = m.group(1)
                if name in real:
                    continue
                line = text.count("\n", 0, offset + m.start()) + 1
                if (name, line) in seen:
                    continue
                seen.add((name, line))
                rel = page.relative_to(ROOT)
                hits.append(f"  {rel}:{line}: make {name}")

    if hits:
        print(
            "check_doc_targets: these docs name a `make` target that does "
            "not exist -- it was renamed or removed and the prose did not "
            "move with it. Run `make help` for the real list:",
            file=sys.stderr,
        )
        print("\n".join(sorted(hits)), file=sys.stderr)
        return 1

    print(
        f"check_doc_targets: OK — every documented target exists "
        f"({len(pages())} pages, {len(real)} targets)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
