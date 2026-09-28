"""The assertion ratchet in `scripts/check_tests_ssot.py`, over a seeded repo.

The ratchet fails any `native/tests/*.c` that has fewer assertions than it
had at the merge base, unless `native/tests/.assertion-ratchet-ignore`
states why. An entry takes three forms, and each makes a different claim:

- `a.c  <reason>`: a REMOVAL, permanent. The file's ratchet is off.
- `a.c -> b.c  <reason>`: a MOVE, checked. `b.c` must gain what `a.c` lost.
- `a.c -> b.c removes=N  <reason>`: a MOVE in which N assertions pinned
  something the same change deleted. It excuses exactly N.

Each case builds a throwaway repo, commits the baseline, edits the working
tree and asks the gate. A case that can only go green would prove nothing,
so every form has a matching red case.
"""

from __future__ import annotations

import importlib.util
import subprocess
from typing import TYPE_CHECKING

import pytest

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path
    from types import ModuleType

SCRIPT = repo_root(__file__) / "scripts" / "check_tests_ssot.py"


def _load() -> ModuleType:
    spec = importlib.util.spec_from_file_location("check_tests_ssot", SCRIPT)
    assert spec is not None and spec.loader is not None
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def _body(n: int) -> str:
    """A C test file with exactly `n` assertions."""
    return "".join(f"  DP_CHECK (x == {i});\n" for i in range(n))


@pytest.fixture
def gate(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> tuple[Path, ModuleType]:
    """A repo whose baseline commit has a.c with 4 assertions, b.c with 0."""
    root = tmp_path / "repo"
    tests = root / "native" / "tests"
    tests.mkdir(parents=True)
    (tests / "a.c").write_text(_body(4), encoding="utf-8")
    (tests / "b.c").write_text(_body(0), encoding="utf-8")
    git = ["git", "-C", str(root)]
    subprocess.run(["git", "init", "-q", str(root)], check=True)
    subprocess.run([*git, "add", "-A"], check=True)
    subprocess.run(
        [
            *git,
            "-c",
            "user.name=t",
            "-c",
            "user.email=t@t",
            "commit",
            "-qm",
            "base",
        ],
        check=True,
    )
    mod = _load()
    monkeypatch.setattr(mod, "ROOT", root)
    monkeypatch.setattr(mod, "TESTS", tests)
    monkeypatch.setattr(mod, "IGNORE", tests / ".assertion-ratchet-ignore")
    return tests, mod


def _run(
    gate: tuple[Path, ModuleType], a: int, b: int, entry: str | None
) -> list[str]:
    """Leave a.c with `a` and b.c with `b` assertions, list `entry`, ask."""
    tests, mod = gate
    (tests / "a.c").write_text(_body(a), encoding="utf-8")
    (tests / "b.c").write_text(_body(b), encoding="utf-8")
    if entry is not None:
        (tests / ".assertion-ratchet-ignore").write_text(
            entry + "\n", encoding="utf-8"
        )
    result: list[str] = mod.ratchet("HEAD")
    return result


@pytest.mark.parametrize(
    ("a", "b", "entry", "red"),
    [
        # nothing lost, nothing to excuse
        (4, 0, None, False),
        # a silent loss is the thing the gate exists for
        (0, 0, None, True),
        # a stated removal excuses any drop, which is why it is rare
        (0, 0, "a.c  deleted with the feature", False),
        # a move is checked against what the destination gained
        (0, 4, "a.c -> b.c  split out", False),
        (0, 3, "a.c -> b.c  split out", True),
        # removes=N excuses exactly N ...
        (0, 3, "a.c -> b.c removes=1  one pinned a deleted function", False),
        # ... and no more: short by one beyond N is still red
        (0, 2, "a.c -> b.c removes=1  one pinned a deleted function", True),
        # removes=0 is the plain move
        (0, 3, "a.c -> b.c removes=0  split out", True),
    ],
)
def test_entry_forms(
    gate: tuple[Path, ModuleType],
    a: int,
    b: int,
    entry: str | None,
    red: bool,
) -> None:
    bad = _run(gate, a, b, entry)
    assert bool(bad) is red, bad


def test_removes_is_not_read_from_the_reason(
    gate: tuple[Path, ModuleType],
) -> None:
    """Existing entries write `-57:` in their reason to mean the size of a
    move. Only the `removes=` token may excuse assertions, so a reason that
    merely states a number must not."""
    bad = _run(gate, 0, 3, "a.c -> b.c  -1: one pinned a deleted function")
    assert bad and "went nowhere" in bad[0], bad
