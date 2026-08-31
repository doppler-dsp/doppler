#!/usr/bin/env python3
"""Fail when a mermaid diagram hardcodes a background or a text colour.

The docs are **dark by default** — ``mkdocs.yml`` lists the ``slate`` palette
first, with the comment "preferred for a DSP / embedded dev audience" — so a
diagram that only works in the light theme is broken for the reader who never
touches the toggle.

Three diagrams were written with ``classDef ... fill:#ede7f6,color:#000``.
That is not a style preference, it is a collision, and the build tells you
why. zensical renders every mermaid block with
``mermaid.initialize({startOnLoad:false, themeCSS: <its own>})`` — it never
sets ``theme:``, and instead drives the visuals from Material's CSS custom
properties::

    .node ... rect { fill: var(--md-mermaid-node-bg-color);
                     stroke: var(--md-mermaid-node-fg-color) }
    .label         { color: var(--md-mermaid-label-fg-color) }

and in the built stylesheet those resolve to::

    --md-mermaid-node-bg-color: var(--md-accent-fg-color--transparent)
    --md-mermaid-label-fg-color: var(--md-code-fg-color)

The node's TEXT colour therefore follows the palette, and in ``slate`` it is
near-white. Pin the node's FILL to a near-white ``#ede7f6`` and you have
pinned one half of a contrast pair while the other half keeps moving: light
text on a light box.

There is no way to fix this from a stylesheet. zensical renders each diagram
into ``attachShadow({mode:"closed"})``, so ``docs/stylesheets/extra.css``
cannot reach the SVG at all. The diagram source is the only lever, which is
why this is a gate on the source.

**The rule: a mermaid style may not set ``fill`` or ``color``.** Leave both to
the theme and they are correct in both palettes by construction. ``stroke``,
``stroke-width`` and ``stroke-dasharray`` are fine and are the intended way to
tell groups of nodes apart -- an outline reads against either background, and
``docs/design/spectral-api-map.md`` was already doing exactly that.

Usage
-----
    python scripts/check_mermaid_theme.py     # report + exit 1 on a violation
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

#: A fenced mermaid block. The info string may carry attributes, as the
#: superfences custom-fence form does.
_FENCE = re.compile(r"^```+\s*mermaid[^\n]*\n(.*?)^```+", re.S | re.M)
#: The statements that carry styling. `linkStyle` is included because an edge
#: label sits on the same painted background as a node.
_STYLING = re.compile(r"^\s*(classDef|linkStyle|style)\b")
#: A `fill:` / `color:` declaration. `stroke-*` is deliberately NOT matched --
#: `stroke-dasharray` would otherwise be caught by a naive `color` search, and
#: an outline is legible on either palette.
_PINNED = re.compile(r"(?<![\w-])(fill|color)\s*:\s*([^,;]+)")
#: Values that name the theme rather than a fixed colour, so they move with it.
_OK_VALUES = re.compile(
    r"^(none|transparent|inherit|currentcolor|var\()", re.I
)


def violations(text: str) -> list[tuple[int, str, str]]:
    """Return (line number, property, whole styling line) for each pin.

    Pure, and line numbers are counted in the ORIGINAL file rather than
    within the fence, so the report names a line an editor can jump to.
    """
    out: list[tuple[int, str, str]] = []
    for fence in _FENCE.finditer(text):
        base = text.count("\n", 0, fence.start(1))
        for offset, line in enumerate(fence.group(1).split("\n")):
            if not _STYLING.match(line):
                continue
            for prop, value in _PINNED.findall(line):
                if _OK_VALUES.match(value.strip()):
                    continue
                out.append((base + offset + 1, prop, line.strip()))
    return out


def main() -> int:
    ap = argparse.ArgumentParser(description="mermaid theme safety")
    ap.add_argument("--root", type=Path, default=ROOT)
    a = ap.parse_args()
    root = a.root.resolve()

    pages = sorted((root / "docs").rglob("*.md"))
    found: dict[str, list[tuple[int, str, str]]] = {}
    n_diagrams = 0
    for page in pages:
        text = page.read_text(encoding="utf-8")
        n_diagrams += len(_FENCE.findall(text))
        if bad := violations(text):
            found[str(page.relative_to(root))] = bad

    # Fail closed, the way the sibling doc gates do. Zero diagrams means the
    # fence syntax changed or the docs moved, and printing OK over nothing
    # checked is how a gate goes quietly inert -- absent output is not a pass.
    if n_diagrams == 0:
        print(
            "mermaid theme: FAIL -- no mermaid diagrams found under docs/.\n"
            "  Nothing to check means nothing was checked. If the fence\n"
            "  syntax or the docs directory moved, this gate moves with it.",
            file=sys.stderr,
        )
        return 1

    if found:
        n = sum(len(v) for v in found.values())
        print(
            f"mermaid theme: FAIL -- {n} hardcoded colour(s) in "
            f"{len(found)} diagram source(s):\n",
            file=sys.stderr,
        )
        for page, bad in sorted(found.items()):
            for line_no, prop, line in bad:
                print(
                    f"    {page}:{line_no}  ({prop})\n      {line}",
                    file=sys.stderr,
                )
        print(
            "\n  The docs default to the dark `slate` palette, and a node's\n"
            "  text colour follows the palette while a pinned `fill` does\n"
            "  not -- so this is light text on a light box for every reader\n"
            "  who never touches the toggle. A stylesheet cannot fix it:\n"
            "  the diagram renders into a CLOSED shadow root.\n"
            "\n  Drop `fill:` and `color:` and let the theme supply both.\n"
            "  Use `stroke:` / `stroke-width:` / `stroke-dasharray:` to tell\n"
            "  groups apart -- an outline reads on either background.",
            file=sys.stderr,
        )
        return 1

    print(
        f"mermaid theme: OK — {n_diagrams} diagram(s) across "
        f"{len(pages)} page(s) leave fill and text colour to the palette"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
