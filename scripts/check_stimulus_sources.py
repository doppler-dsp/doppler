#!/usr/bin/env python3
"""Gate: stimulus and its measurement have ONE home, and it is the library.

doppler is a C library with a Python convenience layer, and it ships a
full-featured waveform generator (`wfm_synth_*` in C, `Synth`/`Composer` in
Python) plus a measurement stack (`ber_evm_db`, `ber_settle_syms`,
`ber_settle_from`). A test, validation harness or example that builds its own
pulse, sets its own level, or computes its own EVM is not just duplicating
code -- it is inventing a CONVENTION, and the convention is the part that goes
wrong silently.

Measured cost, 2026-08-10: `ratesync_demo.py` shaped its own RRC and then
scaled the result by `0.25 / max(|x|)` -- a PEAK normalisation. An RRC stream
peaks at ~1.582x its symbol amplitude, so symbols reached RateSync at ~0.158
against a contract written in unit symbol amplitude. A Gardner detector's
slope goes as A^2, so that is ~40x of loop gain, and the demo failed its own
lock assertion with nothing pointing at the level. wfmgen already defines unit
transmit power and an Es/N0 mode; the hand-rolled generator is what made the
level expressible-but-unstated.

Three signatures, each with a canonical primitive that already exists:

  pulse      a private raised-cosine / RRC  ->  wfm_synth_set_rrc, rrc_taps
  level      normalising a generated stream by its PEAK  ->  Synth(level=,
             snr=, snr_mode=), wfm_snr_over_fs, wfm_source_create_snr
  evm        a private error-vector magnitude  ->  ber_evm_db, over a window
             from ber_settle_syms / ber_settle_from

This scans only the TEST, VALIDATION and EXAMPLE layers. The library itself is
the sanctioned home -- `wfm_synth_core.c` implementing an RRC is the point.

Deliberately NOT gated here, and the omission is the design: hand-rolled
Gaussian noise (`standard_normal`, Box-Muller) hits 72 files, most of them
legitimately -- assembling an expected array, or inert plumbing where the
sample values are irrelevant. A ratchet that large is noise, and a noisy gate
gets switched off. The level marker above already catches the case that
actually bites, which is an invented amplitude convention.

**The allowlist is a RATCHET. It may only shrink.** A new occurrence fails the
gate. Removing one means deleting its line, which the failure message shows.

Usage:  python3 scripts/check_stimulus_sources.py
Exit 0 when the set of occurrences matches the allowlist exactly.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
ALLOW = Path(__file__).parent / ".stimulus-sources-allow"

#: Only the layers that CONSUME the library. `native/src` and `native/inc`
#: are where the canonical implementations live.
SCAN_DIRS = ("native/tests", "native/validation", "src/doppler")

SUFFIXES = (".c", ".h", ".py")

#: marker -> (pattern, what to call instead)
MARKERS: dict[str, tuple[re.Pattern[str], str]] = {
    "pulse": (
        re.compile(
            r"def\s+_?rrc\w*\s*\(|def\s+_?rc\s*\(|"
            r"static\s+(inline\s+)?double\s+_?(rrc|rc)\w*\s*\("
        ),
        "generate with wfmgen -- wfm_synth_set_rrc() in C, or "
        "Synth(pulse='rrc', rrc_beta=..., rrc_span=...) in Python. "
        "wfm_rrc_taps()/rrc_taps() gives the taps if you need them raw.",
    ),
    # A NUMERIC backoff against the peak, e.g. `x *= 0.25 / max(|x|)`.
    #
    # Scaling to a FRACTION of the peak invents a level: the symbol amplitude
    # that results depends on the pulse's PAPR and on beta, so the object
    # receives something nobody stated. Normalising to UNIT peak
    # (`y / max(|y|)`, no constant) is deliberately NOT matched -- that is the
    # display/comparison convention used to overlay an S-curve or a frequency
    # response, and it is not a stimulus level at all. That distinction is
    # what keeps this marker sharp enough to be worth gating: broadened to
    # every peak normalisation it fires on 5 more files, all of them
    # legitimate, and a noisy gate gets switched off.
    "level": (
        re.compile(
            r"[*/]=\s*[\d.]+\s*/\s*np\.max\s*\(\s*np\.abs|"
            r"[*/]\s*[\d.]+\s*/\s*np\.max\s*\(\s*np\.abs|"
            r"[*/]=\s*[\d.]+\s*/\s*\w*max\s*\(\s*\w*abs"
        ),
        "do not normalise a generated stream by its PEAK -- the symbol "
        "amplitude then depends on the pulse's PAPR and on beta, so the "
        "object receives a level nobody stated. wfmgen owns this: Synth's "
        "`level`, `snr` and `snr_mode` (auto/fs/ebno/esno), with sqrt(sps) "
        "scaling for unit transmit power.",
    ),
    "evm": (
        re.compile(
            r"def\s+_?evm(_db)?\s*\(|"
            r"static\s+(inline\s+)?double\s+_?evm(_db)?\w*\s*\("
        ),
        "measure with ber_evm_db() -- doppler.ber.ber_evm_db in Python, "
        "dp_sym_test.h's dp_test_evm_db_hard_range() in C -- over a window "
        "from ber_settle_syms()/ber_settle_from(), never a fixed fraction "
        "of the record.",
    ),
}


def occurrences() -> list[tuple[str, str, int, str]]:
    """Every (marker, file, line-no, line) in the consuming layers."""
    found: list[tuple[str, str, int, str]] = []
    for d in SCAN_DIRS:
        base = ROOT / d
        if not base.exists():
            continue
        for path in sorted(base.rglob("*")):
            if path.suffix not in SUFFIXES or "__pycache__" in path.parts:
                continue
            rel = path.relative_to(ROOT).as_posix()
            for n, line in enumerate(
                path.read_text(errors="replace").splitlines(), 1
            ):
                for marker, (rx, _) in MARKERS.items():
                    if rx.search(line):
                        found.append((marker, rel, n, line.strip()))
    return found


def load_allow() -> dict[str, str]:
    """Allowed `marker::file::snippet` keys mapped to their stated reason."""
    allowed: dict[str, str] = {}
    if not ALLOW.exists():
        return allowed
    for raw in ALLOW.read_text().splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        key, _, reason = line.partition("|")
        allowed[key.strip()] = reason.strip()
    return allowed


def key_for(marker: str, rel: str, line: str) -> str:
    """Line-number-free identity, so unrelated edits don't churn the list."""
    return f"{marker}::{rel}::{' '.join(line.split())}"


