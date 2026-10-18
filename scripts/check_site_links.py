#!/usr/bin/env python3
"""Check every internal link and anchor in the built docs site.

``zensical build --strict`` catches unresolved *markdown reference*
links, but a plain ``[text](broken/path.md)`` renders as a normal
``<a href>`` and ships silently — nothing in the toolchain verifies
that internal hrefs point at pages that exist, or that ``#fragment``
anchors point at real element ids. This gate walks the freshly built
``site/`` tree and checks exactly that, with no external dependencies
and no network: external (``http[s]://``, ``mailto:``) targets are out
of scope (flaky to check, not ours to fix).

What counts as a failure
------------------------
* an internal ``href``/``src`` whose resolved target file (or
  ``<dir>/index.html`` for pretty URLs) does not exist under ``site/``
* a same-page or cross-page ``#fragment`` that matches no ``id="..."``
  or ``name="..."`` in the target document
* a site with no page to check at all. A build that rendered nothing
  (2026-10-18: zensical copied its theme assets and an unrendered
  ``sitemap.xml`` template and wrote no page, locally, in 0.2 s) used to
  read "OK -- 0 pages" and pass. Zero pages checked is not a pass.

Run locally
-----------
    make docs && python scripts/check_site_links.py

``--site DIR`` checks another built tree (the tests point it at one).
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path
from urllib.parse import unquote, urlsplit

ROOT = Path(__file__).resolve().parent.parent
SITE = ROOT / "site"

_HREF = re.compile(r"""(?:href|src)=["'](?P<url>[^"']+)["']""")
_ID = re.compile(r"""(?:id|name)=["'](?P<id>[^"']+)["']""")

# Generated trees are not audited (but links *into* them from
# hand-written pages are): c-api is the separate prebuilt mkdocs build
# (392 generated pages), doxygen/ is raw Doxygen HTML whose graph PNGs
# depend on the local graphviz install.
_SKIP_PARTS = {"c-api", "doxygen"}


def _ids_of(path: Path, cache: dict[Path, set[str]]) -> set[str]:
    if path not in cache:
        try:
            text = path.read_text(encoding="utf-8", errors="replace")
        except OSError:
            cache[path] = set()
        else:
            cache[path] = set(_ID.findall(text))
    return cache[path]


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[1])
    ap.add_argument(
        "--site", type=Path, default=SITE, help="built site (default site/)"
    )
    site = ap.parse_args(argv).site
    if not site.is_dir():
        raise SystemExit(
            f"check_site_links: {site} not found -- run `make docs` first."
        )

    pages = [
        p
        for p in site.rglob("*.html")
        if not _SKIP_PARTS.intersection(p.relative_to(site).parts)
        # 404.html is rendered for an unknown URL depth, so the theme
        # emits deploy-prefix-absolute URLs (/doppler/...) that only
        # resolve on the live host -- not auditable from the file tree.
        and p.relative_to(site).as_posix() != "404.html"
    ]
    if not pages:
        print(
            f"check_site_links: {site} holds no page to check -- the docs "
            "build rendered nothing, and a check over zero pages proves "
            "nothing.",
            file=sys.stderr,
        )
        return 1
    id_cache: dict[Path, set[str]] = {}
    bad: list[str] = []

    for page in pages:
        text = page.read_text(encoding="utf-8", errors="replace")
        rel = page.relative_to(site)
        for m in _HREF.finditer(text):
            url = m.group("url")
            parts = urlsplit(url)
            if parts.scheme or url.startswith(("//", "data:", "{")):
                continue  # external / protocol-relative / templated
            path_part = unquote(parts.path)

            if not path_part:
                target = page  # pure-fragment link, same page
            else:
                if path_part.startswith("/"):
                    resolved = (site / path_part.lstrip("/")).resolve()
                else:
                    resolved = (page.parent / path_part).resolve()
                if resolved.is_dir():
                    resolved = resolved / "index.html"
                target = resolved
                if not target.exists():
                    bad.append(f"  {rel}: broken link -> {url}")
                    continue

            frag = unquote(parts.fragment)
            if (
                frag
                and target.suffix == ".html"
                and not _SKIP_PARTS.intersection(
                    target.resolve().relative_to(site.resolve()).parts
                    if target.resolve().is_relative_to(site.resolve())
                    else ()
                )
                and frag not in _ids_of(target, id_cache)
            ):
                bad.append(f"  {rel}: dead anchor -> {url}")

    if bad:
        print(
            f"check_site_links: {len(bad)} broken internal link(s) in "
            f"the built site:",
            file=sys.stderr,
        )
        print("\n".join(sorted(set(bad))), file=sys.stderr)
        return 1
    print(f"Site links: OK — {len(pages)} pages, no broken internal links")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
