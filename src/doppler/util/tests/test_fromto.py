"""`fromto`: a script's default lands beside the script, not in the cwd.

The property that matters is the one a bare file name gets wrong: run from
anywhere, the default path starts at the CALLING file's directory. So the
caller here is a real file in one temporary directory, imported while the
working directory is another, and the path it gets back must be beside it.
"""

from __future__ import annotations

import doctest
import importlib.util
from typing import TYPE_CHECKING

import pytest

from doppler.util import fromto, paths

if TYPE_CHECKING:
    from pathlib import Path


def _caller(where: Path):  # type: ignore[no-untyped-def]
    """A module file in ``where`` whose ``default()`` calls ``fromto``."""
    src = where / "caller.py"
    src.write_text(
        "from doppler.util import fromto\n"
        "def default():\n"
        "    return fromto('fig.png')\n"
        "def pointed(out):\n"
        "    return fromto(out or 'fig.png')\n"
    )
    spec = importlib.util.spec_from_file_location("fromto_caller", src)
    assert spec is not None and spec.loader is not None
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def test_default_starts_at_the_calling_file_not_the_cwd(tmp_path, monkeypatch):
    script_dir = tmp_path / "script"
    run_dir = tmp_path / "run"
    script_dir.mkdir()
    run_dir.mkdir()
    caller = _caller(script_dir)
    monkeypatch.chdir(run_dir)
    assert caller.default() == script_dir.resolve() / "fig.png"


def test_a_pointed_path_is_used_as_given(tmp_path, monkeypatch):
    script_dir = tmp_path / "script"
    script_dir.mkdir()
    caller = _caller(script_dir)
    monkeypatch.chdir(tmp_path)
    target = (tmp_path / "elsewhere" / "out.png").resolve()
    assert caller.pointed(target) == target
    assert caller.pointed(None) == script_dir.resolve() / "fig.png"


@pytest.mark.parametrize(
    ("to", "want"),
    [
        ("a.png", "/runs/a.png"),
        ("sub/a.png", "/runs/sub/a.png"),
        ("/abs/a.png", "/abs/a.png"),
        (None, "/runs"),
    ],
)
def test_join_rules(to, want):
    assert fromto(to, here="/runs").as_posix() == want


def test_nothing_is_created(tmp_path):
    out = fromto("new/dir/a.png", here=tmp_path)
    assert out == tmp_path / "new" / "dir" / "a.png"
    assert not (tmp_path / "new").exists()


def test_no_caller_file_is_refused():
    with pytest.raises(ValueError, match="no __file__"):
        exec("fromto()", {"fromto": fromto})


def test_docstring_examples_run():
    result = doctest.testmod(paths)
    assert result.attempted > 0
    assert result.failed == 0
