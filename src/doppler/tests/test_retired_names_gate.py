"""The retired-names gate's two moving parts: what it scans, what it matches.

`scripts/check_retired_names.py` reads a fixed tree, so it cannot be pointed
at a seeded one; these tests exercise its pieces directly instead.

The skip rule is the one that failed silently. It used a string prefix, and
`.github/...` starts with `.git`, so every workflow file was skipped. That
surfaced only when `.github` was added to the scan and a sabotaged workflow
still passed.
"""

from __future__ import annotations

import importlib.util
from typing import TYPE_CHECKING

import pytest

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    import re

REPO = repo_root(__file__)
_spec = importlib.util.spec_from_file_location(
    "check_retired_names", REPO / "scripts" / "check_retired_names.py"
)
assert _spec and _spec.loader
gate = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(gate)


@pytest.mark.parametrize(
    "rel",
    [
        ".github/workflows/ci.yml",
        "docs/c-apix.md",  # shares a prefix with docs/c-api, is not it
        "native/src/buildx/a.c",
    ],
)
def test_ours_is_scanned(rel: str) -> None:
    assert not gate._skipped(rel)


@pytest.mark.parametrize(
    "rel",
    [
        ".git/config",
        "docs/c-api/files.md",
        "examples/downstream-jm/.venv/lib/x.py",
        "native/build/CMakeCache.txt",
    ],
)
def test_not_ours_is_skipped(rel: str) -> None:
    assert gate._skipped(rel)


def _windows_rows() -> list[re.Pattern[str]]:
    rows = [pat for pat, _raw, repl in gate._rows() if "Windows" in repl]
    assert rows, "the retired Windows-support claim has no rows"
    return rows


@pytest.mark.parametrize(
    "text",
    [
        # The phrasings removed from the tree on 2026-09-18, each built from
        # two pieces so this file does not itself contain the retired text
        # the gate scans for. join() rather than adjacent literals: ruff's
        # ISC001 fix merges those back into one, which is how the first
        # version of this test tripped the gate it tests.
        "".join(("# Windows / MSVC is intentionally un", "supported (signal")),
        "".join(("# Windows is intentionally un", "supported: signal-")),
        "".join(("doppler does not ", "target Windows natively.")),
        "".join(("# msys2 is absent: doppler doesn't ", "target Windows")),
    ],
)
def test_the_retired_claim_is_caught(text: str) -> None:
    assert any(p.search(text) for p in _windows_rows())


@pytest.mark.parametrize(
    "text",
    [
        "Windows builds with clang/clang-cl.",
        "MSVC's own cl.exe cannot build doppler: it has no C99 _Complex.",
        "The NATS stream layer is not ported to Windows (#1364).",
    ],
)
def test_the_accurate_statements_pass(text: str) -> None:
    assert not any(p.search(text) for p in _windows_rows())
