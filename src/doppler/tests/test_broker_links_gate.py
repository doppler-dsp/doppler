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


@pytest.mark.parametrize(
    "link", ["../install/broker.md", "../../docs/install/broker.md"]
)
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


@pytest.mark.parametrize(
    "usage",
    [
        'pub = Publisher("tcp://x", CF64)',
        "sub = Subscriber(url)",
        "with StreamSink(url) as s:",
        "from doppler.stream import Publisher",
        "doppler-specan --source socket --address x",
        "dp_pub_t *tx = dp_pub_create(url, CF64);",
    ],
)
def test_reaching_a_broker_without_typing_its_name_still_needs_the_link(
    tmp_path: Path, usage: str
) -> None:
    """The gate used to key on `nats-server`/`nats://` only, so a page that
    shows `Subscriber(url)` with the URL elsewhere failed open."""
    _page(tmp_path, "docs/guide/x.md", f"```\n{usage}\n```\n")
    assert len(_g().problems(tmp_path)) == 1


def test_a_link_must_resolve_to_the_broker_page(tmp_path: Path) -> None:
    _page(tmp_path, "docs/guide/x.md", "nats-server [b](../other/broker.md)\n")
    assert len(_g().problems(tmp_path)) == 1
    _page(
        tmp_path, "docs/guide/x.md", "nats-server [b](../install/broker.md)\n"
    )
    assert _g().problems(tmp_path) == []
    # the deployed page's absolute URL (what a shared README uses) counts
    _page(
        tmp_path,
        "docs/guide/x.md",
        "nats-server https://doppler-dsp.github.io/doppler/install/broker/\n",
    )
    assert _g().problems(tmp_path) == []


def test_an_included_document_counts_and_included_source_does_not(
    tmp_path: Path,
) -> None:
    _page(
        tmp_path,
        "r/README.md",
        "<!-- --8<-- [start:b] -->\nrun nats-server\n"
        "<!-- --8<-- [end:b] -->\nlater: Publisher(x)\n",
    )
    _page(tmp_path, "r/main.c", "/* talks to nats://127.0.0.1 */\n")
    _page(tmp_path, "docs/p.md", '--8<-- "r/README.md:b"\n')
    assert len(_g().problems(tmp_path)) == 1  # the SECTION names a server
    _page(tmp_path, "docs/p.md", '```c\n--8<-- "r/main.c"\n```\n')
    assert (
        _g().problems(tmp_path) == []
    )  # a source listing is not an instruction
    _page(tmp_path, "docs/p.md", '--8<-- "r/README.md:other"\n')
    assert _g().problems(tmp_path) == []  # outside the included region


def test_the_real_tree_passes() -> None:
    assert _g().problems(REPO) == []
