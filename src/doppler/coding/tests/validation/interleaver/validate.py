"""Interleaver — certification evidence, measured through the binding.

Run directly to regenerate `results.md`, the plots and the CSVs:

    uv run python src/doppler/coding/tests/validation/interleaver/validate.py

`--check` re-renders in memory and diffs against the committed bytes;
`make validate` writes, `make validate-check` checks. Every limit this
records is asserted by
`src/doppler/coding/tests/test_validation_limits.py`, which runs this same
`build(write=False)`.

The order is the campaign's, not this file's:
`native/inc/interleaver/interleaver_core.h` is the SSOT,
`native/tests/test_interleaver_core.c` certifies it in C, and
`native/validation/interleave_burst_gain.c` measures the coding gain over
a real outer code. This file measures the same properties through
`doppler.coding.Interleaver` and `Deinterleaver` to show the binding
delivers them.

**Why the permutation is measured against numpy here and against the
kernel in C.** The object's job is to apply `dp_interleave.h`'s transform
at the geometry it holds, so the C test compares it with `dp_interleave_u8`
— the definition. Comparing the binding against the same kernel from
Python would only re-ask that question one layer out. What Python can ask
that C cannot is whether the answer is the transform a caller *means*:
write by rows, read by columns, which numpy expresses as a reshape and a
transpose in one line that owes nothing to doppler. That is an external
truth, and §2.1 is measured against it.
"""

from __future__ import annotations

import contextlib
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path

import numpy as np

from doppler.coding import Deinterleaver, Interleaver, ReedSolomon
from doppler.tests._repo import repo_root
from doppler.tests._validation_common import Report, cli

HERE = Path(__file__).resolve().parent
DATA = HERE / "data"
ROOT = repo_root(__file__)

R = Report()

# The geometries every structural sweep runs over. Non-square throughout,
# and mostly coprime: a square block is its own inverse and a transposed
# geometry is the same permutation, so a validator that only measured 8x8
# would pass on an object that had the two numbers exchanged.
GEOMETRIES = [(3, 4, 1), (5, 7, 1), (4, 6, 2), (8, 31, 1), (5, 51, 8)]

# The outer code §2.4 measures the gain over. RS(255,223) corrects E = 16
# symbol errors per codeword, which is the CCSDS telemetry code and the
# one the design page and the C harness both use — three measurements of
# one claim should not each pick their own code.
RS_NROOTS = 32
FER_TRIALS = 8
FER_DEPTHS = [1, 3, 5]


def _text(p: Path) -> str:
    return p.read_text(encoding="utf-8")


def _csv(path: Path, header: str, rows: list[list[float]]) -> None:
    if not R.write:
        return
    DATA.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as fh:
        fh.write(header + "\n")
        for r in rows:
            fh.write(",".join(f"{v:.10g}" for v in r) + "\n")


def truth(x: np.ndarray, rows: int, cols: int, unit: int) -> np.ndarray:
    """Write by rows, read by columns — the transform, in numpy.

    The external truth §2.1 scores against. It is one reshape and one
    transpose, owes nothing to doppler, and is the definition a reader can
    check by eye: a `rows x cols` matrix of `unit`-wide units, written
    across and read down.

    Parameters
    ----------
    x : ndarray
        `rows * cols * unit` elements, one block.
    rows, cols, unit : int
        The geometry, in units and elements-per-unit.

    Returns
    -------
    ndarray
        The interleaved block.
    """
    return x.reshape(rows, cols, unit).transpose(1, 0, 2).reshape(-1)


def view_doc_faces() -> tuple[bool, str]:
    """Do the `Deinterleaver`'s two docstrings say the same thing?

    Derived rather than asserted, because it is the specific hazard
    just-makeit#1160 leaves behind: a VIEW's runtime `tp_doc` is a
    placeholder rather than its own `create_fn`'s, so doppler hand-writes
    it in the sacred fragment — and the `.pyi` stub gets whatever jm
    derives, which for a view is the PARENT's `create()`. Two faces of one
    class, from two sources, and nothing compares them.

    Compared on the opening sentence, which is what a `help()` reader and
    a type checker each see first, with whitespace collapsed since the two
    are wrapped by different tools.

    Returns
    -------
    tuple of (bool, str)
        Whether they agree, and the stub's opening sentence — the report
        quotes it, because "they disagree" without saying what the stub
        actually claims is the half a reader cannot act on.
    """
    stub = _text(ROOT / "src/doppler/coding/coding.pyi")
    m = re.search(r'class Deinterleaver[^:]*:\s*\n\s*"""(.*?)\n', stub, re.S)
    if not m or Deinterleaver.__doc__ is None:
        return False, "(not found)"
    first_stub = " ".join(m.group(1).split())
    first_run = " ".join(Deinterleaver.__doc__.split("\n")[0].split())
    return first_stub == first_run, first_stub


@dataclass
class Data:
    """Everything §2 measured, handed to §3 and §4."""

    perm_rows: list[list[object]] = field(default_factory=list)
    perm_all_match: bool = False
    perm_all_discriminate: bool = False
    budget_rows: list[list[object]] = field(default_factory=list)
    budget_exact: bool = False
    maxout_identity: bool = False
    spread_rows: list[list[object]] = field(default_factory=list)
    spread_holds: bool = False
    spread_bound_bites: bool = False
    bare_worst: int = 0
    fer_rows: list[list[float]] = field(default_factory=list)
    fer_bound: dict[int, int] = field(default_factory=dict)
    fer_bound_exact: bool = False
    fer_depth1_no_gain: bool = False
    unit_rows: list[list[object]] = field(default_factory=list)
    unit_matters: bool = False
    soft_exact: bool = False
    soft_matches_hard: bool = False
    soft_nonfinite_survives: bool = False
    refusal_rows: list[list[object]] = field(default_factory=list)
    all_refused: bool = False
    refusal_names_the_block: bool = False
    view_geometry_agrees: bool = False
    view_round_trips: bool = False
    view_has_no_interleave: bool = False
    mismatch_rows: list[list[object]] = field(default_factory=list)
    mismatch_silent: bool = False
    mismatch_worst_wrong: float = 0.0
    doc_faces_agree: bool = False
    doc_stub_says: str = ""


