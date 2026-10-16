"""Render the Spectrogram's #2062 tables from the run that measured them.

Entry 5.7 of the measurement record
(``docs/design/spectrogram-measurements.md``) measured U2 at one hop, and
entry 5.6 measured U1 at ``hop`` ``nfft/4`` and ``nfft``: the bench held
the other rows out, at 29, under ``jm_bench.h``'s old 32-row cap. With the
cap gone (#2357), one run of ``make bench-interleaved VERSION=0.73.0-2062
K=5`` at e13a27fc1 measured them for #2062, and entry 5.15 quotes this
file's output. The U1 row and the U2 model are ``u1u4.py``'s, at other
hops, so the two runs compute a cell the same way.
``bench_interleaved.py`` keeps only each row's best pass and deletes the
per-pass snapshots with its worktrees, so the ten were saved as they were
written, and they are committed beside this file in ``b2062/<build>/``,
filtered to the ``spectrogram::`` rows, with the merged snapshots'
``doppler_meta`` in ``b2062/meta.json``.

Every table cell in that entry is this file's output, between
``<!-- spectrogram-b2062:<name>:start -->`` and ``:end`` markers in the
record, so a cell cannot be edited by hand or drift from the data. How a
cell is computed, and the ``--write``/``--check`` machinery, are
``_record.py``'s, shared with the other runs the record renders.

Usage::

    python docs/design/spectrogram-measurements/b2062.py          # print
    python docs/design/spectrogram-measurements/b2062.py --write  # rewrite
    python docs/design/spectrogram-measurements/b2062.py --check  # gate

``--check`` is in ``make docs-invariants`` (and so in pre-commit and the
docs gate): it fails when the record's tables differ from what the data
renders.
"""

from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import u1u4
from _record import Pass, Runs, both, main, med, spread, table
from u1u4 import cp, cs, pred, push

DATA = Path(__file__).resolve().parent / "b2062"
#: The commit every pass was measured at (``_record.load`` asserts it).
COMMIT = "e13a27fc1"
#: U2's hops at nfft 1024, and the chunks each is measured at.
HOPS = (64, 256, 1024)
CHUNKS = (256, 1024, 16384)


def u1(P: Runs) -> str:
    """U1 at the midpoint hop, ``nfft/2``: entry 5.6's columns and rows."""
    return table(u1u4.U1_HEAD, [u1u4.u1_row(P, n, n // 2) for n in u1u4.SIZES])


def u2(P: Runs) -> str:
    """5.7's model at each hop: ``c_s`` from the one push, ``c_p`` from the
    chunk-1 row, and each larger chunk against ``c_s + c_p / C``. The
    chunk-1 row's own spread is shown, since ``c_p`` rests on it alone."""
    params, model = [], []
    for h in HOPS:

        def k1(p: Pass, h: int = h) -> float:
            return p[push(h, 1)]

        def k0(p: Pass, h: int = h) -> float:
            return p[push(h)]

        params.append(
            [
                f"{h:,}",
                both(P, lambda q, h=h: cs(q, h), ".3f"),
                both(P, lambda q, f=k0: spread(q, f), ".1%"),
                both(P, lambda q, f=k1: med(q, f), ".3f"),
                both(P, lambda q, f=k1: spread(q, f), ".1%"),
                both(P, lambda q, h=h: cp(q, h), ".3f"),
            ]
        )
        for c in CHUNKS:
            k = push(h, c)
            model.append(
                [
                    f"{h:,}",
                    f"{c:,}",
                    both(P, lambda q, k=k: med(q, lambda p: p[k]), ".3f"),
                    both(P, lambda q, h=h, c=c: pred(q, c, h), ".3f"),
                    both(
                        P,
                        lambda q, k=k, h=h, c=c: (
                            med(q, lambda p: p[k]) / pred(q, c, h) - 1
                        ),
                        "+.1%",
                    ),
                ]
            )
    head = table(
        [
            "hop",
            "*c_s*, ns/sample",
            "spread",
            "chunk 1, ns/sample",
            "chunk 1 spread",
            "*c_p*, ns per push",
        ],
        params,
    )
    body = table(
        [
            "hop",
            "chunk",
            "measured, ns/sample",
            "predicted `c_s + c_p/C`",
            "error",
        ],
        model,
    )
    return head + "\n\n" + body


BLOCKS = {"u1": u1, "u2": u2}


if __name__ == "__main__":
    sys.exit(main("b2062", DATA, COMMIT, BLOCKS, __doc__))
