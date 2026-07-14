"""Fail-closed drift gate: every C snippet in the docs must compile and run.

Companion to ``test_doc_snippets.py`` (the Python gate) — same philosophy,
same reason for existing. A ```` ```c ```` fence claiming to be a complete,
copy-pasteable example is worth nothing if nobody ever actually compiles it;
the homepage's own C "Quick start" snippet sat broken for a release (missing
``#include <complex.h>``, undeclared ``in``/``out`` arrays, top-level
function calls outside ``main()`` — three separate compile errors) because
nothing checked it. "Quick and easy" that silently doesn't compile is worse
than not showing C at all.

This gate closes that hole **by discovery, not registration**: it scans
every page under ``docs/`` and compiles + runs its C fences against the
already-built library (``build/libdoppler.a``). A page is enforced the
moment it exists — there is no opt-in list to forget. The only escape is
visible and reviewed: an inline ``skip=`` marker with a mandatory reason, or
a temporary entry in the burn-down backlog ``docs/.c-doc-snippet-ignore``
(which must shrink to empty, mirroring ``.doc-snippet-ignore``).

The three sanctioned states for a C fence (see ``docs/dev/doc-examples.md``):

* **exec** — a plain ```` ```c ```` block with its own ``int main(void)``;
  this gate compiles it (``-std=gnu99 -Wall -Wextra -Werror`` — the same
  standard doppler's own CMakeLists.txt builds the library as, and a
  warning is a failure here, not just an error) against the built library
  and runs the
  resulting binary, requiring exit code 0.
* **include** — ``--8<--`` pulled byte-for-byte from a tested source
  (``examples/c/*.c``, already built + run by ``make test-examples``); zero
  drift by construction.
* **skip** — ``<!-- docs-snippet: skip=REASON -->``; reason mandatory. For
  genuinely non-standalone blocks: an illustrative fragment (a struct layout,
  a partial diff, a signature-only excerpt) that was never meant to compile
  on its own.

Unlike the Python gate, fences on one page do **not** share a namespace —
each is a fully independent compile-and-run (C has no REPL-style carryover
between top-level programs), so there is no notebook execution model to
port. Run locally with ``pytest -m docs_snippets test_c_doc_snippets.py``
(requires ``make build`` first — see ``docs/dev/doc-examples.md``).
"""

from __future__ import annotations

import shutil
import subprocess
import sys

import pytest

from doppler.tests._docs_snippet_common import DOCS, REPO
from doppler.tests._docs_snippet_common import iter_fences as _iter_fences_raw
from doppler.tests._docs_snippet_common import (
    resolve_snippets as _resolve_snippets,
)

IGNORE_FILE = DOCS / ".c-doc-snippet-ignore"
BUILD_DIR = REPO / "build"

# Per-snippet wall-clock budget (compile + run combined).
SNIPPET_TIMEOUT_S = 30

# Generated / non-authored trees are never gated (matches the Python gate).
_EXCLUDED_PARTS = frozenset({"c-api", "archive"})


def _iter_fences(text):
    return _iter_fences_raw(text, "c")


def _has_fences(path):
    try:
        return next(_iter_fences(path.read_text()), None) is not None
    except OSError:
        return False


def _discover_pages():
    """Every gate-eligible page under docs/ (relpath posix strings)."""
    pages = []
    for path in sorted(DOCS.rglob("*.md")):
        rel = path.relative_to(DOCS)
        if _EXCLUDED_PARTS & set(rel.parts):
            continue
        if _has_fences(path):
            pages.append(rel.as_posix())
    return pages


def _load_ignore():
    """Parse docs/.c-doc-snippet-ignore -> set of relpath strings."""
    if not IGNORE_FILE.exists():
        return set()
    out = set()
    for line in IGNORE_FILE.read_text().splitlines():
        line = line.strip()
        if line and not line.startswith("#"):
            out.add(line)
    return out


ALL_PAGES = _discover_pages()
IGNORED = _load_ignore()
GATED_PAGES = sorted(set(ALL_PAGES) - IGNORED)


