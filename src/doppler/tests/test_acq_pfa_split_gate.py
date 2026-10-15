"""The acq pricing gate, exercised over a seeded tree.

`scripts/check_acq_pfa_split.py` holds every `dp_det_pfa_cell` call under
`native/src/acq/` to the one cell count, `acq_band_cells()`. Pricing a gate
over a hand count of the native cells fired every D > 1 engine at about
twice its `pfa` (doppler#2218), and only the commit site has an exact pin in
test_acq_core.c, so a hand count back at the sizer or the non-coherent
ascend would pass everything else.

The pass cases are load-bearing too: a call split across lines, and the
count multiplied by the rate hypotheses, must stay green, and prose that
names the old count must not trip it.
"""

from __future__ import annotations

import subprocess
import sys
from typing import TYPE_CHECKING

import pytest

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)
SCRIPT = REPO / "scripts" / "check_acq_pfa_split.py"


def _seed(root: Path, body: str) -> Path:
    path = root / "native" / "src" / "acq" / "acq_core.c"
    path.parent.mkdir(parents=True)
    path.write_text(body, encoding="utf-8")
    return root


def _run(root: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(SCRIPT), "--root", str(root)],
        capture_output=True,
        text=True,
    )


@pytest.mark.parametrize(
    "body",
    [
        "pc = dp_det_pfa_cell (pfa, (double)acq_band_cells (D, s, w, c));\n",
        "st->pfa_cell = dp_det_pfa_cell (\n"
        "    pfa, (double)(acq_band_cells (D, sb, wb, cb)\n"
        "                  * acq_rate_count (st)));\n",
        "/* was dp_det_pfa_cell (pfa, sb * cb) */\n"
        "pc = dp_det_pfa_cell (pfa, (double)acq_band_cells (D, s, w, c));\n",
    ],
    ids=["one-line", "multi-line-with-rates", "prose-names-the-old-count"],
)
def test_the_one_count_passes(tmp_path: Path, body: str) -> None:
    r = _run(_seed(tmp_path, body))
    assert r.returncode == 0, r.stdout + r.stderr


@pytest.mark.parametrize(
    "body",
    [
        "pc = dp_det_pfa_cell (pfa, (double)(sb * cb));\n",
        "pc = dp_det_pfa_cell (\n    pfa, (double)(sb * cb * rates));\n",
    ],
    ids=["native-count", "native-count-multi-line"],
)
def test_a_hand_count_fails(tmp_path: Path, body: str) -> None:
    r = _run(_seed(tmp_path, body))
    assert r.returncode == 1, r.stdout + r.stderr
    assert "native/src/acq/acq_core.c:1" in r.stdout


def test_one_bad_site_among_good_ones_is_named(tmp_path: Path) -> None:
    body = (
        "a = dp_det_pfa_cell (pfa, (double)acq_band_cells (D, sb, wb, cb));\n"
        "b = dp_det_pfa_cell (pfa, (double)(sb * cb));\n"
    )
    r = _run(_seed(tmp_path, body))
    assert r.returncode == 1, r.stdout + r.stderr
    assert "acq_core.c:2" in r.stdout and "acq_core.c:1" not in r.stdout


def test_no_call_at_all_is_not_a_pass(tmp_path: Path) -> None:
    """A gate with nothing to check is blind, not green."""
    r = _run(_seed(tmp_path, "int x;\n"))
    assert r.returncode == 2, r.stdout + r.stderr


def test_the_real_tree_passes() -> None:
    r = subprocess.run(
        [sys.executable, str(SCRIPT)], capture_output=True, text=True
    )
    assert r.returncode == 0, r.stdout + r.stderr
    assert "split over acq_band_cells()" in r.stdout
