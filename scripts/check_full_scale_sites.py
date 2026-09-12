#!/usr/bin/env python3
"""Gate: a wire format's full scale has ONE home, `dp_format_full_scale()`.

Normalising between a float sample and an integer wire code needs two things
that must not be written twice: the full-scale constant, and the
scale-round-saturate arithmetic. doppler had four private copies of the pair
(doppler#1117) -- `wfm_sink.c`, `wfm_writer_core.c` and `wfm_reader_core.c`
each carried a `SCALE[]` table or bare literals, and `pocketfft.c` folded the
inverse scale into its input read under a comment *claiming* it matched the
cvt module.

They had already drifted. All three wfm copies TRUNCATED toward zero where
every cvt converter rounds to nearest, which is 6.0 dB of extra quantisation
noise on every integer wire type, and all four used 2^(N-1)-1 as full scale
where cvt uses 2^(N-1) -- so `wfm.Reader` and `doppler.cvt.I16ToF32`
disagreed on 2.3% of int16 codes. A private copy is where the next such
drift lives.

The signature of a private copy is a file that spells the full-scale
constants of more than one WIDTH. Naming both bounds of a single width is
ordinary and stays legal -- `fmaxf(s, -32768.0f)` beside `fminf(s, 32767.0f)`
is one type's saturation clamp, and every cvt converter has one. Naming 127
*and* 32767, or 128 *and* 32768, is a per-format TABLE, which is what
`dp_format_full_scale()` already is.

Library C only (`native/inc`, `native/src`); tests and harnesses are oracles
and may spell a constant as they like. Comments are stripped first, so prose
describing the convention is free. No allowlist: every copy was converted
when this gate landed, so the first new occurrence fails.

Usage:  python3 scripts/check_full_scale_sites.py
Exit 0 when no private full-scale table exists.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SANCTIONED = "native/inc/dp_format.h"
SCAN_DIRS = ("native/inc", "native/src")

# 2^(N-1) for the widths doppler puts on a wire, and the 2^(N-1)-1 spellings
# the private copies used -- both, so reintroducing either is caught.
FULL_SCALE = re.compile(
    r"(?<![\w.])("
    r"128|127|"
    r"32768|32767|"
    r"2147483648|2147483647"
    r")\.0"
)
# Which width each constant belongs to. Both spellings of a width map to it:
# 2^(N-1) is the convention, 2^(N-1)-1 is what the private copies used, and
# either one names the same wire format.
WIDTH = {
    "127": 8,
    "128": 8,
    "32767": 16,
    "32768": 16,
    "2147483647": 32,
    "2147483648": 32,
}
BLOCK_COMMENT = re.compile(r"/\*.*?\*/", re.S)
LINE_COMMENT = re.compile(r"//[^\n]*")


def _code_only(text: str) -> str:
    """Strip comments; a comment may describe the convention freely."""
    return LINE_COMMENT.sub("", BLOCK_COMMENT.sub("", text))


def private_tables() -> list[tuple[str, list[str]]]:
    found: list[tuple[str, list[str]]] = []
    for d in SCAN_DIRS:
        for path in sorted((ROOT / d).rglob("*")):
            if path.suffix not in (".c", ".h"):
                continue
            rel = path.relative_to(ROOT).as_posix()
            if rel == SANCTIONED or "_ext" in path.name:
                continue
            code = _code_only(path.read_text(errors="replace"))
            hits = set(FULL_SCALE.findall(code))
            if len({WIDTH[h] for h in hits}) >= 2:
                found.append((rel, sorted(hits, key=int)))
    return found


def main() -> int:
    found = private_tables()
    if not found:
        print("full-scale: one home (dp_format_full_scale), no private table")
        return 0
    print("full-scale: a per-format full-scale table is written by hand --")
    for rel, consts in found:
        widths = sorted({WIDTH[c] for c in consts})
        names = ", ".join(f"{w}-bit" for w in widths)
        print(
            f"  {rel}: {', '.join(c + '.0' for c in consts)}"
            f"  ({len(widths)} widths: {names})"
        )
    print(
        "  Ask dp_format_full_scale(type) (native/inc/dp_format.h) for\n"
        "  the constant, and let the cvt converters (f32_to_i8/i16/i32 and\n"
        "  their i*_to_f32 inverses) do the scale, rounding and saturation.\n"
        "  Naming BOTH bounds of ONE width is fine and is not what fired\n"
        "  here -- this file names the full scale of two or more different\n"
        "  widths, which is a table. If those widths are genuinely unrelated\n"
        "  to a wire format, spell them as the types' own limits (INT16_MAX,\n"
        "  INT8_MIN), which says what they are and does not trip this gate."
    )
    return 1


if __name__ == "__main__":
    sys.exit(main())
