"""Render the Spectrogram's U1-U4 tables from the run that measured them.

The measurement record's entries 5.6 to 5.9
(``docs/design/spectrogram-measurements.md``) quote one run of
``make bench-interleaved VERSION=0.66.0-a4 K=5`` at c1ae84190 on
cachyos-x8664-ai465. ``bench_interleaved.py`` keeps only each row's best
pass and deletes the per-pass snapshots with its worktrees, so the ten were
saved as they were written, and they are committed beside this file in
``u1u4/<build>/``, filtered to the ``spectrogram::`` and ``psd::`` rows.

Every table cell in those entries is this file's output, between
``<!-- spectrogram-u1u4:<name>:start -->`` and ``:end`` markers in the
record, so a cell cannot be edited by hand or drift from the data. How a
cell is computed, and the ``--write``/``--check`` machinery, are
``_record.py``'s, shared with the other runs the record renders.

Usage::

    python docs/design/spectrogram-measurements/u1u4.py          # print
    python docs/design/spectrogram-measurements/u1u4.py --write  # rewrite
    python docs/design/spectrogram-measurements/u1u4.py --check  # gate

``--check`` is in ``make docs-invariants`` (and so in pre-commit and the
docs gate): it fails when the record's tables differ from what the data
renders. The comparison is cell by cell, so mdformat's column padding is not
a difference.
"""

from __future__ import annotations

import sys
from pathlib import Path
from typing import TYPE_CHECKING

from _record import BUILDS, Pass, Runs, both, main, med, spread, table

if TYPE_CHECKING:
    from collections.abc import Callable

DATA = Path(__file__).resolve().parent / "u1u4"
SIZES = (256, 1024, 4096, 16384, 65536)
BASE = "spectrogram::push[nfft=1024,hop=256]"
ONE = "spectrogram::push[nfft=1024,hop=256,chunk=1]"
DIRECT = "spectrogram::direct[nfft=1024,hop=256]"


def ratio(n: int, h: int) -> Callable[[Pass], float]:
    a = f"nfft={n},hop={h}"
    return lambda p: (
        p[f"spectrogram::push[{a}]"] / p[f"spectrogram::direct[{a}]"]
    )


def u1(P: Runs) -> str:
    rows = []
    for n in SIZES:
        for h in (n // 4, n):
            a = f"nfft={n},hop={h}"
            pk, dk = f"spectrogram::push[{a}]", f"spectrogram::direct[{a}]"
            r = ratio(n, h)
            rows.append(
                [
                    f"{n:,}",
                    f"{h:,}",
                    both(P, lambda q, k=pk: med(q, lambda p: p[k]), ".3f"),
                    both(P, lambda q, k=dk: med(q, lambda p: p[k]), ".3f"),
                    both(
                        P,
                        lambda q, x=pk, y=dk: med(q, lambda p: p[x] - p[y]),
                        ".3f",
                    ),
                    both(P, lambda q, r=r: med(q, r), ".3f"),
                    both(P, lambda q, r=r: max(r(p) for p in q), ".3f"),
                    both(P, lambda q, r=r: spread(q, r), ".1%"),
                ]
            )
    return table(
        [
            "nfft",
            "hop",
            "push, ns/sample",
            "direct, ns/sample",
            "push − direct",
            "push/direct",
            "worst pass",
            "ratio spread",
        ],
        rows,
    )


def u2(P: Runs) -> str:
    def cs(q: list[Pass]) -> float:
        return med(q, lambda p: p[BASE])

    def cp(q: list[Pass]) -> float:
        return med(q, lambda p: p[ONE]) - cs(q)

    rows = [
        [
            "1",
            both(P, lambda q: med(q, lambda p: p[ONE]), ".3f"),
            "(defines *c_p*)",
            "—",
        ]
    ]
    for c in (256, 1024, 16384):
        k = f"spectrogram::push[nfft=1024,hop=256,chunk={c}]"

        def pred(q: list[Pass], c: int = c) -> float:
            return cs(q) + cp(q) / c

        rows.append(
            [
                f"{c:,}",
                both(P, lambda q, k=k: med(q, lambda p: p[k]), ".3f"),
                both(P, pred, ".3f"),
                both(
                    P,
                    lambda q, k=k, pred=pred: (
                        (med(q, lambda p: p[k]) - pred(q)) / pred(q)
                    ),
                    "+.1%",
                ),
            ]
        )
    params = table(
        ["*c_s*, ns/sample", "spread", "*c_p*, ns per push"],
        [
            [
                both(P, cs, ".3f"),
                both(P, lambda q: spread(q, lambda p: p[BASE]), ".1%"),
                both(P, cp, ".3f"),
            ]
        ],
    )
    model = table(
        [
            "chunk",
            "measured, ns/sample",
            "predicted `c_s + c_p/C`",
            "error",
        ],
        rows,
    )
    return params + "\n\n" + model


def u3_row(P: Runs, n: int) -> list[str]:
    """One nfft's kernel costs and the dB conversion's share of a row."""
    f, fp, fd = (
        f"psd::{k}[nfft={n}]" for k in ("fft", "frame_power", "frame_db")
    )

    def cell(g: Callable[[Pass], float], fmt: str) -> str:
        return both(P, lambda q: med(q, g), fmt)

    return [
        f"{n:,}",
        cell(lambda p: p[f] / 1e3, ".3f"),
        cell(lambda p: p[fp] / 1e3, ".3f"),
        cell(lambda p: p[fd] / 1e3, ".3f"),
        cell(lambda p: p[f] / p[fd], ".1%"),
        cell(lambda p: (p[fp] - p[f]) / p[fd], ".1%"),
        cell(lambda p: (p[fd] - p[fp]) / p[fd], ".1%"),
        cell(lambda p: (p[fd] - p[fp]) / n, ".2f"),
        both(P, lambda q: spread(q, lambda p: p[fd]), ".1%"),
    ]


def u3(P: Runs) -> str:
    return table(
        [
            "nfft",
            "fft, µs",
            "frame_power, µs",
            "frame_db, µs",
            "the FFT",
            "window + power",
            "dB conversion",
            "dB, ns per bin",
            "frame_db spread",
        ],
        [u3_row(P, n) for n in SIZES],
    )


def u4(P: Runs) -> str:
    rows = []
    for b in BUILDS:
        q = P[b]
        push = med(q, lambda p: p[BASE])
        sp = spread(q, lambda p: p[BASE])
        check = (
            med(q, lambda p: p["psd::frame_db[nfft=1024]"])
            / 256
            * med(q, lambda p: p[BASE] / p[DIRECT])
        )
        rows.append(
            [
                b,
                f"{push:.2f} (±{sp:.1%})",
                f"{1e3 / push:.0f} M",
                f"{round(1e9 / push / 256, -3):,.0f}",
                f"{check:.2f}",
                f"{(push - check) / check:+.1%}",
            ]
        )
    return table(
        [
            "build",
            "push, ns/sample",
            "samples per second",
            "rows per second",
            "`frame_db`/hop × push/direct",
            "push against it",
        ],
        rows,
    )


BLOCKS = {"u1": u1, "u2": u2, "u3": u3, "u4": u4}


if __name__ == "__main__":
    sys.exit(main("u1u4", DATA, BLOCKS, __doc__))
