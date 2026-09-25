"""The C API link-reference gate, over seeded pages.

``scripts/check_c_api_link_refs.py`` scans mkdoxy's generated
``docs/c-api/*.md`` for a bare ``[...]`` -- markdown's shortcut reference
link, which the strict site build refuses. It is seeded here rather than
tested against the real tree, because the real tree is clean: a gate that can
only be run on passing input cannot be shown to fail.

The two spans that broke CI (doppler#1542, doppler#1552) must be found, and
everything markdown means as something else must not.
"""

from __future__ import annotations

import subprocess
import sys
from typing import TYPE_CHECKING

import pytest

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

SCRIPT = repo_root(__file__) / "scripts" / "check_c_api_link_refs.py"


def _run(tmp_path: Path, text: str) -> subprocess.CompletedProcess[str]:
    (tmp_path / "page.md").write_text(text, encoding="utf-8")
    return subprocess.run(
        [sys.executable, str(SCRIPT), "--root", str(tmp_path)],
        capture_output=True,
        text=True,
        check=False,
    )


@pytest.mark.parametrize(
    "text",
    [
        "Detection probability in [0, 1].",  # doppler#1552
        "`out[e * n_f + j]` is fine, but out[e * n_f + j] is not.",  # #1542
    ],
)
def test_a_bare_span_is_refused(tmp_path: Path, text: str) -> None:
    r = _run(tmp_path, text)
    assert r.returncode == 1
    assert "renders as a link reference" in r.stdout


@pytest.mark.parametrize(
    "text",
    [
        "an inline [link](other.md)",
        "a full [reference][ref] and its [ref]: target",
        "an image ![fig](a.png)",
        "escaped \\[0, 1\\] and entities &#91;0, 1&#93;",
        "code `x[i]` and a half-open [0, n) interval",
        "```c\nint a[4];\n```",
    ],
)
def test_what_markdown_means_otherwise_passes(
    tmp_path: Path, text: str
) -> None:
    assert _run(tmp_path, text).returncode == 0


def test_no_pages_is_a_failure_not_a_pass(tmp_path: Path) -> None:
    # An empty directory would otherwise report "none bare" about nothing.
    r = subprocess.run(
        [sys.executable, str(SCRIPT), "--root", str(tmp_path)],
        capture_output=True,
        text=True,
        check=False,
    )
    assert r.returncode == 1
