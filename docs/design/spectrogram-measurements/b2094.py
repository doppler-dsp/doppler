"""Render the #2094 baseline's tables from the run that measured them.

The measurement record's entries 5.12 to 5.14
(``docs/design/spectrogram-measurements.md``) quote one run of
``make bench-interleaved VERSION=0.66.0-b2094 K=5`` at c36a042e0 on
cachyos-x8664-ai465: main with #2113's bench rows and none of #2094's
changes (#2105's fold, the fused passes, #2117's dB conversion), so it is
the BEFORE of all three. Its ten per-pass snapshots are committed beside
this file in ``b2094/<build>/``, filtered to the ``psd::``,
``acc_trace::`` and ``spectrogram::`` rows, with the run's merged
``doppler_meta`` (machine, governor, flags) in ``b2094/meta.json``.

Every table cell in those entries is this file's output, between
``<!-- spectrogram-b2094:<name>:start -->`` and ``:end`` markers in the
record. How a cell is computed, the provenance checks (every pass at
c36a042e0, clean) and the ``--write``/``--check`` machinery are
``_record.py``'s, shared with ``u1u4.py``. A derived column (a difference
or a ratio of two rows) is the median over the passes of that pass's
difference or ratio, not the difference of the two medians beside it.

Usage::

    python docs/design/spectrogram-measurements/b2094.py          # print
    python docs/design/spectrogram-measurements/b2094.py --write  # rewrite
    python docs/design/spectrogram-measurements/b2094.py --check  # gate

``--check`` is in ``make docs-invariants``.
"""

from __future__ import annotations

import statistics
import sys
from pathlib import Path
from typing import TYPE_CHECKING

sys.path.insert(0, str(Path(__file__).resolve().parent))
import u1u4
from _record import (
    BUILDS,
    Pass,
    Runs,
    both,
    main,
    med,
    runs,
    spread,
    table,
)

if TYPE_CHECKING:
    from collections.abc import Callable

DATA = Path(__file__).resolve().parent / "b2094"
#: The commit every pass was measured at (``_record.load`` asserts it).
COMMIT = "c36a042e0"
SIZES = (256, 1024, 4096, 16384, 65536)
#: The sizes #2113 gave a frame_linear row, so the split is measured there.
SPLIT = (256, 1024, 65536)
MODES = ("mean", "exp", "maxhold", "minhold")
#: (nfft, hop) of the power rows, each timed beside its dB row.
ROWS = ((256, 64), (1024, 256), (65536, 16384))
#: The sizes of power_onesided, the read-out timed one call per round.
ONESIDED = (1024, 4096, 16384)
#: The clock step those single-call passes sit on, and how far off a whole
#: number of steps two of a row's passes may be; ``onesided`` refuses a row
#: whose passes break it, so the step is checked against the data.
STEP_NS, STEP_TOL_NS = 10, 1


def _psd(k: str, n: int) -> str:
    return f"psd::{k}[nfft={n}]"


def _cell(P: Runs, g: Callable[[Pass], float], fmt: str) -> str:
    return both(P, lambda q: med(q, g), fmt)


def _spread(P: Runs, g: Callable[[Pass], float]) -> str:
    return both(P, lambda q: spread(q, g), ".1%")


def frame(P: Runs) -> str:
    """Where one accumulated frame's time goes, against the FFT alone."""
    rows = []
    for n in SIZES:
        f, fp, af = (
            _psd(k, n) for k in ("fft", "frame_power", "accumulate_frame")
        )
        rows.append(
            [
                f"{n:,}",
                _cell(P, lambda p, f=f: p[f] / 1e3, ".3f"),
                _cell(P, lambda p, fp=fp: p[fp] / 1e3, ".3f"),
                _cell(P, lambda p, af=af: p[af] / 1e3, ".3f"),
                _cell(P, lambda p, af=af, f=f: p[af] / p[f], ".2f"),
                _cell(P, lambda p, fp=fp, f=f: p[fp] / p[f], ".2f"),
                _cell(P, lambda p, fp=fp, f=f, n=n: (p[fp] - p[f]) / n, ".3f"),
                _cell(
                    P, lambda p, af=af, fp=fp, n=n: (p[af] - p[fp]) / n, ".3f"
                ),
                _spread(P, lambda p, af=af: p[af]),
            ]
        )
    return table(
        [
            "nfft",
            "fft, µs",
            "frame_power, µs",
            "accumulate_frame, µs",
            "accumulate / fft",
            "frame_power / fft",
            "window + power, ns/bin",
            "fold, ns/bin",
            "accumulate spread",
        ],
        rows,
    )


