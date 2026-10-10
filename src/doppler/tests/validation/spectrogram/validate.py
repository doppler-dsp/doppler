"""Certification evidence for the streaming Spectrogram.

The Spectrogram is `native/inc/doppler/spectrogram/spectrogram_core.h`: a
stream of any-size chunks in, rows of `nfft`-bin spectra out, one row every
`hop` samples -- linear power by default, dBFS when asked for by name. Its
Python face is #1894's slice 3b and does not exist yet, so this follows
`docs/dev/contributing/validation.md`, "Certifying a component with no
binding": **the C harness
`native/validation/spectrogram_certify.c` measures; this file renders and
asserts.** Nothing there decides what is acceptable; every threshold lives in
`limits()` below.

The oracle is the header's own definition, built without the object: row *k*
of a stream is `dp_psd_frame_linear()` (power mode) or `dp_psd_frame_db()`
(dB mode) of samples `[k*hop, k*hop + nfft)`, and the flushed row is the same
of that slice zero-padded. Every certification block runs in both modes.
PSD's kernel is the one part the oracle shares with the object, and it is
certified on its own (`src/doppler/spectral/tests/validation/psd/`).

Run it directly to regenerate `results.md`:

    python src/doppler/tests/validation/spectrogram/validate.py

or with `--check` to see whether the committed report is stale.
"""

from __future__ import annotations

import sys
from pathlib import Path

from doppler.tests._repo import build_dir, exe, repo_root
from doppler.tests._validation_common import (
    Report,
    cli,
    harness_blocks,
    p2db_pin_db,
)

HERE = Path(__file__).resolve().parent
ROOT = repo_root(__file__)
HARNESS = exe(
    build_dir(__file__) / "native/validation/validate_spectrogram_certify"
)

R = Report()

#: Bins per row in the harness's floor blocks.
NFFT = 1024

#: The quiet tone level the leakage count is read at, and the clamp.
QUIET_DBFS = -120.0
FLOOR_DB = -200.0


def _i(row: dict, key: str) -> int:
    return int(row[key])


def _f(row: dict, key: str) -> float:
    return float(row[key])


def _total(rows: list[dict], key: str) -> int:
    return sum(_i(r, key) for r in rows)


#: The harness's mode names, the default first, and how the report says them.
MODES = {"power": "power", "db": "dB"}


def _mode(row: dict) -> str:
    """A row's mode, as the report writes it."""
    return MODES[str(row["mode"])]


def _both(rows: list[dict]) -> bool:
    """Whether a block ran in both modes, and in no other."""
    return {str(r["mode"]) for r in rows} == set(MODES)


def _in(rows: list[dict], mode: str) -> list[dict]:
    """A block's rows in one mode."""
    return [r for r in rows if str(r["mode"]) == mode]


