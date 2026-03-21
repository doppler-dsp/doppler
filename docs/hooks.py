"""
MkDocs build hook: rewrite links for the mkdocs site.

The docs live in docs/ and reference each other by lowercase names
(build.md, overview.md, etc.).  ../README.md needs to become index.md
for mkdocs.  Root-level files not in the site (CLAUDE.md, VENDORED.md)
and source-file paths (c/…, python/…) point to GitHub.
"""

import re

GITHUB_BASE = "https://github.com/hunter-dsp/doppler/blob/main"
GITHUB_TREE = "https://github.com/hunter-dsp/doppler/tree/main"

# Files that live at the repo root but aren't part of the mkdocs nav.
_ROOT_TO_GITHUB = {
    "CLAUDE.md",
    "VENDORED.md",
    "CHANGELOG.md",
}

# Regex: markdown link [text](target)
_MD_LINK = re.compile(r"\[([^\]]*)\]\(([^)]+)\)")


def _rewrite(target: str) -> str:
    """Return the rewritten link target, or the original."""
    if target.startswith("http://") or target.startswith("https://"):
        return target

    # Split off fragment (#anchor).
    fragment = ""
    if "#" in target:
        target, fragment = target.split("#", 1)
        fragment = "#" + fragment

    # ../README.md → index.md (mkdocs serves README as index)
    if target == "../README.md":
        return "index.md" + fragment

    # docs/*.md links from README (served as index.md) → sibling pages
    if target.startswith("docs/"):
        return target[5:] + fragment

    # Bare root-file references from README (CLAUDE.md, etc.) → GitHub
    if target in _ROOT_TO_GITHUB:
        return f"{GITHUB_BASE}/{target}" + fragment

    # ../VENDORED.md, ../CLAUDE.md, etc. → GitHub
    if target.startswith("../"):
        basename = target[3:]
        if basename in _ROOT_TO_GITHUB:
            return f"{GITHUB_BASE}/{basename}" + fragment

    # Source-file paths (from docs that reference c/…, python/…)
    # These are relative to the repo root; strip any leading ../
    clean = target.lstrip("./")
    if clean.startswith("../"):
        clean = clean[3:]
    if (
        clean.startswith("c/")
        or clean.startswith("python/")
        or clean.startswith("ffi/")
    ):
        if clean.endswith("/"):
            return f"{GITHUB_TREE}/{clean.rstrip('/')}"
        return f"{GITHUB_BASE}/{clean}"

    return target + fragment


def on_page_markdown(markdown: str, **kwargs) -> str:
    """Rewrite links in every page's markdown before rendering."""

    def replace(m: re.Match) -> str:
        text, target = m.group(1), m.group(2)
        return f"[{text}]({_rewrite(target)})"

    return _MD_LINK.sub(replace, markdown)
