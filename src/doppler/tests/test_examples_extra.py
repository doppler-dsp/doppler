"""A missing matplotlib or scipy is named, with the command that fixes it.

The example scripts draw with matplotlib and some design filters with scipy,
neither of which is a dependency of the library. A reader who installed the
wheel and ran `python -m doppler.examples.<name>` used to meet a bare
`ModuleNotFoundError: No module named 'matplotlib'` (doppler#2857). They now
meet `needs matplotlib: pip install 'doppler-dsp[examples]'`, and the extra
exists. Discovered over every script, so a new example cannot forget it.
"""

from __future__ import annotations

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
IMPORT = re.compile(
    r"^\s*(?:import (?:matplotlib|scipy)\b|from (?:matplotlib|scipy)\b)",
    re.MULTILINE,
)
EXTRA = "doppler-dsp[examples]"


def test_the_examples_were_found() -> None:
    assert len(EXAMPLES) > 50


@pytest.mark.parametrize("script", EXAMPLES, ids=lambda p: p.name)
def test_an_example_that_needs_an_extra_names_it(script) -> None:
    text = script.read_text(encoding="utf-8")
    if not IMPORT.search(text):
        pytest.skip("imports neither matplotlib nor scipy")
    assert EXTRA in text, (
        f"{script.name} imports matplotlib or scipy but never names "
        f"`pip install '{EXTRA}'`: wrap the import in the try/except the "
        "other examples use"
    )
    assert "except ImportError" in text


def test_the_examples_extra_ships_what_they_import() -> None:
    meta = tomllib.loads((REPO / "pyproject.toml").read_text(encoding="utf-8"))
    extra = meta["project"]["optional-dependencies"]["examples"]
    names = {re.split(r"[<>=!~ ;\[]", d, maxsplit=1)[0] for d in extra}
    assert {"matplotlib", "scipy"} <= names, extra