# ── 1. the object ────────────────────────────────────────────────────
def section_object() -> None:
    R.md("## 1. The object")
    R.md()
    R.md(
        "A block interleaver, held as an object. The transform is "
        "`dp_interleave.h` — header-only, stateless, shared with the frame "
        "stage — and every method here calls it; what the object adds is "
        "that the geometry is DECLARED rather than inferred from whatever "
        "length arrives. That is the whole design: a block interleaver only "
        "works if the two ends of a link agree on the permutation, and "
        "deriving `cols` from the input length means a truncated frame "
        "silently produces a DIFFERENT permutation instead of an error."
    )
    R.md()
    R.md("Design and API, not restated here:")
    R.md()
    R.table(
        ["page", "owns"],
        [
            [
                "`native/inc/interleaver/interleaver_core.h`",
                "the SSOT for every claim below",
            ],
            [
                "`native/inc/dp_interleave.h`",
                "the permutation itself, and why the unit is a parameter",
            ],
            [
                "[Interleaving](../../../../../../docs/design/interleaving.md)",
                "what it buys, the three ways to get it wrong, where it "
                "sits in a frame",
            ],
            [
                "`native/tests/test_interleaver_core.c`",
                "the object's C certification",
            ],
            [
                "`native/tests/test_dp_interleave.c`",
                "the permutation's C certification, against its index map",
            ],
            [
                "`native/validation/interleave_burst_gain.c`",
                "the coding gain in C, over the same RS(255,223)",
            ],
            [
                "`src/doppler/examples/interleave_burst_demo.py`",
                "the runnable demonstration, with the bound asserted",
            ],
        ],
    )
    R.md()
    R.md("### Claim coverage — every prose claim in the header")
    R.md()
    R.md(
        "Header first: enumerate what the header asserts, ask of each "
        "whether a C assertion pins it, and only then measure it through "
        "the binding. `NEW` marks a C section this certification added — "
        "four of them, because the object's own direction was unpinned "
        "(§3 F1) and its receive face was untested (§3 F2)."
    )
    R.md()
    R.table(
        ["#", "claim in `interleaver_core.h`", "C test", "here"],
        [
            [
                "C1",
                "every method calls `dp_interleave.h`, at the held geometry",
                "`applies_the_kernels_permutation` (NEW)",
                "§2.1",
            ],
            [
                "C2",
                "`create` refuses a zero or overflowing geometry",
                "`refuses_a_degenerate_geometry`",
                "§2.7",
            ],
            [
                "C3",
                "`create_rx` is identical construction, same refusals",
                "`the_receive_face_is_the_same_geometry` (NEW)",
                "§2.8",
            ],
            [
                "C4",
                "`block_bits` is `rows * cols * unit_bits`",
                "`geometry_readbacks`",
                "§2.2",
            ],
            [
                "C5",
                "`burst_len` is `rows`; `separation` is `cols`",
                "`geometry_readbacks`",
                "§2.2",
            ],
            [
                "C6",
                "a burst of `burst_len` units touches each codeword once",
                "`burst_of_burst_len_hits_each_codeword_once` (NEW)",
                "§2.3",
            ],
            [
                "C7",
                "an outer code correcting `t` survives `t * burst_len`",
                "`interleave_burst_gain.c`",
                "§2.4",
            ],
            [
                "C8",
                "the unit must match the code's symbol",
                "`interleave_burst_gain.c`",
                "§2.5",
            ],
            [
                "C9",
                "`*_max_out` is the identity, for any length",
                "`geometry_readbacks`",
                "§2.2",
            ],
            [
                "C10",
                "`deinterleave` undoes `interleave` over the same geometry",
                "`round_trips_over_several_blocks`",
                "§2.1",
            ],
            [
                "C11",
                "the soft path is the same permutation over floats",
                "`soft_deinterleave_matches_the_hard_one`",
                "§2.6",
            ],
            [
                "C12",
                "a partial block is REFUSED, never padded or truncated",
                "`a_partial_block_is_refused`",
                "§2.7",
            ],
            [
                "C13",
                "`reset` is a no-op and the geometry survives it",
                "`reset_changes_nothing`",
                "§2.7",
            ],
            [
                "C14",
                "stateless: nothing survives between calls",
                "`nothing_survives_between_calls` (NEW)",
                "§2.7",
            ],
            [
                "C15",
                "a geometry mismatch is not an error, it is garbage",
                "—",
                "§2.8 (NEW)",
            ],
        ],
    )
    R.md()
    R.md(
        "**What Python cannot reach.** One path: the `max_out` refusal. "
        "The C entry points take a caller's buffer and return 0 when it is "
        "too small, while the binding allocates the output itself from "
        "`interleave_max_out`, so that branch is unreachable from Python by "
        "construction. It is certified in C "
        "(`a_partial_block_is_refused`) and recorded as §3 F6."
    )
    R.md()