def split(P: Runs) -> str:
    """A reading's two steps after the power: normalisation, then dB."""
    rows = []
    for n in SPLIT:
        fp, fl, fd = (
            _psd(k, n) for k in ("frame_power", "frame_linear", "frame_db")
        )
        rows.append(
            [
                f"{n:,}",
                _cell(P, lambda p, fl=fl: p[fl] / 1e3, ".3f"),
                _cell(P, lambda p, fd=fd: p[fd] / 1e3, ".3f"),
                _cell(
                    P, lambda p, fl=fl, fp=fp, n=n: (p[fl] - p[fp]) / n, ".3f"
                ),
                _cell(
                    P, lambda p, fd=fd, fl=fl, n=n: (p[fd] - p[fl]) / n, ".3f"
                ),
                _cell(
                    P, lambda p, fd=fd, fl=fl: (p[fd] - p[fl]) / p[fd], ".1%"
                ),
                _spread(P, lambda p, fd=fd: p[fd]),
            ]
        )
    return table(
        [
            "nfft",
            "frame_linear, µs",
            "frame_db, µs",
            "normalisation, ns/bin",
            "dB (frame_db − frame_linear), ns/bin",
            "dB, share of frame_db",
            "frame_db spread",
        ],
        rows,
    )


def fold(P: Runs) -> str:
    """AccTrace's fold on its own, per bin, every mode and size."""
    rows = []
    for m in MODES:
        rows.append(
            [f"`{m}`"]
            + [
                _cell(
                    P,
                    lambda p, k=f"acc_trace::fold[{m},nfft={n}]", n=n: (
                        p[k] / n
                    ),
                    ".3f",
                )
                for n in SIZES
            ]
        )
    return table(["mode, ns/bin"] + [f"{n:,}" for n in SIZES], rows)


def mean(P: Runs) -> str:
    """The isolated `mean` fold beside the derived in-frame difference."""
    rows = []
    for n in SIZES:
        k = f"acc_trace::fold[mean,nfft={n}]"

        def iso(p: Pass, k: str = k, n: int = n) -> float:
            return p[k] / n

        def derived(p: Pass, n: int = n) -> float:
            af, fp = _psd("accumulate_frame", n), _psd("frame_power", n)
            return (p[af] - p[fp]) / n

        rows.append(
            [
                f"{n:,}",
                _cell(P, iso, ".3f"),
                _spread(P, iso),
                _cell(P, derived, ".3f"),
                _spread(P, derived),
            ]
        )
    return table(
        [
            "nfft",
            "`fold[mean]`, ns/bin",
            "its spread",
            "`accumulate_frame − frame_power`, ns/bin",
            "its spread",
        ],
        rows,
    )


def rows(P: Runs) -> str:
    """A power row against the dB row it is timed beside."""
    out = []
    for n, h in ROWS:
        db = f"spectrogram::push[nfft={n},hop={h}]"
        pw = f"spectrogram::push[nfft={n},hop={h},mode=power]"
        out.append(
            [
                f"{n:,}",
                f"{h:,}",
                _cell(P, lambda p, db=db: p[db], ".3f"),
                _spread(P, lambda p, db=db: p[db]),
                _cell(P, lambda p, pw=pw: p[pw], ".3f"),
                _spread(P, lambda p, pw=pw: p[pw]),
                _cell(P, lambda p, db=db, pw=pw: p[db] / p[pw], ".2f"),
                _cell(P, lambda p, pw=pw: 1e3 / p[pw], ".0f"),
            ]
        )
    return table(
        [
            "nfft",
            "hop",
            "dB row, ns/sample",
            "dB spread",
            "power row, ns/sample",
            "power spread",
            "dB / power",
            "power, MSa/s",
        ],
        out,
    )


