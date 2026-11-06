"""``make test-all`` runs every test target CI runs, or says why not.

``scripts/check_test_all_covers_ci.py`` (``make lint-test-all-ci``) reads the
test targets CI's workflows run through make and holds ``TEST_ALL_DEPS`` to
them, with ``scripts/.test-all-exempt`` for a target that cannot run locally.
Each case seeds a tree and points ``--root`` at it; the last runs the real
tree, so the gate is proven red on a copy of it, not only on a toy.
"""

from __future__ import annotations

import importlib.util
import shutil
from typing import TYPE_CHECKING

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path
    from types import ModuleType

REPO = repo_root(__file__)
SCRIPT = REPO / "scripts" / "check_test_all_covers_ci.py"

WORKFLOW = """\
jobs:
  a:
    steps:
      - run: make -s test-python TEST_PATHS=x
      - run: make test-snippets
      - run: make test-sweep
"""


def _load() -> ModuleType:
    spec = importlib.util.spec_from_file_location("ck", SCRIPT)
    assert spec and spec.loader
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def _seed(tmp_path: Path, deps: str, exempt: str | None = None) -> Path:
    (tmp_path / ".github" / "workflows").mkdir(parents=True)
    (tmp_path / ".github" / "workflows" / "ci.yml").write_text(WORKFLOW)
    (tmp_path / "Makefile").write_text(f"TEST_ALL_DEPS = {deps}\n")
    (tmp_path / "scripts").mkdir()
    if exempt is not None:
        (tmp_path / "scripts" / ".test-all-exempt").write_text(exempt)
    return tmp_path


SWEEP = "test-sweep | its budget is CI's runner\n"


def test_every_ci_target_covered_or_exempt_passes(tmp_path: Path) -> None:
    root = _seed(tmp_path, "test-python test-snippets", SWEEP)
    assert _load().check(root) == []


def test_a_ci_target_test_all_skips_fails(tmp_path: Path) -> None:
    """#2886's shape: CI runs test-snippets and test-all does not."""
    errs = _load().check(_seed(tmp_path, "test-python", SWEEP))
    assert len(errs) == 1 and "make test-snippets" in errs[0]


def test_a_continued_deps_line_is_read_whole(tmp_path: Path) -> None:
    root = _seed(tmp_path, "test-python \\\n    test-snippets", SWEEP)
    assert _load().check(root) == []


def test_an_exempt_line_needs_a_reason(tmp_path: Path) -> None:
    root = _seed(tmp_path, "test-python test-snippets", "test-sweep\n")
    errs = _load().check(root)
    assert errs == ["'test-sweep' has no reason"]


def test_a_stale_exempt_line_fails(tmp_path: Path) -> None:
    """Exempt but now in the deps, or exempt and no longer run by CI."""
    mod = _load()
    both = _seed(tmp_path / "a", "test-python test-snippets test-sweep", SWEEP)
    assert any("is in TEST_ALL_DEPS" in e for e in mod.check(both))
    gone = _seed(
        tmp_path / "b",
        "test-python test-snippets",
        SWEEP + "test-gone | no longer in CI\n",
    )
    assert any("'test-gone' is not run by CI" in e for e in mod.check(gone))


def test_the_real_tree_passes_and_fails_when_sabotaged(tmp_path: Path) -> None:
    """The repo as committed passes; dropping test-snippets from its
    TEST_ALL_DEPS, the exact gap #2886 fell through, turns it red."""
    mod = _load()
    assert mod.check(REPO) == []
    copy = tmp_path / "repo"
    shutil.copytree(
        REPO / ".github" / "workflows", copy / ".github" / "workflows"
    )
    (copy / "scripts").mkdir()
    shutil.copy(REPO / "scripts" / ".test-all-exempt", copy / "scripts")
    text = (REPO / "Makefile").read_text(encoding="utf-8")
    deps = mod.DEPS.search(text)
    assert deps is not None
    # Cut test-snippets from the assignment itself, wherever else it appears.
    start = deps.start()
    end = text.index("\n\n", start)
    cut = text[start:end].replace("test-snippets ", "", 1)
    assert cut != text[start:end]
    (copy / "Makefile").write_text(
        text[:start] + cut + text[end:], encoding="utf-8"
    )
    errs = mod.check(copy)
    assert any("make test-snippets" in e for e in errs), errs
