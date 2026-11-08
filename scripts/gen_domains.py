#!/usr/bin/env python3
"""Generate the domain pages, the A-Z index and the Domains nav.

``docs/domains.toml`` is the ONE grouping the docs use: a domain is a Python
module with a task title (``docs/domains.toml`` says why, and what each key
means). Every guide, gallery and design page names its domain on its first
line (``<!-- domain: <module> -->``). From those two facts and the public
symbols ``_pydocs.discover()`` finds, this script writes:

``docs/domains/index.md``
    The Domains index: the domains by area, each with its one-line summary.
``docs/domains/<module>.md``
    One page per domain: every public class and function with its API entry,
    guides, gallery pages, examples and design pages; the pages filed under
    the domain; and the pages of OTHER domains that mention its symbols, which
    is where a class used for another task (``Correlator`` is a spectral
    map and an acquisition search that lives in ``correlation``) is found
    from both sides without being re-filed.
``docs/api/a-z.md``
    Every public class, function and constant, alphabetical, each row
    linking its domain, API entry, guide, gallery and example.
the Domains, Guides and Gallery entries of ``mkdocs.yml``
    Between ``# domains-nav:start`` and ``# domains-nav:end``: each page
    listed once, under its domain, domains in ``domains.toml`` order. The
    pages of a sub-section (a directory below the family with its own
    ``index.md``: wfmgen, wfm-io) are left out; they are nav'd under Tools.
the Design entries of ``mkdocs.yml``
    Between ``# design-nav:start`` and ``# design-nav:end``.
``docs/{guide,gallery,design}/index.md``
    The list between ``<!-- domain-index:start -->`` and ``:end``, the same
    grouping: a domain heading, its summary, then its pages.

It reads the same symbol scan ``gen_related_pages.py`` and
``check_user_docs.py`` use, so the three cannot disagree on what a mention,
an API entry or an example is.

``problems()`` is the rule set ``check_user_docs.py`` enforces on ``make
lint``: a module missing from ``domains.toml`` (or listed that does not
exist), a title that does not contain its module's name or a declared
synonym, an area that is not one of ``AREAS``, and a guide, gallery or design
page with no domain or an unknown one.

Usage
-----
    python scripts/gen_domains.py --write   # regenerate every output
    python scripts/gen_domains.py --check   # exit 1 on any drift
"""

from __future__ import annotations

import functools
import os
import re
import sys
from collections import defaultdict
from pathlib import Path

from _pydocs import discover, kinds
from _release_tag import reachable
from _release_tag import version as _version

if sys.version_info >= (3, 11):
    import tomllib
else:  # pragma: no cover
    import tomli as tomllib

ROOT = Path(__file__).resolve().parent.parent
DOMAINS_TOML = "docs/domains.toml"
OUT_DIR = "docs/domains"
AZ = "docs/api/a-z.md"
MKDOCS = "mkdocs.yml"
EXAMPLES = "src/doppler/examples"
# Example links are pinned to the RELEASE (doppler#2857): a reader of the
# deployed page installed a release, not main. gen_doc_versions re-stamps the
# same form on every release, so the two always agree.
#
# An example added since the last release is NOT in that tag, so a link
# would 404 until the next one, and the repository-link gate rightly refuses
# it. The example is named without a link until it ships: the reader's
# installed wheel does not have it yet either. `_release_tag.reachable`
# decides, by asking the tag, so this generator and the gate cannot disagree.
REPO_BLOB_BASE = "https://github.com/doppler-dsp/doppler/blob/v"


@functools.lru_cache(maxsize=1)
def _in_release():
    return reachable(ROOT, bool(os.environ.get("CI")))


def _example_link(name: str) -> str | None:
    """The pinned URL of an example, or None while it is not in the tag."""
    path = f"{EXAMPLES}/{name}"
    if not _in_release()(path):
        return None
    return f"{REPO_BLOB_BASE}{_version(ROOT)}/{path}"


# api-taxonomy.md section 6's top-level areas, in display order. A domain's
# `area` must be one of these, so a typo cannot create a new one.
AREAS = {
    "sources": "Sources",
    "impairment": "Impairment",
    "filtering": "Filtering",
    "rateconversion": "Rate conversion",
    "downconversion": "Down-conversion",
    "correlation": "Correlation",
    "spectral": "Spectral",
    "synchronization": "Synchronization",
    "receiver": "Receivers",
    "modulation": "Modulation",
    "coding": "Coding",
    "estimation": "Estimation",
    "io": "I/O",
    "numeric": "Numeric",
}

