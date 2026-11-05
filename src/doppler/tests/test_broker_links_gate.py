"""A page that needs a NATS broker links the page that says how to get one.

`scripts/check_broker_links.py` scans every page under docs/; one that names
`nats-server` or a `nats://` endpoint must link `install/broker.md`, unless
`docs/.broker-link-exempt` lists it with a reason (doppler#2857: no page said
how to run a broker, and the streaming examples failed with
`dp_sub_create failed on nats://127.0.0.1:4222/...`).
"""

from __future__ import annotations

import sys
from typing import TYPE_CHECKING

import pytest

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)


def _g():
    sys.path.insert(0, str(REPO / "scripts"))
    import check_broker_links as g

    return g


def _page(root: Path, rel: str, text: str) -> None:
    p = root / rel
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(text, encoding="utf-8")


def test_a_page_that_names_a_broker_without_linking_one_fails(
    tmp_path: Path,
) -> None:
    _page(tmp_path, "docs/guide/x.md", "run `nats-server -js` first\n")
    found = _g().problems(tmp_path)
    assert len(found) == 1 and "docs/guide/x.md:1" in found[0]


@pytest.mark.parametrize("link", ["../install/broker.md", "install/broker.md"])
def test_linking_the_broker_page_satisfies_it(
    tmp_path: Path, link: str
) -> None:
    _page(
        tmp_path, "docs/guide/x.md", f"needs [a broker]({link}) at nats://h\n"
    )
    assert _g().problems(tmp_path) == []


def test_an_exempt_page_may_name_a_broker(tmp_path: Path) -> None:
    _page(tmp_path, "docs/design/y.md", "nats://127.0.0.1:4222/x\n")
    _page(
        tmp_path,
        "docs/.broker-link-exempt",
        "docs/design/** a design record\n",
    )
    assert _g().problems(tmp_path) == []


def test_an_exemption_needs_a_reason(tmp_path: Path) -> None:
    _page(tmp_path, "docs/.broker-link-exempt", "docs/design/**\n")
    with pytest.raises(SystemExit, match="needs a reason"):
        _g().exempt(tmp_path)


def test_the_real_tree_passes() -> None:
    assert _g().problems(REPO) == []
