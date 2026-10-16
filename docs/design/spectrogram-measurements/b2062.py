"""Render the Spectrogram's #2062 tables from the run that measured them.

Entry 5.7 of the measurement record
(``docs/design/spectrogram-measurements.md``) measured U2 at one hop, and
entry 5.6 measured U1 at ``hop`` ``nfft/4`` and ``nfft``: the rows between
them sat past ``jm_bench.h``'s old 32-row cap. With the cap gone (#2357),
one run of ``make bench-interleaved VERSION=0.73.0-2062 K=5`` at e13a27fc1
measured them for #2062, and entry 5.15 quotes this file's output.
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

``--check`` is in ``make docs-invariants``: it fails when the record's
tables differ from what the data renders.
"""

from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from _record import (
    Pass,
    Runs,
    both,
    main,
    med,
    spread,
    table,
)

DATA = Path(__file__).resolve().parent / "b2062"
#: The commit every pass was measured at (``_record.load`` asserts it).
COMMIT = "e13a27fc1"
SIZES = (256, 1024, 4096, 16384, 65536)
#: U2's hops at nfft 1024, and the chunks each is measured at.
HOPS = (64, 256, 1024)
CHUNKS = (256, 1024, 16384)


def _push(n: int, h: int, chunk: int = 0) -> str:
    c = f",chunk={chunk}" if chunk else ""
    return f"spectrogram::push[nfft={n},hop={h}{c}]"


def u1(P: Runs) -> str:
    """U1 at the midpoint hop, ``nfft/2``: entry 5.6's columns."""
    rows = []
    for n in SIZES:
        h = n // 2
        pk = _push(n, h)
        dk = f"spectrogram::direct[nfft={n},hop={h}]"

        def r(p: Pass, pk: str = pk, dk: str = dk) -> float:
            return p[pk] / p[dk]

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


def _cs(q: list[Pass], h: int) -> float:
    """*c_s* at ``hop`` h: the one-push row's per-sample cost."""
    return med(q, lambda p: p[_push(1024, h)])


def _cp(q: list[Pass], h: int) -> float:
    """*c_p* at ``hop`` h: the chunk-1 row's per-sample cost less *c_s*."""
    return med(q, lambda p: p[_push(1024, h, 1)]) - _cs(q, h)


def _pred(q: list[Pass], h: int, c: int) -> float:
    """The model's per-sample cost for a chunk of ``c``: c_s + c_p / C."""
    return _cs(q, h) + _cp(q, h) / c


def u2(P: Runs) -> str:
    """U2's model at each hop: ``c_s`` from the one push, ``c_p`` from the
    chunk-1 row, and each larger chunk against ``c_s + c_p / C``."""
    params, model = [], []
    for h in HOPS:
        base, one = _push(1024, h), _push(1024, h, 1)
        params.append(
            [
                f"{h:,}",
                both(P, lambda q, h=h: _cs(q, h), ".3f"),
                both(P, lambda q, k=base: spread(q, lambda p: p[k]), ".1%"),
                both(P, lambda q, k=one: med(q, lambda p: p[k]), ".3f"),
                both(P, lambda q, h=h: _cp(q, h), ".3f"),
            ]
        )
        for c in CHUNKS:
            k = _push(1024, h, c)
            model.append(
                [
                    f"{h:,}",
                    f"{c:,}",
                    both(P, lambda q, k=k: med(q, lambda p: p[k]), ".3f"),
                    both(P, lambda q, h=h, c=c: _pred(q, h, c), ".3f"),
                    both(
                        P,
                        lambda q, k=k, h=h, c=c: (
                            med(q, lambda p: p[k]) / _pred(q, h, c) - 1
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