# Page trees whose every page (bar its index and archive/) owes a domain.
PAGE_DIRS = ("guide", "gallery", "design")
FAMILY = {"guide": "Guide", "gallery": "Gallery", "design": "Design"}

# The domain is an HTML comment on the page's first line, not YAML front
# matter: the markdown formatter (mdformat) has no front-matter plugin and
# would rewrite a `---` block into a rule and a heading.
DOMAIN_RE = re.compile(
    r"\A<!--\s*domain:\s*([^\s;]+?)\s*(?:;\s*summary:\s*(.+?))?\s*-->"
)
TITLE_RE = re.compile(r"^#\s+(.+?)\s*$", re.MULTILINE)

EXEMPT = "docs/.domain-guides-exempt"
SELF = "scripts/gen_domains.py"
# A summary is one index line, not a paragraph: today's longest is 275.
SUMMARY_MAX = 300

# Two generated runs of mkdocs.yml: the browse sections (Domains, Guides,
# Gallery) and the Design group, which sits under Internals.
NAV_BROWSE = "domains-nav"
NAV_DESIGN = "design-nav"
# The generated list inside a section index.md.
IDX_START = "<!-- domain-index:start -->"
IDX_END = "<!-- domain-index:end -->"
INDEXES = {
    "guide": "docs/guide/index.md",
    "gallery": "docs/gallery/index.md",
    "design": "docs/design/index.md",
}
NOTE = (
    "<!-- GENERATED by scripts/gen_domains.py from docs/domains.toml and "
    "each page's domain line (the first line of a guide, gallery or design "
    "page). Do not edit; run `make docs-relink`. -->"
)


def load(root: Path = ROOT) -> dict[str, dict]:
    """``{module: {title, area, summary, synonyms}}`` in file order."""
    with open(root / DOMAINS_TOML, "rb") as fh:
        data = tomllib.load(fh)
    return dict(data.get("domain", {}))


def modules(root: Path = ROOT) -> dict[str, list[str]]:
    """``{top-level module: [symbols]}``; ``wfm.compose`` joins ``wfm``."""
    out: dict[str, list[str]] = defaultdict(list)
    for dotted, syms in discover(str(root / "src" / "doppler")):
        top = dotted.split(".")[1]
        for s in syms:
            if s not in out[top]:
                out[top].append(s)
    return dict(out)


def page_domain(path: Path) -> str | None:
    """The domain a page's first-line comment names, or None."""
    m = DOMAIN_RE.match(path.read_text(encoding="utf-8"))
    return m[1] if m else None


def page_summary(path: Path) -> str:
    """The one line a page declares for itself in its first-line marker
    (``<!-- domain: spectral; summary: ... -->``), or ""."""
    m = DOMAIN_RE.match(path.read_text(encoding="utf-8"))
    return (m[2] or "") if m else ""


def page_title(path: Path) -> str:
    """The first ``# `` heading, or the file name."""
    text = DOMAIN_RE.sub("", path.read_text(encoding="utf-8"), count=1)
    m = TITLE_RE.search(text)
    return m[1].replace("`", "") if m else path.name


def pages(root: Path = ROOT) -> list[tuple[str, Path]]:
    """``[(family dir, path)]`` for every page that owes a domain."""
    out: list[tuple[str, Path]] = []
    for fam in PAGE_DIRS:
        base = root / "docs" / fam
        for dirpath, dirnames, filenames in os.walk(base):
            dirnames[:] = sorted(d for d in dirnames if d != "archive")
            for fn in sorted(filenames):
                if fn.endswith(".md") and (
                    fn != "index.md" or Path(dirpath) != base
                ):
                    out.append((fam, Path(dirpath) / fn))
    return out


