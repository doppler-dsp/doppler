"""A page that runs an example needing matplotlib or scipy names the extra.

The wheel does not depend on matplotlib or scipy; `pip install
"doppler-dsp[examples]"` brings them. A reader who follows a *Reproduce*
block (`python -m doppler.examples.<name>`) with a fresh install meets the
script's own "needs matplotlib: pip install ..." message, which is correct but
late: the page should say it first (doppler#2857). Discovered over the docs: a
page holding such a run line, for an example whose code imports matplotlib or
scipy, must contain the extra's install command.
"""

from __future__ import annotations

import ast
import re

import pytest

from doppler.tests._repo import repo_root

REPO = repo_root(__file__)
RUN = re.compile(r"python3? (?:-u )?-m doppler\.examples\.(\w+)")
EXTRA = "doppler-dsp[examples]"
EXAMPLES = REPO / "src" / "doppler" / "examples"
PAGES = sorted(
    p
    for p in (REPO / "docs").rglob("*.md")
    if not {"c-api", "archive"} & set(p.relative_to(REPO).parts)
    and p.relative_to(REPO).parts[:2] != ("docs", "dev")
)


def _needs_extra(name: str) -> bool:
    src = EXAMPLES / f"{name}.py"
    # A run line naming a module that is not there is a broken page, not an
    # example that "needs nothing": fail it rather than skip it.
    assert src.exists(), (
        f"a page runs doppler.examples.{name}, which is absent"
    )
    for n in ast.walk(ast.parse(src.read_text(encoding="utf-8"))):
        mods = (
            [a.name for a in n.names]
            if isinstance(n, ast.Import)
            else [n.module or ""]
            if isinstance(n, ast.ImportFrom)
            else []
        )
        if any(m.split(".")[0] in ("matplotlib", "scipy") for m in mods):
            return True
    return False


def test_the_docs_were_found() -> None:
    assert len(PAGES) > 100


@pytest.mark.parametrize(
    "page", PAGES, ids=lambda p: p.relative_to(REPO).as_posix()
)
def test_a_page_that_runs_an_extra_example_names_the_extra(page) -> None:
    text = page.read_text(encoding="utf-8")
    needing = sorted({n for n in RUN.findall(text) if _needs_extra(n)})
    if not needing:
        pytest.skip("runs no example that needs matplotlib or scipy")
    assert EXTRA in text, (
        f"{page.relative_to(REPO)} runs {needing} but never says "
        f'`pip install "{EXTRA}"`: add it to the Reproduce block'
    )
