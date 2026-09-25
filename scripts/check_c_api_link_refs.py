#!/usr/bin/env python3
"""Gate: the generated C API pages carry no bare ``[...]`` outside code.

mkdoxy turns a header's doxygen into ``docs/c-api/*.md``. Most of the time it
writes a bracket in prose as the HTML entities ``&#91;``/``&#93;``, but not
always: a bracketed span it emits RAW is markdown's shortcut reference link,
and the strict site build refuses it as ``unresolved link reference``. The
header line looks innocent -- ``@return Detection probability in [0, 1].``
rendered escaped for ``det_pd`` and raw for ``det_pd_cfar`` beside it -- so
this checks what mkdoxy WROTE, not what the header says.

It broke CI twice in one day (doppler#1542's ``out[e * n_f + j]``,
doppler#1552's ``[0, 1]``), each time found only by the site build, which
``make lint`` does not run. This is the fast half of that: a scan of the
committed pages, which gen-c-api-check already keeps equal to the headers.

A span counts when it is not a link (``[t](url)``, ``[t][ref]``), not an
image, not escaped, and not inside a code span or fence. Spell such text so
it cannot be one: "between 0 and 1", element ``e * n_f + j``, or backticks.

Usage
-----
    python scripts/check_c_api_link_refs.py            # exit 1 on any
    python scripts/check_c_api_link_refs.py --root DIR
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

#: ``[text]`` not preceded by ``\``, ``!`` or ``]`` (an escape, an image, the
#: second half of ``[t][ref]``) and not followed by ``(``, ``[`` or ``:``
#: (an inline link, a full reference, a definition).
_BARE = re.compile(r"(?<![\\!\]])\[([^\]\n]+)\](?![(\[:])")
_FENCE = re.compile(r"```.*?```", re.S)
_CODE = re.compile(r"`[^`\n]*`")


def bare_refs(text: str) -> list[tuple[int, str]]:
    """Every bare ``[...]`` span in *text*, as ``(line, span)``.

    >>> bare_refs("in [0, 1].")
    [(1, '[0, 1]')]
    >>> bare_refs("a [link](x.md), `x[i]`, &#91;0&#93;, ![img](p.png)")
    []
    """
    # Blank the fences rather than delete them, so line numbers survive.
    text = _FENCE.sub(lambda m: "\n" * m.group(0).count("\n"), text)
    found = []
    for n, line in enumerate(text.splitlines(), 1):
        for m in _BARE.finditer(_CODE.sub("", line)):
            found.append((n, m.group(0)))
    return found


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--root", type=Path, default=Path("docs/c-api"))
    args = ap.parse_args(argv)
    pages = sorted(args.root.glob("*.md"))
    if not pages:
        print(f"check_c_api_link_refs: no pages under {args.root}")
        return 1
    bad = 0
    for page in pages:
        for line, span in bare_refs(page.read_text(encoding="utf-8")):
            print(f"{page}:{line}: bare {span} renders as a link reference")
            bad += 1
    if bad:
        print(
            f"check_c_api_link_refs: {bad} bare bracketed span(s). Reword "
            "the header doxygen they came from (see this script's doc)."
        )
        return 1
    print(f"check_c_api_link_refs: OK — {len(pages)} page(s), none bare")
    return 0


if __name__ == "__main__":
    sys.exit(main())
