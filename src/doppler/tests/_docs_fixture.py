"""The first line a synthetic docs page needs in a gate test.

A guide, gallery or design page names its domain AND a one-line summary in
its first-line marker, and ``scripts/gen_domains.py`` refuses a page without
the summary. Every fixture that writes such a page builds the marker here, so
a test written after the summary rule cannot forget it, and a change to the
marker's shape is made in one place.
"""

from __future__ import annotations

__all__ = ["domain_line"]


def domain_line(domain: str, summary: str | None = "A fixture page.") -> str:
    """``<!-- domain: <domain>; summary: ... -->`` plus a blank line.

    ``summary=None`` writes the marker WITHOUT one, which is only for the
    case that proves a page lacking it is refused.
    """
    tail = "" if summary is None else f"; summary: {summary}"
    return f"<!-- domain: {domain}{tail} -->\n\n"
