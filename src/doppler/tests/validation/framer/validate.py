"""Certification evidence for the ring's framed face (the framer).

The framer is `DECLARE_DP_BUFFER_FRAMES` in `native/inc/doppler/buffer/
buffer.h`: any chunk in, fixed and optionally overlapping frames out, with a
bounded, serializable carry. It is a face of the ring and has no Python
binding and is not getting one, so this follows `docs/dev/contributing/
validation.md`, "Certifying a component with no binding": **the C harness
`native/validation/framer_certify.c` measures; this file renders and
asserts.** Nothing there decides what is acceptable; every threshold lives in
`limits()` below.

The oracle is the specification, not another path through the framer: frame
*k* of a stream is `x[k*hop .. k*hop + n)`, zero beyond the end, and every
frame is compared with that slice as it is produced.

Run it directly to regenerate `results.md`:

    python src/doppler/tests/validation/framer/validate.py

or with `--check` to see whether the committed report is stale.
"""

from __future__ import annotations

import sys
from pathlib import Path

from doppler.tests._repo import build_dir, exe, repo_root
from doppler.tests._validation_common import Report, cli, harness_blocks

HERE = Path(__file__).resolve().parent
ROOT = repo_root(__file__)
HARNESS = exe(
    build_dir(__file__) / "native/validation/validate_framer_certify"
)

R = Report()


def _int(row: dict, key: str) -> int:
    return int(row[key])


