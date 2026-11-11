"""A missing matplotlib or scipy is named, with the command that fixes it.

The example scripts draw with matplotlib and some design filters with scipy,
neither of which is a dependency of the library. A reader who installed the
wheel and ran `python -m doppler.examples.<name>` used to meet a bare
`ModuleNotFoundError: No module named 'matplotlib'` (doppler#2857). They now
meet `needs matplotlib: pip install 'doppler-dsp[examples]'`, and the extra
exists. Discovered over every script, so a new example cannot forget it.
"""

from __future__ import annotations

import ast
import re
import sys

import pytest

from doppler.tests._repo import repo_root

if sys.version_info >= (3, 11):
    import tomllib
else:  # pragma: no cover
    import tomli as tomllib

REPO = repo_root(__file__)
EXAMPLES = sorted((REPO / "src" / "doppler" / "examples").glob("*.py"))
SCRIPTS = [p for p in EXAMPLES if not p.name.startswith("_")]


def test_the_examples_were_found() -> None:
    assert len(SCRIPTS) > 50
    assert (REPO / "src" / "doppler" / "examples" / "_extra.py").exists()


def _first_import_line(tree: ast.AST, package: str) -> int | None:
    lines = [
        n.lineno
        for n in ast.walk(tree)
        if (
            isinstance(n, ast.Import)
            and any(a.name.split(".")[0] == package for a in n.names)
        )
        or (
            isinstance(n, ast.ImportFrom)
            and (n.module or "").split(".")[0] == package
        )
    ]
    return min(lines) if lines else None


def _require_lines(tree: ast.AST, package: str) -> list[int]:
    return [
        n.lineno
        for n in ast.walk(tree)
        if isinstance(n, ast.Call)
        and isinstance(n.func, ast.Name)
        and n.func.id == "require"
        and n.args
        and isinstance(n.args[0], ast.Constant)
        and n.args[0].value == package
    ]


@pytest.mark.parametrize("script", SCRIPTS, ids=lambda p: p.name)
def test_an_example_calls_require_before_it_imports_an_extra(script) -> None:
    """Discovered over every script: the first import of matplotlib or scipy is
    preceded by `require("<package>")` from doppler.examples._extra."""
    tree = ast.parse(script.read_text(encoding="utf-8"))
    needed = [
        p for p in ("matplotlib", "scipy") if _first_import_line(tree, p)
    ]
    if not needed:
        pytest.skip("imports neither matplotlib nor scipy")
    assert "doppler.examples._extra" in script.read_text(encoding="utf-8")
    for package in needed:
        first = _first_import_line(tree, package)
        asks = _require_lines(tree, package)
        assert asks and min(asks) < first, (
            f"{script.name} imports {package} at line {first} without "
            f'calling require("{package}") first: a missing package would '
            "end in a bare ModuleNotFoundError"
        )


def test_require_names_the_extra_and_exits() -> None:
    sys.path.insert(0, str(REPO / "src"))
    from doppler.examples._extra import INSTALL, require

    require("math")
    with pytest.raises(SystemExit) as e:
        require("no_such_package_xyz")
    assert str(e.value) == f"needs no_such_package_xyz: {INSTALL}"
    assert "doppler-dsp[examples]" in INSTALL


def test_require_names_another_extra_when_told() -> None:
    """A CLI block example needs pydantic, which `[cli]` installs and
    `[examples]` does not; telling the reader `[examples]` would send them
    to the wrong extra (#3005)."""
    sys.path.insert(0, str(REPO / "src"))
    from doppler.examples._extra import require

    with pytest.raises(SystemExit) as e:
        require("no_such_package_xyz", extra="cli")
    assert str(e.value) == (
        "needs no_such_package_xyz: pip install 'doppler-dsp[cli]'"
    )


def test_the_examples_extra_ships_what_they_import() -> None:
    meta = tomllib.loads((REPO / "pyproject.toml").read_text(encoding="utf-8"))
    extra = meta["project"]["optional-dependencies"]["examples"]
    names = {re.split(r"[<>=!~ ;\[]", d, maxsplit=1)[0] for d in extra}
    assert {"matplotlib", "scipy"} <= names, extra