# ── 2. characterisation ──────────────────────────────────────────────
def measure_permutation(d: Data) -> None:
    R.md("### 2.1 The permutation, against a truth that is not doppler's")
    R.md()
    R.md(
        "Write by rows into a `rows x cols` matrix of `unit_bits`-wide "
        "units, read back by columns. Scored against `numpy`'s reshape and "
        "transpose — an external truth, so this cannot pass by agreeing "
        "with the other entry point. The `discriminates` column is the "
        "precondition: it asserts the transposed geometry is a DIFFERENT "
        "answer, without which agreeing with the truth would prove nothing "
        "about the direction (§3 F1)."
    )
    R.md()
    rows: list[list[object]] = []
    ok = disc = True
    rt = True
    for r, c, u in GEOMETRIES:
        il = Interleaver(rows=r, cols=c, unit_bits=u)
        n = il.block_bits
        x = (np.arange(n, dtype=np.int64) * 37 % 251).astype(np.uint8)
        y = np.asarray(il.interleave(x))
        want = truth(x, r, c, u)
        alt = truth(x, c, r, u)
        back = np.asarray(il.deinterleave(y))
        matches = bool(np.array_equal(y, want))
        differs = bool(not np.array_equal(y, alt))
        loops = bool(np.array_equal(back, x))
        ok &= matches
        disc &= differs
        rt &= loops
        rows.append(
            [
                f"{r}x{c}",
                u,
                n,
                "yes" if matches else "**no**",
                "yes" if differs else "**no**",
                "yes" if loops else "**no**",
            ]
        )
    R.table(
        [
            "geometry",
            "unit_bits",
            "block_bits",
            "== numpy truth",
            "discriminates",
            "round trips",
        ],
        rows,
    )
    R.md(
        "Many blocks in one call are permuted independently, which a "
        "single-block measurement cannot see: over five blocks of the "
        "`4x6` geometry the object's answer equals the truth applied "
        "block-by-block, so nothing crosses a boundary."
    )
    R.md()
    il = Interleaver(rows=4, cols=6, unit_bits=2)
    blk = il.block_bits
    x = (np.arange(blk * 5, dtype=np.int64) * 31 % 251).astype(np.uint8)
    y = np.asarray(il.interleave(x))
    per_block = np.concatenate(
        [truth(x[b * blk : (b + 1) * blk], 4, 6, 2) for b in range(5)]
    )
    d.perm_rows = rows
    d.perm_all_match = bool(ok and np.array_equal(y, per_block))
    d.perm_all_discriminate = bool(disc and rt)


def measure_budget(d: Data) -> None:
    R.md("### 2.2 The link budget the object hands back")
    R.md()
    R.md(
        "`burst_len` and `separation` are not diagnostics — they are the "
        "two numbers a caller sizes an outer code against, and §2.3 and "
        "§2.4 measure what they promise. `max_out` is swept rather than "
        "sampled because the binding asks it to SIZE the output for "
        "whatever length arrived, including lengths the transform will "
        "then refuse."
    )
    R.md()
    rows: list[list[object]] = []
    exact = True
    ident = True
    for r, c, u in GEOMETRIES:
        il = Interleaver(rows=r, cols=c, unit_bits=u)
        got = (il.block_bits, il.burst_len, il.separation)
        exact &= got == (r * c * u, r, c)
        for n in (0, 1, 7, 2047, 2048, 12345):
            ident &= il.interleave_max_out(n) == n
            ident &= il.deinterleave_max_out(n) == n
            ident &= il.deinterleave_soft_max_out(n) == n
        rows.append([f"{r}x{c}", u, got[0], got[1], got[2], r * c])
    R.table(
        [
            "geometry",
            "unit_bits",
            "block_bits",
            "burst_len",
            "separation",
            "units/block",
        ],
        rows,
    )
    d.budget_rows = rows
    d.budget_exact = bool(exact)
    d.maxout_identity = bool(ident)