def _page_fence_count(page):
    return sum(1 for _ in _iter_fences((DOCS / page).read_text()))


# One parametrized case per (page, block index) — a page with N fences is N
# independent test cases, not one, so a single broken block doesn't hide its
# N-1 siblings' results and a failure names the exact block up front.
CASES = [(p, i) for p in GATED_PAGES for i in range(_page_fence_count(p))]


def _cc() -> str:
    for candidate in ("cc", "gcc", "clang"):
        found = shutil.which(candidate)
        if found:
            return found
    pytest.skip("no C compiler (cc/gcc/clang) on PATH")


def _broker_reachable(host="127.0.0.1", port=4222):
    import socket

    try:
        with socket.create_connection((host, port), timeout=0.5):
            return True
    except OSError:
        return False


def _compile_and_run(blockid, code, tmp_path, run=True):
    src = tmp_path / "snippet.c"
    src.write_text(code)
    exe = tmp_path / "snippet"
    lib = BUILD_DIR / "libdoppler.a"
    assert lib.exists(), (
        f"{blockid}: {lib} not found -- build the C library first "
        f"(`make build`) before running this gate; see "
        f"docs/dev/doc-examples.md"
    )
    # libdoppler_stream.a (stream/stream.h's dp_pub_*/dp_sub_*/dp_push_*/
    # dp_pull_* wire layer + vendored nats.c) is a deliberately separate,
    # optional component -- CMakeLists.txt: "a consumer that wants ... the
    # dp_pub_*/dp_sub_* wire layer links `doppler::stream` alongside
    # `doppler::doppler[-static]`". Link it (+ pthread) whenever present so
    # streaming snippets resolve exactly like that documented consumer
    # recipe; --start-group/--end-group makes the link order-independent
    # regardless of which archive references the other's symbols. Harmless
    # for non-streaming snippets -- a static archive only pulls in members
    # actually referenced.
    stream_lib = BUILD_DIR / "libdoppler_stream.a"
    archives = (
        ["-Wl,--start-group", str(lib), str(stream_lib), "-Wl,--end-group"]
        if stream_lib.exists()
        else [str(lib)]
    )
    libs = ["-lm"] + (["-lpthread"] if stream_lib.exists() else [])
    compile_cmd = [
        _cc(),
        str(src),
        # gnu99, not strict c99: doppler's own CMakeLists.txt sets
        # CMAKE_C_STANDARD 99 without CMAKE_C_EXTENSIONS OFF, so CMake's
        # default (extensions ON) already builds the library itself as
        # gnu99 -- strict c99 hides the POSIX/GNU declarations (syscall,
        # ftruncate, ...) buffer.h needs, which doppler's own build never
        # hits. Matching the library's actual, tested standard, not a
        # stricter one the codebase doesn't use.
        "-std=gnu99",
        "-Wall",
        "-Wextra",
        "-Werror",
        "-I",
        str(REPO / "native" / "inc"),
        "-I",
        str(BUILD_DIR / "native" / "inc"),
        *archives,
        *libs,
        "-o",
        str(exe),
    ]
    try:
        compiled = subprocess.run(
            compile_cmd,
            capture_output=True,
            text=True,
            timeout=SNIPPET_TIMEOUT_S,
        )
    except subprocess.TimeoutExpired as exc:
        raise AssertionError(
            f"{blockid}: compile exceeded {SNIPPET_TIMEOUT_S}s"
        ) from exc
    if compiled.returncode != 0:
        raise AssertionError(
            f"{blockid} failed to compile:\n"
            f"----- snippet -----\n{code}\n"
            f"----- cc stderr -----\n{compiled.stderr}"
        )

    if not run:
        return  # no-run= / broker-unreachable: compile check only

    try:
        ran = subprocess.run(
            [str(exe)],
            capture_output=True,
            text=True,
            timeout=SNIPPET_TIMEOUT_S,
            cwd=tmp_path,
        )
    except subprocess.TimeoutExpired as exc:
        raise AssertionError(
            f"{blockid}: run exceeded {SNIPPET_TIMEOUT_S}s -- trim it, "
            f"move it to a tested example (--8<--), or mark it skip="
        ) from exc
    if ran.returncode != 0:
        raise AssertionError(
            f"{blockid} exited {ran.returncode}:\n"
            f"----- stdout -----\n{ran.stdout}\n"
            f"----- stderr -----\n{ran.stderr}"
        )


