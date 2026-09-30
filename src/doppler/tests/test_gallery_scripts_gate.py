"""The gallery gate: every committed plot has a script that re-renders it.

``make gallery`` used to carry two lists for one thing: ``GALLERY_SCRIPTS``
(what it runs, and what ``release-freshness-check`` reads) and a hand-kept
list of PNG names to move into ``docs/assets/``. They disagreed:
``plan_background_demo.png`` was moved on every run while its script was
never run, so the plot went stale unseen and v0.59.0 shipped a picture of a
defect already fixed (#1644).

The move list is now DERIVED from the scripts (``--outputs``), so that pair
cannot disagree. What remains is the direction no derivation can close: a
script that is simply missing from ``GALLERY_SCRIPTS`` while its plot sits
committed. That is what these cases pin, against the real script run on a
scratch repo.
"""

from __future__ import annotations

import subprocess
import sys
from typing import TYPE_CHECKING

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)
SCRIPT = REPO / "scripts" / "check_gallery_scripts.py"
EX = "src/doppler/examples"


def _fixture(tmp_path: Path) -> Path:
    """Two examples whose plots are committed, one example with none."""
    (tmp_path / EX).mkdir(parents=True)
    (tmp_path / "docs/assets").mkdir(parents=True)
    (tmp_path / EX / "a_demo.py").write_text('fig.savefig("a_demo.png")\n')
    (tmp_path / EX / "b_demo.py").write_text(
        'def main(out="b_plot.png"):\n    ...\n'
    )
    # Prints only -- no plot, so it is not a gallery script at all.
    (tmp_path / EX / "c_demo.py").write_text("print('hi')\n")
    (tmp_path / "docs/assets/a_demo.png").write_text("PNG\n")
    (tmp_path / "docs/assets/b_plot.png").write_text("PNG\n")
    return tmp_path


def _run(repo: Path, *args: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(SCRIPT), "--repo", str(repo), *args],
        capture_output=True,
        text=True,
        check=False,
    )


def test_every_plot_listed_passes(tmp_path: Path) -> None:
    repo = _fixture(tmp_path)
    r = _run(repo, f"{EX}/a_demo.py", f"{EX}/b_demo.py")
    assert r.returncode == 0, r.stdout + r.stderr


def test_dropped_script_is_red(tmp_path: Path) -> None:
    """The #1644 shape: a committed plot whose script nothing runs."""
    repo = _fixture(tmp_path)
    r = _run(repo, f"{EX}/a_demo.py")
    assert r.returncode == 1
    assert f"{EX}/b_demo.py" in r.stdout
    assert "b_plot.png" in r.stdout


def test_listed_script_naming_no_plot_is_red(tmp_path: Path) -> None:
    """A listed script with no PNG would be run and its output lost."""
    repo = _fixture(tmp_path)
    r = _run(repo, f"{EX}/a_demo.py", f"{EX}/b_demo.py", f"{EX}/c_demo.py")
    assert r.returncode == 1
    assert f"{EX}/c_demo.py" in r.stdout


def test_listed_script_missing_is_red(tmp_path: Path) -> None:
    repo = _fixture(tmp_path)
    r = _run(repo, f"{EX}/a_demo.py", f"{EX}/b_demo.py", f"{EX}/gone.py")
    assert r.returncode == 1
    assert "gone.py" in r.stdout


def test_outputs_names_each_scripts_plot(tmp_path: Path) -> None:
    """What `make gallery` moves: derived, never restated."""
    repo = _fixture(tmp_path)
    r = _run(repo, "--outputs", f"{EX}/a_demo.py", f"{EX}/b_demo.py")
    assert r.returncode == 0, r.stderr
    assert r.stdout.split() == ["a_demo.png", "b_plot.png"]
