#!/usr/bin/env python3
"""Fail when a flag `wfmgen` accepts appears in none of its guide pages.

A pre-release review of wfmgen against the lifecycle spine
(``docs/dev/contributing/adding-algorithms.md``) counted the flags the
dispatcher accepts against the flags the guide mentions, and found **50 of
56**. The missing six were not scattered: they were exactly the channel
coding stage, which had no page at all. doppler#1044 wrote that page and the
count went whole.

Nothing kept it whole. The count was a fact about one afternoon, established
by a survey nobody was going to repeat, and the next flag to land could
arrive undocumented in silence -- which is how the six got there in the first
place. This is the ratchet the review asked for and did not build.

**Every accepted flag, including every alias.** An alias exists to be typed,
so an undocumented one is a secret: `--randomize` is how `--randomise` is
spelled in the half of the world that spells it that way, the parser accepts
it, and on this gate's first run the guide had never once mentioned it. There
is deliberately no allow-list. A flag costs one table cell to document, which
is cheaper than an exemption list that goes stale and has to be audited to
find out whether its entries are still true.

The pages are DISCOVERED, not registered: a new ``docs/guide/wfmgen/*.md``
counts the moment it exists, and a page that is deleted stops counting. So
this gate cannot be satisfied by editing a list in this file.

Scope: this asks whether every flag is documented. It does NOT ask the
reverse -- whether every flag the pages cite still exists -- which is real
doc rot and is filed separately; the guide currently cites ``--build`` and
``--target``, which belong to `cmake`, so the reverse direction needs a way
to tell whose flag a token is and that is more than this gate should carry.

Usage
-----
    python scripts/check_wfmgen_flag_docs.py     # report + exit 1 on a gap
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from gen_wfmgen_flag_matrix import dispatcher_flags

ROOT = Path(__file__).resolve().parent.parent
GUIDE = "docs/guide/wfmgen"
SRC = "native/src/app/wfmgen.c"


def _mentioned(flag: str, blob: str) -> bool:
    """Is `flag` named in `blob` as a whole flag, not as a prefix?

    The look-ahead is what stops ``--interleave`` from being counted as
    documented by a page that only ever mentions ``--interleave-unit``, and
    stops ``--randomise`` from being satisfied by ``--randomised``. Both are
    live pairs in this tool, so a substring test would report the guide as
    complete while a flag sat unmentioned.
    """
    return re.search(re.escape(flag) + r"(?![A-Za-z0-9-])", blob) is not None


def main() -> int:
    ap = argparse.ArgumentParser(description="wfmgen flag doc coverage")
    ap.add_argument("--root", type=Path, default=ROOT)
    a = ap.parse_args()
    root = a.root.resolve()

    pages = sorted((root / GUIDE).glob("*.md"))
    # Fail closed. An empty or moved guide directory would otherwise make
    # every flag vacuously documented and the gate would print OK while
    # checking nothing -- absent output is not a pass.
    if not pages:
        print(
            f"wfmgen flag docs: FAIL -- no pages under {GUIDE}/.\n"
            "  Nothing to check means nothing was checked. If the guide\n"
            "  moved, this gate has to move with it.",
            file=sys.stderr,
        )
        return 1

    flags = dispatcher_flags(root / SRC)
    blob = "\n".join(p.read_text(encoding="utf-8") for p in pages)
    missing = sorted(f for f in flags if not _mentioned(f, blob))

    if not missing:
        print(
            f"wfmgen flag docs: OK — {len(flags)} flag(s) documented "
            f"across {len(pages)} guide page(s)"
        )
        return 0

    print(
        f"wfmgen flag docs: FAIL -- {len(missing)} of {len(flags)} flag(s) "
        f"are accepted by {SRC}\n"
        f"  but named in no page under {GUIDE}/:\n",
        file=sys.stderr,
    )
    for f in missing:
        print(f"    {f}", file=sys.stderr)
    print(
        "\n  A flag a reader cannot find is a flag they do not have. Put it\n"
        "  on the page that owns its stage -- an alias belongs beside the\n"
        "  name it aliases, not on a page of its own.",
        file=sys.stderr,
    )
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
