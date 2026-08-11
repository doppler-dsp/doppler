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

import ast
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


#: marker -> the primitives that ARE the sanctioned home. A function whose
#: body reaches one of these is a wrapper around the library, not a second
#: copy of it, however its name reads.
CANONICAL: dict[str, set[str]] = {
    "pulse": {
        "rrc_taps",
        # The Python face of the analytic pulses. Omitting these was a real
        # hole for one commit: the gate told callers to use the library and
        # then reported the ones that did.
        "rrc_h",
        "rc_h",
        "wfm_rrc_h",
        "wfm_rc_h",
        "wfm_rrc_taps",
        "wfm_rrc_polyphase_bank",
        "wfm_synth_set_rrc",
        # The generator itself, and the module-level factories that return
        # one. `qpsk(pulse="rrc", ...)` IS wfmgen; a harness calling it is
        # doing exactly what this gate asks for. Names like `noise`, `pn`
        # and `bits` are ordinary words, which is why a bare identifier is
        # not enough -- see `_doppler_imports` below.
        "Synth",
        "Composer",
        "Segment",
        "Timeline",
        "tone",
        "noise",
        "pn",
        "bpsk",
        "qpsk",
        "chirp",
        "bits",
    },
    "evm": {
        "ber_evm_db",
        "dp_test_evm_db_hard",
        "dp_test_evm_db_hard_range",
    },
}


def delegating_defs(src: str) -> set[tuple[str, int]]:
    """`(marker, def-lineno)` for every function that reaches the library.

    The gate matches a function by NAME, which cannot tell a private
    reimplementation from a correctly-written wrapper. `validate.py`'s
    `evm_db()` is `ber_evm_db()` over a `ber_settle_syms()` window with a
    raise instead of the primitive's 0.0-dB "no lock" sentinel; its
    `rrc_bpsk()` reaches `rrc_taps` through two local helpers. Both are the
    behaviour this gate exists to encourage, and allowlisting them would put
    the best code in the tree on a debt list.

    Delegation is followed TRANSITIVELY through module-level functions,
    because the good factoring is exactly the one that hides the primitive a
    level or two down (`rrc_bpsk` -> `shaped_stream` -> `analytic_rrc` ->
    `rrc_taps`). A one-level check would report that chain as private and
    push the author toward inlining, which is the wrong lesson.
    """
    try:
        tree = ast.parse(src)
    except SyntaxError:  # not our file to judge
        return set()

    # Names this module actually imported FROM doppler. A canonical name only
    # counts as delegation if it came from the library, because several of
    # them are ordinary words: a private pulse with a local called `noise`
    # must not exempt itself, and `bits`/`pn`/`tone` are the same hazard.
    doppler_imports: set[str] = set()
    for node in ast.walk(tree):
        if isinstance(node, ast.ImportFrom) and (node.module or "").startswith(
            "doppler"
        ):
            for alias in node.names:
                doppler_imports.add(alias.asname or alias.name)

    funcs = {
        n.name: n
        for n in ast.walk(tree)
        if isinstance(n, (ast.FunctionDef, ast.AsyncFunctionDef))
    }

    def identifiers(node: ast.AST) -> set[str]:
        """Names the code actually REFERENCES.

        Read from the AST, never from the source text: a regex over the
        segment also matches prose, so a private implementation whose
        docstring merely mentions `rrc_taps` would exempt itself. That is
        not hypothetical -- it is what the mutation test caught when this
        function was first written the easy way.
        """
        out: set[str] = set()
        for sub in ast.walk(node):
            if isinstance(sub, ast.Name):
                out.add(sub.id)
            elif isinstance(sub, ast.Attribute):
                out.add(sub.attr)
        return out

    words = {name: identifiers(node) for name, node in funcs.items()}

    def reaches(name: str, marker: str, seen: set[str]) -> bool:
        if name in seen:
            return False
        seen.add(name)
        used = words.get(name, set())
        if CANONICAL[marker] & used & doppler_imports:
            return True
        return any(
            reaches(callee, marker, seen)
            for callee in (used & set(funcs)) - {name}
        )

    return {
        (marker, node.lineno)
        for name, node in funcs.items()
        for marker in CANONICAL
        if reaches(name, marker, set())
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
            text = path.read_text(errors="replace")
            delegating = (
                delegating_defs(text) if path.suffix == ".py" else set()
            )
            for n, line in enumerate(text.splitlines(), 1):
                for marker, (rx, _) in MARKERS.items():
                    if not rx.search(line):
                        continue
                    if (marker, n) in delegating:
                        continue
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