def main() -> int:
    found = occurrences()
    allowed = load_allow()

    seen: set[str] = set()
    new: list[tuple[str, str, int, str]] = []
    for marker, rel, n, line in found:
        k = key_for(marker, rel, line)
        seen.add(k)
        if k not in allowed:
            new.append((marker, rel, n, line))

    stale = sorted(set(allowed) - seen)

    if new:
        print(
            "FAIL: a private stimulus or measurement appeared.\n"
            "\n"
            "doppler is a C library with Python convenience, and it already\n"
            "ships the generator (wfmgen) and the metrics (ber). A private\n"
            "copy invents a CONVENTION -- a level, a window, a scale -- and\n"
            "the convention is what goes wrong silently.\n"
        )
        for marker, rel, n, line in new:
            print(f"  [{marker}] {rel}:{n}\n      {line}")
            print(f"      -> {MARKERS[marker][1]}\n")
        m0, r0, _, l0 = new[0]
        print(
            "If this genuinely cannot go through the library, add it to\n"
            f"  {ALLOW.relative_to(ROOT)}\n"
            "with a reason, in the form:\n"
            f"  {key_for(m0, r0, l0)} | why the library cannot express this"
        )
        return 1

    if stale:
        print(
            "FAIL: the allowlist is a ratchet and has gone stale -- these\n"
            "entries no longer match anything. Delete them; the rule just\n"
            "got stricter and that is the direction it is allowed to move.\n"
        )
        for k in stale:
            print(f"  {k}")
        return 1

    by_marker = {m: sum(1 for f in found if f[0] == m) for m in MARKERS}
    detail = ", ".join(f"{m} {c}" for m, c in by_marker.items())
    print(
        f"stimulus-sources gate: OK -- {len(found)} occurrence(s) "
        f"({detail}), all allowlisted"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