def characterise(d) -> None:
    R.md("## 2. Characterisation")
    R.md()
    R.md(
        "Every number below comes from `native/validation/framer_certify.c`, "
        "run through the framer's own C interface. Each frame handed out is "
        "compared sample by sample with the oracle, and a pass is a count of "
        "zero."
    )
    R.md()

    R.md("### 2.1 Chunk invariance, and the carry bound (C §2, §3)")
    R.md()
    R.md(
        "Each shape streams a long input through 72 partitions: single "
        "samples, 7, `n`-1, `n`, `n`+1, 3`n`+5, 65537, the whole stream as "
        "one chunk, and 64 seeded random splits of 1 to 2`n`+1. A partition "
        "is *bad* if any frame differs from the oracle or the frame count is "
        "not `(len - n) / hop + 1`. After every drain the samples still "
        "buffered are counted; the claim is that it is always fewer than "
        "`n`."
    )
    R.md()
    R.table(
        [
            "n",
            "hop",
            "stream length",
            "partitions",
            "rows",
            "bad partitions",
            "largest carry",
        ],
        [
            [
                f"{_int(r, 'n'):,}",
                f"{_int(r, 'hop'):,}",
                f"{_int(r, 'length'):,}",
                f"{_int(r, 'partitions')}",
                f"{_int(r, 'rows'):,}",
                f"{_int(r, 'bad_partitions')}",
                f"{_int(r, 'max_leftover'):,} (n-1 = {_int(r, 'n') - 1:,})",
            ]
            for r in d["invariance"]
        ],
    )
    R.md()

    R.md(
        "### 2.2 A short output room slows the stream and loses nothing (C §3)"
    )
    R.md()
    R.md(
        "`feed` admits input only as far as the frames it yields fit the "
        "caller's room, so with room for 1, 2 or 7 frames per call it takes "
        "less than it is offered and the caller offers the rest again. "
        "*Offered* counts every offer, re-offers included; *taken* counts "
        "what the framer accepted; *stalls* are the calls that took less "
        "than was offered. The claim is that *taken* ends equal to the "
        "stream's length with every frame correct, whatever the stalls. "
        "*Feeds over room* counts the feeds that made more frames available "
        "than the room they were given, which `feed` promises never to do."
    )
    R.md()
    R.table(
        [
            "chunk",
            "room (frames)",
            "offered",
            "taken",
            "stalls",
            "rows",
            "bad",
            "feeds over room",
            "largest carry",
        ],
        [
            [
                f"{_int(r, 'chunk'):,}",
                f"{_int(r, 'room')}",
                f"{_int(r, 'offered'):,}",
                f"{_int(r, 'taken'):,}",
                f"{_int(r, 'stalls')}",
                f"{_int(r, 'rows')}",
                f"{_int(r, 'bad_rows')}",
                f"{_int(r, 'over_room')}",
                f"{_int(r, 'max_leftover'):,}",
            ]
            for r in d["backpressure"]
        ],
    )
    R.md()
    R.md(
        "Chunks of 1 and 7 never stall at these rooms: a chunk that small "
        "never carries a frame the caller has no room for. The larger chunks "
        f"do, up to {max(_int(r, 'stalls') for r in d['backpressure'])} "
        "times, so the path is exercised."
    )
    R.md()

    R.md("### 2.3 Flush: on the grid, once, only if owed (C §5)")
    R.md()
    R.md(
        "For every stream length from 0 to 4`n`+3, feed it, drain, flush. "
        "The row must exist exactly when the stream holds a sample no earlier "
        "frame covered (*wrong* counts a disagreement), must equal the "
        "oracle's row *k* zero-padded (*off-grid* counts a row that starts "
        "anywhere else), a second flush must return 0, and the next `n` "
        "samples must be row 0 of a new stream (*bad restart*)."
    )
    R.md()
    R.table(
        [
            "n",
            "hop",
            "lengths",
            "rows emitted",
            "wrong decision",
            "off-grid",
            "second flush != 0",
            "bad restart",
        ],
        [
            [
                f"{_int(r, 'n')}",
                f"{_int(r, 'hop')}",
                f"{_int(r, 'lengths')}",
                f"{_int(r, 'emitted')}",
                f"{_int(r, 'wrong_decision')}",
                f"{_int(r, 'off_grid')}",
                f"{_int(r, 'second_nonzero')}",
                f"{_int(r, 'bad_restart')}",
            ]
            for r in d["flush"]
        ],
    )
    R.md()
    R.md(
        "`n = 1, hop = 1` emits no flush row at any length, correctly: every "
        "sample is already a frame, so none is ever uncovered."
    )
    R.md()

    R.md("### 2.4 The snapshot: shape-sized, restorable anywhere (C §6, §9)")
    R.md()
    R.md(
        "At every cut point from 0 to 3`n`+3 the stream is run to the cut, "
        "snapshotted, restored into a **fresh** framer, and finished there in "
        "pieces of 5. *Sizes* counts distinct snapshot sizes across all the "
        "cuts: one, because the size is a function of the shape alone. A "
        "snapshot whose `written` counter is then corrupted must be refused. "
        "So must one claiming a carry no drained framer can hold: once a "
        "frame is out the framer keeps at least `n` - `hop` samples, so the "
        "same blob saying `n` - `hop` - 1 with counters made to agree is "
        "*carry*, tried at every cut with a frame out (a shape with `hop` = "
        "`n` has no impossible carry to try)."
    )
    R.md()
    R.table(
        [
            "n",
            "hop",
            "cut points",
            "distinct sizes",
            "bad resumes",
            "corrupt: tried",
            "corrupt: refused",
            "carry: tried",
            "carry: refused",
        ],
        [
            [
                f"{_int(r, 'n')}",
                f"{_int(r, 'hop')}",
                f"{_int(r, 'cuts')}",
                f"{_int(r, 'distinct_sizes')}",
                f"{_int(r, 'resume_bad')}",
                f"{_int(r, 'corrupt_tried')}",
                f"{_int(r, 'corrupt_refused')}",
                f"{_int(r, 'carry_tried')}",
                f"{_int(r, 'carry_refused')}",
            ]
            for r in d["snapshot"]
        ],
    )
    R.md()

    R.md("### 2.5 An undrained framer is refused, not overflowed (C §8)")
    R.md()
    R.md(
        "Feed with room for any number of frames and do **not** drain, so "
        "whole frames stay buffered. `flush` must be refused with nothing "
        "changed; an undrained `get_state` must write a blob of zeros (no "
        "byte of the caller's buffer survives into it); and no framer may "
        "accept that blob."
    )
    R.md()
    R.table(
        [
            "n",
            "hop",
            "attempts",
            "flush refused",
            "state changed",
            "snapshot with a nonzero byte",
            "snapshot accepted",
        ],
        [
            [
                f"{_int(r, 'n')}",
                f"{_int(r, 'hop')}",
                f"{_int(r, 'attempts')}",
                f"{_int(r, 'flush_refused')}",
                f"{_int(r, 'state_changed')}",
                f"{_int(r, 'snapshot_nonzero')}",
                f"{_int(r, 'snapshot_accepted')}",
            ]
            for r in d["refusal"]
        ],
    )
    R.md()