def _clear(rows: list[dict]) -> list[dict]:
    """Noise rows where no frame's median was clamped.

    A frame's median is clamped exactly when half its bins or more read
    the floor, so a row whose worst frame has fewer than NFFT/2 there has
    every median unclamped, and the clamp cannot bias their mean.
    """
    return [r for r in rows if _i(r, "max_at_floor") < NFFT // 2]


def _leak(d, window: str) -> list[tuple[int, float, float]]:
    """A window's leakage: (offset, +k dBc, -k dBc), from the run."""
    return [
        (_i(r, "offset"), _f(r, "plus_dbc"), _f(r, "minus_dbc"))
        for r in d["leakage"]
        if str(r["window"]) == window
    ]


def _raised(d, window: str) -> tuple[int, int]:
    """Bins a quiet tone raises off the floor: (from its leakage, measured).

    A tone at QUIET_DBFS lifts every bin whose leakage puts it above the
    clamp, so the count is predicted from the full-scale leakage alone.
    """
    predicted = 1 + sum(
        (QUIET_DBFS + p > FLOOR_DB) + (QUIET_DBFS + m > FLOOR_DB)
        for _, p, m in _leak(d, window)
    )
    row = next(
        r
        for r in d["floor_tone"]
        if str(r["window"]) == window and _f(r, "level_dbfs") == QUIET_DBFS
    )
    return predicted, NFFT - _i(row, "bins_at_floor")


def _leak_all(d) -> list[tuple[int, float, float]]:
    """Every leakage row, all windows."""
    return [
        (_i(r, "offset"), _f(r, "plus_dbc"), _f(r, "minus_dbc"))
        for r in d["leakage"]
    ]


def _all_floor_levels(d) -> list[float]:
    """Noise levels at which every frame of every window is all floor."""
    rows = d["floor_noise"]
    levels = sorted({_f(r, "level_dbfs") for r in rows}, reverse=True)
    return [
        lv
        for lv in levels
        if all(
            _i(r, "min_at_floor") == NFFT
            for r in rows
            if _f(r, "level_dbfs") == lv
        )
    ]


def characterise(d) -> None:
    R.md("## 2. Characterisation")
    R.md()
    R.md(
        "Every number below comes from `native/validation/"
        "spectrogram_certify.c`, run through the Spectrogram's own C "
        "interface against the oracle above. Unless a column says "
        "otherwise, a pass is a count of zero. Sections 2.1 to 2.6 run in "
        "both modes, power (the default) first, and each table leads with "
        "the mode; the oracle reads each mode's rows with that mode's call. "
        "Section 2.7 is the dB floor, so it is dB only."
    )
    R.md()

    R.md(
        "### 2.1 Rows are PSD's reading of their slice, however the stream "
        "is cut (C §2, §5, §15)"
    )
    R.md()
    R.md(
        "Each shape streams a seeded complex Gaussian through every "
        "distinct one of: chunks of 1, 7, `nfft`-1, `nfft`, `nfft`+1 and "
        "3`nfft`+5, the whole stream at once, and 64 seeded random splits "
        "of 1 to 2`nfft`+1 (a chunk size that repeats, as `nfft`-1 does at "
        "`nfft` 8 and 2, runs once). The output is NaN-filled before every "
        "partition, so a row a push counts but does not write cannot pass "
        "as the previous partition's. Every push is sized by "
        "`push_max_out`, which "
        "must equal the rows the stream's arithmetic says it completes "
        "(*room wrong*), and must take its whole chunk (*consumed short*). "
        "After each push, fewer than `nfft` samples may be held, by "
        "arithmetic rather than the object's own count: *r* rows retire "
        "*r*·`hop` samples, and what is left must be fewer than `nfft` "
        "(*carry over*). `pending` must be the "
        "samples no written row covers (*pending wrong*). In the "
        "one-sample partitions a row must come back with the push that "
        "delivers its last sample (*late*). A partition is *bad* if any "
        "count is non-zero or any row differs from the oracle by a bit."
    )
    R.md()
    R.table(
        [
            "mode",
            "nfft",
            "hop",
            "window",
            "stream",
            "partitions",
            "rows each",
            "pushes",
            "bad partitions",
            "bad rows",
            "room wrong",
            "consumed short",
            "carry over",
            "pending wrong",
            "late",
        ],
        [
            [
                _mode(r),
                f"{_i(r, 'nfft'):,}",
                f"{_i(r, 'hop'):,}",
                str(r["window"]),
                f"{_i(r, 'length'):,}",
                f"{_i(r, 'partitions')}",
                f"{_i(r, 'rows'):,}",
                f"{_i(r, 'pushes'):,}",
                f"{_i(r, 'bad_partitions')}",
                f"{_i(r, 'bad_rows') + _i(r, 'count_wrong')}",
                f"{_i(r, 'room_wrong')}",
                f"{_i(r, 'consumed_short')}",
                f"{_i(r, 'carry_over')}",
                f"{_i(r, 'pending_wrong')}",
                f"{_i(r, 'late_rows')}",
            ]
            for r in d["rows"]
        ],
    )
    R.md()
    R.md(
        "*Bad rows* also counts a partition whose row count differs from "
        "`(len - nfft) / hop + 1`."
    )
    R.md()

    R.md("### 2.2 A short output slows the stream and loses nothing (C §6)")
    R.md()
    R.md(
        "`nfft` 256, `hop` 64, Hann, 100,003 samples. The output holds "
        "*room* rows plus 13 floats, so a push may use only whole rows of "
        "it, and the caller re-offers what a push did not take. *Offered* "
        "counts every offer, re-offers included; *stalls* are the pushes "
        "that took less than offered. A stall is right only when the room "
        "is full and the next sample would complete a row, which the "
        "harness decides from the samples taken so far, not from the "
        "object (*not maximal* counts any other). *Over room* counts "
        "writes past "
        "the whole rows; *partial* counts writes that were not whole rows; "
        "*tail touched* counts pushes that changed a float past what they "
        "wrote (the buffer is NaN-filled first)."
    )
    R.md()
    R.table(
        [
            "mode",
            "chunk",
            "room (rows)",
            "offered",
            "taken",
            "stalls",
            "rows",
            "bad rows",
            "over room",
            "partial",
            "tail touched",
            "not maximal",
            "carry over",
        ],
        [
            [
                _mode(r),
                f"{_i(r, 'chunk'):,}",
                f"{_i(r, 'room_rows')}",
                f"{_i(r, 'offered'):,}",
                f"{_i(r, 'taken'):,}",
                f"{_i(r, 'stalls'):,}",
                f"{_i(r, 'rows'):,}",
                f"{_i(r, 'bad_rows') + _i(r, 'count_wrong')}",
                f"{_i(r, 'over_room')}",
                f"{_i(r, 'partial_writes')}",
                f"{_i(r, 'tail_touched')}",
                f"{_i(r, 'not_maximal')}",
                f"{_i(r, 'carry_over')}",
            ]
            for r in d["backpressure"]
        ],
    )
    R.md()
    bp = d["backpressure"]
    chunks = list(dict.fromkeys(_i(r, "chunk") for r in bp))
    calm = [
        c
        for c in chunks
        if all(_i(r, "stalls") == 0 for r in bp if _i(r, "chunk") == c)
    ]
    why = (
        " A chunk shorter than the hop completes at most one row, which "
        "every room holds."
        if calm and all(c < 64 for c in calm)
        else ""
    )
    R.md(
        f"Chunks of {', '.join(f'{c:,}' for c in calm) or 'none'} never "
        f"stall at these rooms.{why} The others stall up to "
        f"{max(_i(r, 'stalls') for r in bp):,} times, so the path is "
        "exercised."
    )
    R.md()

    R.md("### 2.3 rows_for and push_max_out, from every carry state (C §4)")
    R.md()
    R.md(
        "For every stream position `p` from 0 to `nfft` + `hop` - 1, which "
        "reaches every carry state the shape has, the state is restored "
        "into a work object and each of nine offers is tried: 0, 1, "
        "`hop`-1, `hop`, `hop`+1, `nfft`-1, `nfft`, `nfft`+1 and "
        "3`nfft`+5. The truth is the stream's arithmetic, "
        "`R(p + n) - R(p)` rows with `R(m) = (m - nfft) / hop + 1`. "
        "`rows_for` must equal it, `push_max_out` must be it times `nfft`, "
        "a push with exactly that room must write it and take all `n`, and "
        "a push with one row less must write one row fewer and take less. "
        "*Saturates* is whether `push_max_out(SIZE_MAX)` reads `SIZE_MAX` "
        "where the row count times `nfft` would wrap."
    )
    R.md()
    R.table(
        [
            "mode",
            "nfft",
            "hop",
            "positions",
            "trials",
            "rows_for wrong",
            "push_max_out wrong",
            "push wrong",
            "one row less wrong",
            "saturates",
        ],
        [
            [
                _mode(r),
                f"{_i(r, 'nfft'):,}",
                f"{_i(r, 'hop'):,}",
                f"{_i(r, 'positions'):,}",
                f"{_i(r, 'trials'):,}",
                f"{_i(r, 'rows_for_wrong')}",
                f"{_i(r, 'max_out_wrong')}",
                f"{_i(r, 'push_wrong')}",
                f"{_i(r, 'one_less_wrong')}",
                "yes" if _i(r, "saturates") else "**no**",
            ]
            for r in d["sizing"]
        ],
    )
    R.md()

    R.md("### 2.4 Flush: on the hop grid, once, only if owed (C §7, §8)")
    R.md()
    R.md(
        "For every stream length from 0 to 4`nfft`+3: push it, flush, "
        "flush again, then push `nfft` more. `pending` must be the samples "
        "no written row covers (*pending wrong*). The flush must write a "
        "row exactly when that is non-zero (*wrong decision*), and the row "
        "must be PSD's reading of the zero-padded slice at the next row start "
        "`k*hop`, not at the first uncovered sample (*off grid*). A second "
        "flush must write nothing, `consumed` and `pending` must read 0 "
        "after it (*after flush*), and the next `nfft` samples must be row "
        "0 of a new stream (*bad restart*)."
    )
    R.md()
    R.table(
        [
            "mode",
            "nfft",
            "hop",
            "window",
            "lengths",
            "rows flushed",
            "pending wrong",
            "wrong decision",
            "off grid",
            "second flush",
            "after flush",
            "bad restart",
        ],
        [
            [
                _mode(r),
                f"{_i(r, 'nfft')}",
                f"{_i(r, 'hop')}",
                str(r["window"]),
                f"{_i(r, 'lengths'):,}",
                f"{_i(r, 'emitted'):,}",
                f"{_i(r, 'wrong_pending')}",
                f"{_i(r, 'wrong_decision')}",
                f"{_i(r, 'off_grid')}",
                f"{_i(r, 'second_nonzero')}",
                f"{_i(r, 'after_flush_wrong')}",
                f"{_i(r, 'bad_restart')}",
            ]
            for r in d["flush"]
        ],
    )
    R.md()
    two = next(
        (r for r in d["flush"] if _i(r, "nfft") == 2 and _i(r, "hop") == 1),
        None,
    )
    if two is not None:
        R.md(
            f"`nfft` 2 at `hop` 1 flushes {_i(two, 'emitted')} row(s) over "
            f"its {_i(two, 'lengths')} lengths. From 2 samples on, every "
            "sample is already in a row, so only a 1-sample stream owes one."
        )
    R.md()

    R.md("### 2.5 The state blob (C §10, §14, §14b)")
    R.md()
    R.md(
        "At every cut point from 0 to 3`nfft`+3 (every 13th at `nfft` "
        "1024), a stream is pushed to the cut and serialized into two "
        "buffers pre-filled with different bytes (*unwritten* counts cuts "
        "where they differ). The blob is restored into a **new** object "
        "that already holds a stray 3-sample carry, so the restore must "
        "replace the carry and reset the count rather than find them "
        "right. It must then read `consumed` 0 and the source's `pending` "
        "(*after restore*), and finish the stream in pieces of 5 to the "
        "oracle's rows bit for bit (*resume bad*). The size must be "
        "one number per `nfft` across cuts, windows, betas and hops "
        "(*sizes*, *varies*). A refusal target holding its own stream is "
        "offered the blob with a corrupt magic, version or size, the blob "
        "of the same `nfft` at another hop, and the blob of half the "
        "`nfft` zero-padded to this size; each must be refused, and the "
        "target must be unchanged in `pending`, `consumed` and, at the "
        "end, in the rows its next push makes against an untouched twin "
        "(*changed*). Last, the blob is restored into an object of "
        "**another window**, and into one of **the other mode**, which the "
        "header says are NOT refused: the rows must continue as that "
        "window's own (*window wrong*), and in that mode's units (*mode "
        "wrong*)."
    )
    R.md()
    R.table(
        [
            "mode",
            "nfft",
            "hop",
            "window",
            "cuts",
            "bytes",
            "sizes",
            "varies",
            "unwritten",
            "after restore",
            "resume bad",
            "envelope refused",
            "hop refused",
            "nfft refused",
            "changed",
            "window refused",
            "window wrong",
            "mode refused",
            "mode wrong",
        ],
        [
            [
                _mode(r),
                f"{_i(r, 'nfft'):,}",
                f"{_i(r, 'hop')}",
                str(r["window"]),
                f"{_i(r, 'cuts'):,}",
                f"{_i(r, 'bytes'):,}",
                f"{_i(r, 'distinct_sizes')}",
                f"{_i(r, 'size_varies')}",
                f"{_i(r, 'unwritten')}",
                f"{_i(r, 'after_restore_wrong') + _i(r, 'restore_refused')}",
                f"{_i(r, 'resume_bad')}",
                f"{_i(r, 'corrupt_refused')} of {_i(r, 'corrupt_tried')}",
                f"{_i(r, 'hop_refused')} of {_i(r, 'hop_tried')}",
                f"{_i(r, 'nfft_refused')} of {_i(r, 'nfft_tried')}",
                f"{_i(r, 'target_changed')}",
                f"{_i(r, 'window_refused')}",
                f"{_i(r, 'window_wrong')}",
                f"{_i(r, 'mode_refused')}",
                f"{_i(r, 'mode_wrong')}",
            ]
            for r in d["state"]
        ],
    )
    R.md()
    R.md(
        "The impossible-carry refusal (a drained carry fewer than "
        "`nfft` - `hop` with a row out) needs the framer's blob layout, so "
        "it is pinned in C (§14) and certified at scale in the framer's own "
        "report rather than here."
    )
    R.md()

    R.md(
        "### 2.6 A full-scale tone reads 1.0, or 0 dBFS, in bin nfft/2 + k "
        "(C §3, §9)"
    )
    R.md()
    R.md(
        "A unit complex tone on bin *k*, for every bin *k* from "
        "-`nfft`/2 to `nfft`/2 - 1, one row each. *Peak wrong* counts rows "
        "whose largest bin is not at index `nfft`/2 + *k*. The error is "
        "that bin's distance from full scale in the row's own units: from "
        "1.0 for a power row, from 0 dB for a dB row."
    )
    R.md()
    R.table(
        ["mode", "window", "nfft", "bins", "peak wrong", "largest |error|"],
        [
            [
                _mode(r),
                str(r["window"]),
                f"{_i(r, 'nfft'):,}",
                f"{_i(r, 'bins'):,}",
                f"{_i(r, 'peak_wrong')}",
                f"{_f(r, 'max_abs_err'):.1e}"
                + (" dB" if str(r["mode"]) == "db" else ""),
            ]
            for r in d["level"]
        ],
    )
    R.md()

    R.md("### 2.7 The dB floor (C §13)")
    R.md()
    R.md(
        "This section is the dB mode's alone: a power row has no floor but "
        "float32's, and an all-zero frame reads 0 there (C §13). PSD's dB "
        "conversion (`dp_power_to_db_f32`, "
        f"{R.ref('db_row_fast_conversion_power')}) reads 1e-20 and below as "
        "exactly -200, so no bin "
        "of a dB row reads below -200 dB. The measurement record's entry 5.4 "
        "(`docs/design/spectrogram-measurements.md`) is the long form; "
        "these are its numbers, at `nfft` 1024. An all-zero frame:"
    )
    R.md()
    R.table(
        ["window", "bins at -200 dB", "lowest", "highest"],
        [
            [
                str(r["window"]),
                f"{_i(r, 'bins_at_floor'):,} of {_i(r, 'bins'):,}",
                f"{_f(r, 'min_db'):.4f}",
                f"{_f(r, 'max_db'):.4f}",
            ]
            for r in d["floor_zero"]
        ],
    )
    R.md()
    R.md(
        "An on-bin tone of power L dBFS reads L in its bin down to the "
        "floor, then the floor. Its other bins show the window's leakage "
        "on the grid: PSD's windows are periodic (#2053), so Hann and "
        "Blackman-Harris, sums of cosines that complete whole periods over "
        "the frame, put an on-bin tone into its first one (Hann) or three "
        "(Blackman-Harris) neighbours each side and only float rounding "
        "further out; Kaiser, not a sum of cosines, leaks into every bin; "
        "and the rectangular window, whose transform is one bin, into none."
    )
    R.md()
    tone = d["floor_tone"]
    levels = sorted({_f(r, "level_dbfs") for r in tone}, reverse=True)
    windows = list(dict.fromkeys(str(r["window"]) for r in tone))
    R.table(
        ["L, dBFS", *windows],
        [
            [
                f"{lv:.0f}",
                *[
                    f"{_f(r, 'tone_bin_db'):.4f} "
                    f"({1024 - _i(r, 'bins_at_floor')})"
                    for w in windows
                    for r in tone
                    if str(r["window"]) == w and _f(r, "level_dbfs") == lv
                ],
            ]
            for lv in levels
        ],
    )
    R.md()
    R.md(
        "*The tone's bin in dB, and in brackets the bins of 1,024 above "
        "the floor.*"
    )
    R.md()
    R.md(
        "Those other bins are the window's leakage on the grid. A "
        "full-scale on-bin tone puts this much into the bins beside it "
        "(dBc, the +k side; the -k side agrees to within rounding):"
    )
    R.md()
    R.table(
        ["window", *[f"+-{k}" for k in range(1, 5)]],
        [
            [w, *[f"{p:.1f}" for k, p, _ in _leak(d, w) if k <= 4]]
            for w in windows
        ],
    )
    R.md()
    R.md(
        f"A tone at {QUIET_DBFS:.0f} dBFS lifts exactly the bins whose "
        f"leakage puts them above the clamp, so the count it leaves above "
        f"the floor is predicted from the leakage of every offset the "
        f"harness reads (up to +-{max(k for k, _, _ in _leak_all(d))}):"
    )
    R.md()
    R.table(
        ["window", "predicted from leakage", "measured"],
        [[w, *map(str, _raised(d, w))] for w in windows],
    )
    R.md()
    R.md(
        "Wideband noise of total power L reaches the floor sooner, by about "
        "`10·log10(nfft)`. Each bin's power is exponential with mean "
        "`mu = P·ENBW/n` against the tone reference, so the median is "
        "`L + 10·log10(ENBW/n) + 10·log10(ln 2)` and a bin reads the floor "
        "with probability `1 - exp(-1e-20/mu)`. The harness averages "
        f"{_i(d['floor_noise'][0], 'frames')} seeded frames per window "
        "and level and prints both expectations beside the means."
    )
    R.md()
    R.table(
        [
            "window",
            "L, dBFS",
            "median: model",
            "median: mean",
            "at floor: model",
            "at floor: mean",
            "at floor: fewest",
            "at floor: most",
        ],
        [
            [
                str(r["window"]),
                f"{_f(r, 'level_dbfs'):.0f}",
                f"{_f(r, 'expected_median_db'):.3f}",
                f"{_f(r, 'mean_median_db'):.3f}",
                f"{_f(r, 'expected_at_floor'):.1f}",
                f"{_f(r, 'mean_at_floor'):.1f}",
                f"{_i(r, 'min_at_floor'):,}",
                f"{_i(r, 'max_at_floor'):,}",
            ]
            for r in d["floor_noise"]
            if _f(r, "level_dbfs") not in _all_floor_levels(d)
        ],
    )
    R.md()
    gone = _all_floor_levels(d)
    if gone:
        R.md(
            f"At {', '.join(f'{lv:.0f}' for lv in gone)} dBFS every frame "
            f"of every window is all floor, {NFFT:,} of {NFFT:,} bins, so "
            "the table leaves those levels out."
        )
        R.md()


def review(d) -> None:
    R.md("## 3. Review")
    R.md()
    R.find(
        "byte_written_claim",
        "FIXED",
        "The inventory (§1) found the header's \"every one [byte] "
        'written" for `get_state` pinned only by heap contents: the C test '
        "compared two `malloc`'d buffers, which agree or not by what the "
        "allocator handed back. §14 now serializes into buffers pre-filled "
        "with 0xAA and 0x55 and requires them equal. It goes red under an "
        "unwritten 8-byte tail (`state_bytes` + 8) and under the framer's "
        "pad memset removed; §2.5 measures it at every cut.",
    )
    R.find(
        "claims_c_only_no_python_face",
        "C-ONLY",
        "Every claim here is verified in C, because the Spectrogram has no "
        "Python face yet: #1894's slice 3b declares one through jm, and it "
        "waits on just-buildit/just-makeit#2152. When it lands, the face "
        "is a binding over the claims certified here.",
    )
    R.find(
        "bin_reads_no_lower_db",
        "BY DESIGN",
        "A bin reads no lower than -200 dB, so a tone under the floor and "
        "digital silence give the same row (§2.7), and noise reaches it "
        "about `10·log10(nfft)` sooner. A logarithm has to stop somewhere; "
        "the header and the guide now say where. It is the dB mode's "
        "floor: a caller that must tell zero from tiny takes power rows, "
        f"the default since #1968 ({R.ref('mode_power_reserved_refused')}), "
        "whose only floor is float32's and "
        "where an all-zero frame reads exactly 0.",
    )
    R.find(
        "blob_restores_into_other_config",
        "BY DESIGN",
        "A blob restores into an object of another window, beta or mode "
        "without complaint, and the rows that follow are that object's "
        "(§2.5). The blob carries the stream position and no "
        "configuration, so keeping the create arguments the same is the "
        "caller's precondition, and the header states it. `nfft` and `hop` "
        "are the exceptions, refused because they change what the position "
        "means. The mode is accepted by #2022's own criterion: a setting is "
        'packed as a reject key when it "changes what the blob means (a '
        "mode, a rate)\", and the Spectrogram's blob is the carry, the same "
        "samples whichever units the rows are read in.",
    )
    R.find(
        "object_not_thread_safe",
        "BY DESIGN",
        "One object is not thread-safe: the kernel uses the object's own "
        "scratch. That is a contract, not something a test can prove. "
        "Threads each own an object, and the state blob is how a stream "
        "moves between them.",
    )
    lv = d["level"]
    pw = max((_f(r, "max_abs_err") for r in _in(lv, "power")), default=0.0)
    R.find(
        "mode_power_reserved_refused",
        "FIXED",
        "`mode = power` was reserved and refused. #1968 wired it to PSD's "
        "normalised per-frame power, `dp_psd_frame_linear`, and made it "
        "the default, since the dB conversion is most of a dB row's cost "
        "(the measurement record's entry 5.8); dB rows are asked for by "
        "name. Every certification block now runs in both modes: a power "
        "row is `dp_psd_frame_linear` of its slice bit for bit (§2.1), and "
        f"a full-scale tone reads 1.0 to within {pw:.1e} under every window "
        "(§2.6).",
    )
    hann2 = next(p for k, p, _ in _leak(d, "hann") if k == 2)
    R.find(
        "rows_inherited_symmetric",
        "FIXED",
        "Rows inherited PSD's symmetric windows, which are not orthogonal "
        "on the N-point grid, so an on-bin tone under Hann leaked -69.7 dBc "
        "into bins +-2. PSD's windows are periodic now (#2053) and the rows "
        "follow, with no change here: under Hann an on-bin tone puts "
        f"{hann2:.1f} dBc into bins +-2, float rounding (§2.7).",
    )
    R.find(
        "nfft_power_of_two_row_width",
        "BY DESIGN",
        "`nfft` must be a power of two, and the frame length and the row "
        "width are the same number: a frame PSD would zero-pad to a longer "
        "transform is refused rather than given rows wider than its frames. "
        "Separating the two is #1966.",
    )
    R.find(
        "flush_stays_explicit",
        "BY DESIGN",
        "`flush` stays explicit. Only the caller knows where its own stream "
        "ends, and the transports' end-of-stream marker can be dropped "
        "(PUB/SUB) or repeated (PUSH/PULL), so a flush implied by it could "
        "lose the last row or emit one mid-stream. The survey is the "
        "measurement record's entry 5.5.",
    )
    R.find(
        "speed_not_certified",
        "BY DESIGN",
        "Speed is not in this report, which certifies what a caller may "
        "rely on. What a row costs and how many rows one core sustains are "
        "the design's U1 to U4, measured on a pinned machine and recorded "
        "in the measurement record's entries 5.6 to 5.9. The decision they "
        "raised, the cost of the dB conversion, made power rows the default "
        f"(#1968, {R.ref('mode_power_reserved_refused')}) and gives dB rows a "
        "faster `log10` (#2074, through "
        "#2094).",
    )
    R.find(
        "hop_refusal_masked_in_c",
        "FIXED",
        "Sabotaging the header's refusals (C21) found the hop refusal "
        "masked in C. With the framer's stored-hop check removed, §14 "
        "stayed green: its other-hop blob had rows out, so it also failed "
        "the counter check (written - frames·hop is not the carry). With "
        "no row out the counters agree under any hop, and only the stored "
        "hop refuses. §14 now pins that case, which the same sabotage turns "
        "red; §2.5's every-cut sweep already caught it at the cuts before "
        "the first row.",
    )
    tone = d["floor_tone"]
    above = [r for r in tone if _f(r, "level_dbfs") >= FLOOR_DB]
    moved = max(
        (abs(_f(r, "tone_bin_db") - _f(r, "level_dbfs")) for r in above),
        default=0.0,
    )
    R.find(
        "db_row_fast_conversion_power",
        "BY DESIGN",
        "**A dB row is the fast conversion of the power row** (#2094, the "
        "owner's decision on #2074). Every dB value is "
        "`dp_power_to_db_f32` of the matching power value, bit for bit "
        "(C §16): 10·log10 within 0.01 dB, 3.25e-4 dB measured over every "
        "float32, exact at every power of two. So a dB reading off a power "
        "of two moves from the exact logarithm by at most that. In this "
        f"report an on-bin tone's level moved by up to {moved:.4f} dB "
        "(§2.7), the full-scale level is still within 1e-4 dB (§2.6), and "
        "the -200 dB floor is exact.",
    )


def limits(d) -> None:
    R.md("## 4. Limits")
    R.md()
    R.md("Claims a caller may rely on, asserted by this run.")
    R.md()

    # Every limit below is over a block's rows, and all() over an empty
    # block is True: each one also requires its block to have run.
    blocks = ("rows", "backpressure", "sizing", "flush", "state", "level")
    R.limit(
        all(_both(d[b]) for b in blocks),
        "every certification block below ran in both modes, power and dB, "
        "so each limit that follows holds in both",
    )
    rows = d["rows"]
    parts = _total(rows, "partitions")
    R.limit(
        bool(rows)
        and all(_i(r, "rows") > 0 for r in rows)
        and all(
            _i(r, "bad_rows") == 0 and _i(r, "count_wrong") == 0 for r in rows
        ),
        f"every row is PSD's reading of its slice in the row's mode, "
        f"dp_psd_frame_linear or dp_psd_frame_db, bit for bit, and the row "
        f"count is (len - nfft)/hop + 1, under all {parts // 2} distinct "
        f"partitions of {len(_in(rows, 'power'))} shapes (nfft 2 to 4,096, "
        f"every window), in each mode",
    )
    R.limit(
        bool(rows)
        and all(
            _i(r, "room_wrong") == 0 and _i(r, "consumed_short") == 0
            for r in rows
        ),
        f"push_max_out is exactly the rows a push completes, and a push "
        f"given that room takes its whole chunk, before every one of "
        f"{_total(rows, 'pushes'):,} pushes",
    )
    R.limit(
        bool(rows) and all(_i(r, "carry_over") == 0 for r in rows),
        "after every push, fewer than nfft samples are held",
    )
    R.limit(
        bool(rows) and all(_i(r, "pending_wrong") == 0 for r in rows),
        "after every push, pending is the samples no written row covers",
    )
    R.limit(
        bool(rows)
        and all(_i(r, "late_rows") == 0 for r in rows)
        and all(_i(r, "pushes") > _i(r, "length") for r in rows),
        "fed one sample at a time, a row comes back with the push that "
        "delivers its last sample: zero latency in samples",
    )

    bp = d["backpressure"]
    R.limit(
        bool(bp)
        and all(_i(r, "taken") == 100003 for r in bp)
        and all(_i(r, "no_progress") == 0 for r in bp),
        "a short output loses nothing: all 100,003 samples are taken at "
        "every chunk size and every room",
    )
    R.limit(
        bool(bp)
        and all(
            _i(r, "bad_rows") == 0 and _i(r, "count_wrong") == 0 for r in bp
        ),
        "every row is still the oracle's under backpressure",
    )
    R.limit(
        bool(bp)
        and all(
            _i(r, "over_room") == 0
            and _i(r, "partial_writes") == 0
            and _i(r, "tail_touched") == 0
            for r in bp
        ),
        "a push writes whole rows only, never past them, and leaves the "
        "rest of the output untouched",
    )
    R.limit(
        bool(bp) and all(_i(r, "not_maximal") == 0 for r in bp),
        "a push stops short only when its room is full and the next sample "
        "would complete a row",
    )
    R.limit(
        bool(bp) and all(_i(r, "carry_over") == 0 for r in bp),
        "under backpressure, fewer than nfft samples are still held after "
        "every push",
    )
    R.limit(
        any(_i(r, "stalls") > 0 for r in bp),
        f"backpressure is exercised (up to "
        f"{max((_i(r, 'stalls') for r in bp), default=0):,} stalled pushes)",
    )

    sz = d["sizing"]
    R.limit(
        bool(sz)
        and all(
            _i(r, k) == 0
            for r in sz
            for k in (
                "restore_refused",
                "rows_for_wrong",
                "max_out_wrong",
                "push_wrong",
                "one_less_wrong",
            )
        ),
        f"from every carry state, rows_for and push_max_out equal the "
        f"stream's arithmetic, a push with that room takes all its input, "
        f"and one row less takes less ({_total(sz, 'trials'):,} trials)",
    )
    R.limit(
        bool(sz) and all(_i(r, "saturates") == 1 for r in sz),
        "push_max_out saturates at SIZE_MAX rather than wrapping",
    )

    fl = d["flush"]
    R.limit(
        bool(fl)
        and all(
            _i(r, "wrong_pending") == 0 and _i(r, "wrong_decision") == 0
            for r in fl
        ),
        "flush writes a row exactly when the stream holds a sample no row "
        "covered, which pending reports, at every length from 0 to "
        "4nfft+3",
    )
    R.limit(
        bool(fl) and all(_i(r, "off_grid") == 0 for r in fl),
        "the flushed row sits on the hop grid: PSD's reading of the "
        "zero-padded slice at the next row start",
    )
    R.limit(
        bool(fl)
        and all(
            _i(r, "second_nonzero") == 0
            and _i(r, "after_flush_wrong") == 0
            and _i(r, "bad_restart") == 0
            for r in fl
        ),
        "after a flush, a second flush writes nothing, consumed and pending "
        "read 0, and the next samples are row 0 of a new stream",
    )
    R.limit(
        _total(fl, "emitted") > 0,
        f"flushes were exercised ({_total(fl, 'emitted'):,} rows)",
    )

    st = d["state"]
    R.limit(
        bool(st)
        and all(
            _i(r, "distinct_sizes") == 1 and _i(r, "size_varies") == 0
            for r in st
        ),
        "the blob's size is a function of nfft alone: one size across cut "
        "points, windows, betas and hops",
    )
    R.limit(
        bool(st) and all(_i(r, "unwritten") == 0 for r in st),
        "get_state writes every byte of the blob, at every cut",
    )
    R.limit(
        bool(st)
        and all(_i(r, "cuts") > 0 for r in st)
        and all(
            _i(r, "restore_refused") == 0
            and _i(r, "after_restore_wrong") == 0
            and _i(r, "resume_bad") == 0
            for r in st
        ),
        f"a blob taken at any of {_total(_in(st, 'power'), 'cuts'):,} cut "
        f"points, in each mode, restores "
        f"into a new object, replacing its carry and resetting consumed, "
        f"and resumes bit for bit",
    )
    R.limit(
        bool(st)
        and all(
            _i(r, "corrupt_refused") == _i(r, "corrupt_tried")
            and _i(r, "hop_refused") == _i(r, "hop_tried")
            and _i(r, "nfft_refused") == _i(r, "nfft_tried")
            for r in st
        ),
        "a blob with a corrupt magic, version or size, from another hop, or "
        "from another nfft is always refused",
    )
    R.limit(
        bool(st) and all(_i(r, "target_changed") == 0 for r in st),
        "a refused blob changes nothing: pending, consumed and the next "
        "rows are the untouched twin's",
    )
    R.limit(
        bool(st)
        and all(
            _i(r, "window_refused") == 0 and _i(r, "window_wrong") == 0
            for r in st
        ),
        "a blob restores into an object of another window, and the rows "
        "that follow are that window's own",
    )

    R.limit(
        bool(st)
        and all(
            _i(r, "mode_refused") == 0 and _i(r, "mode_wrong") == 0 for r in st
        ),
        "a blob restores into an object of the other mode, and the rows "
        "that follow are in that mode's units",
    )

    lv = d["level"]
    R.limit(
        bool(lv) and all(_i(r, "peak_wrong") == 0 for r in lv),
        "a tone on bin k peaks at index nfft/2 + k, for every bin of nfft "
        "8, 64 and 1,024 under every window",
    )
    # 1e-4 dB in dB rows, and the same bound as a ratio in power rows
    db_tol = 1e-4
    lin_tol = 10 ** (db_tol / 10) - 1
    lp, ld = _in(lv, "power"), _in(lv, "db")
    R.limit(
        bool(lp) and all(_f(r, "max_abs_err") < lin_tol for r in lp),
        f"a full-scale tone on a bin reads 1.0 in a power row within "
        f"{lin_tol:.1e} (1e-4 dB) under every window (largest "
        f"{max((_f(r, 'max_abs_err') for r in lp), default=0.0):.1e})",
    )
    R.limit(
        bool(ld) and all(_f(r, "max_abs_err") < db_tol for r in ld),
        f"a full-scale tone on a bin reads 0 dBFS in a dB row within "
        f"{db_tol:.0e} dB under every window (largest "
        f"{max((_f(r, 'max_abs_err') for r in ld), default=0.0):.1e} dB)",
    )

    zero = d["floor_zero"]
    R.limit(
        bool(zero)
        and all(_i(r, "bins_at_floor") == _i(r, "bins") for r in zero),
        "an all-zero frame reads exactly -200 dB in every bin of a dB row, "
        "every window",
    )
    tone = d["floor_tone"]
    above = [r for r in tone if _f(r, "level_dbfs") >= FLOOR_DB]
    below = [r for r in tone if _f(r, "level_dbfs") < FLOOR_DB]
    tone_worst = max(
        (abs(_f(r, "tone_bin_db") - _f(r, "level_dbfs")) for r in above),
        default=0.0,
    )
    # the reading is one conversion of the power row: its bound, as pinned
    pin = p2db_pin_db()
    R.limit(
        bool(above)
        and all(
            abs(_f(r, "tone_bin_db") - _f(r, "level_dbfs")) < pin
            for r in above
        ),
        "an on-bin tone reads its level in its bin within the dB "
        f"conversion's bound, {pin:.0e} dB (dp_power_to_db_f32; 3.25e-4 "
        "over every float32), down to -200 dBFS under every window "
        f"(largest {tone_worst:.1e} dB)",
    )
    R.limit(
        bool(below) and all(_i(r, "tone_bin_is_floor") == 1 for r in below),
        f"below -200 dBFS the tone's bin reads exactly -200 dB, the clamp, "
        f"under every window ({len(below)} cases)",
    )
    windows = list(dict.fromkeys(str(r["window"]) for r in tone))
    R.limit(
        bool(windows)
        and all(_raised(d, w)[0] == _raised(d, w)[1] for w in windows),
        f"the bins a {QUIET_DBFS:.0f} dBFS tone raises off the floor are "
        f"exactly those its window's leakage puts above the clamp, under "
        f"every window",
    )

    noise = d["floor_noise"]
    clear = _clear(noise)
    worst = max(
        (
            abs(_f(r, "mean_median_db") - _f(r, "expected_median_db"))
            for r in clear
        ),
        default=float("inf"),
    )
    R.limit(
        len(clear) >= 8 and worst < 0.1,
        f"noise's median bin, averaged over frames, sits within 0.1 dB of "
        f"L + 10 log10(ENBW/n) + 10 log10(ln 2), with ENBW from each "
        f"window's definition, wherever no frame's median is clamped "
        f"(largest {worst:.3f} dB, {len(clear)} cases)",
    )
    gap = max(
        (
            abs(_f(r, "mean_at_floor") - _f(r, "expected_at_floor"))
            for r in noise
        ),
        default=float("inf"),
    )
    R.limit(
        bool(noise) and gap < 5.0,
        f"the exponential-bin model predicts the mean count of bins at the "
        f"floor within 5 of 1,024, at every level and window (largest "
        f"{gap:.1f})",
    )
    deep = [r for r in noise if _f(r, "level_dbfs") <= -190]
    R.limit(
        bool(deep) and all(_i(r, "min_at_floor") == NFFT for r in deep),
        f"noise of total power -190 dBFS or less is all floor, every bin of "
        f"every frame under every window "
        f"({len(deep)} cases of {_i(noise[0], 'frames') if noise else 0} "
        f"frames)",
    )


def build(write: bool = True) -> Report:
    d = harness_blocks(HARNESS, "spectrogram", ROOT)

    R.md("# The Spectrogram — certification evidence")
    R.md()
    R.md("## 1. The object")
    R.md()
    R.md(
        "A stream of any-size chunks in, rows of `nfft`-bin spectra out, "
        "one row every `hop` samples: linear power by default, dBFS when "
        "the caller asks for it by name. Row *k* is PSD's reading of "
        "stream samples `[k*hop, k*hop + nfft)` in the row's mode, "
        "whatever the chunking. The carry "
        "between calls is the ring's framer, the spectrum is PSD's "
        "per-frame kernel, and the object composes both and re-implements "
        "neither. `flush` ends the stream with the one zero-padded row it "
        "owes, and a fixed-size blob makes the stream resumable."
    )
    R.md()
    R.md("Design and API, not restated here:")
    R.md()
    R.md(
        "- `native/inc/doppler/spectrogram/spectrogram_core.h`: the SSOT "
        "for every claim below\n"
        "- `native/tests/test_spectrogram_core.c`: the C pins\n"
        "- [The Spectrogram](../../../../../docs/design/spectrogram.md): "
        "the design, and "
        "[its measurement record](../../../../../docs/design/"
        "spectrogram-measurements.md)\n"
        "- [The guide](../../../../../docs/guide/spectrogram.md) and "
        "`native/examples/spectrogram_demo.c`, its tested C twin\n"
        "- [PSD's certification](../../../spectral/tests/validation/psd/"
        "results.md), the kernel every row is\n"
        "- [The framer's certification](../framer/results.md), the carry\n"
        "- `native/validation/spectrogram_certify.c`: the runs below"
    )
    R.md()
    R.md("### Claim coverage: every prose claim in the header")
    R.md()
    R.md(
        "The campaign's order is header first. *Pin* is the section of the "
        "C test that asserts the claim. *Red under* names a sabotage of "
        "the code that turned that pin red, and the pull request whose "
        "record it is (#1975 built the object, #2043 pinned what the "
        "first inventory found unpinned, and this certification adds the "
        "rest). A claim that holds by construction cites the code instead, "
        "and a contract has no pin. *Here* is the section of this report "
        "that measures it at scale."
    )
    R.md()
    R.table(
        ["#", "claim in the header", "pin", "red under", "here"],
        [
            [
                "C1",
                "`create` refuses an `nfft` that is not a power of two of "
                "at least 2, a hop outside 1..`nfft`, a bad window index or "
                "one with no gain at `nfft` (Hann at 2), in either mode, "
                "and any mode but power and dB; it accepts both modes",
                "§1",
                "#1975: the `nfft` check removed (N1), and its validation "
                "cases. #1968: power refused again; any mode accepted",
                "—",
            ],
            [
                "C2",
                "row *k* is `dp_psd_frame_linear` (power, the default) or "
                "`dp_psd_frame_db` (dB) of samples `[k*hop, k*hop + nfft)`",
                "§2, §4",
                "#1975: a rotation in the row (D1); raw power as a second "
                "kernel (S3); a framer defect both paths share (T1). "
                "#1968: dB rows always, linear rows always, the modes "
                "swapped",
                "§2.1",
            ],
            [
                "C3",
                "the rows do not depend on how the stream was split",
                "§5",
                "#1975 T1; #2043: a row drained a push late (K8, K8b)",
                "§2.1",
            ],
            [
                "C4",
                "fewer than `nfft` samples are held once a push returns",
                "§5, §6",
                "#2043: one frame past the room (K1)",
                "§2.1, §2.2",
            ],
            [
                "C5",
                "a short output never loses input: whole rows only, "
                "`floor(max_out / nfft)` of them, nothing written to an "
                "output too small for one, and input that completes no row "
                "taken whole even with `max_out` 0",
                "§4, §6, §11",
                "#1975: the 0/0 rule (S1); stop at `rows == room` (S2). "
                "#2043: one float written with no row (M1), the tail left "
                "untaken (M2), one float past the last row (T1)",
                "§2.2",
            ],
            [
                "C6",
                "`push_max_out(n)` is the room that makes a push take all "
                "`n`, and saturates at `SIZE_MAX`",
                "§4",
                "#1975: `push_max_out` wrapping",
                "§2.1, §2.3",
            ],
            [
                "C7",
                "`rows_for(n)` is the carry plus `n` cut into frames, and a "
                "push with that room writes exactly that many",
                "§4",
                "#1975: a capped `rows_for`; the framer's `frames_in` "
                "forgetting the owed hop, wrapping, capped "
                f"({R.ref('byte_written_claim')}-"
                f"{R.ref('bin_reads_no_lower_db')})",
                "§2.3",
            ],
            [
                "C8",
                "`consumed` is `n_in` unless the output ran out, and 0 "
                "after create, reset, flush and set_state",
                "§6, §7, §8, §10",
                "#1975: the `consumed` resets",
                "§2.1, §2.4, §2.5",
            ],
            [
                "C9",
                "`flush` writes the grid row iff a sample is uncovered, "
                "then restarts as after reset, so a second flush writes "
                "nothing",
                "§7",
                "#1975: a stale flush row. #2043: the row cut off the grid "
                "(M3)",
                "§2.4",
            ],
            [
                "C10",
                "`pending` is the samples no written row covers",
                "§7, §8",
                "#2043: pending off by one (M4)",
                "§2.1, §2.4",
            ],
            [
                "C11",
                "`reset` drops the carry and restarts at sample 0, keeping "
                "the configuration",
                "§8",
                "#2043: reset forgets the window (M5)",
                "—",
            ],
            [
                "C12",
                "`destroy(NULL)` is a no-op",
                "§1",
                "this report: the NULL check removed (the test crashes, "
                "exit 139)",
                "—",
            ],
            [
                "C13",
                "rows are DC-centred: bin *k* at index `nfft/2 + k`",
                "§9",
                "#1975 D1",
                "§2.6",
            ],
            [
                "C14",
                "a full-scale tone on a bin reads 1.0 in power and 0 dB in "
                "dB, whatever the window",
                "§3",
                "#1975 D1, S3. #1968: dB rows always (power reads 0 dB, "
                "not 1.0), linear rows always",
                "§2.6",
            ],
            [
                "C15",
                "`beta` is ignored outside the Kaiser window",
                "§12",
                "#2043: beta used by every window (K2)",
                "—",
            ],
            [
                "C16",
                "in dB, a bin reads no lower than -200 dB, so an all-zero "
                "frame and one below the floor write the same row; in "
                "power, an all-zero frame reads 0",
                "§13",
                "#2043: the floor moved to -300 dB (K3). #1968: dB rows "
                "always (power reads -200)",
                "§2.7, dB only; the power half is C §13 only",
            ],
            [
                "C17",
                "the blob's size is a function of `nfft` alone",
                "§10, §14",
                "#2043: the size depends on the window (K4)",
                "§2.5",
            ],
            [
                "C18",
                "the blob is 'SPGM' version 1 and carries the stream "
                "position only: not `consumed`, the window, beta or mode",
                "§14",
                "#2043: the blob carries `consumed` (M6); version 2 (M8)",
                "—",
            ],
            [
                "C19",
                "`get_state` writes every byte of the blob",
                "§14 (NEW)",
                "this report: an unwritten tail; the framer's pad left "
                f"unwritten ({R.ref('byte_written_claim')})",
                "§2.5",
            ],
            [
                "C20",
                "a restored blob continues the stream bit for bit in a "
                "fresh object",
                "§10",
                "#1975: an empty restore; a dropped carry",
                "§2.5",
            ],
            [
                "C21",
                "`set_state` refuses a wrong magic, version, size, `nfft`, "
                "hop or an impossible carry, changing nothing",
                "§14",
                "#2043: any version (K5), any size (K6), an impossible "
                "carry (K7), mutate before validating (M7). This report: "
                "the magic check removed; the stored-hop check removed, "
                "which stayed green until §14 gained a no-row case "
                f"({R.ref('hop_refusal_masked_in_c')})",
                "§2.5 (not the carry)",
            ],
            [
                "C22",
                "another window, beta or mode is NOT refused on restore, "
                "and the rows that follow are the restoring object's",
                "§14b",
                "#2043: the restore takes the source's window back (S14b) "
                "or its beta (S14b-β). #1968: the modes made one, which "
                "the mode case's precondition refuses",
                "§2.5",
            ],
            [
                "C23",
                "a row arrives with the push that delivers its last sample",
                "§15",
                "#2043: a one-sample push's row one push late (K8c)",
                "§2.1",
            ],
            [
                "C24",
                "one object is not thread-safe",
                "— (a contract)",
                f"— (not testable, {R.ref('object_not_thread_safe')})",
                "—",
            ],
            [
                "C25",
                "the object composes its parts and re-implements none: the "
                "carry is the ring's framer, the spectrum PSD's per-frame "
                "kernel",
                "— (by construction)",
                "`spectrogram_core.c` calls `dp_f32_framer_*` for the carry "
                "and `dp_psd_frame_linear` or `dp_psd_frame_db` for every "
                "row, and holds no window, FFT, conversion or carry code of "
                "its own",
                "§2.1",
            ],
            [
                "C26",
                "complex float32 input only",
                "— (by construction)",
                "the signature takes `const float _Complex *`",
                "—",
            ],
            [
                "C27",
                "a sample is taken unless taking it would complete a row "
                "the output has no room for: the framer's feed contract",
                "§4, §6",
                "#1975: stop at `rows == room` (S2). #2043: the tail left "
                "untaken (M2)",
                "§2.2",
            ],
            [
                "C28",
                "a dB row is dp_power_to_db_f32 of the power row, bit for "
                "bit, so converting power rows is exactly the dB mode",
                "§16",
                "#2094: PSD's dB path given its own 10·log10 back (and "
                "PSD's own pin, test_psd_core.c)",
                f"§2.7 ({R.ref('db_row_fast_conversion_power')})",
            ],
        ],
    )
    R.md()

    characterise(d)
    review(d)
    limits(d)

    shapes = d["rows"]
    R.executive(
        "The Spectrogram",
        source=(
            "Generated by `validate.py` in this folder. The Spectrogram has "
            "no Python binding yet, so every number is measured by "
            "`native/validation/spectrogram_certify.c` and rendered here. "
            "§2.7 also sets the measured floor beside an exponential-bin "
            "model, named where it is used. Re-run to regenerate."
        ),
        takeaways=[
            "**Power rows are the default, dB rows are asked for by name, "
            "and every claim below holds in both.** A power row is "
            "`dp_psd_frame_linear` of its frame, so a full-scale tone reads "
            "1.0 under every window (§2.6, "
            f"{R.ref('mode_power_reserved_refused')}).",
            "**Any split of the stream gives the same rows, and every row "
            "is PSD's reading of its own slice, bit for bit.** "
            f"{_total(_in(shapes, 'power'), 'partitions')} distinct "
            f"partitions over {len(_in(shapes, 'power'))} shapes, from single "
            "samples to the whole stream at once, agree with an oracle built "
            "without the object, in each mode (§2.1).",
            "**A short output slows the stream and loses nothing**, and "
            "`push_max_out` is exactly the room that takes a whole chunk, "
            "from every carry state the shape has (§2.2, §2.3).",
            "**`flush` lands on the hop grid, once, only if a sample is "
            "owed**, and the stream restarts after it. It stays explicit, "
            "because only the caller knows where its stream ends (§2.4, "
            f"{R.ref('flush_stays_explicit')}).",
            "**The blob is a fixed size per `nfft`, every byte written, "
            "and resumes bit for bit from any cut.** It carries no "
            "configuration: another window or mode restores without "
            "complaint and continues as itself, so keeping the create "
            "arguments is the caller's job (§2.5, "
            f"{R.ref('blob_restores_into_other_config')}).",
            "**A dB row reads no lower than -200 dB.** A tone under the floor "
            "and digital silence give the same row, and wideband noise "
            "reaches it about `10·log10(nfft)` sooner; at `nfft` 1,024 a "
            "total of -190 dBFS is all floor, every bin of every frame "
            f"under every window (§2.7, {R.ref('bin_reads_no_lower_db')}).",
            "**The evidence shares one part with the object: PSD's "
            "kernel.** Every row is checked against `dp_psd_frame_linear` "
            "or `dp_psd_frame_db` of its slice, so a defect inside that "
            "kernel would pass every row check here. Only the physical "
            "checks would see it: a full-scale tone reading 1.0, or 0 dBFS, "
            "in its own bin (§2.6) and the floor (§2.7). The kernel is "
            "certified on its own, in PSD's report.",
        ],
    )
    R.summary(
        "\n- The runs behind §2 are `native/validation/spectrogram_certify.c`"
        ", emitted as CSV; the pins are `native/tests/test_spectrogram_core"
        ".c`."
    )
    if write:
        R.emit(HERE / "results.md")
    return R


if __name__ == "__main__":
    sys.exit(cli(build, HERE))
