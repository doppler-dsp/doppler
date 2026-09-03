"""`make gen-c-api-run` replaces the generated C API tree only with a
complete render.

The target used to ``rm -rf docs/c-api`` before building, so any failure in
between -- on 2026-09-02, a ``uv sync`` running concurrently with the
pre-commit hook that invokes it -- left the committed tree wiped, the
hand-written ``index.md`` with it, and the ``git checkout`` restore failing
because the deletions had by then been staged. The recipe now builds beside
the tree and swaps by rename, and this file is the gate on that ordering:
it drives the real recipe with a build command that fails (``CAPI_BUILD=
false``) against a scratch copy of the tree, and requires the copy untouched.

The build is stubbed, not run: doxygen and mkdoxy are the success path,
which ``make gen-c-api-check`` exercises in ``make lint``. What this proves
is the property no green run can -- that failure is not destructive.
"""

from __future__ import annotations

import hashlib
import shutil
import subprocess
from typing import TYPE_CHECKING

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)
TREE = REPO / "docs" / "c-api"


def _fingerprint(root: Path) -> dict[str, str]:
    """Every file under `root`, relative path -> content hash."""
    return {
        str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest()
        for p in sorted(root.rglob("*"))
        if p.is_file()
    }


def test_failed_build_leaves_the_tree_untouched(tmp_path: Path) -> None:
    scratch = tmp_path / "c-api"
    shutil.copytree(TREE, scratch)
    before = _fingerprint(scratch)
    assert "index.md" in before, "the hand-written landing page must be there"

    r = subprocess.run(
        [
            "make",
            "-s",
            "gen-c-api-run",
            f"CAPI_OUT={scratch}",
            "CAPI_BUILD=false",
        ],
        capture_output=True,
        text=True,
        cwd=REPO,
    )
    assert r.returncode != 0, "a failing build must fail the target"

    after = _fingerprint(scratch)
    assert after == before, "a failed render must not touch the tree"
    assert not (tmp_path / "c-api.new").exists()
