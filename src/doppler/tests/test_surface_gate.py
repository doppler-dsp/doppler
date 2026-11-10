"""The built-reference gate goes red for each way a public name goes missing.

``scripts/check_surface.py`` reads every package's ``__all__`` and requires
each public name's anchor in the built page autonav made for it (#3000).
These cases run the real script on a synthetic tree and break one thing at a
time.
"""

from __future__ import annotations

import subprocess
import sys
from typing import TYPE_CHECKING

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)
SCRIPT = REPO / "scripts" / "check_surface.py"
LIST = "docs/.surface-unrendered"


def _w(root: Path, rel: str, text: str) -> None:
    p = root / rel
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(text, encoding="utf-8")


def _page(*names: str) -> str:
    return "".join(f'<h2 id="doppler.sig.{n}">{n}</h2>' for n in names)


def _fixture(root: Path) -> Path:
    """One package, two public names and a private one, all rendered."""
    _w(
        root,
        "src/doppler/sig/__init__.py",
        '__all__ = ["Widget", "make", "_hidden"]\n',
    )
    _w(root, "site/index.html", "<html></html>")
    _w(root, "site/reference/doppler/sig/index.html", _page("Widget", "make"))
    return root


def _run(root: Path, *extra: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(SCRIPT), "--root", str(root), *extra],
        capture_output=True,
        text=True,
        check=False,
    )


def _git(root: Path, *args: str) -> None:
    subprocess.run(
        ["git", "-c", "user.name=t", "-c", "user.email=t@t", *args],
        cwd=root,
        check=True,
        capture_output=True,
    )


def test_a_fully_rendered_package_passes(tmp_path: Path) -> None:
    r = _run(_fixture(tmp_path), "--no-ratchet")
    assert r.returncode == 0, r.stderr
    assert "2 of 2" in r.stdout


def test_a_name_missing_from_its_page_fails(tmp_path: Path) -> None:
    root = _fixture(tmp_path)
    _w(root, "site/reference/doppler/sig/index.html", _page("Widget"))
    r = _run(root, "--no-ratchet")
    assert r.returncode == 1
    assert "doppler.sig.make" in r.stderr


def test_a_package_with_no_page_fails_for_every_name(tmp_path: Path) -> None:
    root = _fixture(tmp_path)
    (root / "site/reference/doppler/sig/index.html").unlink()
    r = _run(root, "--no-ratchet")
    assert r.returncode == 1
    assert "doppler.sig.Widget" in r.stderr
    assert "doppler.sig.make" in r.stderr


def test_a_private_name_owes_no_entry(tmp_path: Path) -> None:
    r = _run(_fixture(tmp_path), "--no-ratchet")
    assert "_hidden" not in r.stdout + r.stderr


def test_a_computed_all_is_refused_not_skipped(tmp_path: Path) -> None:
    root = _fixture(tmp_path)
    _w(root, "src/doppler/sig/__init__.py", '__all__ = ["Widget"] + []\n')
    r = _run(root, "--no-ratchet")
    assert r.returncode == 1
    assert "not a literal list" in r.stderr


def test_a_listed_name_covers_it_and_a_rendered_one_is_stale(
    tmp_path: Path,
) -> None:
    root = _fixture(tmp_path)
    _w(root, "site/reference/doppler/sig/index.html", _page("Widget"))
    _w(root, LIST, "doppler.sig.make  #1 why\n")
    assert _run(root, "--no-ratchet").returncode == 0
    _w(root, "site/reference/doppler/sig/index.html", _page("Widget", "make"))
    r = _run(root, "--no-ratchet")
    assert r.returncode == 1
    assert "renders now" in r.stderr


def test_the_list_only_shrinks(tmp_path: Path) -> None:
    root = _fixture(tmp_path)
    _w(root, "site/reference/doppler/sig/index.html", _page("Widget"))
    _w(root, "scripts/check_surface.py", "# the gate\n")
    _w(root, LIST, "# none\n")
    _git(root, "init", "-q", "-b", "main")
    _git(root, "add", "-A")
    _git(root, "commit", "-qm", "base")
    _w(root, LIST, "doppler.sig.make  #1 why\n")
    r = _run(root, "--base", "main")
    assert r.returncode == 1
    assert "only shrinks" in r.stderr


def test_a_package_without_all_fails(tmp_path: Path) -> None:
    root = _fixture(tmp_path)
    _w(root, "src/doppler/app/__init__.py", '"""An app."""\n')
    r = _run(root, "--no-ratchet")
    assert r.returncode == 1
    assert "declares no __all__" in r.stderr


def test_an_empty_all_declares_no_api(tmp_path: Path) -> None:
    root = _fixture(tmp_path)
    _w(root, "src/doppler/app/__init__.py", "__all__: list[str] = []\n")
    _w(root, "src/doppler/app/helpers.py", '__all__ = ["helper"]\n')
    assert _run(root, "--no-ratchet").returncode == 0


def test_an_all_inside_a_block_is_refused(tmp_path: Path) -> None:
    root = _fixture(tmp_path)
    _w(
        root,
        "src/doppler/sig/__init__.py",
        'try:\n    __all__ = ["Widget", "make"]\nexcept ImportError:\n'
        "    __all__ = []\n",
    )
    r = _run(root, "--no-ratchet")
    assert r.returncode == 1
    assert "not one top-level assignment" in r.stderr


def test_a_submodule_export_its_package_drops_fails(tmp_path: Path) -> None:
    root = _fixture(tmp_path)
    _w(root, "src/doppler/sig/extra.py", '__all__ = ["make", "orphan"]\n')
    r = _run(root, "--no-ratchet")
    assert r.returncode == 1
    assert "exports orphan" in r.stderr
    assert "make," not in r.stderr  # re-exported, so not reported


def test_the_lint_half_needs_no_site(tmp_path: Path) -> None:
    root = _fixture(tmp_path)
    (root / "site/index.html").unlink()
    assert _run(root, "--no-ratchet").returncode == 1
    r = _run(root, "--no-ratchet", "--no-site")
    assert r.returncode == 0, r.stderr
