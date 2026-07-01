"""Drift gate: execute the Python snippets embedded in the prose docs.

The API-reference pages under ``docs/api/`` are already gated as doctests
(CI step "Doctest API reference pages"), but the *narrative* pages —
``quickstart.md`` and the guides — use plain ```python``` fenced blocks with
no ``>>>`` prompt, so nothing runs them and they rot silently. That is
exactly how the quickstart's ``HalfbandDecimator()`` example went stale: the
constructor gained a required ``h`` argument and no test noticed.

This module extracts every registered page's Python fences and executes them,
in document order, in a single shared namespace (notebook semantics — a later
block may use names bound by an earlier one, the way the FFT block defines
``x`` and the filter block consumes it). Any exception fails the test and
names the offending block.

Opting a block out
------------------
A block that cannot run headless — a blocking network ``recv()``, a hardware
source, a two-terminal demo — is skipped with an HTML comment placed
immediately before its fence (invisible in the rendered site)::

    <!-- docs-snippet: skip=two-terminal ZMQ demo; see stream tests -->
    ```python
    ...
    ```

The reason after ``skip=`` is mandatory: a bare marker fails the test, so
every exclusion is a visible, reviewed decision rather than silent erosion.

Adding a page
-------------
Append its path (relative to ``docs/``) to :data:`PAGES` once its snippets
run end-to-end. The goal is that no narrative code snippet is untested.
"""

from __future__ import annotations

import re
from pathlib import Path

import pytest

# repo docs/ dir: .../doppler/src/doppler/tests/ -> parents[3] == repo root.
DOCS = Path(__file__).resolve().parents[3] / "docs"

# Narrative pages whose ```python blocks are executed as a drift gate.
PAGES = ["quickstart.md"]

# A ```python fence, optionally preceded by a `docs-snippet:` skip marker.
_FENCE = re.compile(
    r"(?:<!--\s*docs-snippet:\s*(?P<skip>.*?)\s*-->\s*\n\s*\n?)?"
    r"```python\n(?P<code>.*?)\n```",
    re.DOTALL,
)


def _blocks(md: str):
    """Yield ``(skip_marker_or_None, code)`` per python fence, in order."""
    for m in _FENCE.finditer(md):
        yield m.group("skip"), m.group("code")


def test_registered_pages_exist_and_have_snippets():
    """Guard the harness itself: every registered page must parse to blocks."""
    for page in PAGES:
        path = DOCS / page
        assert path.is_file(), f"registered page missing: {page}"
        blocks = list(_blocks(path.read_text()))
        assert blocks, f"{page}: no ```python blocks found — parser broken?"


@pytest.mark.parametrize("page", PAGES)
def test_doc_page_snippets_execute(page):
    """Execute a page's runnable python blocks in one shared namespace."""
    text = (DOCS / page).read_text()
    namespace: dict = {}
    ran = 0
    for i, (skip, code) in enumerate(_blocks(text)):
        if skip is not None:
            # A marker is present — it must carry a non-empty reason.
            assert skip.startswith("skip=") and skip[len("skip=") :].strip(), (
                f"{page} block {i}: skip marker needs a reason, e.g. "
                f"`<!-- docs-snippet: skip=why it can't run headless -->`"
            )
            continue
        try:
            exec(compile(code, f"{page}#block{i}", "exec"), namespace)
        except Exception as exc:
            raise AssertionError(
                f"{page} block {i} raised "
                f"{type(exc).__name__}: {exc}\n"
                f"----- snippet -----\n{code}\n-------------------"
            ) from exc
        ran += 1
    assert ran, f"{page}: every block was skipped — nothing was gated"