def _run_one(blockid, marker, code, tmp_path):
    run = True
    if marker is not None:
        kind, _, rest = marker.partition("=")
        kind, rest = kind.strip(), rest.strip()
        if kind == "skip":
            assert rest, (
                f"{blockid}: skip marker needs a reason -- "
                f"`<!-- docs-snippet: skip=why -->`"
            )
            return
        elif kind == "no-run":
            # Compile with the full -Werror consumer recipe but never
            # execute: for a complete program whose run blocks on a
            # live peer by design. The snippet stays verified against
            # the real headers and link line -- skip= would drop even
            # that.
            assert rest, f"{blockid}: no-run marker needs a reason"
            run = False
        elif kind == "broker":
            # Compile always; execute only when a NATS broker is
            # reachable on 127.0.0.1:4222 (CI's python-tests job
            # starts one) -- the same idiom as the Python gate's
            # broker= and the example gate's broker: registry class.
            assert rest, f"{blockid}: broker marker needs a reason"
            run = _broker_reachable()
        else:
            raise AssertionError(
                f"{blockid}: unknown docs-snippet marker {marker!r} "
                f"(expected skip=, no-run=, or broker= -- C fences "
                f"don't support raises=)"
            )

    code = _resolve_snippets(code)  # inline any --8<-- gold-standard includes
    _compile_and_run(blockid, code, tmp_path, run=run)


@pytest.mark.docs_snippets
@pytest.mark.parametrize(
    "page,index", CASES, ids=[f"{p}#block{i}" for p, i in CASES]
)
def test_c_doc_page_snippet(page, index, tmp_path):
    """Compile + run one C fence; fail naming the exact block."""
    text = (DOCS / page).read_text()
    marker, code = list(_iter_fences(text))[index]
    _run_one(f"{page}#block{index}", marker, code, tmp_path)


# --------------------------------------------------------------------------- #
# Meta-tests -- these make enforcement default-on and self-cleaning           #
# --------------------------------------------------------------------------- #
@pytest.mark.docs_snippets
def test_discovery_nonempty():
    assert ALL_PAGES, "no doc pages with C fences found -- parser broken?"


@pytest.mark.docs_snippets
def test_ignore_list_not_stale():
    """Every ignore entry must name a real, still-fenced page.

    A page that was fixed (or lost its fences) leaves a stale entry, which
    fails here and forces its removal -- so the backlog can only shrink.
    """
    discovered = set(ALL_PAGES)
    stale = sorted(IGNORED - discovered)
    assert not stale, (
        "docs/.c-doc-snippet-ignore has stale entries (page gone or no "
        f"longer has C fences) -- remove them: {stale}"
    )


@pytest.mark.docs_snippets
def test_ignored_pages_have_no_inline_markers():
    """No half-triaged pages: an ignored page is skipped wholesale, so a
    per-block ``skip=`` marker on it is contradictory. Triage the page off
    the ignore list instead."""
    offenders = []
    for page in sorted(IGNORED & set(ALL_PAGES)):
        text = (DOCS / page).read_text()
        if any(marker is not None for marker, _ in _iter_fences(text)):
            offenders.append(page)
    assert not offenders, (
        "these pages are in docs/.c-doc-snippet-ignore yet carry inline "
        "docs-snippet markers -- finish triaging them off the list: "
        f"{offenders}"
    )


if __name__ == "__main__":  # pragma: no cover
    sys.exit(pytest.main([__file__, "-v"]))
