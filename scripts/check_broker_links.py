#!/usr/bin/env python3
"""Gate: a page that needs a NATS broker links how to get one.

doppler is a NATS *client*; nothing installs or starts a server. A reader who
follows the streaming quick start, ``wfmgen --output nats://…``, the specan
socket source or a streaming example hits ``dp_sub_create failed on
nats://127.0.0.1:4222/…`` with no page saying what to run (doppler#2857).
``docs/install/broker.md`` is that page, so any page that names ``nats-server``
or a ``nats://`` endpoint, or reaches a broker through ``Publisher(``,
``Subscriber(``, ``StreamSink(``, ``from doppler.stream import``,
``--source socket`` or
``dp_pub_create``/``dp_sub_create``, must link it. The link must RESOLVE to the
broker page; a page that only ``--8<--``-includes such text is held to it too.

Discovered, not registered: every page under ``docs/`` is scanned. A page that
is allowed to mention a broker without linking it (the design and API
reference pages, a page that bundles its own broker) is listed in
``docs/.broker-link-exempt`` as ``<glob> <why>``; the reason is mandatory.

Usage
-----
    python scripts/check_broker_links.py
"""

from __future__ import annotations

import posixpath
import re
import sys
from pathlib import Path

from _docs_pages import exempt_globs, is_exempt, pages

ROOT = Path(__file__).resolve().parent.parent
EXEMPT = "docs/.broker-link-exempt"
BROKER_PAGE = "docs/install/broker.md"

# What says a page needs a broker. The endpoint and the server are the obvious
# two; the rest are how a reader REACHES one without ever typing either: the
# Python classes, the module, the specan socket source and the `wfmgen` sink.
NEEDS = re.compile(
    r"nats-server|nats://|\bPublisher\(|\bSubscriber\(|\bStreamSink\(|"
    r"from doppler\.stream import|import doppler\.stream|--source socket|"
    r"dp_(?:pub|sub|push|pull)_create"
)
LINK = re.compile(r"\]\(([^)\s#]+\.md)(?:#[^)]*)?\)")
INCLUDE = re.compile(
    r'^[ \t]*--8<--[ \t]+"(?P<path>[^":]+)(?::(?P<sec>[^"]+))?"[ \t]*$', re.M
)


def exempt(root: Path) -> list[str]:
    return exempt_globs(root, EXEMPT)


def rendered(root: Path, page: Path) -> str:
    """The page's text with each ``--8<--`` include of a DOCUMENT (a ``.md``)
    replaced by what it names (one level, honoring ``path:section``), because a
    page that only includes a README or another page's instructions is just as
    much in need of the link. Included SOURCE (a tab of ``main.c``, a build
    script) is a listing, not an instruction, and is left out: a CMake comment
    that mentions ``nats://`` does not mean the reader must run a server."""

    def one(m: re.Match[str]) -> str:
        src = root / m["path"]
        if src.suffix != ".md" or not src.is_file():
            return m[0]
        text = src.read_text(encoding="utf-8")
        sec = m["sec"]
        if not sec:
            return text
        # `path:name` is the region between the file's own markers
        tag = re.escape(sec)
        start = re.search(rf"--8<--\s*\[start:{tag}\]", text)
        end = re.search(rf"--8<--\s*\[end:{tag}\]", text)
        return text[start.end() : end.start()] if start and end else ""

    return INCLUDE.sub(one, page.read_text(encoding="utf-8"))


SITE_PAGE = "https://doppler-dsp.github.io/doppler/install/broker/"


def links_broker_page(rel: str, text: str) -> bool:
    """Whether a link in the page RESOLVES to ``docs/install/broker.md``
    (relative to the page, not merely ending in ``broker.md``), or is the
    deployed page's absolute URL (what a README shared with GitHub uses)."""
    here = posixpath.dirname(rel)
    return SITE_PAGE in text or any(
        posixpath.normpath(posixpath.join(here, t)) == BROKER_PAGE
        for t in LINK.findall(text)
    )


def problems(root: Path = ROOT) -> list[str]:
    skip = exempt(root)
    out: list[str] = []
    for page in pages(root):
        rel = page.relative_to(root).as_posix()
        if is_exempt(rel, skip):
            continue
        text = rendered(root, page)
        if NEEDS.search(text) and not links_broker_page(rel, text):
            line = next(
                n
                for n, ln in enumerate(text.splitlines(), start=1)
                if NEEDS.search(ln)
            )
            out.append(
                f"{rel}:{line}: reaches a NATS broker but does not link "
                f"{BROKER_PAGE} (how to run one); add the link, or list the "
                f"page in {EXEMPT} with a reason"
            )
    return out


def main() -> int:
    found = problems()
    if found:
        print("check_broker_links: FAILED", file=sys.stderr)
        for line in found:
            print(f"  {line}", file=sys.stderr)
        return 1
    print("check_broker_links: OK -- every page that needs a broker links one")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