def _marker_problems(rel: str, path: Path) -> list[str]:
    """What is wrong with the first-line marker's SHAPE, apart from which
    domain it names: it must be one line that ends at its own ``-->``, and
    a summary must not carry a heading, a newline or a terminator, because
    the index writes it into a list line (a ``##`` there swallowed the next
    section's heading on nine pages) and may not run past SUMMARY_MAX."""
    first = path.read_text(encoding="utf-8").split("\n", 1)[0]
    if "domain:" not in first or not first.startswith("<!--"):
        return []
    if not first.rstrip().endswith("-->") or first.count("-->") != 1:
        return [
            f"{rel}: the domain marker must be ONE line ending at its "
            "only `-->` (no newline or `-->` inside the summary)"
        ]
    summary = page_summary(path)
    if "##" in summary:
        return [f"{rel}: the summary holds `##`; it is one index line"]
    if len(summary) > SUMMARY_MAX:
        return [
            f"{rel}: the summary is {len(summary)} characters; "
            f"keep it to {SUMMARY_MAX}"
        ]
    return []


def problems(root: Path = ROOT) -> list[str]:
    """Every domain finding, as one line each; empty when the tree is sound."""
    out: list[str] = []
    try:
        doms = load(root)
    except (OSError, tomllib.TOMLDecodeError) as e:
        return [f"{DOMAINS_TOML}: unreadable: {e}"]
    mods = modules(root)
    for m in sorted(set(mods) - set(doms)):
        out.append(
            f"{DOMAINS_TOML}: module `{m}` has no [domain.{m}] table; add "
            "its title, area and summary"
        )
    for m in sorted(set(doms) - set(mods)):
        out.append(
            f"{DOMAINS_TOML}: [domain.{m}] names no public module; "
            "delete it or fix the name"
        )
    for m, d in doms.items():
        missing = [k for k in ("title", "area", "summary") if not d.get(k)]
        if missing:
            out.append(f"{DOMAINS_TOML}: [domain.{m}] lacks {missing}")
            continue
        if d["area"] not in AREAS:
            out.append(
                f"{DOMAINS_TOML}: [domain.{m}] area {d['area']!r} is not "
                f"one of {sorted(AREAS)}"
            )
        words = [m, *d.get("synonyms", [])]
        if not any(w.lower() in d["title"].lower() for w in words):
            out.append(
                f"{DOMAINS_TOML}: [domain.{m}] title {d['title']!r} names "
                f"neither the module nor a synonym ({words}); a title that "
                "drifts from its module is a finding, not a mapping"
            )
    for _fam, path in pages(root):
        rel = path.relative_to(root).as_posix()
        out += _marker_problems(rel, path)
        dom = page_domain(path)
        if dom is None:
            out.append(
                f"{rel}: no domain; make its first line "
                "`<!-- domain: <module> -->` (a module in docs/domains.toml)"
            )
        elif dom not in doms:
            out.append(
                f"{rel}: domain `{dom}` is not a module in {DOMAINS_TOML}"
            )
        elif not page_summary(path) and not in_subsection(root, path):
            out.append(
                f"{rel}: no summary; its index line is written from the "
                f"page: `<!-- domain: {dom}; summary: <one line> -->`"
            )
    return out


# ------------------------------------------------------------------- the gate


def guide_gaps(root: Path = ROOT) -> set[str]:
    """Modules with at least one public class and no guide page filed under
    them: a domain a reader cannot learn to use."""
    import check_user_docs as cud

    classes = {k.split(".")[1] for k in cud.public_classes(root)}
    with_guide = {page_domain(p) for fam, p in pages(root) if fam == "guide"}
    return {m for m in classes if m not in with_guide}


def _exempt_entries(text: str) -> list[tuple[str, str]]:
    """``[(module, why)]`` from ``<module> guide: <why>`` lines."""
    out: list[tuple[str, str]] = []
    for line in text.splitlines():
        s = line.strip()
        if not s or s.startswith("#"):
            continue
        m = re.match(r"^(\w+)\s+guide:\s*(\S.*)$", s)
        out.append((m[1], m[2]) if m else (s, ""))
    return out