def review(d) -> None:
    R.md("## 3. Review")
    R.md()
    R.find(
        "F1",
        "FIXED",
        "Reading the header against `test_framer_core.c` (the inventory in "
        "§1) found `framer_frames_for` and `framer_reset` with **zero** "
        'mentions, `framer_drained` with two, and "zero-copy" as prose. '
        "All are pinned now (C §10). The inventory also caught a pin that "
        "was too weak: a sabotage of `frames_for` that ignored the owed hop "
        "passed until the test also computed it while a hop was outstanding.",
    )
    R.find(
        "F2",
        "FIXED",
        "`flush` on an undrained framer copied `live >= n` samples into an "
        "`n`-sample row: a heap overflow on a natural end-of-stream call "
        "through a public API. It is now refused with `DP_ERR_INVALID`, "
        "nothing changed (C §8, measured at scale in §2.5); with the guard "
        "removed the test is an AddressSanitizer stack-buffer-overflow. "
        "Found in review, not by a test.",
    )
    R.find(
        "F3",
        "FIXED",
        "The first snapshot did not store the hop, so a framer of the same "
        "`n` and a different hop accepted a `frames = 0` snapshot whose "
        "counters meant something else. The hop is stored and checked (C "
        "§9). An undrained `get_state` used to truncate silently; it now "
        "writes zeros, which no framer accepts (§2.5).",
    )
    R.find(
        "F4",
        "C-ONLY",
        "Every claim here is verified in C and none is reachable from "
        "Python, because the framer has no binding. That is the design: its "
        "callers are C objects (the spectrogram, and later the ring's other "
        "consumers). The Python face of anything built on it is declared "
        "separately and is the jm toolchain's to generate.",
    )
    R.find(
        "F5",
        "BY DESIGN",
        "The framer owns its ring **exclusively**: `feed` is the only write "
        "path, and the carry bound holds only because nothing else writes. "
        "That is a contract, not a runtime check, so no test can prove it; "
        "a caller that writes to the ring behind the framer's back voids the "
        "bound. Position-addressed history, which needs absolute stream "
        "positions, is the ring used directly (`burst_capture`), not the "
        "framer.",
    )
    R.find(
        "F6",
        "BY DESIGN",
        "The cost against the hand-written drain loop is not in this "
        "report: it is a speed claim about the ring, measured with the "
        "ring's other speed claims in the ring's measurement record, and "
        "this report certifies what a caller may rely on, not how fast it "
        "is.",
    )


