#!/usr/bin/env python3
"""Gate: a repo path named in prose is a repo path that exists.

`check_site_links.py` holds every LINK in the built site. It cannot see a
path written as a code span -- ``docs/gallery/foo.md`` in backticks is
text, not a link, so a page can cite a file that has never existed and
every docs gate stays green. Three did, and they were found by hand rather
than by anything that runs:

- ``native/inc/dsss_receiver/dsss_receiver_core.h`` cited
  ``docs/gallery/dsss-acq-async-data.md`` and
  ``docs/gallery/dsss-despread-async-data.md``. Neither was ever committed
  under those names -- git records ``async-dsss-receiver.md`` and
  ``dsss-despread.md``, both since deleted -- so the reader was sent to two
  pages that did not exist, and mkdoxy copied the citation faithfully into
  ``docs/c-api/dsss__receiver__core_8h.md``.
- ``docs/dev/archive/nats-jetstream-transport-migration.md`` closed its
  "this is history, read X for the current state" note with
  ``docs/dev/streaming-roadmap.md``. That file has never existed in this
  repo's history at all.

**Headers are scanned, not just docs.** The first of those lives in a C
header and reached the site only because mkdoxy renders it; fixing the
generated page would have been fixing the copy. Scanning the header is what
makes the finding land where the text is written.

Cross-repo citations are excluded, because they are correct and common
here: ``just-makeit``'s ``docs/developers/docstring-derivation.md`` and
``just-bashit``'s ``src/just_bashit/datetime.sh`` both exist, in those
repos. A path is treated as another repo's when a sibling project is named
in the same paragraph or inside the path itself -- so the exclusion is
derived from the text, not from a list of blessed paths that would go stale
the same way the citations do.

Nothing is registered: the file set is globbed and the path set comes out
of the prose, so a new page or a new citation is covered by existing.

Usage:  python3 scripts/check_doc_paths.py
Exit 0 when every repo-local path named in prose exists.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

#: Where prose lives. Headers are included deliberately -- see above.
SOURCES = ("docs/**/*.md", "native/inc/**/*.h")

#: Directories that are unmistakably this repo's, so a code span starting with
#: one is a claim about THIS tree. Derived from the top level rather than
#: listed by hand would be nice, but a bare `src/` also prefixes just-bashit's
#: paths, which is exactly the collision the sibling check below resolves.
REPO_PREFIX = (
    "native/",
    "src/",
    "scripts/",
    "docs/",
    "objects/",
    "examples/",
    "tests/",
    "ffi/",
    ".github/",
)

#: Sibling projects whose paths legitimately appear here.
SIBLINGS = (
    "just-makeit",
    "just_makeit",
    "just-bashit",
    "just_bashit",
    "just-buildit",
    "just_buildit",
)

#: A backticked path with a file extension. Directories are not checked: a
#: bare `stream/` is usually shorthand for a subtree, not a literal path.
_PATH = re.compile(
    r"`([A-Za-z0-9_./-]+\.(?:c|h|py|pyi|toml|md|txt|sh|yml|yaml|json|in))`"
)

#: A placeholder rather than a real path -- `<obj>_core.c`, `bench_*.c`.
_PLACEHOLDER = re.compile(r"[*<>{}]")


def _paragraph_around(text: str, pos: int) -> str:
    """The blank-line-delimited block containing `pos`.

    The unit a sibling is named in: "``just-makeit``'s ``docs/...``" puts
    the qualifier one or two lines from the path, and a whole-file search
    would excuse every path in any file that mentions a sibling once.
    """
    start = text.rfind("\n\n", 0, pos) + 1
    end = text.find("\n\n", pos)
    return text[start : end if end != -1 else len(text)]


def main() -> int:
    failures: list[str] = []
    checked = 0

    files = sorted({p for pat in SOURCES for p in ROOT.glob(pat)})
    for src in files:
        text = src.read_text(encoding="utf-8", errors="replace")
        for m in _PATH.finditer(text):
            path = m.group(1)
            if not path.startswith(REPO_PREFIX) or _PLACEHOLDER.search(path):
                continue
            para = _paragraph_around(text, m.start())
            if any(s in path or s in para for s in SIBLINGS):
                continue
            checked += 1
            if (ROOT / path).exists():
                continue
            failures.append(
                f"{src.relative_to(ROOT)}: cites `{path}`, which does not "
                "exist. Fix the path, or if it belongs to a sibling project "
                "name that project in the same paragraph."
            )

    if failures:
        print("check_doc_paths: FAIL\n", file=sys.stderr)
        for f in sorted(set(failures)):
            print(f"  - {f}\n", file=sys.stderr)
        return 1

    print(
        f"check_doc_paths: OK — {checked} repo path(s) named in prose across "
        f"{len(files)} file(s) all exist"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
