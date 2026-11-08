#!/usr/bin/env python3
"""Gate: a public C symbol this branch removes has a `.retired-names` row.

`check_retired_names.py` keeps a retired identifier retired -- once someone
adds its row. Nothing required the row. Step 3.2 of the correlator work
(doppler#2886) retired every `dp_corr_*` and `dp_corr2d_*` symbol with no
row, `example-projects/uno-q/bench.c` still called the old names, and every
Build job failed. A row would have named the straggler on the first push
(doppler#2905).

So this gate asks what the branch REMOVED, against the merge base with
``--base`` (default ``origin/main``):

- **symbols** -- names in ``cmake/public-symbols.txt`` at the base that are
  not in it now. That list is what ``gen_public_symbols.py`` extracts from
  the headers, and ``make lint-public-symbols`` holds the working copy equal
  to them, so this reads the headers' symbols without a second parser;
- **headers** -- paths under ``native/inc/doppler/`` at the base that no
  longer exist, because a consumer ``#include``s the path.

Each must be matched (``re.search``) by some row's pattern, read by
``check_retired_names.rows``. A symbol renamed in place is a removal of the
old name, so it needs a row; a symbol or header that still exists needs
nothing, whatever else changed. On ``main`` itself the merge base is HEAD and
nothing is removed.

Outside a git repository there is no base to compare with and the gate
passes. A base that will not resolve (a shallow clone) FAILS: a gate that
cannot read what was removed has not checked anything.

Usage:  python3 scripts/check_retired_symbols.py [--root DIR] [--base REF]
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

from _gitbase import (
    BaseUnreadableError,
    in_git_repo,
    ls_tree_at,
    resolve_base,
    show_at,
)
from check_installed_headers import private_symbols
from check_retired_names import rows
from gen_public_symbols import names

ROOT = Path(__file__).resolve().parent.parent
SYMBOLS = "cmake/public-symbols.txt"
HEADERS = "native/inc/doppler"
RETIRED = "scripts/.retired-names"


def removed(root: Path, base: str) -> list[str]:
    """Public symbols and header paths at the merge base and gone now."""
    rev = resolve_base(root, base)
    then = show_at(root, rev, SYMBOLS)
    now_path = root / SYMBOLS
    now = names(now_path.read_text()) if now_path.is_file() else set()
    # A name a `*_private.h` still declares is internal, not retired: the
    # export list leaves private headers out (#2930), and a row would forbid
    # a function its own object still calls.
    internal = set(private_symbols(root / "native" / "inc"))
    gone = sorted(names(then) - now - internal) if then is not None else []
    gone += sorted(
        rel
        for rel in ls_tree_at(root, rev, HEADERS)
        if rel.endswith(".h") and not (root / rel).is_file()
    )
    return gone


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--root", type=Path, default=ROOT)
    ap.add_argument("--base", default="origin/main")
    a = ap.parse_args(argv)
    root = a.root.resolve()
    if not in_git_repo(root):
        print("retired-symbols: not a git repository -- nothing to compare")
        return 0
    try:
        gone = removed(root, a.base)
    except BaseUnreadableError:
        print(
            f"FAIL: cannot resolve {a.base}, so a removed symbol cannot be\n"
            "  found. A gate that cannot read its baseline has not passed.\n"
            "  Fetch it:  git fetch --no-tags --depth=1 origin \\\n"
            "               +refs/heads/main:refs/remotes/origin/main"
        )
        return 1
    retired = root / RETIRED
    declared = rows(retired) if retired.is_file() else []
    missing = [g for g in gone if not any(p.search(g) for p, _, _ in declared)]
    if missing:
        print(
            f"retired-symbols: {len(missing)} public name(s) removed since "
            f"{a.base} with no {RETIRED} row:"
        )
        print("\n".join(f"  {m}" for m in missing))
        print(
            "\nAdd a row (`<regex>\\t<what to use instead>`) in this commit,"
            "\nso check_retired_names.py finds every caller still using it."
        )
        return 1
    print(
        f"retired-symbols: OK -- {len(gone)} removed public name(s), "
        "each matched by a row"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
