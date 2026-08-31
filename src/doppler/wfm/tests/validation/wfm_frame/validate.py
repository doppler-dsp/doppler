"""Frame — certification evidence, measured through the binding.

Run directly to regenerate `results.md` and the CSVs:

    uv run python src/doppler/wfm/tests/validation/wfm_frame/validate.py

`--check` re-renders in memory and diffs against the committed bytes;
`make validate` writes, `make validate-check` checks. Every limit this
records is asserted by
`src/doppler/wfm/tests/test_validation_limits.py`.

The order is the campaign's: `native/inc/wfm/wfm_frame.h` is the SSOT
and `native/tests/test_wfm_frame.c` certifies it in C. This file
measures the same properties through `doppler.wfm.FrameDesc`.

**The question this object exists to answer is whether it is generic**,
and the header stakes the design on it: a stage's kind is an open
`uint32_t` so that "a mission that is not CCSDS" is a configuration
rather than a pull request against the header. That is a claim, and §2.1
and §2.2 measure it: arbitrary frames that contain nothing CCSDS at all
build and self-check, and a CCSDS CADU is then shown to be one
configuration of the same machinery rather than its shape. §2.4 measures
where the claim stops.

**`bits(count)` counts FRAMES, not bits.** Passing a bit count asks for
that many copies of the frame; every call here asks for exactly one. The
PN limit is what caught it -- a 31-bit m-sequence materialised 31 times
has 496 ones rather than 16, and §2.1 and §2.3 were quietly measuring
multi-frame buffers until the arithmetic disagreed.

Truths here are arithmetic and structure -- a frame's length is the sum
of its fields, an m-sequence over a full period has exactly `2^(n-1)`
ones -- not another doppler path that could share the same mistake.
"""

from __future__ import annotations

import sys
from dataclasses import dataclass, field
from pathlib import Path

import numpy as np

from doppler.tests._repo import repo_root
from doppler.tests._validation_common import Report, cli
from doppler.wfm import FrameDesc, ccsds_asm_bits

HERE = Path(__file__).resolve().parent
DATA = HERE / "data"
ROOT = repo_root(__file__)

R = Report()

EMPTY = np.empty(0, np.uint8)
CRC_BITS = 16

# Sequence kinds, as add_field() indexes them.
SEQ_LITERAL, SEQ_PN, SEQ_GOLD, SEQ_DOTTED = 0, 1, 2, 3
# Stage kinds, as add_stage() indexes them.
ST_CRC16, ST_RS, ST_RANDOMISE = 0, 1, 2
# The first kind reserved for callers; doppler never allocates here.
ST_USER = 0x1000


def _csv(path: Path, header: str, rows: list[list[float]]) -> None:
    if not R.write:
        return
    DATA.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as fh:
        fh.write(header + "\n")
        for r in rows:
            fh.write(",".join(f"{v:.10g}" for v in r) + "\n")


def octets(n: int) -> np.ndarray:
    """`n` octets of deterministic payload, as unpacked bits."""
    return np.unpackbits(
        np.array([(i * 29 + 5) & 0xFF for i in range(n)], np.uint8)
    ).astype(np.uint8)


def plain_frame(sync: list[int], payload_octets: int, crc: bool = True):
    """sync | payload | CRC — a frame with nothing CCSDS in it."""
    d = FrameDesc(EMPTY, EMPTY, EMPTY)
    d.add_field(np.array(sync, np.uint8))
    d.add_field(octets(payload_octets))
    if crc:
        d.add_field(EMPTY, derived_by=1, derived_bits=CRC_BITS)
        d.add_stage(ST_CRC16, first_field=1, n_fields=2)
    d.build()
    return d


@dataclass
class Data:
    """Everything measured, so review/limits read data rather than re-run."""

    plain_rows: list[list[str]] = field(default_factory=list)
    plain_all_build: bool = False
    plain_len_exact: bool = False
    cover_excludes_sync: bool = False
    cadu_bits: int = 0
    cadu_asm_bits: int = 0
    cadu_stage_starts: list[int] = field(default_factory=list)
    cadu_skips_marker: bool = False
    flip_rows: list[list[str]] = field(default_factory=list)
    flips_all_caught: bool = False
    no_crc_is_minus_one: bool = False
    seq_rows: list[list[str]] = field(default_factory=list)
    dotted_starts_high: bool = False
    pn_is_maximal: bool = False
    gold_uses_both: bool = False
    user_kind_accepted: bool = False
    user_kind_unbuildable: bool = False
    user_above_builtins: bool = False
    tiles_exactly: bool = False
    all_bits_binary: bool = False
    unreachable: list[str] = field(default_factory=list)


