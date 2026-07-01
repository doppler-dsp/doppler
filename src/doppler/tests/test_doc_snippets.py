"""Fail-closed drift gate: every Python snippet in the docs must be tested.

The API-reference pages (``docs/api/*.md``) and the ``.pyi`` stubs are gated as
doctests in CI, and the 21 example scripts run under ``make
test-examples-python``. Everything else — the guides, gallery, and top-level
prose — used plain ```` ```python ```` blocks that nothing executed, so they
rot silently (the quickstart's ``HalfbandDecimator()`` broke exactly this way
when the constructor gained a required argument).

This gate closes that hole **by discovery, not registration**: it scans every
page under ``docs/`` and runs its Python fences. A page is enforced the moment
it exists — there is no opt-in list to forget. The only escapes are visible and
reviewed: an inline ``skip=`` marker with a mandatory reason, or a temporary
entry in the burn-down backlog ``docs/.doc-snippet-ignore`` (which must shrink
to empty, mirroring ``.api-coverage-ignore`` / ``.serializable-ignore``).

The four sanctioned states for a fence (see ``docs/dev/doc-examples.md``):

* **exec** — a plain ```` ```python ```` block; this gate runs it. The floor:
  it proves the snippet still *runs*.
* **doctest** — a ``>>>`` block; this gate checks its output. Proves the
  snippet still shows the *right result*.
* **include** — ``--8<--`` pulled byte-for-byte from a tested source
  (``src/doppler/examples/*.py``); zero drift by construction.
* **skip** — ``<!-- docs-snippet: skip=REASON -->``; reason mandatory. For
  genuinely unrunnable blocks (hardware, blocking multi-process, illustrative).

Execution model — **a page is one notebook**: its fences share a single
namespace and run top to bottom (so a block may use names a previous block
bound), seeded ``numpy.random.seed(0)`` for determinism, in a throwaway working
directory (file-writing blocks are safe and self-cleaning), with each block
under a wall-clock timeout (a blocking ``recv()`` or runaway loop fails fast
rather than hanging CI). Run locally with ``pytest -m docs_snippets``.
"""

from __future__ import annotations

import doctest
import io
import os
import re
import signal
import sys
from contextlib import contextmanager
from pathlib import Path

import pytest

os.environ.setdefault("MPLBACKEND", "Agg")  # any plotting block is headless

# repo layout: .../doppler/src/doppler/tests/ -> parents[3] == repo root.
REPO = Path(__file__).resolve().parents[3]
DOCS = REPO / "docs"
IGNORE_FILE = DOCS / ".doc-snippet-ignore"

# Per-block wall-clock budget. Doc snippets are illustrative; anything heavy
# belongs in an example script (--8<-- included), not inlined.
BLOCK_TIMEOUT_S = 30

# Generated / non-authored trees are never gated.
_EXCLUDED_PARTS = frozenset({"c-api", "archive"})
_EXCLUDED_RELPATHS = frozenset({"api.md", "benchmarks.md"})

# A python/pycon fence, indent-aware: fences nested in admonitions/tabs carry a
# leading indent that must be stripped (captured as `ind`, matched on the
# closing fence too).
_FENCE = re.compile(
    r"^(?P<ind>[ \t]*)```(?P<lang>python|pycon)[^\n]*\n"
    r"(?P<code>.*?)\n(?P=ind)```",
    re.DOTALL | re.MULTILINE,
)
# A skip=/raises= opt-out, on the line (optional blank line) before a fence.
_MARKER = re.compile(r"<!--\s*docs-snippet:\s*(.*?)\s*-->")


def _iter_fences(text):
    """Yield ``(marker, code)`` for each python/pycon fence, in document order.

    ``marker`` is the raw ``docs-snippet:`` payload (``"skip=..."`` /
    ``"raises=..."``) from an HTML comment immediately before the fence, or
    ``None``. ``code`` is dedented by the fence's own indentation so it
    compiles/execs as top-level statements.
    """
    for m in _FENCE.finditer(text):
        # Marker detection is whitespace-tolerant: scan the immediately
        # preceding non-blank line (allowing one blank line before the fence),
        # so a marker works at column 0 or indented inside a tab block.
        marker = None
        for line in reversed(text[: m.start()].splitlines()[-2:]):
            if not line.strip():
                continue
            hit = _MARKER.search(line)
            if hit:
                marker = hit.group(1)
            break
        ind = m.group("ind")
        code = m.group("code")
        if ind:
            code = "\n".join(
                line[len(ind) :] if line.startswith(ind) else line
                for line in code.splitlines()
            )
        yield marker, code


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
        if rel.as_posix() in _EXCLUDED_RELPATHS:
            continue
        if _has_fences(path):
            pages.append(rel.as_posix())
    return pages


def _load_ignore():
    """Parse docs/.doc-snippet-ignore → set of relpath strings."""
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


# --------------------------------------------------------------------------- #
# Sandbox + timeout                                                           #
# --------------------------------------------------------------------------- #
@contextmanager
def _block_timeout(seconds):
    """Raise TimeoutError if a block runs longer than ``seconds`` (SIGALRM)."""

    def _fire(signum, frame):
        raise TimeoutError(
            f"snippet exceeded {seconds}s — trim it, move it to a tested "
            f"example script (--8<--), or mark it skip="
        )

    prev = signal.signal(signal.SIGALRM, _fire)
    signal.alarm(seconds)
    try:
        yield
    finally:
        signal.alarm(0)
        signal.signal(signal.SIGALRM, prev)