def limits(d) -> None:
    R.md("## 4. Limits")
    R.md()
    R.md("Claims a caller may rely on, asserted by this run.")
    R.md()

    inv = d["invariance"]
    parts = sum(_int(r, "partitions") for r in inv)
    R.limit(
        all(_int(r, "bad_partitions") == 0 for r in inv),
        f"every frame equals the oracle's slice, and the frame count is "
        f"(len - n)/hop + 1, under all {parts} partitions of {len(inv)} "
        f"shapes (single samples through random splits)",
    )
    R.limit(
        all(_int(r, "max_leftover") <= _int(r, "n") - 1 for r in inv),
        "after a drain, fewer than n samples are ever left buffered (the "
        "largest carry observed is n-1 in every shape)",
    )
    R.limit(
        all(
            _int(r, "bad_partitions") == 0 and _int(r, "rows") > 0 for r in inv
        ),
        "every shape yielded rows, so the zeros above are not an empty run",
    )

    bp = d["backpressure"]
    R.limit(
        all(_int(r, "taken") == 100003 for r in bp),
        "a short output room loses nothing: all 100,003 samples are "
        "accepted at every chunk size and every room",
    )
    R.limit(
        all(_int(r, "bad_rows") == 0 for r in bp),
        "every frame is still correct under backpressure",
    )
    R.limit(
        all(_int(r, "over_room") == 0 for r in bp),
        "the room is a promise: no feed ever makes more frames available "
        "than the room it was given",
    )
    R.limit(
        all(_int(r, "max_leftover") <= 1023 for r in bp),
        f"under backpressure the carry still never reaches n (largest "
        f"{max(_int(r, 'max_leftover') for r in bp):,} against n = 1,024)",
    )
    R.limit(
        any(_int(r, "stalls") > 0 for r in bp),
        f"backpressure is actually exercised (up to "
        f"{max(_int(r, 'stalls') for r in bp)} stalled feeds)",
    )

    fl = d["flush"]
    R.limit(
        all(_int(r, "wrong_decision") == 0 for r in fl),
        "flush emits a row exactly when the stream holds an uncovered "
        "sample, at every length from 0 to 4n+3",
    )
    R.limit(
        all(_int(r, "off_grid") == 0 for r in fl),
        "the flushed row sits on the hop grid: it equals the oracle's "
        "zero-padded row, never one starting at the first uncovered sample",
    )
    R.limit(
        all(_int(r, "second_nonzero") == 0 for r in fl),
        "a second flush returns 0",
    )
    R.limit(
        all(_int(r, "bad_restart") == 0 for r in fl),
        "after a flush the framer restarts at sample 0",
    )

    sn = d["snapshot"]
    R.limit(
        all(_int(r, "distinct_sizes") == 1 for r in sn),
        "the snapshot size is a function of the shape alone (one size "
        "across every cut point)",
    )
    R.limit(
        all(_int(r, "resume_bad") == 0 for r in sn),
        f"a snapshot taken at any of {sum(_int(r, 'cuts') for r in sn)} cut "
        f"points resumes in a fresh framer bit-for-bit",
    )
    R.limit(
        all(
            _int(r, "corrupt_refused") == _int(r, "corrupt_tried") for r in sn
        ),
        "a snapshot with a corrupted counter is always refused",
    )
    carry_tried = sum(_int(r, "carry_tried") for r in sn)
    R.limit(
        carry_tried > 0
        and all(
            _int(r, "carry_refused") == _int(r, "carry_tried") for r in sn
        ),
        f"a snapshot claiming a carry no drained framer can hold (fewer than "
        f"n - hop samples with a frame out) is always refused, "
        f"{carry_tried} tried",
    )

    rf = d["refusal"]
    R.limit(
        all(_int(r, "flush_refused") == _int(r, "attempts") for r in rf),
        f"flush is refused on every one of "
        f"{sum(_int(r, 'attempts') for r in rf)} undrained attempts",
    )
    R.limit(
        all(_int(r, "state_changed") == 0 for r in rf),
        "a refused flush changes nothing",
    )
    R.limit(
        all(
            _int(r, "snapshot_nonzero") == 0
            and _int(r, "snapshot_accepted") == 0
            for r in rf
        ),
        "an undrained snapshot is all zeros and no framer accepts it",
    )