# ── 1. the object ────────────────────────────────────────────────────
def section_object() -> None:
    R.md("## 1. The object")
    R.md()
    R.md(
        "`FrameDesc` is a frame as a DESCRIPTION -- an ordered list of "
        "fields, and a list of transforms each of which declares the span "
        "of fields it covers. The design is "
        "[docs/design/frame-description.md]"
        "(../../../../../../docs/design/frame-description.md); "
        "the API is `native/inc/wfm/wfm_frame.h`, certified in C by "
        "`native/tests/test_wfm_frame.c`. Neither is restated here."
    )
    R.md()


# ── 2. characterisation ──────────────────────────────────────────────
def measure_generic(d: Data) -> None:
    R.md("### 2.1 The representation is generic, and that is measurable")
    R.md()
    R.md(
        "The header stakes the design on this: a stage's kind is an open "
        '`uint32_t` so that *"a mission that is not CCSDS" is a '
        "configuration rather than a pull request against this header*. "
        "The first half of the claim is that an ordinary frame -- an "
        "arbitrary sync word, an arbitrary payload, a check -- needs "
        "nothing from CCSDS to exist. Six of them, across three sync "
        "lengths that are not any standard's:"
    )
    R.md()
    ok, exact, cover = True, True, True
    for sync in ([1, 0, 1, 1, 0, 0, 1, 0], [1, 1, 1, 1], [0, 1] * 12):
        for n_oct in (4, 16):
            f = plain_frame(sync, n_oct)
            bits = np.asarray(f.bits(1))  # ONE frame
            want = len(sync) + n_oct * 8 + CRC_BITS
            good = f.crc_ok(bits) == 1
            ok = ok and good
            exact = exact and f.nbits == want
            # the CRC's cover starts AFTER the sync -- declared, not
            # inherited from whatever ran before it
            cover = cover and f.stage_first(0) == len(sync)
            d.plain_rows.append(
                [
                    f"{len(sync)}",
                    f"{n_oct * 8}",
                    str(int(f.nbits)),
                    str(want),
                    f"{f.stage_first(0)}.."
                    f"{f.stage_first(0) + f.stage_bits(0)}",
                    "1" if good else "**0**",
                ]
            )
    d.plain_all_build, d.plain_len_exact = ok, exact
    d.cover_excludes_sync = cover
    # `bits(count)` counts FRAMES: n frames is exactly n copies of one, and
    # every emitted bit is 0/1. Pinned because getting it wrong is silent --
    # a bit count passed here materialises that many frames and the result
    # still looks like a bit array (it is what this validator did first).
    f = plain_frame([1, 0, 1, 1, 0, 0, 1, 0], 4)
    one = np.asarray(f.bits(1))
    three = np.asarray(f.bits(3))
    d.tiles_exactly = bool(
        one.size == f.nbits
        and three.size == 3 * f.nbits
        and np.array_equal(three, np.tile(one, 3))
    )
    d.all_bits_binary = bool(np.all((three == 0) | (three == 1)))
    R.table(
        [
            "sync bits",
            "payload bits",
            "nbits",
            "expected",
            "CRC covers",
            "crc_ok",
        ],
        d.plain_rows,
    )
    R.md(
        "Every one builds, is exactly `sync + payload + 16` bits long, and "
        "passes its own check. The cover column is the load-bearing part: "
        "the CRC starts at the sync's end because the description SAYS so, "
        "not because it inherited what ran before it."
    )
    R.md()


