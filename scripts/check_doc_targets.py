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

import functools
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# Generated or vendored trees: their contents are owned elsewhere.
SKIP_PARTS = {"c-api", "archive", "build", "site", ".venv", "vendor"}

# A documented invocation may carry variable assignments before the target --
# `make PREFIX=~/.local run` is the form every example-projects README uses.
# Without this the target was simply not seen: the old pattern required a
# lowercase word immediately after `make`, so those lines went unchecked and
# a sabotage of the target they name did not redden anything. Assignments are
# safe to skip because `NAME=value` is not something English produces after
# the word "make".
_ASSIGN = r"(?:[A-Za-z_][A-Za-z0-9_]*=\S*\s+)*"

# `make foo` inside backticks, anywhere.
BACKTICKED = re.compile(r"`make " + _ASSIGN + r"([a-z][a-z0-9-]*)")
# `make foo` at the start of a line, but ONLY inside a fenced block. Scanning
# the whole document for this instead was the first version, and prose caught
# it immediately: "the receiver's code loop has to / make up" wraps so that a
# sentence begins with `make `. One false positive of that kind is enough to
# get a checker disabled, which costs more than the drift it was catching.
FENCE_LINE = re.compile(
    r"^\s*(?:\$ )?make " + _ASSIGN + r"([a-z][a-z0-9-]*)", re.M
)
FENCE = re.compile(r"^```.*?^```", re.M | re.S)


@functools.cache
def real_targets(cwd: Path) -> set[str]:
    """Every target make knows about in *cwd*, from its own database."""
    proc = subprocess.run(
        ["make", "-rpn", "--no-print-directory"],
        cwd=cwd,
        capture_output=True,
        text=True,
    )
    # A Makefile that fails to parse yields an empty database, and then
    # every documented target reads as missing -- true, but it names the
    # symptom instead of the cause.
    if proc.returncode != 0:
        raise SystemExit(
            f"check_doc_targets: `make -rpn` failed in {cwd}, so there is "
            "no target list to check against:\n"
            + (proc.stderr or "(no stderr)")
        )
    # A NAME IS NOT A RULE. `.PHONY: build run clean` makes make emit a bare
    # `run:` entry for every name listed, with no recipe behind it -- so
    # scraping target lines alone accepted a documented target that had been
    # RENAMED, which is the single drift this gate exists to catch. Found by
    # sabotage: renaming `run:` in all three example projects left the check
    # green, because `.PHONY` still carried the old name.
    #
    # make's own database distinguishes them: a real target either carries a
    # `#  recipe to execute` line or has prerequisites. An aggregate like
    # `test-all: $(TEST_ALL_DEPS)` has no recipe at all and is perfectly
    # runnable, so requiring a recipe alone flagged three true sentences --
    # either is the correct test, neither is the bare `.PHONY` echo.
    real: set[str] = set()
    blocks = re.split(r"\n(?=[a-zA-Z0-9_][a-zA-Z0-9_.-]*:)", proc.stdout)
    for block in blocks:
        m = re.match(r"^([a-zA-Z0-9_][a-zA-Z0-9_.-]*):(.*)", block)
        if not m:
            continue
        head = block.split("\n\n", 1)[0]
        if m.group(2).strip() or "recipe to execute" in head:
            real.add(m.group(1))
    return real


def owning_makefile_dir(page: Path) -> Path:
    """The directory whose Makefile a reader of *page* would actually run.

    A README inside `example-projects/<project>/` documents THAT project's
    Makefile, not this repo's -- those projects are standalone downstreams
    with their own `build` / `run` / `clean`. Resolving every page against
    the root database would report `make run` as missing while a reader
    typing it gets exactly what the README promised, which is the checker
    being confidently wrong: worse than not checking, because the fix it
    implies is to delete a true sentence.

    Walks up from the page to the first directory holding a Makefile,
    stopping at ROOT. Every page outside a project therefore resolves
    against ROOT exactly as before.
    """
    for d in [page.parent, *page.parent.parents]:
        if (d / "Makefile").is_file():
            return d
        if d == ROOT:
            break
    return ROOT


def targets_for(page: Path) -> set[str]:
    """The targets a reader of *page* could legitimately be told to run.

    The UNION of its own project's Makefile and this repo's root, because a
    project README genuinely names both: `examples/downstream-jm/README.md`
    documents its own `make docs` a few lines from doppler's
    `make drift-check`, and each is correct where it stands. Resolving
    against the project alone flagged four true sentences there.

    The union is a weaker guarantee than a single database would be -- a
    root-only target named in a project README passes -- and it is the one
    that matches how these files are actually read. What the gate is for
    survives: a name that exists in NEITHER context is drift, and that is
    every case it has ever caught.
    """
    own = owning_makefile_dir(page)
    # COPY: real_targets is lru_cached, so `out |= ...` on its return
    # value mutates the cache. That is not theoretical -- the index
    # page resolves against ROOT and unions its children, which wrote
    # `run` into the cached ROOT set, and every later page then saw a
    # target the root Makefile does not have. The gate stayed green
    # through a sabotage because of it.
    out = set(real_targets(own))
    if own != ROOT:
        out |= real_targets(ROOT)
    # An INDEX page documents its CHILDREN. `example-projects/README.md` sits
    # in a directory with no Makefile of its own and tells a reader to
    # `make run` inside any of the three projects beside it -- true, and
    # invisible to a checker that only ever looks upward.
    #
    # Only for an index, though: `page.parent` having its own Makefile means
    # the page documents THAT, and unioning its children would import a
    # CMake-generated `build/Makefile`'s several hundred target names into
    # the namespace. Sabotage caught exactly that -- renaming `run` in a
    # project left its own README green, because the build tree beside it
    # happened to define one. SKIP_PARTS keeps generated trees out for the
    # same reason it does when collecting pages.
    if own != page.parent:
        for child in sorted(page.parent.iterdir()):
            if child.name in SKIP_PARTS or not child.is_dir():
                continue
            if (child / "Makefile").is_file():
                out |= real_targets(child)
    return out


def pages() -> list[Path]:
    out = [ROOT / "README.md", ROOT / "CONTRIBUTING.md"]
    for root in ("docs", "examples", "example-projects"):
        for page in sorted((ROOT / root).rglob("*.md")):
            if SKIP_PARTS.intersection(page.relative_to(ROOT).parts):
                continue
            out.append(page)
    return [p for p in out if p.is_file()]


def main() -> int:
    hits: list[str] = []

    for page in pages():
        real = targets_for(page)
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

    n_makefiles = len({owning_makefile_dir(p) for p in pages()})
    print(
        f"check_doc_targets: OK — every documented target exists "
        f"({len(pages())} pages against {n_makefiles} makefile(s))"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