@contextmanager
def _sandboxed(workdir):
    """chdir into a scratch dir and close stdin for the duration."""
    old_cwd = os.getcwd()
    old_stdin = sys.stdin
    os.chdir(workdir)
    sys.stdin = io.StringIO("")  # input() -> EOFError, never blocks
    try:
        yield
    finally:
        os.chdir(old_cwd)
        sys.stdin = old_stdin


def _run_one(blockid, marker, code, ns):
    """Execute or doctest a single fence against the shared namespace ``ns``.

    Returns "skipped" or "ran"; raises AssertionError (naming the block) on any
    failure so the parametrized test points straight at the offending snippet.
    """
    expect_raises = None
    if marker is not None:
        kind, _, rest = marker.partition("=")
        kind, rest = kind.strip(), rest.strip()
        if kind == "skip":
            assert rest, (
                f"{blockid}: skip marker needs a reason — "
                f"`<!-- docs-snippet: skip=why it can't run headless -->`"
            )
            return "skipped"
        if kind == "raises":
            assert rest, f"{blockid}: raises marker needs an exception type"
            expect_raises = rest
        else:
            raise AssertionError(
                f"{blockid}: unknown docs-snippet marker {marker!r} "
                f"(expected skip= or raises=)"
            )

    examples = doctest.DocTestParser().get_examples(code)
    raised = None
    with _block_timeout(BLOCK_TIMEOUT_S):
        if examples:
            # Console session: doctest checks the output. It copies globs, so
            # thread state back into ns afterwards.
            test = doctest.DocTest(examples, ns, blockid, None, None, None)
            runner = doctest.DebugRunner(optionflags=0)
            try:
                runner.run(test, clear_globs=False)
            except doctest.DocTestFailure as exc:
                raise AssertionError(
                    f"{blockid} doctest output mismatch:\n"
                    f"  example: {exc.example.source.strip()}\n"
                    f"  expected: {exc.example.want.strip()!r}\n"
                    f"  got:      {exc.got.strip()!r}"
                ) from exc
            except doctest.UnexpectedException as exc:
                raised = exc.exc_info[1]
            ns.update(test.globs)
        else:
            try:
                exec(compile(code, blockid, "exec"), ns)
            except KeyboardInterrupt:
                raise
            except (
                BaseException
            ) as exc:  # SystemExit from a snippet == failure
                raised = exc

    if expect_raises is not None:
        got = type(raised).__name__ if raised is not None else None
        assert got == expect_raises, (
            f"{blockid}: expected it to raise {expect_raises}, "
            f"but it raised {got}"
        )
        return "ran"

    if raised is not None:
        raise AssertionError(
            f"{blockid} raised {type(raised).__name__}: {raised}\n"
            f"----- snippet -----\n{code}\n-------------------"
        ) from raised
    return "ran"


# --------------------------------------------------------------------------- #
# The executor — one parametrized test per gated page (good failure names)    #
# --------------------------------------------------------------------------- #
@pytest.mark.docs_snippets
@pytest.mark.parametrize("page", GATED_PAGES)
def test_doc_page_snippets(page, tmp_path):
    """Run a page's fences as one notebook; fail naming the exact block."""
    text = (DOCS / page).read_text()
    ns = {}
    try:
        import numpy

        # Seed the legacy global RNG: snippets call np.random.* module funcs,
        # not a passed Generator, so this is what makes them deterministic.
        numpy.random.seed(0)  # noqa: NPY002
    except ImportError:
        pass
    with _sandboxed(tmp_path):
        for i, (marker, code) in enumerate(_iter_fences(text)):
            # A page whose blocks are all skip-marked is still validly gated:
            # every block is accounted for, and any block added later is
            # enforced (fail-closed). So there is no minimum-ran requirement.
            _run_one(f"{page}#block{i}", marker, code, ns)


# --------------------------------------------------------------------------- #
# Meta-tests — these make enforcement default-on and self-cleaning            #
# --------------------------------------------------------------------------- #
@pytest.mark.docs_snippets
def test_discovery_nonempty():
    assert ALL_PAGES, "no doc pages with python fences found — parser broken?"


@pytest.mark.docs_snippets
def test_ignore_list_not_stale():
    """Every ignore entry must name a real, still-fenced page.

    A page that was fixed (or lost its fences) leaves a stale entry, which
    fails here and forces its removal — so the backlog can only shrink.
    """
    discovered = set(ALL_PAGES)
    stale = sorted(IGNORED - discovered)
    assert not stale, (
        "docs/.doc-snippet-ignore has stale entries (page gone or no longer "
        f"has python fences) — remove them: {stale}"
    )


@pytest.mark.docs_snippets
def test_ignored_pages_have_no_inline_markers():
    """No half-triaged pages: an ignored page is skipped wholesale, so a
    per-block ``skip=``/``raises=`` marker on it is contradictory. Triage the
    page off the ignore list instead."""
    offenders = []
    for page in sorted(IGNORED & set(ALL_PAGES)):
        text = (DOCS / page).read_text()
        if any(marker is not None for marker, _ in _iter_fences(text)):
            offenders.append(page)
    assert not offenders, (
        "these pages are in docs/.doc-snippet-ignore yet carry inline "
        "docs-snippet markers — finish triaging them off the list: "
        f"{offenders}"
    )