def gate(root: Path, base: str, ratchet: bool) -> list[str]:
    """Every domain finding ``check_user_docs.py`` reports: the tree's
    soundness, a module with classes but no guide that is not on the
    shrink-only list, and a stale or grown list."""
    from _gitbase import BaseUnreadableError, added_since_base, in_git_repo

    errors = problems(root)
    ex = root / EXEMPT
    entries = (
        _exempt_entries(ex.read_text(encoding="utf-8")) if ex.exists() else []
    )
    listed: set[str] = set()
    for mod, why in entries:
        if not why:
            errors.append(f"{EXEMPT}: malformed line: {mod}")
        else:
            listed.add(mod)
    gaps = guide_gaps(root)
    for m in sorted(gaps - listed):
        errors.append(
            f"doppler.{m}: has classes but no guide page filed under it -- "
            f"write one (first line `<!-- domain: {m} -->`)"
        )
    for m in sorted(listed - gaps):
        errors.append(
            f"{EXEMPT}: '{m} guide' is stale (a guide exists, or the module "
            "is gone); delete the line"
        )
    if ratchet and in_git_repo(root):
        try:
            grown = added_since_base(
                root,
                base,
                EXEMPT,
                sorted(listed),
                lambda t: {m for m, why in _exempt_entries(t) if why},
                since=SELF,
            )
        except BaseUnreadableError:
            errors.append(
                f"cannot read {EXEMPT} at {base}; the ratchet is unchecked"
            )
            grown = []
        for m in grown:
            errors.append(
                f"{EXEMPT}: '{m} guide' is new; the list only shrinks. "
                f"Write the guide instead (`<!-- domain: {m} -->`)"
            )
    return errors


# ---------------------------------------------------------------- rendering


def _scan(root: Path):
    """The symbol -> pages map and the example usage, from the same
    scanners ``gen_related_pages`` and ``check_user_docs`` use."""
    import check_user_docs as cud
    import gen_related_pages as grp

    corpus = grp._scan_corpus()
    api_dir = root / "docs" / "api"
    api_page: dict[str, str] = {}
    for p in sorted(api_dir.glob("*.md")):
        text = p.read_text(encoding="utf-8")
        for dotted in grp.DIRECTIVE_RE.findall(text) + sorted(
            cud._directive_members(text)
        ):
            api_page.setdefault(dotted.rsplit(".", 1)[-1], p.name)
        for h in grp.HEADING_RE.findall(text):
            api_page.setdefault(h, p.name)
    examples = cud.example_names(root)
    kinds_of = {
        s: k
        for ks in kinds(str(root / "src" / "doppler")).values()
        for s, k in ks.items()
    }
    return corpus, api_page, examples, kinds_of


LINK_RE = re.compile(r"\]\(([^)#\s]+\.md)(?:#[^)\s]*)?\)")


def _page_links(
    root: Path, page_dom: dict[str, str | None]
) -> list[tuple[str, str]]:
    """Every ``(page, page)`` link between two domain pages, sorted.

    Targets are resolved against the linking page's directory; a link to
    anything that is not a guide, gallery or design page is dropped.
    """
    out: set[tuple[str, str]] = set()
    for src in page_dom:
        text = (root / "docs" / src).read_text(encoding="utf-8")
        for target in LINK_RE.findall(text):
            dst = os.path.normpath(
                os.path.join(os.path.dirname(src), target)
            ).replace(os.sep, "/")
            if dst in page_dom and dst != src:
                out.add((src, dst))
    return sorted(out)


def _links(items: list[tuple[str, str | None]], cap: int = 3) -> str:
    """A table cell: up to ``cap`` links, then a count. An item with no URL
    is named in plain text (an example not in the release yet)."""
    if not items:
        return "&nbsp;"
    shown = [f"[{t}]({u})" if u else t for t, u in items[:cap]]
    if len(items) > cap:
        shown.append(f"+{len(items) - cap} more")
    return "<br>".join(shown)


def _symbol_row(
    sym: str,
    kind: str,
    domain_of: dict[str, str],
    page_dom: dict[str, str | None],
    corpus,
    api_page,
    examples,
    rel: str,
    dom: str | None = None,
) -> str:
    """One table row. ``rel`` is the prefix from the page to ``docs/``."""
    hits = corpus.get(sym, [])
    by_fam: dict[str, list[tuple[str, str]]] = defaultdict(list)
    own = domain_of.get(sym)
    for fam, rel_from_api, title in sorted(hits):
        # corpus paths are relative to docs/api/ -- re-root them on docs/
        target = os.path.normpath(os.path.join("api", rel_from_api)).replace(
            os.sep, "/"
        )
        if target.endswith("/index.md"):
            continue  # a section's own index names everything; no signal
        mine = page_dom.get(target) == own
        by_fam[fam].append(((not mine, title), title, rel + target))
    for fam, items in by_fam.items():
        by_fam[fam] = [(t, u) for _k, t, u in sorted(items)]
    api = [(sym, f"{rel}api/{api_page[sym]}")] if sym in api_page else []
    ex = [
        (n.removesuffix(".py"), _example_link(n))
        for n, used in sorted(examples.items())
        if sym in used
    ]
    cells = [
        f"`{sym}`" if dom is None else f"[`{sym}`]({rel}domains/{dom}.md)",
        kind,
        _links(api),
        _links(by_fam["Guides"]),
        _links(by_fam["Gallery"]),
        _links(ex),
        _links(by_fam["Design"]),
    ]
    return "| " + " | ".join(cells) + " |"


