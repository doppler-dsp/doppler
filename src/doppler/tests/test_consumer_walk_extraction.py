"""`make consumer-walk` runs the deb steps docs-walk.sh declares, not its own.

The extraction of the .deb block, the build tools, the consumer fetch and the
README's `make run` is declared ONCE, in `tests/install/docs-walk.sh`
(`plan deb`, held by test_docs_walk_plan.py). consumer-walk.sh used to restate
the same three anchors; this keeps it from growing them back, which is how a
fix to one copy leaves the other wrong (doppler#2879).
"""

from __future__ import annotations

from doppler.tests._repo import repo_root

REPO = repo_root(__file__)
WALK = (REPO / "tests" / "install" / "consumer-walk.sh").read_text(
    encoding="utf-8"
)


def test_the_deb_steps_come_from_docs_walks_plan() -> None:
    assert "docs-walk.sh" in WALK
    assert "plan deb" in WALK


def test_the_walk_does_not_extract_from_pages_itself() -> None:
    assert "tests/install/fence.sh" not in WALK
    assert 'fence "' not in WALK
