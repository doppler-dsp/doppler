"""A ``<!-- docs-snippet: ... -->`` marker is directly above the fence it
governs (doppler#2875).

The doc-fence gates bind a marker to the fence on the next non-blank line.
Insert a paragraph between them and the marker silently detaches: the gates
then run, or compile, the fence they were told to skip, and the failure names
the fence rather than the edit that caused it (a clone block landed between
`streaming.md`'s `skip=` marker and its fragment, and the C gate failed on
code that had not changed). So a marker that is not followed by a fence is
itself the error, found where it was made.

Discovered over every page under ``docs/``; no list to forget.
"""

from __future__ import annotations

import re
import sys

from doppler.tests._repo import repo_root

REPO = repo_root(__file__)
MARKER = re.compile(r"^\s*<!--\s*docs-snippet:")
FENCE = re.compile(r"^\s*(```|~~~)")


def detached(text: str) -> list[int]:
    """Line numbers of markers whose next non-blank line is not a fence."""
    lines = text.splitlines()
    out = []
    for i, line in enumerate(lines):
        if not MARKER.match(line):
            continue
        j = i + 1
        while j < len(lines) and not lines[j].strip():
            j += 1
        if j >= len(lines) or not FENCE.match(lines[j]):
            out.append(i + 1)
    return out


def test_a_marker_above_a_paragraph_is_detached() -> None:
    assert detached(
        "<!-- docs-snippet: skip=x -->\n\ntext\n\n```c\n```\n"
    ) == [1]


def test_a_marker_above_its_fence_is_attached() -> None:
    assert detached("<!-- docs-snippet: skip=x -->\n\n```c\n```\n") == []
    assert detached("  <!-- docs-snippet: skip=x -->\n  ```c\n  ```\n") == []


def test_every_marker_in_docs_is_above_a_fence() -> None:
    sys.path.insert(0, str(REPO / "scripts"))
    from _docs_pages import pages

    bad = [
        f"{p.relative_to(REPO).as_posix()}:{n}"
        for p in pages(REPO)
        for n in detached(p.read_text(encoding="utf-8"))
    ]
    assert not bad, "marker not directly above a fence: " + ", ".join(bad)
