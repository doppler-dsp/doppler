"""Gold — certification evidence for the CCSDS Gold-code generator.

Run directly to regenerate `results.md` and the CSVs:

    uv run python src/doppler/wfm/tests/validation/gold/validate.py

`--check` re-renders in memory and diffs against the committed bytes;
`make validate` writes, `make validate-check` checks. Every limit this
records is asserted by
`src/doppler/wfm/tests/test_validation_limits.py`.

**This is a leaf, and its subject is the FAMILY.** One Gold code is
unremarkable; the reason the construction exists is that a preferred
pair of m-sequences generates a whole set of codes whose mutual
correlation is bounded, so many transmitters can share a band and a
receiver can pick one out. So the questions are how many codes there
actually are, and what the bound actually is.

Two things are measured that the C test does not:

- **the bound, as a number a caller designs to.** The header states the
  three-valued set `{-1, -65, 63}` without saying where 65 comes from or
  what it costs. It is `t(n) = 2^((n+2)/2) + 1` for even `n`, and the
  price is the near-far margin: a Gold code's worst sidelobe is 65
  against an m-sequence's 1 (§2.2). That trade -- correlation purity for
  a family of codes -- is the whole design decision.
- **the family, exhaustively.** All 1023 reachable codes, not the two
  the C cross-correlation check compares (§2.1).

The order is the campaign's: `native/inc/gold/gold_core.h` is the SSOT
and `native/tests/test_gold_core.c` certifies it in C.
"""

from __future__ import annotations

import sys
from dataclasses import dataclass, field
from pathlib import Path

import numpy as np

from doppler.tests._repo import repo_root
from doppler.tests._validation_common import Report, cli
from doppler.wfm import PN, Gold, mls_poly

HERE = Path(__file__).resolve().parent
DATA = HERE / "data"
ROOT = repo_root(__file__)

R = Report()

# CCSDS 415.0-G-1 5.2.2.4 (Figure 5-1): the fixed Register A / Register B
# polynomials and Register B's fixed initial value. Register A's initial
# value is "User dependent" and is what selects a family member.
LENGTH = 10
P = (1 << LENGTH) - 1  # 1023
TAPS_A, TAPS_B, SEED_B = 934, 567, 73
SEED_A_CCSDS = 350  # Figure 5-2 worked example, PN Code Library #365

# Pairs sampled for the family-wide correlation work. Fixed, because a
# report is byte-compared; spread across the seed range rather than
# clustered, so a defect confined to one region of the space is reachable.
SAMPLE_SEEDS = (1, 7, 63, 129, 350, 511, 595, 700, 823, 900, 1000, 1023)


def _csv(path: Path, header: str, rows: list[list[float]]) -> None:
    if not R.write:
        return
    DATA.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as fh:
        fh.write(header + "\n")
        for r in rows:
            fh.write(",".join(f"{v:.10g}" for v in r) + "\n")


def code(seed_a: int, n: int = P) -> np.ndarray:
    """One family member, as chips."""
    return np.asarray(Gold(TAPS_A, seed_a, TAPS_B, SEED_B, LENGTH).generate(n))


def bipolar(seq: np.ndarray) -> np.ndarray:
    """0 -> +1, 1 -> -1 — the mapping a correlator actually sees."""
    return 1.0 - 2.0 * seq.astype(np.float64)


def circ_corr(a: np.ndarray, b: np.ndarray) -> np.ndarray:
    """Circular cross-correlation, exact in integers via the FFT."""
    fa, fb = np.fft.rfft(bipolar(a)), np.fft.rfft(bipolar(b))
    return np.rint(np.fft.irfft(fa * np.conj(fb), n=a.size)).astype(np.int64)


