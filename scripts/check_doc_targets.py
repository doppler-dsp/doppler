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
A **backticked** ``make <target>``, a line inside a fenced block that
starts with ``make <target>``, and a bare name (below). Ordinary English
-- "make changes", "make sure", "standard make targets" -- is left alone
on purpose: a checker that flags prose is a checker that gets switched
off.

A bare name
-----------
A target is also named without ``make``: "`bench-save` / `bench-compare`
remain". That form slipped past the first version three times, all of them
renames this gate's own history records as fixed (#2116):
``bench-baseline`` and ``bench-check`` in CLAUDE.md, ``bench-check`` in
benchmarking.md, and ``test-example-tarball`` in the downstream-jm README,
whose ``make`` forms 6fb7aff51 renamed to ``test-starter-tarball``.

So a backticked hyphenated name whose first segment starts a real target
(``bench-``, ``test-``, ``lint-``...) must name SOMETHING this repo
declares: a target, a CI job, a workflow ``name:`` (an artifact or an
environment), a pre-commit hook, or one of the outside names in
``EXTERNAL``. The family filter keeps file stems and flags out, and the
declared names keep CI jobs (``ci-passed``) and hooks (``gen-c-api-drift``)
in, since those share a family with a target. Measured over every tracked
page when it was written, it found four stale names (the three above and
a ``python-tests`` CI job that is ``python``), and its only other hits
were the outside names now in ``EXTERNAL``.

What it cannot see, so a rename still needs its own row: a rename that
changes the FIRST segment (``bump-version`` -> ``version-bump`` leaves no
``bump-`` family to hold the old name to), and a name that coincides with
one in another namespace (a hook id equal to a retired target). The
renaming commit's row in ``scripts/.retired-names`` covers both, in every
file, so that is where a rename is recorded. ``make -C dir`` and
``make -s`` forms are not parsed either.

The page set is every tracked ``.md`` file, not a list, so a new page is
held the day it exists. ``CHANGELOG.md`` and ``changelog.d/`` are records:
they name retired targets by design.

Targets are read from make's own database (``make -rpn``), not scraped
from the Makefile, so the ``lint-<tool>`` rules that ``standard.mk``
stamps out with ``$(eval)`` are seen too -- they appear as literal text in
no file at all.

Usage
-----
    python scripts/check_doc_targets.py    # exit 1 on any missing target
    python scripts/check_doc_targets.py PAGE...   # only these pages
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
# Release records: they name retired targets on purpose, as history.
RECORDS = {"CHANGELOG.md", "changelog.d"}

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
# A whole backtick span that is one hyphenated lowercase name: `bench-save`.
BARE = re.compile(r"`([a-z][a-z0-9]*(?:-[a-z0-9]+)+)`")

# Names the docs use that share a family with a target and that nothing in
# this repo declares. Each one names a thing outside it.
EXTERNAL = {
    "nats-server": "the NATS broker's binary, which the streaming pages run",
    "nats-jetstream": "NATS's persistence layer, as deploy/README.md names it",
    "docker-buildx": "Docker's build plugin, which the image recipes need",
    "docker-compose": "Docker's multi-container tool, in the deploy docs",
    "just-makeit": "the codegen tool, pinned in pyproject.toml's dev group",
    "just-bashit": "a sibling just-buildit project (shell helpers)",
    "just-buildit": "the sibling build tool and its GitHub org",
    "release-process": "an mcp-store skill the release page cites",
}