def measure_cadu(d: Data) -> None:
    R.md("### 2.2 CCSDS is one configuration of it, not its shape")
    R.md()
    R.md(
        "The other half of the claim. The same three calls that made a "
        "plain frame make a CADU -- an attached sync marker, a "
        "Reed-Solomon outer code and a randomiser -- by declaring "
        "different covers."
    )
    R.md()
    asm = ccsds_asm_bits()
    f = FrameDesc(EMPTY, EMPTY, EMPTY)
    f.add_field(asm)
    f.add_field(octets(223))
    f.add_field(EMPTY, derived_by=1, derived_bits=32 * 8)
    f.add_stage(ST_RS, first_field=1, n_fields=2, depth=1)
    f.add_stage(ST_RANDOMISE, first_field=1, n_fields=2)
    f.build()
    n_st = f.n_stages()
    d.cadu_bits = int(f.nbits)
    d.cadu_asm_bits = len(asm)
    d.cadu_stage_starts = [int(f.stage_first(s)) for s in range(n_st)]
    d.cadu_skips_marker = all(
        s == d.cadu_asm_bits for s in d.cadu_stage_starts
    )
    R.table(
        ["stage", "kind", "first bit", "bits covered"],
        [
            [
                str(s),
                ["crc16", "rs", "randomise"][[ST_RS, ST_RANDOMISE][s]],
                str(f.stage_first(s)),
                str(f.stage_bits(s)),
            ]
            for s in range(n_st)
        ],
    )
    R.md(
        f"{d.cadu_bits} bits: a {d.cadu_asm_bits}-bit marker followed by "
        f"{d.cadu_bits - d.cadu_asm_bits} bits of coded, randomised frame. "
        "**Both stages start at the marker's end**, which is the property "
        "the whole representation exists for: in a CADU the marker is "
        "covered by the inner code and by neither the outer code nor the "
        'randomiser, and a stage that inherited "everything before me" '
        'could not say that. A chain of optional transforms over "the '
        'frame" is right at three boundaries and wrong at the fourth -- '
        "wrong in the direction that still encodes, still decodes against "
        "itself, and syncs to nothing."
    )
    R.md()


def measure_check(d: Data) -> None:
    R.md("### 2.3 The check needs no payload truth")
    R.md()
    R.md(
        "What makes a frame error rate measurable on a real capture: the "
        "check reads the description and the received bits and nothing "
        "else. So it works where no truth exists, and unlike a "
        "self-referenced EVM it still catches a false lock, because a "
        "rotated constellation fails rather than looking clean."
    )
    R.md()
    f = plain_frame([1, 0, 1, 1, 0, 0, 1, 0], 16)
    clean = np.asarray(f.bits(1)).copy()
    caught = True
    for pos in (8, 40, 100, int(f.nbits) - 1):
        bad = clean.copy()
        bad[pos] ^= 1
        got = f.crc_ok(bad)
        caught = caught and got == 0
        d.flip_rows.append(
            [
                str(pos),
                "payload" if pos < f.nbits - CRC_BITS else "CRC",
                str(got),
                "caught" if got == 0 else "**MISSED**",
            ]
        )
    d.flips_all_caught = caught
    R.table(["bit flipped", "in", "crc_ok", ""], d.flip_rows)
    R.md(
        "The clean frame returns 1. A frame carrying NO check returns "
        "**-1**, and the three values are distinct on purpose: a frame "
        'error rate that read "carries no check" as "the check failed" '
        "would count every unprotected frame as an error."
    )
    R.md()
    plain = plain_frame([1, 0, 1, 0], 4, crc=False)
    d.no_crc_is_minus_one = plain.crc_ok(np.asarray(plain.bits(1))) == -1
    R.md()


