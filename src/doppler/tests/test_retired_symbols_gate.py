"""The removed-public-name gate, over seeded git repositories.

`scripts/check_retired_symbols.py` exists because #2886 removed every
`dp_corr_*` symbol without a `.retired-names` row, and a caller it missed
broke every Build job (doppler#2905). Each case seeds a repository with a
`main` holding a public-symbol list, a header and the retired-names list,
branches off it, changes what the case is about, and runs the gate with
``--base main``: a gate that can only be tested against the real tree
cannot be sabotaged.
"""

from __future__ import annotations

import subprocess
import sys
from typing import TYPE_CHECKING

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)
SCRIPT = REPO / "scripts" / "check_retired_symbols.py"

SYMBOLS = "cmake/public-symbols.txt"
HEADER = "native/inc/doppler/widget/widget_core.h"
KEPT = "native/inc/doppler/fir/fir_core.h"
RETIRED = "scripts/.retired-names"
LIST = "# generated\ndp_widget_create function\nfir_create function\n"


def _git(root: Path, *args: str) -> None:
    subprocess.run(
        ["git", *args], cwd=root, check=True, capture_output=True, text=True
    )


def _write(root: Path, rel: str, text: str) -> None:
    p = root / rel
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(text, encoding="utf-8")


def _seed(root: Path) -> Path:
    """A `main` with two public symbols, two headers and no rows."""
    _git(root, "init", "-q", "-b", "main")
    _git(root, "config", "user.email", "t@example.com")
    _git(root, "config", "user.name", "t")
    _write(root, SYMBOLS, LIST)
    _write(root, HEADER, "/* corr */\n")
    _write(root, KEPT, "/* fir */\n")
    _write(root, RETIRED, "# rows\n")
    _git(root, "add", "-A")
    _git(root, "commit", "-q", "-m", "base")
    _git(root, "checkout", "-q", "-b", "topic")
    return root


def _commit(root: Path) -> None:
    _git(root, "add", "-A")
    _git(root, "commit", "-q", "-m", "change")


def _run(root: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(SCRIPT), "--root", str(root), "--base", "main"],
        capture_output=True,
        text=True,
    )


def test_no_removal_passes(tmp_path: Path) -> None:
    root = _seed(tmp_path)
    _write(root, HEADER, "/* corr, edited */\n")
    _commit(root)
    r = _run(root)
    assert r.returncode == 0, r.stdout


def test_removed_symbol_without_a_row_fails(tmp_path: Path) -> None:
    root = _seed(tmp_path)
    _write(root, SYMBOLS, "# generated\nfir_create function\n")
    _commit(root)
    r = _run(root)
    assert r.returncode == 1, r.stdout
    assert "dp_widget_create" in r.stdout
    assert "fir_create" not in r.stdout.split("row:", 1)[1]


def test_removed_symbol_with_a_matching_row_passes(tmp_path: Path) -> None:
    root = _seed(tmp_path)
    _write(root, SYMBOLS, "# generated\nfir_create function\n")
    _write(root, RETIRED, "\\bdp_widget_[a-z]+\tdp_thing_*\n")
    _commit(root)
    r = _run(root)
    assert r.returncode == 0, r.stdout


def test_a_row_for_another_name_does_not_cover_it(tmp_path: Path) -> None:
    root = _seed(tmp_path)
    _write(root, SYMBOLS, "# generated\nfir_create function\n")
    _write(root, RETIRED, "\\bdp_gadget_[a-z]+\tdp_thing_*\n")
    _commit(root)
    assert _run(root).returncode == 1


def test_a_symbol_renamed_in_place_needs_a_row(tmp_path: Path) -> None:
    root = _seed(tmp_path)
    _write(root, SYMBOLS, LIST.replace("dp_widget_create", "dp_widget_new"))
    _commit(root)
    r = _run(root)
    assert r.returncode == 1, r.stdout
    assert "dp_widget_create" in r.stdout


def test_removed_header_without_a_row_fails(tmp_path: Path) -> None:
    root = _seed(tmp_path)
    (root / HEADER).unlink()
    _commit(root)
    r = _run(root)
    assert r.returncode == 1, r.stdout
    assert HEADER in r.stdout


def test_removed_header_with_a_matching_row_passes(tmp_path: Path) -> None:
    root = _seed(tmp_path)
    (root / HEADER).unlink()
    _write(root, RETIRED, "doppler/widget/widget_core\\.h\tdoppler/thing\n")
    _commit(root)
    assert _run(root).returncode == 0


def test_an_unresolvable_base_fails_closed(tmp_path: Path) -> None:
    root = _seed(tmp_path)
    r = subprocess.run(
        [sys.executable, str(SCRIPT), "--root", str(root), "--base", "nope"],
        capture_output=True,
        text=True,
    )
    assert r.returncode == 1, r.stdout


def test_a_name_a_private_header_still_declares_is_not_retired(
    tmp_path: Path,
) -> None:
    """Leaving the export list for a `*_private.h` is internal, not retired.

    The list leaves private headers out (#2930): a function an object's own
    sources still call, declared in its `*_private.h`, left the list without
    leaving the tree, and a row would forbid it.
    """
    root = _seed(tmp_path)
    _write(root, SYMBOLS, "# generated\nfir_create function\n")
    _write(
        root,
        "native/inc/doppler/widget/widget_private.h",
        "int dp_widget_create (void);\n",
    )
    _commit(root)
    r = _run(root)
    assert r.returncode == 0, r.stdout + r.stderr
