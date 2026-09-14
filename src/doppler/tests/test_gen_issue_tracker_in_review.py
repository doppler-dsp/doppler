"""Which issues the tracker marks "in review", driven over seeded PRs.

`scripts/gen_issue_tracker.py` used to decide this by scanning every open PR
body for `#N` tokens. Measured 2026-09-14: doppler#1337's body said it closes
#1115 and also named `just-buildit/just-makeit#1307`, an issue in another
repository -- and the regenerated tier map marked doppler#1307, an unrelated
test-codes issue, as in review in #1337. A body scan cannot tell "closes"
from "mentions", nor this repository from another.

The answer now comes from GitHub's own `closingIssuesReferences`. These cases
seed the shapes that fooled the scan, so they fail against it: sabotaging
`pr_closes` back to a body scan turns the first two red.
"""

from __future__ import annotations

import importlib.util

from doppler.tests._repo import repo_root

REPO = repo_root(__file__)
SCRIPT = REPO / "scripts" / "gen_issue_tracker.py"
SLUG = "doppler-dsp/doppler"


def _gen():
    """Import the generator as a module -- the code path `make issues` runs."""
    spec = importlib.util.spec_from_file_location("_issue_tracker", SCRIPT)
    assert spec and spec.loader
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def _ref(n: int, slug: str = SLUG) -> dict:
    return {"number": n, "url": f"https://github.com/{slug}/issues/{n}"}


def test_a_cross_repository_close_is_not_this_repositorys_issue() -> None:
    """The #1337 shape: a same-numbered issue in another repository."""
    prs = [
        {
            "number": 1337,
            "body": "Closes #1115. Filed just-buildit/just-makeit#1307.",
            "closingIssuesReferences": [
                _ref(1115),
                _ref(1307, "just-buildit/just-makeit"),
            ],
        }
    ]
    got = _gen().pr_closes(prs, SLUG)
    assert got == {1115: 1337}
    assert 1307 not in got


def test_a_mention_is_not_a_close() -> None:
    """A PR that names an issue without closing it leaves it open."""
    prs = [
        {
            "number": 40,
            "body": "Related to #12; see also #13.",
            "closingIssuesReferences": [],
        }
    ]
    assert _gen().pr_closes(prs, SLUG) == {}


def test_every_close_of_a_pr_is_attributed_to_it() -> None:
    """A PR closing several issues marks each; a PR with none marks none."""
    prs = [
        {"number": 50, "closingIssuesReferences": [_ref(1), _ref(2)]},
        {"number": 51, "closingIssuesReferences": None},
    ]
    assert _gen().pr_closes(prs, SLUG) == {1: 50, 2: 50}