def measure_sequences(d: Data) -> None:
    R.md("### 2.4 The generated field kinds (C §seq_bits)")
    R.md()
    R.md(
        "A field's bits come from a literal or from a generator, and "
        '`add_field` reaches all four kinds. `wfm_seq_bits` -- "the one '
        'place a `wfm_seq_t` becomes bits" -- had no C coverage at all '
        "before this certification (F1); these are the same properties, "
        "asked through the binding."
    )
    R.md()
    # DOTTED starts high, so a one-bit field is not silently zeros
    one = FrameDesc(EMPTY, EMPTY, EMPTY)
    one.add_field(EMPTY, kind=SEQ_DOTTED, gen_len=1)
    one.build()
    d.dotted_starts_high = int(np.asarray(one.bits(1))[0]) == 1
    dot = FrameDesc(EMPTY, EMPTY, EMPTY)
    dot.add_field(EMPTY, kind=SEQ_DOTTED, gen_len=9)
    dot.build()
    dbits = np.asarray(dot.bits(1))
    alternates = bool(np.array_equal(dbits, (np.arange(9) & 1) ^ 1))
    # PN with poly 0 must be maximal-length, not a feedback-free register
    pn_ok = True
    for n in (5, 7, 9):
        period = (1 << n) - 1
        p = FrameDesc(EMPTY, EMPTY, EMPTY)
        p.add_field(
            EMPTY, kind=SEQ_PN, gen_len=period, reg_bits=n, poly=0, seed=0
        )
        p.build()
        ones = int(np.asarray(p.bits(1)).sum())
        good = ones == (1 << (n - 1))
        pn_ok = pn_ok and good
        d.seq_rows.append(
            [
                f"pn n={n}",
                str(period),
                str(ones),
                str(1 << (n - 1)),
                "maximal" if good else "**NOT maximal**",
            ]
        )
    d.pn_is_maximal = pn_ok

    # GOLD must use BOTH registers
    def gold(seed_b: int) -> np.ndarray:
        g = FrameDesc(EMPTY, EMPTY, EMPTY)
        g.add_field(
            EMPTY,
            kind=SEQ_GOLD,
            gen_len=255,
            reg_bits=8,
            seed_a=1,
            seed_b=seed_b,
        )
        g.build()
        return np.asarray(g.bits(1))

    d.gold_uses_both = not np.array_equal(gold(1), gold(2))
    R.table(["kind", "period", "ones", "2^(n-1)", ""], d.seq_rows)
    R.md(
        "A PN field asked for `poly = 0` gets the maximal-length "
        "polynomial for its register, not a literal zero -- which would be "
        "a register with no feedback, emitting the seed and then zeros: a "
        "constant field that still looks like a field. Counting ones over "
        "a full period is what separates the two, and it is arithmetic "
        "about m-sequences rather than a second doppler opinion."
    )
    R.md()
    R.md(
        "The dotted pattern alternates"
        + ("" if alternates else " **-- except it does not**")
        + " and starts HIGH"
        + (
            ", so a one-bit dotted field is a 1 rather than being silently "
            "identical to an absent one."
            if d.dotted_starts_high
            else " **-- except it does not.**"
        )
        + " A Gold field changes when only the SECOND register's seed "
        "changes"
        + (
            ", so it is not a plain m-sequence under a Gold label."
            if d.gold_uses_both
            else " **-- except it does not.**"
        )
    )
    R.md()


def measure_extension(d: Data) -> None:
    R.md("### 2.5 Where the openness stops: the kernel is C-only (F2)")
    R.md()
    R.md(
        "The claim's mechanism is that a caller allocates a kind from "
        "`WFM_STAGE_USER` (0x1000) up and supplies the kernel through a "
        "`wfm_frame_ops_t` table. In C that works and is proven: "
        "`test_wfm_frame.c` allocates `WFM_STAGE_USER + 1`, assembles with "
        "its own table, and REVERSES through the same open lookup. From "
        "Python there is no ops-table argument anywhere, so a caller kind "
        "can be declared and never built."
    )
    R.md()
    f = FrameDesc(EMPTY, EMPTY, EMPTY)
    f.add_field(np.array([1, 0, 1, 0], np.uint8))
    f.add_field(octets(4))
    idx = f.add_stage(ST_USER + 1, first_field=0, n_fields=2)
    d.user_kind_accepted = idx >= 0
    try:
        f.build()
        d.user_kind_unbuildable = False
        outcome = "**built** -- unexpected"
    except ValueError as exc:
        d.user_kind_unbuildable = True
        outcome = f"refused: `{exc}`"
    d.user_above_builtins = ST_USER > ST_RANDOMISE
    R.table(
        ["step", "result"],
        [
            ["`add_stage(0x1001, ...)`", f"accepted, index {idx}"],
            ["`build()`", outcome],
        ],
    )
    R.md(
        "The refusal is the designed behaviour -- no kernel, no frame, "
        "never a silent skip -- and it is the same answer a "
        "declared-but-unsupplied built-in gets. What is missing is the "
        "other half: nothing on the Python face can supply one. So the "
        "five built-in kinds are the whole reachable menu from Python, "
        'which is the "fixed menu" the header argues against. Filed as '
        "gh-1125; the representation itself is unaffected (§2.1, §2.2)."
    )
    R.md()


def measure_reach(d: Data) -> None:
    R.md("### 2.6 What the Python face does not reach")
    R.md()
    d.unreachable = [
        (
            "`wfm_frame_ops_t` -- the caller's kernel table, and with it "
            "any stage kind doppler does not implement (§2.5, gh-1125)."
        ),
        (
            "`wfm_seq_bits` directly -- Python reaches all four kinds "
            "through `add_field`, which is the same function one layer up "
            "(§2.4)."
        ),
        (
            "`wfm_frame_dsss_nchips` -- the burst chip count, used by "
            "`wfm_synth` when it builds a DSSS burst rather than by a "
            "caller."
        ),
    ]
    for u in d.unreachable:
        R.md(f"- {u}")
    R.md()


