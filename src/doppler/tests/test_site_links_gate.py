"""`check_site_links.py`: a built site's internal links, and an empty site.

The gate walks the built `site/` and fails on an internal link or `#anchor`
that resolves to nothing (`make docs-check` runs it after the build). On
2026-10-18 a local docs build rendered no page at all (theme assets and an
unrendered `sitemap.xml` template, in 0.2 s) and the gate printed "OK -- 0
pages": a check over nothing read as a pass. It now refuses a site with no
page to check, and this file pins that next to the two failures it exists
for, plus the clean case.
"""

from __future__ import annotations

import subprocess
import sys
from typing import TYPE_CHECKING

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)
SCRIPT = REPO / "scripts" / "check_site_links.py"


def _run(site: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(SCRIPT), "--site", str(site)],
        capture_output=True,
        text=True,
        cwd=REPO,
    )


def _page(site: Path, rel: str, body: str) -> None:
    path = site / rel
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(f"<html><body>{body}</body></html>", encoding="utf-8")


def test_a_site_with_no_page_is_refused(tmp_path: Path) -> None:
    """Assets and an unrendered template, as the empty build left: no page."""
    site = tmp_path / "site"
    (site / "assets").mkdir(parents=True)
    (site / "assets" / "main.css").write_text("", encoding="utf-8")
    (site / "sitemap.xml").write_text("{%- for page in pages -%}", "utf-8")
    run = _run(site)
    assert run.returncode == 1
    assert "no page to check" in run.stderr


def test_a_site_of_only_skipped_pages_is_refused(tmp_path: Path) -> None:
    """Pages the gate does not audit (404, c-api) are not pages checked."""
    site = tmp_path / "site"
    _page(site, "404.html", "not found")
    _page(site, "c-api/index.html", "generated")
    run = _run(site)
    assert run.returncode == 1
    assert "no page to check" in run.stderr


def test_good_links_pass(tmp_path: Path) -> None:
    site = tmp_path / "site"
    _page(site, "index.html", '<a href="guide/">guide</a>')
    _page(site, "guide/index.html", '<h2 id="x">x</h2><a href="#x">x</a>')
    run = _run(site)
    assert run.returncode == 0, run.stderr
    assert "OK — 2 pages" in run.stdout


def test_a_broken_link_fails(tmp_path: Path) -> None:
    site = tmp_path / "site"
    _page(site, "index.html", '<a href="missing/">gone</a>')
    run = _run(site)
    assert run.returncode == 1
    assert "broken link -> missing/" in run.stderr


def test_a_dead_anchor_fails(tmp_path: Path) -> None:
    site = tmp_path / "site"
    _page(site, "index.html", '<a href="other.html#nowhere">x</a>')
    _page(site, "other.html", '<h2 id="somewhere">y</h2>')
    run = _run(site)
    assert run.returncode == 1
    assert "dead anchor -> other.html#nowhere" in run.stderr


def test_a_cross_reference_rendered_as_text_fails(tmp_path: Path) -> None:
    """mdformat escapes `[x][doppler.mpsk]` to `\\[x\\][doppler.mpsk]` unless
    .mdformat.toml says not to, and it then renders as literal brackets: no
    link, no build warning (#3000)."""
    site = tmp_path / "site"
    _page(site, "index.html", "<p>[<code>mpsk</code>][doppler.mpsk] maps.</p>")
    run = _run(site)
    assert run.returncode == 1
    assert "cross-reference rendered as text -> [doppler.mpsk]" in run.stderr


def test_a_resolved_cross_reference_passes(tmp_path: Path) -> None:
    site = tmp_path / "site"
    _page(site, "ref.html", '<h2 id="doppler.mpsk">mpsk</h2>')
    _page(
        site,
        "index.html",
        '<p><a href="ref.html#doppler.mpsk"><code>mpsk</code></a> maps.</p>',
    )
    run = _run(site)
    assert run.returncode == 0, run.stderr


def test_the_syntax_shown_in_code_is_not_a_reference(tmp_path: Path) -> None:
    """A page that documents the convention shows it in a code span or a
    fence; the escaped form still fails, because its `][...]` sits outside
    the <code> it wraps."""
    site = tmp_path / "site"
    _page(
        site,
        "index.html",
        "<p>Write <code>[text][doppler.mod.Name]</code>.</p>"
        "<pre><code>[x][doppler.mpsk]</code></pre>",
    )
    assert _run(site).returncode == 0
    _page(site, "index.html", "<p>[<code>x</code>][doppler.mpsk]</p>")
    assert _run(site).returncode == 1