HEADER = (
    "| Name | Kind | API | Guide | Gallery | Example | Design |\n"
    "| ---- | ---- | --- | ----- | ------- | ------- | ------ |"
)


def render(root: Path = ROOT) -> dict[str, str]:
    """``{repo-relative path: text}`` for every generated file."""
    doms = load(root)
    mods = modules(root)
    corpus, api_page, examples, kinds_of = _scan(root)
    page_dom = {
        path.relative_to(root / "docs").as_posix(): page_domain(path)
        for _fam, path in pages(root)
    }
    titles = {
        path.relative_to(root / "docs").as_posix(): page_title(path)
        for _fam, path in pages(root)
    }
    sym_dom = {s: m for m, ss in mods.items() for s in ss}
    page_links = _page_links(root, page_dom)
    out: dict[str, str] = {}

    for m, d in doms.items():
        if m not in mods:
            continue
        syms = sorted(mods[m], key=str.lower)
        lines = [
            NOTE,
            "",
            f"# {d['title']}",
            "",
            d["summary"],
            "",
            f"Python module `doppler.{m}`. Area: "
            f"[{AREAS[d['area']]}](index.md#{d['area']}).",
            "",
        ]
        lines += ["## Classes and functions", "", HEADER]
        for s in syms:
            kind = kinds_of[s]
            lines.append(
                _symbol_row(
                    s,
                    kind,
                    sym_dom,
                    page_dom,
                    corpus,
                    api_page,
                    examples,
                    "../",
                )
            )
        lines.append("")
        mine: dict[str, list[tuple[str, str]]] = defaultdict(list)
        for rel, dom in sorted(page_dom.items()):
            if dom == m:
                mine[rel.split("/", 1)[0]].append((titles[rel], rel))
        for fam in PAGE_DIRS:
            if mine.get(fam):
                lines += [f"## {FAMILY[fam]} pages in this domain", ""]
                for t, rel in sorted(mine[fam], key=lambda x: x[0]):
                    blurb = page_summary(root / "docs" / rel)
                    lines.append(
                        f"- [{t}](../{rel})" + (f" — {blurb}" if blurb else "")
                    )
                lines.append("")
        # pages of OTHER domains that mention this module's symbols
        elsewhere: dict[str, dict[str, str]] = defaultdict(dict)
        for s in syms:
            for fam, rel_from_api, title in corpus.get(s, []):
                if fam not in ("Guides", "Gallery", "Design"):
                    continue
                if rel_from_api.endswith("/index.md"):
                    continue
                target = os.path.normpath(
                    os.path.join("api", rel_from_api)
                ).replace(os.sep, "/")
                other = page_dom.get(target)
                if other and other != m:
                    elsewhere[other][target] = title
        # ... and pages of other domains LINKED to or from a page filed
        # here (the acquisition guide links the delay-doppler guide, which
        # is filed under `spectral`, so `acquire` lists it too)
        for a, b in page_links:
            for here, there in ((a, b), (b, a)):
                if page_dom.get(here) == m and page_dom.get(there) not in (
                    None,
                    m,
                ):
                    elsewhere[page_dom[there]][there] = titles[there]
        if elsewhere:
            lines += [
                "## Used in other domains",
                "",
                "Pages filed under another domain that name a symbol of "
                "this module, or link to or from a page filed here: a class "
                "used for another task is found from both sides, never "
                "re-filed.",
                "",
            ]
            for other in sorted(elsewhere, key=lambda o: list(doms).index(o)):
                ot = doms[other]["title"]
                links = ", ".join(
                    f"[{t}](../{p})"
                    for p, t in sorted(
                        elsewhere[other].items(), key=lambda x: x[1]
                    )
                )
                lines.append(f"- [{ot}]({other}.md): {links}")
            lines.append("")
        out[f"{OUT_DIR}/{m}.md"] = "\n".join(lines).rstrip("\n") + "\n"

    # the Domains index
    idx = [
        NOTE,
        "",
        "# Domains",
        "",
        "Every guide, gallery and design page is filed under one domain: the "
        "Python module that holds the class, with a title for the task. "
        "Find the task, open its domain, and the page lists its classes and "
        "functions with their API entries and examples, and the pages filed "
        "there. The full alphabetical list is the "
        "[A-Z index](../api/a-z.md).",
        "",
    ]
    for area, label in AREAS.items():
        members = [(m, d) for m, d in doms.items() if d["area"] == area]
        if not members:
            continue
        idx += [f'<a id="{area}"></a>', "", f"## {label}", ""]
        idx += [
            "| Domain | Module | What it holds |",
            "| ------ | ------ | ------------- |",
        ]
        for m, d in members:
            idx.append(
                f"| [{d['title']}]({m}.md) | `doppler.{m}` | {d['summary']} |"
            )
        idx.append("")
    out[f"{OUT_DIR}/index.md"] = "\n".join(idx).rstrip("\n") + "\n"

    # the A-Z index
    az = [
        NOTE,
        "",
        "# A-Z index",
        "",
        "Every public class, function and constant, alphabetically. Each "
        "row links its "
        "domain, its API entry, and the guides, gallery pages and examples "
        "that use it.",
        "",
    ]
    names = sorted(sym_dom, key=lambda s: (s[0].upper(), s.lower()))
    letter = ""
    for s in names:
        if s[0].upper() != letter:
            letter = s[0].upper()
            az += ["", f"## {letter}", "", HEADER]
        kind = kinds_of[s]
        az.append(
            _symbol_row(
                s,
                kind,
                sym_dom,
                page_dom,
                corpus,
                api_page,
                examples,
                "../",
                dom=sym_dom[s],
            )
        )
    out[AZ] = "\n".join(az).rstrip("\n") + "\n"
    return out