def characterise() -> Data:
    R.md("## 2. Characterisation")
    R.md()
    R.md(
        "Measured behaviour, no verdicts. Each heading names the C section "
        "it tracks where there is one; the numbering is this report's own."
    )
    R.md()
    d = Data()
    measure_generic(d)
    measure_cadu(d)
    measure_check(d)
    measure_sequences(d)
    measure_extension(d)
    measure_reach(d)
    return d


# ── 3. review ────────────────────────────────────────────────────────
def review(d: Data) -> None:
    R.md("## 3. Review -- findings, with verdicts")
    R.md()
    R.find(
        "F1",
        "FIXED",
        '**"The one place a `wfm_seq_t` becomes bits" was tested by '
        "nothing.** `wfm_seq_bits` had zero mentions in any C test in the "
        "tree, while a descriptor materialises every generated field "
        "through it and the DSSS chip builder expands a sequence through "
        "it. The hazard it guards is stated in the header and was "
        'unasserted: `poly = 0` must mean "the maximal-length polynomial '
        'for this register", because a literal 0 reaching `pn_create()` '
        "is a register with NO FEEDBACK -- it emits the seed and then "
        "zeros, a constant field that still looks like a field. Closed by "
        "a section that counts ones over a full period (an m-sequence of "
        "period 2^n-1 has exactly 2^(n-1)), plus the mask-to-0/1 on "
        "LITERAL, DOTTED starting high, GOLD using its second register, "
        "and every refusal. Five sabotages, five red -- including passing "
        "`poly` and `seed` straight through.",
    )
    R.find(
        "F2",
        "GAP",
        "**The openness the design is staked on is C-only** (gh-1125). "
        "The header's argument is that an open `uint32_t` kind makes \"a "
        'mission that is not CCSDS" a configuration rather than a pull '
        "request. In C it holds and is proven -- a `WFM_STAGE_USER + 1` "
        "kind with a caller's ops table assembles and reverses. From "
        "Python `add_stage` accepts the kind and `build()` then refuses "
        "it, because the kernel must arrive through a `wfm_frame_ops_t` "
        "table that has no Python parameter (§2.5). The refusal is "
        "correct; the missing half is any way to supply the kernel, which "
        "leaves the five built-in kinds as the whole reachable menu from "
        "Python -- the fixed menu the header argues against.",
    )
    R.find(
        "F3",
        "BY DESIGN",
        "**A frame that carries no check answers -1, not 0.** Three "
        "distinct values -- 1 pass, 0 fail, -1 no check -- and the "
        "distinction is the difference between a frame error rate that "
        "means something and one that counts every unprotected frame as "
        "an error. Measured in §2.3 rather than assumed, because a "
        "two-valued reading of this is the kind of thing that looks right "
        "in every test where a CRC happens to be present.",
    )
    R.find(
        "F4",
        "C-ONLY",
        "**`wfm_frame_dsss_nchips` belongs to this layer and was tested "
        "only through the synthesiser.** The chip count a DSSS burst "
        "occupies is frame geometry, and until this certification it was "
        "exercised only where `wfm_synth` builds a burst -- so it was "
        "certified as a side effect of testing something else. Now "
        "checked in `test_wfm_frame.c` against the arithmetic the header "
        "states, including that a CRC costs exactly `WFM_FRAME_CRC_BITS` "
        "spread symbols and that frame bits with no data code are "
        "unbuildable. Not on the Python face, so it stays C-certified.",
    )