@functools.cache
def real_targets(cwd: Path) -> set[str]:
    """Every target make knows about in *cwd*, from its own database."""
    proc = subprocess.run(
        ["make", "-rpn", "--no-print-directory"],
        cwd=cwd,
        capture_output=True,
        text=True,
        encoding="utf-8",
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


@functools.cache
def declared_names() -> frozenset[str]:
    """Every hyphenated name the repo declares outside its makefiles.

    Read from the declarations themselves, so a renamed CI job or hook
    reddens the page that still names the old one: workflow job ids,
    one-word ``name:`` scalars (artifacts and environments), pre-commit
    hook ids, and ``EXTERNAL``.
    """
    out = set(EXTERNAL)
    for wf in sorted((ROOT / ".github" / "workflows").glob("*.y*ml")):
        text = wf.read_text(encoding="utf-8")
        out |= set(re.findall(r"^  ([a-z][a-z0-9-]*):\s*$", text, re.M))
        out |= set(
            re.findall(r"^\s+name:\s*([a-z][a-z0-9-]*)\s*$", text, re.M)
        )
    hooks = ROOT / ".pre-commit-config.yaml"
    if hooks.is_file():
        out |= set(
            re.findall(
                r"^\s+- id:\s*([a-z0-9-]+)",
                hooks.read_text(encoding="utf-8"),
                re.M,
            )
        )
    return frozenset(out)


def bare_misses(text: str, real: set[str]) -> list[tuple[str, int]]:
    """Bare backticked names in a target family that name nothing declared.

    >>> real = {"bench-save", "bench-compare", "test"}
    >>> bare_misses("`bench-save` and `bench-check` remain", real)
    [('bench-check', 1)]
    >>> bare_misses("the `report-v2` file and `--bench-check` flag", real)
    []
    """
    families = {t.split("-", 1)[0] for t in real if "-" in t}
    out = []
    for m in BARE.finditer(text):
        name = m.group(1)
        if name in real or name.split("-", 1)[0] not in families:
            continue
        if name in declared_names():
            continue
        out.append((name, text.count("\n", 0, m.start()) + 1))
    return out


def pages() -> list[Path]:
    """Every tracked Markdown page, less generated trees and records.

    Derived, not listed: the hand list this replaced left out CLAUDE.md,
    which names more targets than any other page (#2116), and
    deploy/docker/README.md. Tracked files only, so a local scratch page
    never decides the verdict.
    """
    proc = subprocess.run(
        ["git", "ls-files", "-z", "--", "*.md"],
        cwd=ROOT,
        capture_output=True,
        text=True,
        encoding="utf-8",
    )
    if proc.returncode != 0:
        raise SystemExit(
            "check_doc_targets: `git ls-files` failed, so there is no page "
            "list to check:\n" + (proc.stderr or "(no stderr)")
        )
    out = []
    for rel in sorted(filter(None, proc.stdout.split("\0"))):
        parts = Path(rel).parts
        if SKIP_PARTS.intersection(parts) or parts[0] in RECORDS:
            continue
        out.append(ROOT / rel)
    return [p for p in out if p.is_file()]


def _rel(page: Path) -> str:
    try:
        return str(page.relative_to(ROOT))
    except ValueError:
        return str(page)


def main(argv: list[str]) -> int:
    hits: list[str] = []
    # Explicit pages are how test_doc_targets_gate.py proves each rule red
    # on a page written for it; with none, the gate reads the repo's set.
    todo = [Path(a).resolve() for a in argv] if argv else pages()
    missing = [p for p in todo if not p.is_file()]
    if not todo or missing:
        print(
            f"check_doc_targets: no page to check: {missing or 'none found'}",
            file=sys.stderr,
        )
        return 2

    for page in todo:
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
                hits.append(f"  {_rel(page)}:{line}: make {name}")

        for name, line in bare_misses(text, real):
            hits.append(f"  {_rel(page)}:{line}: `{name}` (bare)")

    if hits:
        print(
            "check_doc_targets: these docs name a `make` target that does "
            "not exist -- it was renamed or removed and the prose did not "
            "move with it. Run `make help` for the real list. A (bare) "
            "name is in a target's family but is no target, CI job or "
            "hook: if it is a renamed one, use the new name (and give the "
            "old one a row in scripts/.retired-names); if it names a thing "
            "outside this repo, add it to EXTERNAL in "
            "scripts/check_doc_targets.py with its reason:",
            file=sys.stderr,
        )
        print("\n".join(sorted(hits)), file=sys.stderr)
        return 1

    n_makefiles = len({owning_makefile_dir(p) for p in todo})
    print(
        f"check_doc_targets: OK — every documented target exists "
        f"({len(todo)} pages against {n_makefiles} makefile(s))"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
