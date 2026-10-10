"""Render the #2094 baseline's tables from the run that measured them.

The measurement record's entries 5.12 to 5.14
(``docs/design/spectrogram-measurements.md``) quote one run of
``make bench-interleaved VERSION=0.66.0-b2094 K=5`` at c36a042e0 on
cachyos-x8664-ai465: main with #2113's bench rows and none of #2094's
changes (#2105's fold, the fused passes, #2117's dB conversion), so it is
the BEFORE of all three. Its ten per-pass snapshots are committed beside
this file in ``b2094/<build>/``, filtered to the ``psd::``,
``acc_trace::`` and ``spectrogram::`` rows.

Every table cell in those entries is this file's output, between
``<!-- spectrogram-b2094:<name>:start -->`` and ``:end`` markers in the
record. How a cell is computed, and the ``--write``/``--check`` machinery,
are ``_record.py``'s, shared with ``u1u4.py``.

Usage::

    python docs/design/spectrogram-measurements/b2094.py          # print
    python docs/design/spectrogram-measurements/b2094.py --write  # rewrite
    python docs/design/spectrogram-measurements/b2094.py --check  # gate

``--check`` is in ``make docs-invariants``.
"""

from __future__ import annotations

import sys
from pathlib import Path
from typing import TYPE_CHECKING

from _record import Pass, Runs, both, main, med, spread, table

if TYPE_CHECKING:
    from collections.abc import Callable

DATA = Path(__file__).resolve().parent / "b2094"
SIZES = (256, 1024, 4096, 16384, 65536)
#: The sizes #2113 gave a frame_linear row, so the split is measured there.
SPLIT = (256, 1024, 65536)
MODES = ("mean", "exp", "maxhold", "minhold")
#: (nfft, hop) of the power rows, each timed beside its dB row.
ROWS = ((256, 64), (1024, 256), (65536, 16384))


def _psd(k: str, n: int) -> str:
    return f"psd::{k}[nfft={n}]"


def _cell(P: Runs, g: Callable[[Pass], float], fmt: str) -> str:
    return both(P, lambda q: med(q, g), fmt)


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
                _cell(P, lambda p, fp=fp, f=f, n=n: (p[fp] - p[f]) / n, ".2f"),
                _cell(
                    P, lambda p, af=af, fp=fp, n=n: (p[af] - p[fp]) / n, ".2f"
                ),
                both(P, lambda q, af=af: spread(q, lambda p: p[af]), ".1%"),
            ]
        )
    return table(
        [
            "nfft",
            "fft, µs",
            "frame_power, µs",
            "accumulate_frame, µs",
            "accumulate / fft",
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
                    P, lambda p, fl=fl, fp=fp, n=n: (p[fl] - p[fp]) / n, ".2f"
                ),
                _cell(
                    P, lambda p, fd=fd, fl=fl, n=n: (p[fd] - p[fl]) / n, ".2f"
                ),
                _cell(
                    P, lambda p, fd=fd, fl=fl: (p[fd] - p[fl]) / p[fd], ".1%"
                ),
                both(P, lambda q, fd=fd: spread(q, lambda p: p[fd]), ".1%"),
            ]
        )
    return table(
        [
            "nfft",
            "frame_linear, µs",
            "frame_db, µs",
            "normalisation, ns/bin",
            "dB, ns/bin",
            "dB, share of frame_db",
            "frame_db spread",
        ],
        rows,
    )


def fold(P: Runs) -> str:
    """The fold on its own, per bin, beside the fold inside PSD's frame."""
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
                    ".2f",
                )
                for n in SIZES
            ]
        )
    rows.append(
        ["in PSD's frame (`mean`)"]
        + [
            _cell(
                P,
                lambda p, n=n: (
                    (
                        p[_psd("accumulate_frame", n)]
                        - p[_psd("frame_power", n)]
                    )
                    / n
                ),
                ".2f",
            )
            for n in SIZES
        ]
    )
    return table(["mode, ns/bin"] + [f"{n:,}" for n in SIZES], rows)


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
                _cell(P, lambda p, pw=pw: p[pw], ".3f"),
                _cell(P, lambda p, db=db, pw=pw: p[db] / p[pw], ".2f"),
                _cell(P, lambda p, pw=pw: 1e3 / p[pw], ".0f"),
                both(P, lambda q, pw=pw: spread(q, lambda p: p[pw]), ".1%"),
            ]
        )
    return table(
        [
            "nfft",
            "hop",
            "dB row, ns/sample",
            "power row, ns/sample",
            "dB / power",
            "power, MSa/s",
            "power spread",
        ],
        out,
    )


BLOCKS = {"frame": frame, "split": split, "fold": fold, "rows": rows}


if __name__ == "__main__":
    sys.exit(main("b2094", DATA, BLOCKS, __doc__))