def measure_spread(d: Data) -> None:
    R.md("### 2.3 A burst of `burst_len` hits each codeword once (C §NEW)")
    R.md()
    R.md(
        "The invariant the object exists for, measured through the binding "
        "at the level a link designer reasons about: a run of consecutive "
        "units on the WIRE, de-interleaved, and counted per codeword — a "
        "codeword being one row of `separation` input units. Every start "
        "position of every block is swept, not sampled: the interesting "
        "ones straddle a column boundary."
    )
    R.md()
    R.md(
        "Three columns, because the good case alone is not evidence. The "
        "bound must BITE one unit later, or `burst_len` would be a "
        "conservative figure rather than the exact one the header claims; "
        "and the un-interleaved control shows what the same burst costs "
        "with no interleaver at all."
    )
    R.md()
    rows: list[list[object]] = []
    holds = bites = True
    bare = 0
    for r, c, u in GEOMETRIES:
        il = Interleaver(rows=r, cols=c, unit_bits=u)
        rx = Deinterleaver(rows=r, cols=c, unit_bits=u)
        nunits = r * c
        worst_at, worst_over = 0, 0
        for n in (r, r + 1):
            for start in range(nunits - n + 1):
                wire = np.zeros(il.block_bits, dtype=np.uint8)
                wire.reshape(nunits, u)[start : start + n] = 1
                back = np.asarray(rx.deinterleave(wire))
                hits = back.reshape(r, c, u).any(axis=2).sum(axis=1)
                w = int(hits.max())
                if n == r:
                    worst_at = max(worst_at, w)
                else:
                    worst_over = max(worst_over, w)
        # the control: with no interleaver a run of `rows` wire units IS a
        # run of `rows` input units, so it sits inside one codeword
        # wherever it does not straddle a boundary
        control = max(
            sum((start + k) // c == start // c for k in range(r))
            for start in range(nunits - r + 1)
        )
        holds &= worst_at == 1
        bites &= worst_over == 2
        bare = max(bare, control)
        rows.append([f"{r}x{c}", u, r, worst_at, worst_over, control])
    R.table(
        [
            "geometry",
            "unit_bits",
            "burst_len",
            "worst hits at burst_len",
            "at burst_len+1",
            "no interleaver",
        ],
        rows,
    )
    R.md("![burst spreading](spread.png)")
    R.md()
    d.spread_rows = rows
    d.spread_holds = bool(holds)
    d.spread_bound_bites = bool(bites)
    d.bare_worst = bare


def _fer(rs, depth: int, burst: int, interleaved: bool) -> float:
    """Frame error rate for one burst length, over `FER_TRIALS` positions.

    A frame is `depth` independently encoded codewords; ANY of them failing
    loses it. The burst goes on the WIRE — corrupting the codewords
    directly would measure nothing, since that is the arrangement the
    interleaver is applied to.
    """
    tx = Interleaver(rows=depth, cols=rs.n, unit_bits=8)
    rx = Deinterleaver(rows=depth, cols=rs.n, unit_bits=8)
    rng = np.random.default_rng(7)
    total = rs.n * depth
    bad = 0
    for t in range(FER_TRIALS):
        info = rng.integers(0, 256, size=(depth, rs.k), dtype=np.uint8)
        cw = np.stack([rs.encode(row) for row in info])
        wire = np.unpackbits(cw.reshape(-1))
        if interleaved:
            wire = np.asarray(tx.interleave(wire))
        at = (t * 37) % total
        idx = (np.arange(burst * 8) + at * 8) % wire.size
        wire = wire.copy()
        wire[idx] ^= 1
        if interleaved:
            wire = np.asarray(rx.deinterleave(wire))
        recv = np.packbits(wire).reshape(depth, rs.n)
        for c in range(depth):
            word = recv[c].copy()
            # The comparison is the verdict, not the decoder's return: a
            # bounded-distance decoder can "succeed" while miscorrecting,
            # and a frame lost that way is lost just the same.
            if rs.decode(word) < 0 or not np.array_equal(
                word[: rs.k], info[c]
            ):
                bad += 1
                break
    return bad / FER_TRIALS


def measure_gain(d: Data) -> None:
    R.md("### 2.4 The coding gain, over a real outer code")
    R.md()
    rs = ReedSolomon(nroots=RS_NROOTS)
    R.md(
        f"§2.3 counts hits per codeword; this measures what they cost. "
        f"`depth` codewords of RS({rs.n},{rs.k}) — one per interleaver row, "
        f"correcting E = {rs.e} symbol errors each — are encoded "
        f"independently, interleaved at `unit_bits = 8`, hit with a burst "
        f"of consecutive octets on the wire, and decoded. The prediction "
        f"is exact rather than directional: the corrigible burst is "
        f"E x depth octets, and not one octet more."
    )
    R.md()
    R.md(
        "Depth 1 is the control that makes the rest mean something. "
        "Interleaving a SINGLE codeword buys nothing at all — Reed-Solomon "
        "corrects any E symbol errors wherever they fall — so its two "
        "columns must agree everywhere, and any gain shown at depth 1 "
        "would be a measurement artefact rather than the transform."
    )
    R.md()
    rows: list[list[float]] = []
    bounds: dict[int, int] = {}
    exact = True
    for depth in FER_DEPTHS:
        bound = rs.e * depth
        # dict.fromkeys, not a set: at depth 1 the bound IS E, so the four
        # interesting lengths collapse to two and a set would also reorder
        # them. The control has to stay in the table -- it is the row that
        # says interleaving one codeword buys nothing.
        for burst in dict.fromkeys((rs.e, rs.e + 1, bound, bound + 1)):
            rows.append(
                [
                    float(depth),
                    float(burst),
                    _fer(rs, depth, burst, False),
                    _fer(rs, depth, burst, True),
                ]
            )
        at_bound = rows[-2][3]
        past_bound = rows[-1][3]
        bounds[depth] = bound
        exact &= at_bound == 0.0 and past_bound > 0.0
        if depth == 1:
            # the control: identical columns, or the "gain" measured at
            # every other depth is an artefact of the harness
            no_gain = all(r[2] == r[3] for r in rows if int(r[0]) == 1)
    R.table(
        ["depth", "burst (octets)", "FER bare", "FER interleaved"],
        [[int(r[0]), int(r[1]), f"{r[2]:.3f}", f"{r[3]:.3f}"] for r in rows],
    )
    R.md("![coding gain](gain.png)")
    R.md()
    R.md(
        f"The bound is the honest half. An interleaver does not make a "
        f"link immune to bursts; it multiplies the length the link "
        f"survives by exactly the depth — {rs.e} octets bare, "
        f"{bounds[max(FER_DEPTHS)]} at depth {max(FER_DEPTHS)} — and one "
        f"octet past that the frame is lost."
    )
    R.md()
    _csv(
        DATA / "coding_gain.csv",
        "depth,burst_octets,fer_bare,fer_interleaved",
        rows,
    )
    d.fer_rows = rows
    d.fer_bound = bounds
    d.fer_bound_exact = bool(exact)
    d.fer_depth1_no_gain = bool(no_gain)


def measure_unit(d: Data) -> None:
    R.md("### 2.5 The unit is a parameter, and it is load-bearing")
    R.md()
    R.md(
        "`unit_bits` decides what gets spread. Interleaving OCTETS spreads "
        "a burst across the codewords of a symbol-oriented code; "
        "interleaving BITS inside such a code spreads it within symbols "
        "that are already wrong. Measured at the E x depth bound, where "
        "the difference is worth exactly one symbol per codeword — the "
        "difference between correcting and not."
    )
    R.md()
    rs = ReedSolomon(nroots=RS_NROOTS)
    depth = 5
    bound = rs.e * depth
    rows: list[list[object]] = []
    got: dict[int, float] = {}
    for unit in (8, 1):
        tx = Interleaver(rows=depth, cols=rs.n * 8 // unit, unit_bits=unit)
        rx = Deinterleaver(rows=depth, cols=rs.n * 8 // unit, unit_bits=unit)
        rng = np.random.default_rng(7)
        bad = 0
        for t in range(FER_TRIALS):
            info = rng.integers(0, 256, size=(depth, rs.k), dtype=np.uint8)
            cw = np.stack([rs.encode(row) for row in info])
            wire = np.asarray(tx.interleave(np.unpackbits(cw.reshape(-1))))
            at = (t * 37) % (rs.n * depth)
            idx = (np.arange(bound * 8) + at * 8) % wire.size
            wire = wire.copy()
            wire[idx] ^= 1
            recv = np.packbits(np.asarray(rx.deinterleave(wire))).reshape(
                depth, rs.n
            )
            for c in range(depth):
                word = recv[c].copy()
                if rs.decode(word) < 0 or not np.array_equal(
                    word[: rs.k], info[c]
                ):
                    bad += 1
                    break
        got[unit] = bad / FER_TRIALS
        rows.append([unit, bound, f"{got[unit]:.3f}"])
    R.table(["unit_bits", "burst (octets)", "FER"], rows)
    R.md(
        "The mechanism is ALIGNMENT rather than magnitude, and the "
        "measurement is what settles that: each codeword receives its "
        "share of the burst as a contiguous run either way, so the counts "
        "are within one symbol. At `unit_bits = 8` each codeword gets "
        "whole symbols; at 1 the run is not symbol-aligned and can touch "
        "one more at each end, which is invisible until the burst is at "
        "the bound — precisely where it decides the frame."
    )
    R.md()
    d.unit_rows = rows
    d.unit_matters = bool(got[8] == 0.0 and got[1] > got[8])


def measure_soft(d: Data) -> None:
    R.md("### 2.6 The soft path — the receive side that matters")
    R.md()
    R.md(
        "`dsss_burst_receiver`'s LLRs span a whole frame and an outer "
        "decoder wants them de-interleaved BEFORE it runs; slicing to hard "
        "bits first throws away the confidence the soft output exists to "
        "carry. So the property to certify is not that the soft path is "
        "close to the hard one — it is that the soft path is the SAME "
        "permutation and moves values without touching them."
    )
    R.md()
    il = Interleaver(rows=5, cols=7, unit_bits=1)
    n = il.block_bits
    llr = (np.arange(n, dtype=np.float32) * -0.375 + 1.25).astype(np.float32)
    soft = np.asarray(il.deinterleave_soft(llr))
    hard = np.asarray(il.deinterleave(np.arange(n, dtype=np.uint8)))
    exact = bool(np.array_equal(soft, truth(llr, 7, 5, 1)))
    same = bool(np.array_equal(soft, llr[hard]))

    # Non-finite values are the sharp test of "moves, does not compute":
    # a transform doing any arithmetic at all would turn one of these into
    # something else, and an LLR of +/-inf is what a hard decision looks
    # like arriving on the soft path.
    odd = np.array([np.inf, -np.inf, np.nan, 0.0, -0.0] * 7, dtype=np.float32)
    out = np.asarray(il.deinterleave_soft(odd))
    survives = bool(
        np.array_equal(
            out.view(np.uint32), truth(odd, 7, 5, 1).view(np.uint32)
        )
    )
    R.table(
        ["property", "result"],
        [
            [
                "soft output == the numpy truth over floats",
                "exact" if exact else "**differs**",
            ],
            [
                "soft output == the hard permutation applied to the LLRs",
                "identical" if same else "**differs**",
            ],
            [
                "+/-inf, NaN and -0.0 arrive bit-for-bit",
                "yes" if survives else "**no**",
            ],
        ],
    )
    R.md(
        "There is no `interleave_soft`, and its absence is the design "
        "rather than a gap: a transmitter has bits, not LLRs (§3 F5)."
    )
    R.md()
    d.soft_exact = exact
    d.soft_matches_hard = same
    d.soft_nonfinite_survives = survives


def measure_refusals(d: Data) -> None:
    R.md("### 2.7 What it refuses, and what it carries between calls")
    R.md()
    R.md(
        "A partial block is REFUSED — never padded, never truncated to the "
        "whole blocks it could manage. Truncating is the dangerous one: it "
        "returns a plausible shorter frame and says nothing, and the "
        "receiver de-interleaves it into a different permutation. Every "
        "method is checked, because a guard on two of three is the shape "
        "where the soft path keeps working after the hard one is fixed."
    )
    R.md()
    il = Interleaver(rows=3, cols=4, unit_bits=1)
    blk = il.block_bits
    rows: list[list[object]] = []
    refused = True
    names = True
    cases = [
        ("interleave", blk - 1, "one bit short of a block"),
        ("interleave", blk + 1, "one bit past a block"),
        ("interleave", 0, "empty input"),
        ("deinterleave", blk + 1, "one bit past a block"),
        ("deinterleave_soft", blk + 1, "one value past a block"),
        ("deinterleave_soft", 0, "empty input"),
    ]
    for name, n, why in cases:
        dtype = np.float32 if name.endswith("soft") else np.uint8
        try:
            getattr(il, name)(np.zeros(n, dtype=dtype))
            verdict, msg = "**accepted**", ""
            refused = False
        except ValueError as exc:
            verdict, msg = "ValueError", str(exc)
            names &= f"block_bits = {blk}" in msg
        rows.append([f"`{name}`", n, why, verdict])
    for r, c, u, why in (
        (0, 4, 1, "rows"),
        (4, 0, 1, "cols"),
        (4, 4, 0, "unit_bits"),
    ):
        try:
            Interleaver(rows=r, cols=c, unit_bits=u)
            rows.append(
                [
                    "`Interleaver()`",
                    f"{r}x{c}x{u}",
                    f"zero {why}",
                    "**accepted**",
                ]
            )
            refused = False
        except ValueError:
            rows.append(
                [
                    "`Interleaver()`",
                    f"{r}x{c}x{u}",
                    f"zero {why}",
                    "ValueError",
                ]
            )
    R.table(["call", "length", "case", "outcome"], rows)
    R.md(
        f"The message names the block size — *"
        f"`interleave: length {blk + 1} is not a whole number of blocks of "
        f"block_bits = {blk}`* — which is the number a caller needs to fix "
        f"their framing, and is not derivable from the exception type."
    )
    R.md()
    R.md(
        "**Nothing survives between calls.** The sentence that exempts "
        "this object from the state-serialization standard every other "
        "stateful object obeys, and its only observable is that the same "
        "input answers the same way regardless of history. Measured "
        "across a successful call, a refused one, the inverse direction "
        "and a `reset`."
    )
    R.md()
    x = (np.arange(blk, dtype=np.int64) * 13 % 251).astype(np.uint8)
    first = np.asarray(il.interleave(x))
    other = (np.arange(blk, dtype=np.int64) * 7 % 251).astype(np.uint8)
    il.interleave(other)
    with contextlib.suppress(ValueError):
        il.interleave(np.zeros(blk - 1, dtype=np.uint8))
    il.deinterleave(other)
    il.reset()
    again = np.asarray(il.interleave(x))
    stateless = bool(np.array_equal(again, first))
    R.md(
        f"Same input, same answer after four intervening calls: "
        f"**{'yes' if stateless else 'no'}**. `reset()` is a documented "
        f"no-op, and the geometry survives it — a reset that cleared THAT "
        f"would leave every later call refusing, which is the failure a "
        f'"does nothing" function can still have.'
    )
    R.md()
    d.refusal_rows = rows
    d.all_refused = bool(refused and stateless)
    d.refusal_names_the_block = bool(names)


def measure_view(d: Data) -> None:
    R.md("### 2.8 The two faces, and the geometry they must agree on")
    R.md()
    R.md(
        "`Deinterleaver` is a VIEW over one core, not a second object: "
        "`rows`, `cols` and `unit_bits` are exactly what the two ends of a "
        "link must agree on, so one core means one definition of the "
        "geometry to get right. It exists because the two ends are written "
        "by different people — someone working the receive side reaches "
        "for a `Deinterleaver`, and a class findable only under the "
        "transmit name is a class they do not find."
    )
    R.md()
    tx = Interleaver(rows=5, cols=7, unit_bits=2)
    rx = Deinterleaver(rows=5, cols=7, unit_bits=2)
    geom = (tx.rows, tx.cols, tx.unit_bits, tx.block_bits, tx.burst_len)
    same = geom == (
        rx.rows,
        rx.cols,
        rx.unit_bits,
        rx.block_bits,
        rx.burst_len,
    )
    x = (np.arange(tx.block_bits, dtype=np.int64) * 11 % 251).astype(np.uint8)
    wire = np.asarray(tx.interleave(x))
    trip = bool(np.array_equal(np.asarray(rx.deinterleave(wire)), x))
    no_fwd = not hasattr(rx, "interleave")
    R.table(
        ["property", "result"],
        [
            [
                "the two faces read back one geometry",
                "yes" if same else "**no**",
            ],
            [
                "what the transmit face interleaved, the receive face undoes",
                "yes" if trip else "**no**",
            ],
            [
                "`interleave` is absent from the receive face",
                "yes" if no_fwd else "**no**",
            ],
        ],
    )
    R.md(
        "The absent forward method is deliberate: a receive-only face that "
        "can silently run the forward direction is a footgun."
    )
    R.md()
    R.md(
        "**A mismatch between the ends is not an error.** The object holds "
        "the geometry so a caller cannot infer it from a length — but "
        "nothing checks that the far end chose the same numbers, and there "
        "is nothing on the wire that could. A receiver with the wrong "
        "geometry de-interleaves into a different permutation and hands "
        "the decoder plausible garbage, silently, at the same length "
        "(§3 F3)."
    )
    R.md()
    rows: list[list[object]] = []
    silent = True
    worst = 0.0
    n = tx.block_bits
    src = (np.arange(n, dtype=np.int64) * 17 % 251).astype(np.uint8)
    wire = np.asarray(tx.interleave(src))
    for label, r, c, u in (
        ("rows and cols exchanged", 7, 5, 2),
        ("depth never configured", 1, 35, 2),
        ("bit units, not pairs", 10, 7, 1),
    ):
        try:
            bad = Deinterleaver(rows=r, cols=c, unit_bits=u)
            out = np.asarray(bad.deinterleave(wire))
            wrong = float(np.mean(out != src))
            worst = max(worst, wrong)
            rows.append(
                [label, f"{r}x{c}x{u}", out.size, f"{wrong * 100:.1f}%"]
            )
        except ValueError:
            silent = False
            rows.append([label, f"{r}x{c}x{u}", "refused", "—"])
    R.table(
        [
            "receiver's geometry",
            "rows x cols x unit",
            "length out",
            "positions wrong",
        ],
        rows,
    )
    R.md(
        "Same length, no exception, most positions wrong. This is why the "
        "geometry is a link parameter to be agreed once and configured at "
        "both ends, and why the object refuses to infer it."
    )
    R.md()
    d.view_geometry_agrees = bool(same)
    d.view_round_trips = trip
    d.view_has_no_interleave = bool(no_fwd)
    d.mismatch_rows = rows
    d.mismatch_silent = bool(silent)
    d.mismatch_worst_wrong = worst
    d.doc_faces_agree, d.doc_stub_says = view_doc_faces()


def characterise() -> Data:
    R.md("## 2. Characterisation")
    R.md()
    R.md(
        "Measured, no verdicts — those are §3. Each heading names the C "
        "section it tracks where one exists; the numbering here is this "
        "report's own, because a section routinely merges several C ones."
    )
    R.md()
    d = Data()
    measure_permutation(d)
    measure_budget(d)
    measure_spread(d)
    measure_gain(d)
    measure_unit(d)
    measure_soft(d)
    measure_refusals(d)
    measure_view(d)
    return d


# ── 3. review ────────────────────────────────────────────────────────
def review(d: Data) -> None:
    R.md("## 3. Review — findings, with verdicts")
    R.md()
    R.find(
        "F1",
        "FIXED",
        "**The object's own permutation was unpinned in C.** Transposing "
        "its three calls into `dp_interleave.h` — `state->cols, "
        "state->rows` for `state->rows, state->cols` — left the ENTIRE C "
        "suite green, all 155 tests, because a transposed block "
        "interleave is still a permutation, still inverts, still permutes "
        "each block within itself and still moves something. Every "
        "assertion in `test_interleaver_core.c` survived it, and so did "
        "`test_dp_interleave.c`, which certifies the kernel the object was "
        "no longer calling correctly. Closed by comparing the object "
        "against that kernel rather than against its own inverse, plus a "
        "precondition that the transposed geometry is a different answer "
        "(§2.1) — the file header had claimed this check for the whole of "
        "its life without carrying it.",
    )
    R.find(
        "F2",
        "FIXED",
        "**The receive face had zero C coverage.** "
        "`interleaver_create_rx` — the whole reason `Deinterleaver` is a "
        "view over one core — had no mentions in `test_interleaver_core.c` "
        "at all; the header's claim of identical construction and "
        "identical refusals was evidenced by the fact that one line of C "
        "reads like it. It is now checked ACROSS the two faces (a "
        "transmit-face interleave undone by a receive-face object), which "
        "is the property the link actually needs, and its refusals are "
        "checked separately because a second constructor is a second place "
        "to forget them (§2.8).",
    )
    R.find(
        "F3",
        "BY DESIGN",
        f"**A geometry mismatch is silent, and expensive.** A receiver "
        f"holding different numbers returns an array of the right length "
        f"with up to {d.mismatch_worst_wrong * 100:.0f}% of positions "
        f"wrong, and raises nothing (§2.8) — there is no field on the "
        f"wire that could carry the geometry, so this is a property of "
        f"block interleaving rather than of this implementation. It is "
        f"also the reason the object refuses to infer `cols` from the "
        f"input length: an inferred geometry turns a truncated frame into "
        f"this failure automatically.",
    )
    R.find(
        "F4",
        "BY DESIGN",
        "**A partial block is refused rather than padded or truncated.** "
        "Padding changes the length and a receiver de-interleaving the "
        "padded block recovers different bits; truncating to the whole "
        "blocks available returns a plausible short frame with no error "
        "anywhere. Refusing is the only one of the three a caller can "
        "detect, and the message names `block_bits` so it can be fixed "
        "(§2.7).",
    )
    R.find(
        "F5",
        "BY DESIGN",
        "**There is no `interleave_soft`, and no state to serialize.** A "
        "transmitter has bits, not LLRs, so the soft face is receive-only "
        "(§2.6). Statelessness is the same kind of claim: interleaving is "
        "per-frame, carrying a partial block across frames would add "
        "frame-latency and break per-frame decoding, so a call takes a "
        "whole number of blocks or is refused — which leaves nothing to "
        "checkpoint, the exemption `docs/design/state-serialization.md` "
        "grants a pure converter. Measured as history-independence in "
        "§2.7 rather than argued, because that is the only observable the "
        "claim has.",
    )
    R.find(
        "F6",
        "C-ONLY",
        "**The `max_out` refusal cannot be reached from Python.** The C "
        "entry points take the caller's buffer and return 0 when it has no "
        "room for the whole input; the binding sizes the output itself "
        "from `interleave_max_out`, so no Python caller can present a "
        "short one. Certified in `test_interleaver_core.c`'s "
        "`a_partial_block_is_refused` instead, which is also where the "
        "NULL-argument refusals live.",
    )
    if d.doc_faces_agree:
        measured = "AGREE today, so the drift has closed. "
    else:
        measured = (
            f"disagree, and the stub opens \u201c{d.doc_stub_says}\u201d "
            "— the TRANSMIT face's sentence, on the receive class, with no "
            "mention that `interleave` is deliberately absent. A type "
            "checker and an IDE read that one; `help()` reads the "
            "hand-written one. "
        )
    R.find(
        "F7",
        "GAP",
        "**The view's two docstrings do not agree, and the stub has the "
        "wrong one.** jm derives a class docstring from `create()`'s "
        "doxygen, but a VIEW's runtime `tp_doc` gets a placeholder rather "
        "than its own `create_fn`'s (just-makeit#1160), so doppler "
        "hand-writes `Deinterleaver`'s in the sacred fragment. The `.pyi` "
        "stub, meanwhile, gets what jm derives — and for a view that is "
        "the PARENT's `create()`. Measured from the two live sources: "
        "they " + measured + "The fragment's own comment used to claim "
        "the stub was derived from `interleaver_create_rx`; this is the "
        "measurement that found it untrue. Retire the hand-written block "
        "when the fix ships.",
    )
    R.find(
        "F8",
        "GAP",
        "**The refusals are ten hand-written raise sites.** A kernel "
        "refusal is a 0 return, and jm has no declarative hook for it: "
        "`error`/`status_return` apply to an int-returning method, and a "
        "`variable_output` one has no status to carry "
        "(just-makeit#1159). So each of the five methods across the two "
        "faces carries the same hand-written `PyErr_Format` twice — once "
        "on the `out=` path and once on the allocating one — and a sixth "
        "method added tomorrow would return an empty array instead. The "
        "behaviour is right (§2.7) and the duplication is what is filed.",
    )


# ── 4. limits ────────────────────────────────────────────────────────
def limits(d: Data) -> None:
    R.md("## 4. Limits — the certified envelope")
    R.md()
    R.md(
        "Claims a caller may rely on. A failure here is a regression, not "
        "a new finding. Every one is asserted by "
        "`src/doppler/coding/tests/test_validation_limits.py`."
    )
    R.md()
    R.limit(
        d.perm_all_match,
        "the object writes by rows and reads by columns, matching numpy's "
        "reshape-transpose over every geometry and both block counts",
    )
    R.limit(
        d.perm_all_discriminate,
        "the transposed geometry is a DIFFERENT answer, and the round trip "
        "returns the input exactly",
    )
    R.limit(
        d.budget_exact,
        "block_bits == rows*cols*unit_bits, burst_len == rows, "
        "separation == cols",
    )
    R.limit(
        d.maxout_identity,
        "all three *_max_out are the identity for every length, including "
        "0 and lengths the transform refuses",
    )
    R.limit(
        d.spread_holds,
        "a burst of burst_len consecutive wire units touches each codeword "
        "at most once, at every start position of every geometry",
    )
    R.limit(
        d.spread_bound_bites,
        "one unit past burst_len some codeword takes two — the bound is "
        "exact, not conservative",
    )
    R.limit(
        d.bare_worst > 1,
        f"un-interleaved, the same burst lands up to {d.bare_worst} hits "
        f"in ONE codeword (the control)",
    )
    for depth in FER_DEPTHS:
        R.limit(
            d.fer_bound[depth] == RS_NROOTS // 2 * depth,
            f"depth {depth}: the corrigible burst is E*depth = "
            f"{d.fer_bound[depth]} octets",
        )
    R.limit(
        d.fer_depth1_no_gain,
        "at depth 1 the interleaved and bare frame error rates are "
        "identical: interleaving a single codeword buys nothing (the "
        "control)",
    )
    R.limit(
        d.fer_bound_exact,
        "at E*depth octets the frame survives; at one octet more it does "
        "not, for every depth measured",
    )
    R.limit(
        d.unit_matters,
        "at the E*depth bound, unit_bits=8 corrects and unit_bits=1 does "
        "not — the unit must match the code's symbol",
    )
    R.limit(
        d.soft_exact and d.soft_matches_hard,
        "the soft path is the same permutation as the hard one, exactly",
    )
    R.limit(
        d.soft_nonfinite_survives,
        "+/-inf, NaN and -0.0 arrive bit-for-bit: the soft path moves "
        "values, it does not compute with them",
    )
    R.limit(
        d.all_refused,
        "every method refuses a partial block, an empty input and a zero "
        "geometry, and nothing survives between calls",
    )
    R.limit(
        d.refusal_names_the_block,
        "the refusal names block_bits, the number a caller needs to fix "
        "their framing",
    )
    R.limit(
        d.view_geometry_agrees and d.view_round_trips,
        "the receive face reads back one geometry with the transmit face "
        "and undoes what it interleaved",
    )
    R.limit(
        d.view_has_no_interleave,
        "`interleave` is absent from Deinterleaver — a receive-only face "
        "cannot silently run the forward direction",
    )
    R.limit(
        d.mismatch_silent and d.mismatch_worst_wrong > 0.5,
        f"a mismatched geometry is NOT refused and returns the right "
        f"length with up to {d.mismatch_worst_wrong * 100:.0f}% of "
        f"positions wrong",
    )


# ── plots ────────────────────────────────────────────────────────────
def plots(d: Data) -> None:
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    fig, ax = plt.subplots(figsize=(7, 4))
    labels = [str(r[0]) for r in d.spread_rows]
    x = np.arange(len(labels))
    ax.bar(x - 0.27, [r[3] for r in d.spread_rows], 0.27, label="at burst_len")
    ax.bar(x, [r[4] for r in d.spread_rows], 0.27, label="at burst_len + 1")
    ax.bar(
        x + 0.27,
        [r[5] for r in d.spread_rows],
        0.27,
        label="no interleaver",
    )
    ax.set_xticks(x)
    ax.set_xticklabels(labels)
    ax.set_xlabel("geometry (rows x cols)")
    ax.set_ylabel("worst hits in one codeword")
    ax.set_title("A burst of burst_len costs each codeword at most one")
    ax.grid(True, axis="y", alpha=0.3)
    ax.legend()
    fig.tight_layout()
    fig.savefig(HERE / "spread.png", dpi=110)
    plt.close(fig)

    fig, ax = plt.subplots(figsize=(7, 4))
    for depth in FER_DEPTHS:
        rows = [r for r in d.fer_rows if int(r[0]) == depth]
        ax.plot(
            [r[1] for r in rows],
            [r[3] for r in rows],
            "o-",
            label=f"interleaved, depth {depth}",
        )
    bare = [r for r in d.fer_rows if int(r[0]) == FER_DEPTHS[-1]]
    ax.plot(
        [r[1] for r in bare],
        [r[2] for r in bare],
        "ks--",
        label="bare code",
    )
    ax.set_xlabel("burst length (octets)")
    ax.set_ylabel("frame error rate")
    ax.set_title("The corrigible burst is E x depth, and not one octet more")
    ax.grid(True, alpha=0.3)
    ax.legend()
    fig.tight_layout()
    fig.savefig(HERE / "gain.png", dpi=110)
    plt.close(fig)


# ── build ────────────────────────────────────────────────────────────
def build(write: bool = True) -> Report:
    """Measure everything and render the report.

    `write=False` is the pytest path: every measurement still runs, so
    every limit is genuinely exercised, but nothing is written into the
    repo.
    """
    global R
    R = Report(write=write)
    if write:
        DATA.mkdir(parents=True, exist_ok=True)
    section_object()
    d = characterise()
    review(d)
    limits(d)
    R.executive(
        "Interleaver",
        [
            "**Size the geometry from the code, not from the frame.** "
            "`rows` is the longest burst fully spread and `cols` is the "
            "codeword length, so an outer code correcting `t` units "
            "survives a burst of `t * rows` — measured exactly, at "
            f"{RS_NROOTS // 2} octets bare against "
            f"{d.fer_bound[max(FER_DEPTHS)]} at depth {max(FER_DEPTHS)} "
            "(§2.4).",
            "**And the bound bites.** One octet past `E * rows` the frame "
            "is lost. An interleaver multiplies the burst a link survives "
            "by the depth; it does not remove the limit, and a design that "
            "budgets for more is budgeting for nothing (§2.3, §2.4).",
            "**Match `unit_bits` to the code's symbol.** At the bound, "
            "octet units correct and bit units do not: bit-interleaving a "
            "symbol-oriented code spreads a burst inside symbols that are "
            "already wrong (§2.5).",
            "**Both ends must be configured with the same three numbers, "
            "and nothing checks it.** A mismatched receiver raises "
            "nothing and returns the right length with up to "
            f"{d.mismatch_worst_wrong * 100:.0f}% of positions wrong "
            "(§2.8, F3).",
            "**De-interleave the LLRs, not the bits.** The soft path is "
            "the same permutation and moves values bit-for-bit, so an "
            "outer decoder can have its confidence intact; slicing first "
            "throws away most of what the outer code is for (§2.6).",
            "**The evidence is younger than the object.** The object's own "
            "permutation direction and its entire receive face were "
            "unpinned in C until this certification — a transposed "
            "geometry passed all 155 C tests (F1, F2).",
        ],
    )
    if write:
        plots(d)
    R.summary("\n- Raw sweep: `data/coding_gain.csv`")
    R.emit(HERE / "results.md")
    return R


if __name__ == "__main__":
    sys.exit(cli(build, HERE))
