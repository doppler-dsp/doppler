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


# ── --narrow: a run narrowed to one script renders only that script (#2058) ──
# The real recipe's shape, cut to what the check reads: the script loop, the
# derived move, an echoed recipe comment, the characterization behind the
# origin test, and the tag-time freshness gate. Each case below breaks one
# piece the way a careless edit would; `make -n` executes none of it.
CHAR = "chars/sweep/characterize.py"
_MAKEFILE = f"""\
GALLERY_CHARACTERIZATIONS := {CHAR}
GALLERY_SCRIPTS := {EX}/a_demo.py {EX}/b_demo.py

gallery:
\t@for script in $(GALLERY_SCRIPTS); do \\
\t    printf "  %-45s" "$$script"; \\
\t    uv run python $$script > /dev/null 2>&1 || exit 1; \\
\tdone
\t@for png in $$(uv run python scripts/check_gallery_scripts.py \\
\t        --outputs $(GALLERY_SCRIPTS)); do \\
\t    mv -f "$$png" docs/assets/; \\
\tdone
\t# Echoed by make -n, and no command: other_demo.py, other.png.
ifeq ($(origin GALLERY_SCRIPTS),file)
\t@printf "  %-45s" "$(GALLERY_CHARACTERIZATIONS)"
\t@uv run python $(GALLERY_CHARACTERIZATIONS) \\
\t     docs/assets/sweep.png > /dev/null 2>&1
endif

release-freshness-check:
\t@uv run python scripts/check_release_freshness.py \\
\t    --version $(VERSION) $(GALLERY_SCRIPTS) $(GALLERY_CHARACTERIZATIONS)
"""


def _narrow_repo(tmp_path: Path, makefile: str = _MAKEFILE) -> Path:
    repo = _fixture(tmp_path)
    (repo / "Makefile").write_text(makefile, encoding="utf-8")
    return repo


def _narrow(repo: Path, *chars: str) -> subprocess.CompletedProcess[str]:
    return _run(
        repo, "--narrow", f"{EX}/a_demo.py", "--characterizations", *chars
    )


def _cut(old: str, new: str = "") -> str:
    """The fixture Makefile with exactly one piece replaced."""
    assert _MAKEFILE.count(old) == 1, old
    return _MAKEFILE.replace(old, new)


def test_narrow_passes_on_the_fixed_recipe(tmp_path: Path) -> None:
    """Also the echoed comment's .py and .png: a comment is no command."""
    r = _narrow(_narrow_repo(tmp_path), CHAR)
    assert r.returncode == 0, r.stdout + r.stderr
    assert "1 characterization(s)" in r.stdout


def test_narrow_unscoped_characterization_is_red(tmp_path: Path) -> None:
    """The #2058 recipe: the step ran on a narrowed run as well."""
    mk = _cut("ifeq ($(origin GALLERY_SCRIPTS),file)\n")
    mk = mk.replace("\nendif\n", "\n")
    r = _narrow(_narrow_repo(tmp_path, mk), CHAR)
    assert r.returncode == 1
    assert "docs/assets/sweep.png" in r.stdout
    assert CHAR in r.stdout


def test_narrow_label_without_the_run_is_red(tmp_path: Path) -> None:
    """A partial deletion: the printf still NAMES the subject; no run."""
    mk = _cut(
        "\t@uv run python $(GALLERY_CHARACTERIZATIONS) \\\n"
        "\t     docs/assets/sweep.png > /dev/null 2>&1\n"
    )
    r = _narrow(_narrow_repo(tmp_path, mk), CHAR)
    assert r.returncode == 1
    assert f"no longer runs {CHAR}" in r.stdout


def test_narrow_gutted_script_loop_is_red(tmp_path: Path) -> None:
    """The script is still in the `for` list and `--outputs`; not run."""
    mk = _cut("\t    uv run python $$script > /dev/null 2>&1 || exit 1; \\\n")
    r = _narrow(_narrow_repo(tmp_path, mk), CHAR)
    assert r.returncode == 1
    assert f"does not run {EX}/a_demo.py" in r.stdout


def test_narrow_empty_characterization_list_is_red(tmp_path: Path) -> None:
    """An emptied or renamed GALLERY_CHARACTERIZATIONS checks nothing."""
    r = _narrow(_narrow_repo(tmp_path))
    assert r.returncode == 1
    assert "no characterization subjects" in r.stdout


def test_narrow_freshness_not_given_the_subject_is_red(
    tmp_path: Path,
) -> None:
    """Only the full run renders it, so the tag gate must see it change."""
    mk = _cut(
        " $(GALLERY_SCRIPTS) $(GALLERY_CHARACTERIZATIONS)\n",
        " $(GALLERY_SCRIPTS)\n",
    )
    r = _narrow(_narrow_repo(tmp_path, mk), CHAR)
    assert r.returncode == 1
    assert f"release-freshness-check` is not given {CHAR}" in r.stdout