# ── 4. limits ────────────────────────────────────────────────────────
def limits(d: Data) -> None:
    R.md("## 4. Limits -- the certified envelope")
    R.md()
    R.md(
        "Claims a caller may rely on. A failure here is a regression, not "
        "a new finding. Every one is asserted by "
        "`src/doppler/wfm/tests/test_validation_limits.py`."
    )
    R.md()
    R.limit(
        d.plain_all_build,
        "a frame with an arbitrary sync word, an arbitrary payload and a "
        "CRC builds and passes its own check -- nothing CCSDS is required "
        "to describe a frame",
    )
    R.limit(
        d.plain_len_exact,
        "and its length is exactly sync + payload + 16 bits, at every "
        "geometry measured",
    )
    R.limit(
        d.cover_excludes_sync,
        "the CRC's cover starts at the sync's end because the description "
        "declares it, not because it inherited what ran before",
    )
    R.limit(
        d.cadu_bits == 2072 and d.cadu_asm_bits == 32,
        f"a CCSDS CADU is one configuration of the same machinery: "
        f"{d.cadu_bits} bits behind a {d.cadu_asm_bits}-bit marker",
    )
    R.limit(
        d.cadu_skips_marker,
        "and BOTH its stages start at the marker's end -- the property a "
        "stage that inherited 'everything before me' could not express",
    )
    R.limit(
        d.flips_all_caught,
        "a single flipped bit fails the check at every position tried, "
        "with no payload truth supplied",
    )
    R.limit(
        d.no_crc_is_minus_one,
        "a frame carrying no check answers -1, distinct from the 0 that "
        "means the check failed",
    )
    R.limit(
        d.pn_is_maximal,
        "a PN field with poly 0 is maximal-length -- exactly 2^(n-1) ones "
        "over its period, at register widths 5, 7 and 9",
    )
    R.limit(
        d.dotted_starts_high,
        "a dotted field starts HIGH, so a one-bit one is not silently the "
        "same as an absent one",
    )
    R.limit(
        d.gold_uses_both,
        "a Gold field changes when only the second register's seed does, "
        "so it is not a plain m-sequence under a Gold label",
    )
    R.limit(
        d.user_kind_accepted,
        "a caller's own stage kind from WFM_STAGE_USER up is accepted by "
        "the description",
    )
    R.limit(
        d.user_kind_unbuildable,
        "and refused at build time from Python, because the kernel has no "
        "way in -- recorded so gh-1125 cannot be closed without this "
        "report moving",
    )
    R.limit(
        d.user_above_builtins,
        "the caller range sits above every kind doppler names, so a kind "
        "chosen today cannot collide with a built-in added later",
    )
    R.limit(
        d.tiles_exactly,
        "bits(n) materialises n FRAMES -- exactly n copies of bits(1) -- so "
        "a stream compared against it lines up with the one transmitted",
    )
    R.limit(
        d.all_bits_binary,
        "and every emitted bit is 0 or 1, one per byte",
    )
    R.limit(
        len(d.unreachable) == 3,
        "three header entry points are not on the Python face and are "
        "certified in C instead -- counted, so one appearing or vanishing "
        "is a change",
    )


# ── build ────────────────────────────────────────────────────────────
def build(write: bool = True) -> Report:
    """Measure everything and render the report."""
    global R
    R = Report(write=write)
    if write:
        DATA.mkdir(parents=True, exist_ok=True)
    section_object()
    d = characterise()
    review(d)
    limits(d)
    R.executive(
        "Frame",
        [
            "**A frame here is not a CCSDS frame with options.** An "
            "arbitrary sync word, an arbitrary payload and a check build "
            "and self-check with nothing from CCSDS involved, at every "
            "geometry measured; the CADU is then the same three calls "
            "with different covers declared (§2.1, §2.2).",
            "**Declare each stage's cover; never let it inherit.** Both of "
            "a CADU's stages start at the marker's end, which is what "
            "makes a CADU expressible at all -- the marker is covered by "
            "the inner code and by neither the outer code nor the "
            "randomiser. A chain that applied each transform to 'the "
            "frame' is right at three boundaries and wrong at the fourth "
            "(§2.2).",
            "**The check needs no truth, so it works on a real capture.** "
            "A single flipped bit fails at every position tried, and a "
            "frame with no check answers -1 rather than 0 -- so an FER "
            "does not count unprotected frames as errors (§2.3, F3).",
            "**A custom stage needs C.** A caller's kind is accepted by "
            "the description and refused at build time from Python, "
            "because the kernel table has no Python parameter. The "
            "representation is open; the extension point is not, on this "
            "face (§2.5, F2, gh-1125).",
            "**Ask a generated field for `poly = 0`, not a literal 0.** "
            'Zero means "the maximal-length polynomial for this '
            'register"; a literal zero reaching the LFSR is a register '
            "with no feedback that emits a constant field still shaped "
            "like a field. Nothing tested that until this certification "
            "(§2.4, F1).",
        ],
    )
    R.summary()
    R.emit(HERE / "results.md")
    return R


if __name__ == "__main__":
    sys.exit(cli(build, HERE))
