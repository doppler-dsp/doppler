#!/usr/bin/env python3
"""Gate: a dB spectrum has ONE converter in library C, `dp_power_to_db_f32`.

#2094 made `dp_power_to_db_f32` (`spectral_core.h`) the library's dB
conversion for power: 10*log10 within 0.01 dB, exact at every power of two,
vectorized, with PSD's -200 dB floor. PSD and the Spectrogram read every dB
through it, so a dB value is that function of the linear one, bit for bit.
A second converter beside it is a peer implementation of the same primitive,
which CLAUDE.md forbids: the three measurement spectra (`tonemeas`,
`imdmeas`, `nprmeas`) each carried a private `10*log10(p/ref + 1e-30)`,
floored about 100 dB lower than PSD and rounded twice, while composing the
very PSD whose reader they duplicated. #2108 deleted them; this keeps the
next one from arriving silently.

**What it flags.** A statement in library C (`native/src`, `native/inc`)
that assigns a `log10`/`log10f` expression to an INDEXED element
(`out[i] = ... log10 (...)`), which is what converting an array looks like.
Comments are stripped and statements are joined across lines first, so a
wrapped expression is still one statement.

**What it does not look at, by rule.** A measurement returned as a scalar
(an SNR, a band total, a peak level) keeps its double `log10`: it is one
value, not a spectrum, and its caller gets a double. That is not an
exemption list; such a statement is not an indexed assignment, so it is out
of the gate's scope by construction.

**The allowance.** Two converters keep their own `log10` on purpose, each
counted per file and held exactly, so it may only shrink: a site that is
converted makes the count fall below the allowance, and the gate then asks
for the allowance to be lowered in the same change.

Usage:  python3 scripts/check_db_conversion_sites.py [--root DIR]
Exit 0 when every indexed log10 assignment is the primitive's or allowed.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SCAN_DIRS = ("native/src", "native/inc")
NAME = "db-conversion"

#: The primitive itself, the one home.
HOME = "native/src/spectral/power_to_db_f32.c"

#: Files that keep their own indexed log10, how many statements each, and
#: why. Held exactly (see the module docstring).
ALLOWED: dict[str, tuple[int, str]] = {
    "native/src/psd/psd_core.c": (
        1,
        "band_power: a measurement per band, summed in double and read the "
        "way total_band_power reads the whole span, not a per-bin spectrum",
    ),
    "native/src/spectral/magnitude_db_cf32.c": (
        1,
        "an AMPLITUDE converter whose floor is the caller's lin_floor, which "
        "may sit below the primitive's fixed -200 dB",
    ),
    "native/src/spectral/magnitude_db_cf64.c": (
        1,
        "as magnitude_db_cf32, and its double input can exceed float's range, "
        "which the float primitive cannot take",
    ),
}

COMMENT = re.compile(r"/\*.*?\*/|//[^\n]*", re.DOTALL)
#: An indexed lvalue assigned to, anywhere in a statement: splitting on `;`
#: leaves a loop's `i++) out[i] = ...` after its `for (...` header.
INDEXED_ASSIGN = re.compile(r"[\w.>\-]+\s*\[[^\]]*\]\s*=(?!=)")
LOG10 = re.compile(r"(?<![\w.])log10f?\s*\(")


def _blank(m: re.Match[str]) -> str:
    """A comment's text as spaces, keeping its newlines for line numbers."""
    return re.sub(r"[^\n]", " ", m.group(0))


def sites(root: Path) -> dict[str, list[tuple[int, str]]]:
    """Every indexed log10 assignment, per file: (line, statement)."""
    found: dict[str, list[tuple[int, str]]] = {}
    for d in SCAN_DIRS:
        for path in sorted((root / d).rglob("*")):
            if path.suffix not in (".c", ".h") or "_ext" in path.name:
                continue
            rel = path.relative_to(root).as_posix()
            if rel == HOME:
                continue
            text = COMMENT.sub(_blank, path.read_text(encoding="utf-8"))
            start = 0
            for stmt in text.split(";"):
                m = INDEXED_ASSIGN.search(stmt)
                if m and LOG10.search(stmt, m.end()):
                    line = text.count("\n", 0, start + m.start()) + 1
                    flat = " ".join(stmt[m.start() :].split())
                    found.setdefault(rel, []).append((line, flat))
                start += len(stmt) + 1
    return found


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--root", type=Path, default=ROOT)
    root = ap.parse_args(argv).root

    found = sites(root)
    rc = 0
    for rel, hits in sorted(found.items()):
        allowed = ALLOWED.get(rel, (0, ""))[0]
        if len(hits) > allowed:
            rc = 1
            print(f"{NAME}: {rel} converts an array to dB with its own log10:")
            for line, flat in hits:
                print(f"  {rel}:{line}: {flat[:100]}")
    for rel, (allowed, _why) in sorted(ALLOWED.items()):
        have = len(found.get(rel, []))
        if have < allowed:
            rc = 1
            print(
                f"{NAME}: {rel} is allowed {allowed} own log10 site(s) and "
                f"has {have}: lower its allowance (it may only shrink)."
            )
    if rc:
        print(
            "  A dB spectrum goes through dp_power_to_db_f32 "
            "(native/inc/doppler/spectral/spectral_core.h), the library's\n"
            "  one dB conversion for power (#2094, #2108). A scalar "
            "measurement keeps double and is not flagged."
        )
        return 1
    print(
        f"{NAME}: one dB converter for arrays (dp_power_to_db_f32); "
        f"{sum(a for a, _ in ALLOWED.values())} allowed own site(s)"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