def cross(P: Runs) -> str:
    """Every row this run shares with 5.6's run, by group: how far it moved.

    A row's move is its median here over its median in the a4 run, minus
    one. The groups #2113 changed the timing context of (PSD's kernel rows,
    whose rotation gained two rows, and the three dB pushes that gained a
    power row beside them) are kept apart from the rest.
    """
    A = runs(u1u4.DATA, u1u4.COMMIT)
    shared = sorted(set(A["portable"][0]) & set(P["portable"][0]))
    moved = {f"spectrogram::push[nfft={n},hop={h}]" for n, h in ROWS}

    def group(k: str) -> str:
        g = k.split("[")[0]
        if g in ("psd::fft", "psd::frame_power", "psd::frame_db"):
            return f"`{g}` (rotation changed, #2113)"
        if k in moved:
            return "`spectrogram::push`, dB beside a new power row (#2113)"
        return f"`{g}`"

    groups: dict[str, list[str]] = {}
    for k in shared:
        groups.setdefault(group(k), []).append(k)

    def delta(b: str, k: str) -> float:
        now = statistics.median(p[k] for p in P[b])
        then = statistics.median(p[k] for p in A[b])
        return now / then - 1.0

    out = []
    for g in sorted(groups):
        ks = groups[g]
        out.append(
            [
                g,
                str(len(ks)),
                " / ".join(
                    format(statistics.median(delta(b, k) for k in ks), "+.2%")
                    for b in BUILDS
                ),
                " / ".join(
                    format(max((delta(b, k) for k in ks), key=abs), "+.2%")
                    for b in BUILDS
                ),
            ]
        )
    out.append(["all shared rows", str(len(shared)), "", ""])
    return table(["rows", "count", "median move", "largest move"], out)


def onesided(P: Runs) -> str:
    """power_onesided's passes in both runs, in steps of the clock.

    The row times ONE call per round, so a pass's minimum is a single
    interval between two clock reads, and a row's passes take only a few
    values, ``STEP_NS`` apart. Every pair of a row's passes, across both
    runs, must differ by a whole number of steps within ``STEP_TOL_NS``, or
    this refuses: the step is a claim the data is checked against. A move
    is the median's change here over 5.6's, also in steps.
    """
    A = runs(u1u4.DATA, u1u4.COMMIT)
    out = []
    for n in ONESIDED:
        k = _psd("power_onesided", n)
        for b in BUILDS:
            then = [p[k] for p in A[b]]
            now = [p[k] for p in P[b]]
            # the clock reads whole ns, so a pass is an integer up to the
            # 1e-9 scaling's rounding
            ns = [round(x) for x in then + now]
            off = max(
                abs(x - y - STEP_NS * round((x - y) / STEP_NS))
                for x in ns
                for y in ns
            )
            if off > STEP_TOL_NS:
                raise SystemExit(
                    f"{k} ({b}): two passes are {off:.1f} ns off a whole "
                    f"number of {STEP_NS} ns steps"
                )
            m0, m1 = statistics.median(then), statistics.median(now)
            out.append(
                [
                    f"{n:,}",
                    b,
                    ", ".join(f"{x:.0f}" for x in then),
                    ", ".join(f"{x:.0f}" for x in now),
                    format(m1 / m0 - 1.0, "+.2%"),
                    format((m1 - m0) / STEP_NS, "+.0f"),
                    format(STEP_NS / m1, ".1%"),
                    " / ".join(
                        format(spread(q, lambda p, k=k: p[k]), ".1%")
                        for q in (A[b], P[b])
                    ),
                ]
            )
    return table(
        [
            "nfft",
            "build",
            "5.6's passes, ns",
            "these passes, ns",
            "median move",
            "in steps",
            "one step / median",
            "spread, 5.6 / here",
        ],
        out,
    )


BLOCKS = {
    "frame": frame,
    "split": split,
    "fold": fold,
    "mean": mean,
    "rows": rows,
    "cross": cross,
    "onesided": onesided,
}


if __name__ == "__main__":
    sys.exit(main("b2094", DATA, COMMIT, BLOCKS, __doc__))