def _nav_label(text: str) -> str:
    return '"' + text.replace("\\", "\\\\").replace('"', '\\"') + '"'


def in_subsection(root: Path, path: Path) -> bool:
    """True for a page in a directory below its family that has its own
    ``index.md``: a sub-section (wfmgen, wfm-io) the nav shows under Tools.
    The structure says it, so no list does."""
    family_dir = root / "docs" / path.relative_to(root / "docs").parts[0]
    return path.parent != family_dir and (path.parent / "index.md").exists()


def _by_domain(
    root: Path, fam: str, skip_sub: bool = False
) -> list[tuple[str, list[tuple[str, str]]]]:
    """``[(domain, [(title, path under docs/)])]`` for one page family.

    Domains come in ``domains.toml`` order; a domain with no page of this
    family is left out. Pages inside a domain sort by title.
    """
    doms = load(root)
    found: dict[str, list[tuple[str, str]]] = defaultdict(list)
    for f, path in pages(root):
        rel = path.relative_to(root / "docs").as_posix()
        dom = page_domain(path)
        if f != fam or dom not in doms:
            continue
        if skip_sub and in_subsection(root, path):
            continue
        found[dom].append((page_title(path), rel))
    return [
        (m, sorted(found[m], key=lambda t: (t[0].casefold(), t[1])))
        for m in doms
        if m in found
    ]