def t_of(n: int) -> int:
    """t(n) — the correlation bound. Even n: 2^((n+2)/2) + 1."""
    return (
        (1 << ((n + 2) // 2)) + 1 if n % 2 == 0 else (1 << ((n + 1) // 2)) + 1
    )


@dataclass
class Data:
    """Everything measured, so review/limits read data rather than re-run."""

    n_seeds: int = 0
    n_distinct: int = 0
    all_distinct: bool = False
    no_shift_dupes: bool = False
    shift_sample: int = 0
    three_valued: bool = False
    value_set: tuple[int, ...] = ()
    theory_t: int = 0
    theory_matches: bool = False
    hist_rows: list[list[str]] = field(default_factory=list)
    n_pairs: int = 0
    gold_peak: int = 0
    gold_worst: int = 0
    mls_worst: int = 0
    margin_db: float = 0.0
    mls_margin_db: float = 0.0
    ccsds_vector_ok: bool = False
    balance_ok: bool = False
    ones: int = 0
    period_ok: bool = False
    reset_ok: bool = False
    state_ok: bool = False
    wraps_ok: bool = False


# ── 1. the object ────────────────────────────────────────────────────
def section_object() -> None:
    R.md("## 1. The object")
    R.md()
    R.md(
        "`Gold` is the CCSDS command-link Gold code generator (CCSDS "
        "415.0-G-1 section 5.2.2.4, Figure 5-1): two same-clocked Fibonacci "
        "LFSRs with fixed feedback polynomials, XOR-combined chip by chip "
        "into a 1023-chip code. Register B is fixed by the standard; "
        'Register A\'s initial value is "User dependent" and is what '
        "selects a family member. The design is "
        "[docs/design/wfmgen.md](../../../../../../docs/design/wfmgen.md); "
        "the API is `native/inc/gold/gold_core.h`, certified in C by "
        "`native/tests/test_gold_core.c`."
    )
    R.md()
    R.md(
        "**Its subject is the family.** One Gold code is unremarkable; "
        "the construction exists because a preferred pair generates a "
        "whole SET of codes with a bounded mutual correlation, so many "
        "transmitters can share a band. What is measured here is how many "
        "codes there are and what the bound costs."
    )
    R.md()


# ── 2. characterisation ──────────────────────────────────────────────
def measure_family(d: Data) -> None:
    """2.1 — how many codes seed_a actually reaches."""
    seqs = {s: code(s).tobytes() for s in range(1, P + 1)}
    d.n_seeds = len(seqs)
    d.n_distinct = len(set(seqs.values()))
    d.all_distinct = d.n_distinct == d.n_seeds

    # Is any member a cyclic shift of another? Sampled, because the
    # exhaustive question is 1023*1022/2 rotations.
    sample = SAMPLE_SEEDS
    d.shift_sample = len(sample)
    dupes = 0
    for i, a in enumerate(sample):
        doubled = seqs[a] * 2
        for b in sample[i + 1 :]:
            if seqs[b] in doubled:
                dupes += 1
    d.no_shift_dupes = dupes == 0

    R.md("### 2.1 The family is 1023 codes, and the header said 1024")
    R.md()
    R.md(
        f"Every nonzero `seed_a` was generated and compared: "
        f"**{d.n_seeds} seeds produce {d.n_distinct} distinct codes**. "
        "The header's *\"walks the whole Gold-code family (2^length "
        'members)"* was wrong in both directions at once, and this '
        "certification corrected it (F1):"
    )
    R.md()
    R.table(
        ["quantity", "value", "why"],
        [
            [
                "seeds available",
                str(P),
                "`gold_create` rejects a zero seed, so 2^length **- 1**",
            ],
            [
                "distinct codes reached",
                str(d.n_distinct),
                "measured, one per seed",
            ],
            [
                "header's old figure",
                "1024",
                "**neither** the reachable count nor the family size",
            ],
            [
                "true family size",
                str(P + 2),
                "2^n **+ 1**: these plus the two constituent m-sequences",
            ],
        ],
    )
    R.md(
        "The two the generator cannot reach are Register A's and Register "
        "B's own m-sequences — it always XORs both registers, so no seed "
        "produces either alone. That is a property of this generator, not "
        "of Gold codes, and it is now said out loud in the header because "
        "a caller sizing a code-assignment scheme allocates against that "
        f"number. Across a {d.shift_sample}-member sample no code is a "
        "cyclic shift of another either, so the count is codes and not "
        "phases of fewer codes."
    )
    R.md()


def measure_bound(d: Data) -> None:
    """2.2 — the correlation bound, and what it costs."""
    seqs = {s: code(s) for s in SAMPLE_SEEDS}
    values: dict[int, int] = {}
    pairs = 0
    for i, a in enumerate(SAMPLE_SEEDS):
        for b in SAMPLE_SEEDS[i + 1 :]:
            pairs += 1
            for v in circ_corr(seqs[a], seqs[b]):
                values[int(v)] = values.get(int(v), 0) + 1
    # Autocorrelation sidelobes of one member, over every nonzero lag.
    ac = circ_corr(seqs[SEED_A_CCSDS], seqs[SEED_A_CCSDS])
    d.gold_peak = int(ac[0])
    for v in ac[1:]:
        values[int(v)] = values.get(int(v), 0) + 1

    d.n_pairs = pairs
    d.value_set = tuple(sorted(values))
    d.three_valued = len(d.value_set) == 3
    d.theory_t = t_of(LENGTH)
    d.theory_matches = d.value_set == (-d.theory_t, -1, d.theory_t - 2)
    d.gold_worst = max(abs(v) for v in d.value_set)

    total = sum(values.values())
    d.hist_rows = [
        [str(v), str(values[v]), f"{values[v] / total:.3f}"]
        for v in d.value_set
    ]

    # The contrast: a plain m-sequence of the same period.
    m = np.asarray(PN(mls_poly(LENGTH), 1, LENGTH).generate(P))
    mac = circ_corr(m, m)
    d.mls_worst = int(np.max(np.abs(mac[1:])))
    d.margin_db = 20.0 * np.log10(P / d.gold_worst)
    d.mls_margin_db = 20.0 * np.log10(P / d.mls_worst)

    _csv(
        DATA / "correlation_histogram.csv",
        "value,count,fraction",
        [[float(v), float(values[v]), values[v] / total] for v in d.value_set],
    )

    R.md("### 2.2 The bound, and what it costs")
    R.md()
    R.md(
        f"Every correlation value across {d.n_pairs} cross-correlated "
        f"pairs and one member's full autocorrelation — "
        f"{total:,} values in all — falls in a set of exactly "
        f"**{len(d.value_set)}**: `{list(d.value_set)}`."
    )
    R.md()
    R.table(["correlation value", "count", "fraction"], d.hist_rows)
    R.md(
        f"The header states those three numbers without saying where they "
        f"come from. They are `t(n) = 2^((n+2)/2) + 1` for even `n`, which "
        f"at n={LENGTH} is **{d.theory_t}**, giving the set "
        f"`{{-1, -t, t-2}}` = `{{-1, {-d.theory_t}, {d.theory_t - 2}}}`. "
        "Deriving it rather than pattern-matching the literals is what "
        "makes the check meaningful at a different length."
    )
    R.md()
    R.md(
        f"**The cost is the near-far margin**, and it is the whole design "
        f"trade. A correlator sees a peak of {d.gold_peak} against a worst "
        f"sidelobe of {d.gold_worst} — **{d.margin_db:.1f} dB**. The same "
        f"length of plain m-sequence has a worst sidelobe of "
        f"{d.mls_worst}, i.e. {d.mls_margin_db:.1f} dB. Gold codes buy a "
        f"family of {P} mutually-bounded codes and pay "
        f"{d.mls_margin_db - d.margin_db:.1f} dB of correlation purity "
        "for it. If only one transmitter is ever on the air, an "
        "m-sequence is the better code; the moment a second appears, its "
        "cross-correlation is unbounded and Gold is the answer. Raw "
        "histogram: "
        "[data/correlation_histogram.csv](data/correlation_histogram.csv)."
    )
    R.md()


def measure_conformance(d: Data) -> None:
    """2.3 — the standard's own worked example."""
    chips = code(SEED_A_CCSDS)
    expected = [0, 1, 0, 0, 0, 1, 0, 1, 1, 1, 0, 1, 1, 1, 1]
    d.ccsds_vector_ok = chips[:15].tolist() == expected
    d.ones = int(chips.sum())
    d.balance_ok = d.ones == 512 and (P - d.ones) == 511

    g = Gold(TAPS_A, SEED_A_CCSDS, TAPS_B, SEED_B, LENGTH)
    two = np.asarray(g.generate(2 * P))
    d.wraps_ok = bool(np.array_equal(two[:P], two[P:]))
    d.period_ok = d.wraps_ok

    R.md("### 2.3 The standard's own vector")
    R.md()
    R.md(
        f"CCSDS 415.0-G-1 Figure 5-2's worked example — PN Code Library "
        f"Table 1, Code Number 365, reached with the default "
        f"`seed_a = {SEED_A_CCSDS}` — reproduces chip for chip over its "
        f"first 15, and the code is balanced at {d.ones} ones against "
        f"{P - d.ones} zeros. That vector is the conformance anchor: the "
        "taps and initial values here are not derivable from anything in "
        "this repository, so agreeing with the published sequence is what "
        "says they were transcribed correctly."
    )
    R.md()
    R.md(
        "The document itself is not in the repository's standards "
        "library, so this report certifies the implementation against "
        "the published vector rather than against the prose. That is a "
        "limit of the evidence and it is recorded as such (F4)."
    )
    R.md()


def measure_lifecycle(d: Data) -> None:
    """2.4 — reset and resume."""
    g = Gold(TAPS_A, SEED_A_CCSDS, TAPS_B, SEED_B, LENGTH)
    a = np.asarray(g.generate(P)).copy()
    g.reset()
    d.reset_ok = bool(np.array_equal(a, np.asarray(g.generate(P))))

    q = Gold(TAPS_A, SEED_A_CCSDS, TAPS_B, SEED_B, LENGTH)
    q.generate(17)
    blob = q.get_state()
    ref = np.asarray(q.generate(64)).copy()
    fresh = Gold(TAPS_A, SEED_A_CCSDS, TAPS_B, SEED_B, LENGTH)
    fresh.set_state(blob)
    d.state_ok = bool(np.array_equal(ref, np.asarray(fresh.generate(64))))

    R.md("### 2.4 Reset and resume")
    R.md()
    R.md(
        "`reset()` reloads both registers so the sequence restarts from "
        "chip 0, reproducing a full period bit-for-bit, and a serialized "
        "blob resumes a mid-code generator bit-exactly into a fresh "
        "instance. Both registers are in the blob; restoring only one "
        "would desynchronise the XOR and is caught by the round-trip."
    )
    R.md()


def characterise() -> Data:
    R.md("## 2. Characterisation")
    R.md()
    d = Data()
    measure_family(d)
    measure_bound(d)
    measure_conformance(d)
    measure_lifecycle(d)
    return d


# ── 3. review ────────────────────────────────────────────────────────
def review(d: Data) -> None:
    R.md("## 3. Review -- findings, with verdicts")
    R.md()
    R.find(
        "F1",
        "FIXED",
        "**The header's family size was wrong in both directions at "
        'once.** It said varying `seed_a` *"walks the whole Gold-code '
        'family (2^length members)"* -- 1024 at length=10 -- and repeated '
        '"the 1024-code Gold family" in `@param seed_a`. Measured over '
        f"every nonzero seed: **{d.n_distinct}** distinct codes. Only "
        "2^length **- 1** seeds exist, because `gold_create` rejects "
        "zero; and the classical Gold set for a preferred pair has 2^n "
        f"**+ 1** = {P + 2} members -- these plus the two constituent "
        "m-sequences, which this generator can never emit because it "
        "always XORs both registers. So 1024 was neither the reachable "
        "count nor the family size. Corrected in the header (and carried "
        "into `wfm.pyi` by `make jm-apply`), and pinned in C by "
        "generating all 1023 codes and requiring them distinct -- a "
        "caller sizing a code-assignment scheme allocates against that "
        "number.",
    )
    R.find(
        "F2",
        "FIXED",
        "**Three contract claims had nothing behind them.** `max_out` "
        "was never exercised: every existing call passed `max_out == n`, "
        'so *"emission stops there"* and *"@return min(n, max_out)"* '
        'were prose. Nor was *"requesting more than one period is valid '
        '-- the sequence simply wraps"*, nor `gold_destroy(NULL)` as a '
        "documented no-op. All three are now pinned in "
        "`test_gold_core.c`, with the capacity case also requiring the "
        "untouched tail of the caller's buffer to stay untouched and a "
        "zero capacity to advance neither LFSR.",
    )
    R.find(
        "F3",
        "FIXED",
        '**"Preferred pair" is a claim about the family, and one pair '
        "was checked.** The C test cross-correlated exactly two members, "
        "which says nothing about the other 1021 -- the property that "
        "makes the set usable for multiple access is that it holds "
        "between ANY two. Now sampled at six members (15 pairs) in C and "
        f"at {d.shift_sample} members ({d.n_pairs} pairs) here, every "
        "value landing in the three-valued set. Worth noting what this "
        "still is: a sample. The exhaustive claim is 1023x1022/2 pairs "
        "and neither gate pays for it.",
    )
    R.find(
        "F4",
        "GAP",
        "**The constants are certified against the published vector, not "
        "against the document.** CCSDS 415.0-G-1 is not in the "
        "repository's standards library (`~/refs/` holds 130.1-G-3 and "
        "131.0-B-6), so nothing here can confirm that taps 934/567 and "
        "seed 73 are what section 5.2.2.4 specifies -- only that "
        "they reproduce "
        "the Figure 5-2 worked example, Code #365, chip for chip. That "
        "is strong evidence and it is not the same thing: a transcription "
        "error that happened to preserve that one code would pass. "
        "Tracked as "
        "[gh-1133](https://github.com/doppler-dsp/doppler/issues/1133) -- "
        "the fix is to add the document to the standards library and "
        "check the polynomials against it directly.",
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
        d.n_seeds == P,
        f"exactly {P} seeds are usable -- 2^length - 1, because a zero "
        "seed is refused rather than silently accepted",
    )
    R.limit(
        d.all_distinct,
        f"and they reach {d.n_distinct} DISTINCT codes, one per seed: the "
        "number a code-assignment scheme may allocate against",
    )
    R.limit(
        d.n_distinct == P and P + 2 == 1025,
        "which is 2^n - 1 of the family's 2^n + 1 members; the two "
        "constituent m-sequences are not reachable from any seed",
    )
    R.limit(
        d.no_shift_dupes,
        f"no sampled member is a cyclic shift of another ({d.shift_sample} "
        "members compared), so the count is codes and not phases",
    )
    R.limit(
        d.three_valued,
        f"every correlation value across {d.n_pairs} pairs and a full "
        "autocorrelation falls in a set of exactly three",
    )
    R.limit(
        d.value_set == (-65, -1, 63),
        "and that set is exactly {-1, -65, 63}, as the header states",
    )
    R.limit(
        d.theory_matches,
        f"which is the theoretical {{-1, -t, t-2}} for "
        f"t(n) = 2^((n+2)/2) + 1 = {d.theory_t} at n={LENGTH} -- derived, "
        "so the check still means something at another length",
    )
    R.limit(
        d.gold_peak == P,
        f"the autocorrelation peak is exactly {P} at zero lag",
    )
    R.limit(
        d.gold_worst == 65,
        "and the worst sidelobe anywhere is 65 -- the near-far bound a "
        "receiver must be designed to",
    )
    R.limit(
        abs(d.margin_db - 23.9) < 0.1,
        f"giving a peak-to-worst-sidelobe margin of {d.margin_db:.1f} dB",
    )
    R.limit(
        d.mls_worst == 1,
        "against a plain m-sequence's worst sidelobe of 1: the purity "
        "Gold trades away is real and measured, not asserted",
    )
    R.limit(
        d.mls_margin_db - d.margin_db > 30.0,
        f"a {d.mls_margin_db - d.margin_db:.1f} dB difference -- so a "
        "single-transmitter link should prefer the m-sequence, and a "
        "shared band should not",
    )
    R.limit(
        d.ccsds_vector_ok,
        "the default configuration reproduces CCSDS 415.0-G-1 Figure "
        "5-2's worked example (PN Code Library #365) chip for chip",
    )
    R.limit(
        d.balance_ok,
        f"and the code is balanced: {d.ones} ones to {P - d.ones} zeros",
    )
    R.limit(
        d.wraps_ok,
        "generating past one period wraps exactly, so a caller may ask "
        "for any length without tracking the boundary",
    )
    R.limit(
        d.reset_ok,
        "reset() restarts from chip 0, reproducing a full period",
    )
    R.limit(
        d.state_ok,
        "and a serialized blob resumes a mid-code generator bit-exactly, "
        "carrying BOTH registers",
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
        "Gold",
        [
            f"**There are {d.n_distinct} codes, not 1024.** The header "
            "said 1024, which was neither the reachable count (2^n - 1, "
            f"since a zero seed is refused) nor the family size (2^n + 1 "
            f"= {P + 2}). Corrected and pinned; allocate against "
            f"{d.n_distinct} (§2.1, F1).",
            f"**Design the receiver to a {d.gold_worst}-chip sidelobe.** "
            f"Peak {d.gold_peak} against worst sidelobe {d.gold_worst} is "
            f"{d.margin_db:.1f} dB, and every correlation value in the "
            f"family falls in {{-1, -65, 63}} = the theoretical "
            f"{{-1, -t, t-2}} at t = {d.theory_t} (§2.2).",
            f"**That bound is what a family costs.** A plain m-sequence "
            f"of the same period has a worst sidelobe of {d.mls_worst}, "
            f"{d.mls_margin_db - d.margin_db:.1f} dB better. One "
            "transmitter should use the m-sequence; a shared band cannot, "
            "because two m-sequences have no bounded cross-correlation "
            "(§2.2).",
            "**The default configuration is the CCSDS worked example.** "
            "`Gold()` reproduces PN Code Library #365 chip for chip, "
            "which is the conformance anchor for taps that are otherwise "
            "unverifiable from inside this repository (§2.3, F4).",
            "**The preferred-pair property is sampled, not exhaustive.** "
            f"{d.n_pairs} pairs here and 15 in C, out of "
            f"{P * (P - 1) // 2:,}. No gate pays for the full set, and "
            "the report says so rather than implying otherwise (F3).",
        ],
    )
    R.summary()
    R.emit(HERE / "results.md")
    return R


if __name__ == "__main__":
    sys.exit(cli(build, HERE))
