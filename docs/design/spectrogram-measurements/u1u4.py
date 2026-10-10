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
record, so a cell cannot be edited by hand or drift from the data:

- a row's cost in one pass is ``stats.min / stats.iterations``, ns per
  sample for the spectrogram rows and ns per frame for PSD's kernel rows;
- a cell is the median of that over the passes, portable / native;
- a spread is ``(max - min) / median`` over the passes.

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

import argparse
import json
import re
import statistics
import sys
from pathlib import Path
from typing import Callable

HERE = Path(__file__).resolve().parent
DATA = HERE / "u1u4"
RECORD = HERE.parent / "spectrogram-measurements.md"
BUILDS = ("portable", "native")
SIZES = (256, 1024, 4096, 16384, 65536)
BASE = "spectrogram::push[nfft=1024,hop=256]"
ONE = "spectrogram::push[nfft=1024,hop=256,chunk=1]"
DIRECT = "spectrogram::direct[nfft=1024,hop=256]"

Pass = dict[str, float]


def load(build: str) -> list[Pass]:
    """Each pass of one build: row name to cost per unit, in ns."""
    passes = []
    for f in sorted((DATA / build).glob("*-c.json")):
        rows = json.loads(f.read_text())["benchmarks"]
        passes.append(
            {
                r["name"]: r["stats"]["min"] / r["stats"]["iterations"] * 1e9
                for r in rows
            }
        )
    return passes


def med(passes: list[Pass], f: Callable[[Pass], float]) -> float:
    return statistics.median(f(p) for p in passes)


def spread(passes: list[Pass], f: Callable[[Pass], float]) -> float:
    v = [f(p) for p in passes]
    return (max(v) - min(v)) / statistics.median(v)


def both(
    P: dict[str, list[Pass]], f: Callable[[list[Pass]], float], fmt: str
) -> str:
    return " / ".join(format(f(P[b]), fmt) for b in BUILDS)


def table(head: list[str], rows: list[list[str]]) -> str:
    out = ["| " + " | ".join(head) + " |"]
    out.append("| " + " | ".join("---" for _ in head) + " |")
    out += ["| " + " | ".join(r) + " |" for r in rows]
    return "\n".join(out)


def ratio(n: int, h: int) -> Callable[[Pass], float]:
    a = f"nfft={n},hop={h}"
    return lambda p: (
        p[f"spectrogram::push[{a}]"] / p[f"spectrogram::direct[{a}]"]
    )


def u1(P: dict[str, list[Pass]]) -> str:
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


def u2(P: dict[str, list[Pass]]) -> str:
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


def u3_row(P: dict[str, list[Pass]], n: int) -> list[str]:
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


def u3(P: dict[str, list[Pass]]) -> str:
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


def u4(P: dict[str, list[Pass]]) -> str:
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


def render() -> dict[str, str]:
    P = {b: load(b) for b in BUILDS}
    if any(len(P[b]) != 5 for b in BUILDS):
        raise SystemExit(f"u1u4: expected 5 passes per build in {DATA}")
    return {name: f(P) for name, f in BLOCKS.items()}


def _span(name: str) -> re.Pattern[str]:
    return re.compile(
        rf"(<!-- spectrogram-u1u4:{name}:start -->)(.*?)"
        rf"(<!-- spectrogram-u1u4:{name}:end -->)",
        re.S,
    )


def _cells(text: str) -> list[list[str]]:
    """A block's table rows as stripped cells; padding and dashes ignored."""
    rows = []
    for line in text.strip().splitlines():
        cells = [c.strip() for c in line.strip().strip("|").split("|")]
        rows.append([re.sub(r"^:?-+:?$", "---", c) for c in cells])
    return rows


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    mode = ap.add_mutually_exclusive_group()
    mode.add_argument("--write", action="store_true")
    mode.add_argument("--check", action="store_true")
    a = ap.parse_args()
    blocks = render()
    if not (a.write or a.check):
        for name, body in blocks.items():
            print(f"== {name}\n{body}\n")
        return 0
    text = RECORD.read_text()
    stale = []
    for name, body in blocks.items():
        m = _span(name).search(text)
        if m is None:
            raise SystemExit(f"u1u4: no {name} markers in {RECORD.name}")
        if _cells(m.group(2)) != _cells(body):
            stale.append(name)
        text = _span(name).sub(
            lambda m, b=body: m.group(1) + "\n" + b + "\n" + m.group(3), text
        )
    if a.write:
        RECORD.write_text(text)
        print(f"u1u4: wrote {len(blocks)} table block(s) in {RECORD.name}")
        return 0
    if stale:
        print(
            f"u1u4: {RECORD.name}'s {', '.join(stale)} table(s) are not "
            f"what the run's data renders. Run `python "
            f"docs/design/spectrogram-measurements/u1u4.py --write`; a cell "
            f"is the data's output, never an edit."
        )
        return 1
    print(f"u1u4: OK — {len(blocks)} table block(s) match the run's data")
    return 0


if __name__ == "__main__":
    sys.exit(main())