def nav_blocks(root: Path = ROOT) -> dict[str, str]:
    """The two marked runs of ``mkdocs.yml``, by marker name."""
    doms = load(root)
    mods = modules(root)
    lines = [
        "  - Domains:",
        "      - Domains: domains/index.md",
    ]
    for area, label in AREAS.items():
        members = [
            (m, d) for m, d in doms.items() if d["area"] == area and m in mods
        ]
        if not members:
            continue
        lines.append(f"      - {label}:")
        for m, d in members:
            lines.append(
                f"          - {_nav_label(d['title'])}: domains/{m}.md"
            )
    for fam, head, skip in (
        ("guide", "Guides", True),
        ("gallery", "Gallery", False),
    ):
        lines += [f"  - {head}:", f"      - {head}: {fam}/index.md"]
        for m, items in _by_domain(root, fam, skip):
            lines.append(f"      - {_nav_label(doms[m]['title'])}:")
            for title, rel in items:
                lines.append(f"          - {_nav_label(title)}: {rel}")
    design = ["      - Design:", "          - Design: design/index.md"]
    for m, items in _by_domain(root, "design"):
        design.append(f"          - {_nav_label(doms[m]['title'])}:")
        for title, rel in items:
            design.append(f"              - {_nav_label(title)}: {rel}")
    return {
        name: "\n".join([f"# {name}:start", *body, f"# {name}:end"]) + "\n"
        for name, body in ((NAV_BROWSE, lines), (NAV_DESIGN, design))
    }


def apply_nav(text: str, name: str, block: str) -> str:
    """``mkdocs.yml`` text with the ``name`` run replaced by ``block``."""
    start, end = f"# {name}:start", f"# {name}:end"
    pat = re.compile(
        r"^" + re.escape(start) + r"\n.*?^" + re.escape(end) + r"\n",
        re.DOTALL | re.MULTILINE,
    )
    if len(pat.findall(text)) != 1:
        raise SystemExit(
            f"{MKDOCS}: want exactly one {start} / {end} marker pair"
        )
    return pat.sub(lambda _m: block, text, count=1)


def index_blocks(root: Path = ROOT) -> dict[str, str]:
    """``{section index path: text}`` with its generated list filled in."""
    doms = load(root)
    out: dict[str, str] = {}
    for fam, rel in INDEXES.items():
        lines = [IDX_START, ""]
        for m, items in _by_domain(root, fam, fam == "guide"):
            lines += [f"## {doms[m]['title']}", "", doms[m]["summary"], ""]
            for title, page in items:
                link = os.path.relpath(page, fam).replace(os.sep, "/")
                blurb = page_summary(root / "docs" / page)
                lines.append(
                    f"- [{title}]({link})" + (f" — {blurb}" if blurb else "")
                )
            lines.append("")
        lines.append(IDX_END)
        text = (root / rel).read_text(encoding="utf-8")
        pat = re.compile(
            re.escape(IDX_START) + r".*?" + re.escape(IDX_END), re.DOTALL
        )
        if len(pat.findall(text)) != 1:
            raise SystemExit(
                f"{rel}: want exactly one {IDX_START} / {IDX_END} pair"
            )
        block = "\n".join(lines)
        out[rel] = pat.sub(lambda _m, b=block: b, text, count=1)
    return out


def main(argv: list[str] | None = None) -> int:
    argv = sys.argv[1:] if argv is None else argv
    if "--write" not in argv and "--check" not in argv:
        print(__doc__)
        return 1
    check = "--check" in argv
    found = problems()
    if found:
        print("gen_domains: the domain tree is unsound:", file=sys.stderr)
        for line in found:
            print(f"  {line}", file=sys.stderr)
        return 1
    want = render()
    nav_path = ROOT / MKDOCS
    nav_old = nav_path.read_text(encoding="utf-8")
    for name, block in nav_blocks().items():
        nav_old = apply_nav(nav_old, name, block)
    want[MKDOCS] = nav_old
    want.update(index_blocks())
    drift: list[str] = []
    for rel, text in sorted(want.items()):
        p = ROOT / rel
        have = p.read_text(encoding="utf-8") if p.exists() else None
        if have == text:
            continue
        if check:
            drift.append(rel)
        else:
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_text(text, encoding="utf-8")
    # a stale domain page (its module gone) is drift too
    stale = [
        f"{OUT_DIR}/{p.name}"
        for p in sorted((ROOT / OUT_DIR).glob("*.md"))
        if f"{OUT_DIR}/{p.name}" not in want
    ]
    if check:
        if drift or stale:
            print(
                "Domains drift -- run `python scripts/gen_domains.py "
                "--write` (make docs-relink):",
                file=sys.stderr,
            )
            for rel in drift + stale:
                print(f"  {rel}", file=sys.stderr)
            return 1
        print(f"Domains: OK -- {len(want)} generated file(s) up to date")
        return 0
    for rel in stale:
        (ROOT / rel).unlink()
    print(f"Domains: regenerated {len(want)} file(s)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