def build(write: bool = True) -> Report:
    d = harness_blocks(HARNESS, "framer", ROOT)

    R.md("# The ring's framer — certification evidence")
    R.md()
    R.md("## 1. The object")
    R.md()
    R.md(
        "The framed face of the ring (`DECLARE_DP_BUFFER_FRAMES`): any chunk "
        "in, fixed and optionally overlapping frames out. `feed` is the only "
        "write path and admits input only as far as the frames it yields fit "
        "the caller's room; `next` hands out each frame zero-copy; `flush` "
        "ends the stream with the one zero-padded row it owes; and a "
        "fixed-size snapshot of the carry makes it resumable."
    )
    R.md()
    R.md("Design and API, not restated here:")
    R.md()
    R.md(
        "- `native/inc/doppler/buffer/buffer.h` — the SSOT for every claim "
        "below\n"
        "- `native/tests/test_framer_core.c` — the C pins\n"
        "- [The Ring Buffer](../../../../../docs/design/ring-buffer.md) — the "
        "framer's section, and the ring it is a face of\n"
        "- the spectrogram program it was built for, tracked in #1894\n"
        "- `native/validation/framer_certify.c` — the runs below"
    )
    R.md()
    R.md("### Claim coverage — every prose claim in the header")
    R.md()
    R.md(
        "The campaign's order is header first. *Pin* is the C section that "
        "asserts the claim; *red under* names a sabotage of the code that "
        "turned it red; *here* is the section of this report that measures "
        "it at scale. This inventory is what found F1 and F2."
    )
    R.md()
    R.table(
        ["#", "claim in the header", "pin", "red under", "here"],
        [
            [
                "C1",
                "`init` refuses hop 0, hop > n, n > capacity, a non-empty "
                "ring",
                "§0",
                "hop bound dropped; empty-ring check dropped",
                "—",
            ],
            [
                "C2",
                "frame *k* covers stream samples `[k*hop, k*hop + n)`",
                "§1",
                "the owed hop off by one",
                "§2.1",
            ],
            [
                "C3",
                "the frames do not depend on how the input was chunked",
                "§2",
                "feed bound off by one; hop off by one",
                "§2.1",
            ],
            [
                "C4",
                "once drained, fewer than n samples are left",
                "§3",
                "feed bound off by one",
                "§2.1",
            ],
            [
                "C5",
                "a short output room loses nothing",
                "§3",
                "feed ignores its room",
                "§2.2",
            ],
            [
                "C6",
                "`next` is zero-copy and its pointer holds until the next "
                "call",
                "§10 (NEW)",
                "a pointer into the mirror alias",
                "—",
            ],
            [
                "C7",
                "`settle` retires exactly the owed hop, once",
                "§10 (NEW)",
                "hop - 1; owed not cleared",
                "—",
            ],
            [
                "C8",
                '`drained` is exactly "`next` would return NULL"',
                "§10 (NEW), §8",
                "`<=` for `<`",
                "—",
            ],
            [
                "C9",
                "`frames_for(n)` is what `feed(n, SIZE_MAX)` then a drain "
                "yields, with or without a hop owed and where the ring, not "
                "the input, is the limit",
                "§10 (NEW)",
                "off by one; ignores the owed hop; ignores the ring's room",
                "—",
            ],
            [
                "C10",
                "`flush` emits the grid row iff it holds an uncovered "
                "sample, once, and restarts",
                "§5",
                "off the grid; second flush; no restart",
                "§2.3",
            ],
            [
                "C11",
                "`flush` is refused, changing nothing, while frames are "
                "buffered",
                "§8",
                "guard removed (ASan stack overflow)",
                "§2.5",
            ],
            [
                "C12",
                "`reset` restarts at sample 0",
                "§10 (NEW)",
                "`written` not cleared",
                "—",
            ],
            [
                "C13",
                "the snapshot size depends on the shape alone",
                "§6, §10",
                "sized by fill; ignoring n",
                "§2.4",
            ],
            [
                "C14",
                "a snapshot resumes bit-for-bit in a fresh framer, anywhere",
                "§6",
                "`frames` not stored",
                "§2.4",
            ],
            [
                "C15",
                "a snapshot of another shape, hop, sample type, with "
                "corrupt counters or with a carry no stream can produce is "
                "refused",
                "§6, §9, §10",
                "hop check removed; counter check removed; sample-type "
                "check removed; carry bound removed or off by one",
                "§2.4",
            ],
            [
                "C16",
                "an undrained `get_state` writes zeros, never a truncated "
                "blob",
                "§8",
                "guard removed",
                "§2.5",
            ],
            [
                "C17",
                "the element views are casts of the scalar face",
                "§7 (by use)",
                "—",
                "—",
            ],
            [
                "C18",
                "the framer owns its ring exclusively",
                "—",
                "a contract; not testable (F5)",
                "—",
            ],
        ],
    )
    R.md()

    characterise(d)
    review(d)
    limits(d)

    R.executive(
        "The ring's framer",
        [
            "**Any split of the input gives the same frames.** 72 "
            "partitions of each of 8 shapes, from single samples to the "
            "whole stream at once, agree with the oracle on every frame; "
            "the frame count is exactly `(len - n) / hop + 1`.",
            "**The carry is bounded: after a drain, fewer than `n` samples "
            "are left.** That bound is what makes the snapshot fixed-size, "
            "and the snapshot restores into a fresh framer bit-for-bit "
            "from any cut point tried.",
            "**A short output room slows the stream and loses nothing.** "
            "`feed` takes less than it is offered and the caller offers the "
            "rest again; every sample is accepted in the end.",
            "**`flush` lands on the hop grid, once, only if a sample is "
            "owed**, and the framer restarts afterwards. It is refused, "
            "changing nothing, while whole frames are still buffered. That "
            "was a heap overflow before review found it (F2).",
            "**The evidence is C, and that is the design.** The framer has "
            "no Python face (F4), and its ring-exclusivity contract cannot "
            "be tested (F5).",
        ],
    )
    R.summary(
        "\n- The runs behind §2 are `native/validation/framer_certify.c`, "
        "emitted as CSV; the pins are `native/tests/test_framer_core.c`."
    )
    if write:
        R.emit(HERE / "results.md")
    return R


if __name__ == "__main__":
    sys.exit(cli(build, HERE))
