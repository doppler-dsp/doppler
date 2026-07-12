"""Shared plumbing between the Python and C docs-snippet drift gates.

Both ``test_doc_snippets.py`` and ``test_c_doc_snippets.py`` need the exact
same ``--8<--`` include resolution (pymdownx.snippets' syntax, which the docs
build inlines at build time — the gate must inline it identically, or it runs
different code than the reader sees), and the same repo-relative path roots.
Factored out once so the two gates cannot drift apart on this shared rule.
"""

from __future__ import annotations

import re
from pathlib import Path

# repo layout: .../doppler/src/doppler/tests/ -> parents[3] == repo root.
REPO = Path(__file__).resolve().parents[3]
DOCS = REPO / "docs"

# `--8<-- "path"` (whole file) or `--8<-- "path:section"` (the lines between
# `--8<-- [start:section]` / `[end:section]` markers). Paths are repo-relative
# (base_path is "." in mkdocs.yml).
_SNIPPET_LINE = re.compile(
    r'^[ \t]*-{2}8<-{2}[ \t]+"(?P<path>[^":]+)(?::(?P<sec>[^"]+))?"[ \t]*$',
    re.MULTILINE,
)


def _snippet_section(text, name):
    tag = re.escape(name)
    start = re.search(rf"-{{2}}8<-{{2}}[ \t]*\[start:{tag}\]", text)
    end = re.search(rf"-{{2}}8<-{{2}}[ \t]*\[end:{tag}\]", text)
    assert start and end, f"snippet region [start/end:{name}] not found"
    body = text[start.end() : end.start()]
    # Drop any nested marker lines; trim the blank lines around the region.
    kept = [ln for ln in body.splitlines() if "--8<--" not in ln]
    return "\n".join(kept).strip("\n")


def resolve_snippets(code, _seen=frozenset()):
    """Inline `--8<--` includes so the gate runs the built (inlined) code."""
    if "--8<--" not in code:
        return code

    def _repl(m):
        rel, sec = m.group("path"), m.group("sec")
        src = REPO / rel
        assert src.exists(), f"snippet source not found: {rel}"
        assert (rel, sec) not in _seen, f"recursive include: {rel}:{sec}"
        text = src.read_text()
        text = _snippet_section(text, sec) if sec else text
        return resolve_snippets(text, _seen | {(rel, sec)})

    return _SNIPPET_LINE.sub(_repl, code)


# A skip=/raises= opt-out, on the line (optional blank line) before a fence.
_MARKER = re.compile(r"<!--\s*docs-snippet:\s*(.*?)\s*-->")


def iter_fences(text, lang_re):
    """Yield ``(marker, code)`` for each fence matching ``lang_re``, in
    document order. ``marker`` is the raw ``docs-snippet:`` payload
    (``"skip=..."`` / ``"raises=..."``) from an HTML comment immediately
    before the fence, or ``None``. ``code`` is dedented by the fence's own
    indentation so it compiles/execs as top-level statements."""
    # \b after the language group is required: without it, lang_re="c"
    # substring-matches the "c" prefix of "```console"/"```cpp"/etc fences
    # too (a real bug caught while building the C snippet gate -- it was
    # silently compiling ```console shell-session blocks as C).
    fence = re.compile(
        r"^(?P<ind>[ \t]*)```(?P<lang>" + lang_re + r")\b[^\n]*\n"
        r"(?P<code>.*?)\n(?P=ind)```",
        re.DOTALL | re.MULTILINE,
    )
    for m in fence.finditer(text):
        # Marker detection is whitespace-tolerant: scan the immediately
        # preceding non-blank line (allowing one blank line before the
        # fence), so a marker works at column 0 or indented inside a tab
        # block.
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
