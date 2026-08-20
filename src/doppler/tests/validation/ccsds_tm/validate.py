"""ccsds_tm — certification evidence for a component with no Python face.

Run directly to regenerate `results.md`, the plots and the CSVs:

    uv run python src/doppler/tests/validation/ccsds_tm/validate.py

`--check` re-renders in memory and diffs against the committed bytes;
`make validate` writes, `make validate-check` checks. Every limit this
records is asserted by `src/doppler/tests/test_validation_limits.py`,
which runs this same `build(write=False)`.

## Why this one is shaped differently

`ccsds_tm` is the third component certified with no binding, after `conv`
and `rs`, and the split is theirs — **C measures, Python renders**:
`native/validation/ccsds_tm_certify.c` runs the sweeps and emits CSV; this
file parses it, characterises, reviews and asserts the limits through the
same `Report` every other report uses.

What is different is the KIND of external truth. `rs`'s report is measured
against a probability model and `conv`'s against a coding-gain curve;
`ccsds_tm` mostly owns *the standard's own facts*, so its oracles are
published values — the ASM pattern, Annex G's generator, both dual-basis
matrices — and those live in the C tests, where a wrong one is a failure
rather than a row in a table.

That left a real question for this report: what is there to measure? The
answer is the three things this component adds ON TOP of the code it
configures, none of which any single codeword can show —

- the **interleaver**, whose exchange rate the header states and nothing
  measured;
- the **sync marker as a detector**, whose threshold the header calls "the
  whole of the trade" without a number beside it;
- the **randomiser's spectrum**, which is the stated reason B-6 demoted the
  255-bit sequence, and is checkable with the spectrum analyser this tree
  already ships.

Nothing here computes a measurement, and nothing there decides whether a
number is acceptable.
"""

from __future__ import annotations

import csv
import math
import subprocess
import sys
from pathlib import Path

from doppler.tests._repo import build_dir, repo_root
from doppler.tests._validation_common import Report, cli

HERE = Path(__file__).resolve().parent
DATA = HERE / "data"
ROOT = repo_root(__file__)
HARNESS = build_dir(__file__) / "native/validation/validate_ccsds_tm_certify"

R = Report()

#: Correctable symbols per codeword, 131.0-B-3 4.3.2d. The interleaver's
#: whole claim is stated in units of this.
E = 16

#: Length of the sync marker in bits (9.4.1, figure 9-1).
ASM_BITS = 32


def _harness() -> dict[str, list[dict[str, float | str]]]:
    """Run the C harness and parse its CSV blocks.

    A missing binary is a hard failure rather than a skip, for the reason
    `docs/dev/contributing/validation.md` gives: a skipped measurement and a
    passing one are indistinguishable in a log, and this report's entire
    content comes from that binary.
    """
    if not HARNESS.exists():
        raise SystemExit(
            f"ccsds_tm: {HARNESS.relative_to(ROOT)} is not built — run "
            f"`make build` first. This report has no measurement of its "
            f"own; the C harness is where every number in it comes from."
        )
    out = subprocess.run(
        [str(HARNESS), "--emit"], capture_output=True, text=True, check=True
    ).stdout

    def _val(text: str) -> float | str:
        try:
            return float(text)
        except ValueError:
            return text

    blocks: dict[str, list[dict[str, float | str]]] = {}
    name, rows, header = "", [], []
    for line in out.splitlines():
        line = line.strip()
        if line.startswith("#"):
            if name:
                blocks[name] = rows
            name, rows, header = line[1:].strip(), [], []
        elif not line:
            continue
        else:
            fields = next(csv.reader([line]))
            if not header:
                header = fields
            else:
                rows.append(dict(zip(header, (_val(v) for v in fields))))
    if name:
        blocks[name] = rows
    return blocks


def _fa_model(t: int) -> float:
    """Probability a random 32-bit window is within `t` of the marker.

    A random window's Hamming distance to any fixed pattern is
    `Binomial(32, 1/2)`, and `asm_find` searches the complement too, so the
    per-offset rate is::

        P_fa(t) = 2 * sum_{i<=t} C(32, i) / 2^32

    This is the closed form the H0 sweep is read against. It needs nothing
    from the implementation, which is what makes it an oracle rather than a
    restatement: an `asm_find` that compared against the wrong pattern
    entirely would produce exactly this rate.
    """
    return (
        2.0 * sum(math.comb(ASM_BITS, i) for i in range(t + 1)) / 2**ASM_BITS
    )


def _rate(count: float, trials: float) -> str:
    """A measured rate, or the bound zero events supports."""
    if count > 0:
        return f"{count / trials:.4g}"
    return f"< {3.0 / trials:.1e}"


def _burst_by_depth(d) -> dict[int, list[dict]]:
    """The burst block, grouped by interleaving depth, order preserved."""
    out: dict[int, list[dict]] = {}
    for r in d["burst"]:
        out.setdefault(int(r["depth"]), []).append(r)
    return out


def _survived_blocks(by_depth: dict[int, list[dict]]) -> int:
    """Blocks at a point where every codeword decoded, over every depth."""
    return sum(
        int(r["blocks"])
        for rows in by_depth.values()
        for r in rows
        if r["all_ok"] == r["blocks"]
    )


def _edge(rows: list[dict]) -> tuple[int, int]:
    """The longest burst every block survived, and the shortest that broke."""
    ok = [int(r["burst"]) for r in rows if r["all_ok"] == r["blocks"]]
    bad = [int(r["burst"]) for r in rows if r["all_ok"] == 0]
    return (max(ok) if ok else 0), (min(bad) if bad else 0)


def _pd(d, ber: float, t: int) -> tuple[int, int, int]:
    """Trials, correct detections and polarity agreement at one point.

    Both polarities are summed: the header's claim is that a complemented
    stream is found *and said to be complemented*, so reporting only the
    upright case would measure half of it.
    """
    rows = [
        r
        for r in d["asm_pd"]
        if abs(float(r["ber"]) - ber) < 1e-9 and int(r["max_errors"]) == t
    ]
    return (
        sum(int(r["trials"]) for r in rows),
        sum(int(r["correct"]) for r in rows),
        sum(int(r["polarity_ok"]) for r in rows),
    )


def _spectrum(d, which: str) -> dict:
    return next(r for r in d["spectrum"] if r["which"] == which)


def characterise(d) -> None:
    R.md("## 2. Characterisation")
    R.md()
    R.md(
        "Every number below is `native/validation/ccsds_tm_certify.c`'s: 200 "
        "codeblocks per (depth, burst) point, 2000000 random bits per "
        "false-alarm point, and 4000 markers per (BER, threshold, polarity) "
        "point. Error patterns are placed with the generator the C tests "
        "place them with, so a row here and an assertion there describe the "
        "same experiment."
    )
    R.md()

    # ── 2.1 the interleaver ─────────────────────────────────────────
    R.md("### 2.1 What the interleaver buys, exactly (C rs §12, §13)")
    R.md()
    R.md(
        "4.4.1 hands successive symbols to successive encoders, so a "
        "contiguous burst of `B` symbols lands as `ceil(B/depth)` errors in "
        "the worst-hit codeword. The header states that exchange rate; this "
        "measures where it stops."
    )
    R.md()

    by_depth = _burst_by_depth(d)
    rows = []
    for depth in sorted(by_depth):
        ok, bad = _edge(by_depth[depth])
        rows.append(
            [
                str(depth),
                str(depth * E),
                str(ok),
                str(bad),
                f"{(255 * depth):d}",
                f"{100.0 * ok / (255.0 * depth):.1f}",
            ]
        )
    R.table(
        [
            "depth",
            "predicted `depth*E`",
            "longest burst survived",
            "shortest that broke",
            "block symbols",
            "% of the block",
        ],
        rows,
    )
    R.md()
    R.md(
        "**The prediction is exact, and the edge is a cliff rather than a "
        "slope.** At every allowed depth the longest fully-survived burst is "
        "`depth * E` and one symbol more fails in 200 blocks out of 200 — "
        "not *most* blocks, all of them. That is not a statistical result "
        "and should not be read as one: a burst of `depth*E + 1` contiguous "
        "symbols must put `E + 1` into some codeword by the pigeonhole "
        "principle, wherever it starts. The interleaver converts a burst "
        "into a guarantee, and the guarantee has no tail."
    )
    R.md()
    R.md(
        "**Depth is free in rate, and buys absolute burst length rather than "
        "a bigger fraction.** Every row above is the same (255,223) code at "
        "the same 87.5 % rate; depth 8 costs eight times the latency and "
        f"eight times the buffer, and buys a burst tolerance of {8 * E} "
        f"symbols against depth 1's {E}. But the rightmost column is "
        "**constant** — the correctable burst is the same 6.3 % of the block "
        "at every depth, because the block grew by the same factor the "
        "tolerance did. An interleaver does not make the outer code "
        "stronger; it makes the damage the code sees look random, and the "
        "number a caller designs to is the burst in symbols, not the "
        "proportion of a codeblock."
    )
    R.md()

    # ── 2.2 the marker's false-alarm tail ───────────────────────────
    R.md("### 2.2 The marker as a detector: H0 (C asm §5)")
    R.md()
    R.md(
        "`asm_find` reports the first offset within `max_errors` of the "
        "marker, in either polarity — a detector with a threshold, and the "
        'header calls that threshold "the whole of the trade". Its H0 is '
        "random data, where the distance to any fixed 32-bit pattern is "
        "`Binomial(32, 1/2)`, giving a per-offset rate of "
        "`2 * sum_{i<=t} C(32,i) / 2^32`. The factor two is the complement "
        "search."
    )
    R.md()

    rows = []
    for r in d["asm_fa"]:
        t = int(r["max_errors"])
        bits = float(r["bits"])
        model = _fa_model(t)
        rows.append(
            [
                str(t),
                _rate(float(r["hits"]), bits),
                f"{model:.3e}",
                f"{model * bits:.1f}",
                str(int(r["hits"])),
            ]
        )
    R.table(
        [
            "`max_errors`",
            "measured rate / bit",
            "closed form",
            "expected hits",
            "measured hits",
        ],
        rows,
    )
    R.md()
    R.md(
        "**The measurement tracks the closed form across five decades**, "
        "which is worth more than it looks: the model knows nothing about "
        "the implementation, so an `asm_find` correlating against the wrong "
        "32-bit pattern would reproduce this table exactly. It is the C "
        "test's transcription of figure 9-1 that fixes *which* pattern, and "
        "this that fixes the threshold's cost."
    )
    R.md()
    R.md(
        "Read it as a rate per bit of stream searched, because that is how a "
        "synchroniser meets it. At `t = 4` a false marker appears about once "
        "every 52000 bits; at `t = 8`, once every 143 — inside a single "
        "CADU, many times over."
    )
    R.md()

    # ── 2.3 the marker's detection tail ─────────────────────────────
    R.md(
        "### 2.3 The marker as a detector: H1, and where FIRST bites "
        "(C asm §6)"
    )
    R.md()
    R.md(
        "The other tail, against the decoded bit error rate rather than "
        "Es/N0 — a synchroniser sees Viterbi output, so quoting an SNR here "
        "would silently re-run `conv`'s measurement instead of this one. "
        "Each point is 4000 markers in each polarity, preceded by 96 random "
        "bits of stream."
    )
    R.md()

    thresholds = sorted({int(r["max_errors"]) for r in d["asm_pd"]})
    bers = sorted({float(r["ber"]) for r in d["asm_pd"]})
    rows = []
    for ber in bers:
        row = [f"{ber:.2f}"]
        for t in thresholds:
            trials, correct, _ = _pd(d, ber, t)
            row.append(f"{correct / trials:.3f}")
        rows.append(row)
    R.table(
        ["decoded BER"] + [f"`t = {t}`" for t in thresholds],
        rows,
    )
    R.md()

    clean_hi = _pd(d, 0.0, max(thresholds))
    clean_mid = _pd(d, 0.0, 6)
    R.md(
        "**A looser threshold is not a better detector, and the table shows "
        "where it turns around.** At `t = 8` the marker is found at the "
        f"right offset only {clean_hi[1] / clean_hi[0]:.3f} of the time on a "
        "**clean** stream — no channel errors at all. Nothing is wrong with "
        "the correlation; the marker is exactly where it was put and within "
        "zero errors of it. What fails is the *search*: `asm_find` reports "
        "the FIRST offset under threshold, and at `t = 8` each of the 96 "
        "preceding bits is an independent chance to beat it. "
        f"Treating those 96 offsets as independent predicts a miss rate of "
        f"`1 - (1 - {_fa_model(8):.3e})^96` = "
        f"{1 - (1 - _fa_model(8)) ** 96:.2f}; measured is "
        f"{1 - clean_hi[1] / clean_hi[0]:.2f}. The model sits high, and it "
        f"should: consecutive 32-bit windows share 31 bits, so the offsets "
        f"are heavily correlated and there are fewer independent chances "
        f"than there are positions. The independent-offset form is an upper "
        f"bound, which is the right side to be wrong on when it is being "
        f"used to choose a threshold."
    )
    R.md()
    R.md(
        "**So the lead-in length is part of the threshold choice, and the "
        "header does not say so.** The 96 bits above are not padding — "
        "double them and `t = 8` gets worse, halve them and it improves. A "
        "caller choosing `max_errors` is choosing it against how far the "
        "synchroniser searches before the marker arrives, which is a "
        "property of their stream and not of this function."
    )
    R.md()
    R.md(
        f"**`t = 6` is the useful setting for a coded link.** It holds "
        f"{clean_mid[1] / clean_mid[0]:.3f} on a clean stream and "
        f"{_pd(d, 0.10, 6)[1] / _pd(d, 0.10, 6)[0]:.3f} at a decoded BER of "
        "0.10, dominating `t = 8` at every BER measured while costing "
        f"{_fa_model(6) / _fa_model(4):.0f}x `t = 4`'s false-alarm rate. "
        "Below a decoded BER of 0.02 — which is where a Viterbi behind any "
        "usable link operates — `t = 2` and `t = 4` are both better still, "
        "and `t = 4` is the one that survives the channel going bad."
    )
    R.md()

    # ── 2.4 the randomiser's spectrum ───────────────────────────────
    R.md("### 2.4 Why B-6 demoted the 255-bit randomiser (C rand §7)")
    R.md()
    R.md(
        '131.0-B-6 keeps the degree-8 sequence only "for backward '
        'compatibility with legacy systems", and gives a reason rather than '
        'a preference: it "may introduce spectral lines at 1/255 of the '
        'symbol rate" and so "could not guarantee full compliance with ITU '
        'power flux density limits". That is a claim about a waveform, and '
        "this tree has a spectrum analyser."
    )
    R.md()
    R.md(
        "The measurement is the worst case `ccsds_tm.h` already names: "
        "randomise a run of CONSTANT data, so what goes on the wire IS the "
        "sequence. A PN payload would be flat whichever generator ran, which "
        "is exactly why the C test for the randomiser uses zeros. Both are "
        "NRZ-mapped and read through `psd_core` — the shipped meter — at the "
        "same analysis length, and referenced to the median bin rather than "
        "the mean, since a mean over a spectrum whose lines carry the power "
        "is dragged up by the thing being measured."
    )
    R.md()

    rows = []
    for which in ("default", "legacy"):
        s = _spectrum(d, which)
        rows.append(
            [
                f"`CCSDS_TM_RAND{'' if which == 'default' else '_LEGACY'}`",
                f"{int(s['period'])}",
                f"{float(s['peak_db']):.1f}",
                f"{float(s['at_1_255_db']):.1f}",
            ]
        )
    R.table(
        [
            "randomiser",
            "period (bits)",
            "strongest line (dB over floor)",
            "at 1/255 (dB)",
        ],
        rows,
    )
    R.md()

    leg = _spectrum(d, "legacy")
    dfl = _spectrum(d, "default")
    R.md(
        f"**The standard's rationale is measurable and it is enormous.** The "
        f"legacy sequence puts a line {float(leg['peak_db']):.0f} dB above "
        f"its own noise floor, and it is at 1/255 — the two numbers agree to "
        f"{abs(float(leg['peak_db']) - float(leg['at_1_255_db'])):.2f} dB, so "
        f"the strongest thing in the spectrum is the harmonic B-6 names and "
        f"not something else. The 131071-bit sequence over the same analysis "
        f"length has no line at all: its strongest bin is "
        f"{float(dfl['peak_db']):.1f} dB over the floor, which is the "
        f"ordinary fluctuation of an averaged periodogram."
    )
    R.md()
    R.md(
        "**This is the one measurement here that says a caller should change "
        "something.** `CCSDS_TM_RAND_LEGACY` is reachable and does what it "
        "says; on constant or highly correlated data it also emits a "
        f"{float(leg['peak_db']):.0f} dB carrier-like line at 1/255 of the "
        "symbol rate. Reach for it to talk to something old, as the header "
        "says — and if the payload can go quiet, expect the line."
    )
    R.md()


def review(d) -> None:
    R.md("## 3. Review")
    R.md()

    R.find(
        "F1",
        "FIXED",
        "**`ccsds_tm_randomise`'s docblock described the wrong generator.** "
        'It said "131.0-B-3 section 10.4.1: an 8-stage generator over '
        '`h(x) = x^8 + x^7 + x^5 + x^3 + 1`", that the register was '
        '"initialised to all ones", and that the sequence "repeats after '
        "255 bits\". All three are the LEGACY randomiser's. The function "
        "applies `CCSDS_TM_RAND` — degree 17, preset `11000111000111000`, "
        "period 131071 — which the same header states correctly two "
        "declarations above. Three facts, each wrong, each about the "
        "function's own behaviour, and invisible to every gate: no test "
        "reads prose, and the doctest beside it passes either way. §2.4 is "
        "why it matters rather than merely being untidy — the two "
        "generators differ by 91 dB in exactly the property the docblock "
        "misattributes. Corrected in this certification's own commit — a "
        "three-fact prose error with a one-paragraph fix is not worth an "
        "issue to carry it, and `FIXED` is the verdict the process defines "
        "for a defect a certification finds and repairs.",
    )
    R.find(
        "F2",
        "GAP",
        "**The threshold's cost depends on the lead-in, and no interface "
        "says so.** `asm_find`'s header explains that it reports the FIRST "
        "match rather than the best, and separately that a large "
        '`max_errors` "invites a false hit on random data". §2.3 shows '
        "those two are the same sentence: at `t = 8` the detection rate on "
        "a **clean** stream is capped at 0.57 by 96 bits of preceding "
        "noise, and the cap moves with the lead-in rather than with the "
        "channel. A caller reading either claim alone would choose "
        "`max_errors` from the marker length; it has to be chosen from the "
        "search window. Tracked as gh-897.",
    )
    R.find(
        "F3",
        "BY DESIGN",
        "**The interleaver's edge is deterministic, and that is why this "
        "report has no burst-tolerance curve.** §2.1's transition is 200/200 "
        "to 0/200 in one symbol at every depth, so there is nothing to "
        "characterise statistically — the pigeonhole argument is exact and "
        "the measurement confirms it rather than estimating it. A report "
        "that plotted a sigmoid here would be plotting its own trial count.",
    )
    R.find(
        "F4",
        "C-ONLY",
        "**Every identity claim is certified in C, not here.** The ASM "
        "pattern (`test_ccsds_tm_asm.c`, against figure 9-1), the generator "
        "polynomial (`test_ccsds_tm_rs.c`, against Annex G), both "
        "dual-basis matrices (against 4.3.9.3, plus a derived check that "
        "they are a dual basis at all), the inner code's impulse response "
        "(`test_ccsds_tm_conv.c`) and both randomiser prefixes "
        "(`test_ccsds_tm_rand.c`). Those are published values, so a wrong "
        "one is a test failure and not a table row — which is the whole "
        "reason this report measures behaviour the standard does not print.",
    )
    R.find(
        "F5",
        "C-ONLY",
        "**The coverage table is pinned in C and cannot be measured here.** "
        "That the outer code and the randomiser stop after the marker while "
        "the inner code reaches over it (9.2.1.5, 10.3.4) is asserted as "
        "three span comparisons in `test_ccsds_tm_frame.c`, against a CADU "
        "built by the encoder itself. It is the component's central claim "
        "and it is structural: there is no threshold to sweep and no rate "
        "to estimate, only a boundary that is right or wrong.",
    )


def limits(d) -> None:
    R.md("## 4. Limits")
    R.md()
    R.md("Claims a caller may rely on, asserted by this run.")
    R.md()

    by_depth = _burst_by_depth(d)

    # ── the interleaver ─────────────────────────────────────────────
    exact = all(
        _edge(rows)[0] == depth * E for depth, rows in by_depth.items()
    )
    R.limit(
        exact,
        f"a contiguous burst of `depth * {E}` symbols is corrected at every "
        f"allowed depth — {len(by_depth)} depths, "
        f"{_survived_blocks(by_depth)} "
        f"blocks, zero uncorrectable codewords",
    )
    R.limit(
        all(
            _edge(rows)[1] == depth * E + 1 for depth, rows in by_depth.items()
        ),
        f"and one symbol more always breaks it: at `depth * {E} + 1` every "
        f"block fails, at every depth — the edge is a cliff, not a slope",
    )
    R.limit(
        all(
            r["frame_exact"] == r["blocks"]
            for rows in by_depth.values()
            for r in rows
            if r["all_ok"] == r["blocks"]
        ),
        "a block whose codewords all decode is recovered symbol-exact — no "
        "miscorrection was observed inside the guaranteed radius",
    )

    # ── the marker, H0 ──────────────────────────────────────────────
    fa_ok = []
    for r in d["asm_fa"]:
        t = int(r["max_errors"])
        expected = _fa_model(t) * float(r["bits"])
        if expected < 10.0:
            continue  # too few events for a rate to mean anything
        fa_ok.append(abs(float(r["hits"]) - expected) / expected)
    R.limit(
        bool(fa_ok) and max(fa_ok) < 0.20,
        f"the false-alarm rate is the binomial closed form to within 20 % at "
        f"every threshold where the count supports one (worst "
        f"{100 * max(fa_ok):.0f} %, over {len(fa_ok)} points)",
    )
    R.limit(
        all(
            float(r["hits"]) == 0.0
            for r in d["asm_fa"]
            if int(r["max_errors"]) <= 1
        ),
        "at `max_errors <= 1` no false marker appears in 2000000 random bits, "
        "which the closed form puts at under 0.01 expected",
    )

    # ── the marker, H1 ──────────────────────────────────────────────
    trials, correct, _polarity = _pd(d, 0.0, 2)
    R.limit(
        correct == trials,
        f"on a clean stream at `max_errors = 2` the marker is found at its "
        f"exact offset in every trial ({trials}, both polarities)",
    )
    all_pol = [
        (_pd(d, b, t)[1], _pd(d, b, t)[2])
        for b in sorted({float(r["ber"]) for r in d["asm_pd"]})
        for t in sorted({int(r["max_errors"]) for r in d["asm_pd"]})
    ]
    R.limit(
        all(c == p for c, p in all_pol),
        f"whenever the marker is found at the right offset, its polarity is "
        f"reported correctly — {sum(c for c, _ in all_pol)} detections across "
        f"every BER and threshold, zero disagreements",
    )
    t6_clean = _pd(d, 0.0, 6)
    t8_clean = _pd(d, 0.0, 8)
    R.limit(
        t8_clean[1] / t8_clean[0] < t6_clean[1] / t6_clean[0],
        f"`max_errors = 8` detects WORSE than 6 even with no channel errors "
        f"({t8_clean[1] / t8_clean[0]:.3f} against "
        f"{t6_clean[1] / t6_clean[0]:.3f}) — a looser threshold is not a "
        f"more sensitive detector once the search has a lead-in",
    )
    t4_10 = _pd(d, 0.10, 4)
    R.limit(
        t4_10[1] / t4_10[0] > 0.70,
        f"at a decoded BER of 0.10 — far worse than any usable coded link — "
        f"`max_errors = 4` still finds the marker "
        f"{t4_10[1] / t4_10[0]:.3f} of the time",
    )

    # ── the randomiser ──────────────────────────────────────────────
    leg = _spectrum(d, "legacy")
    dfl = _spectrum(d, "default")
    R.limit(
        float(leg["peak_db"]) > 60.0,
        f"the legacy 255-bit randomiser puts a "
        f"{float(leg['peak_db']):.0f} dB line above its own noise floor on "
        f"constant data, which is B-6 10.4.2's stated reason for demoting it",
    )
    R.limit(
        abs(float(leg["peak_db"]) - float(leg["at_1_255_db"])) < 1.0,
        f"and that line is AT 1/255 of the symbol rate, exactly where the "
        f"standard says — the strongest bin and the 1/255 bin agree to "
        f"{abs(float(leg['peak_db']) - float(leg['at_1_255_db'])):.2f} dB",
    )
    R.limit(
        float(dfl["peak_db"]) < 6.0,
        f"the default 131071-bit randomiser has no line: its strongest bin "
        f"is {float(dfl['peak_db']):.1f} dB over the floor, against the "
        f"legacy sequence's {float(leg['peak_db']):.0f} dB at the same "
        f"analysis length",
    )


def _write_csv(d, write: bool) -> None:
    """Raw sweeps beside the report, so any number above can be re-derived."""
    if not write:
        return
    DATA.mkdir(parents=True, exist_ok=True)
    for name in ("burst", "asm_fa", "asm_pd", "spectrum"):
        rows = d[name]
        with (DATA / f"{name}.csv").open("w", newline="") as fh:
            w = csv.DictWriter(fh, fieldnames=list(rows[0]))
            w.writeheader()
            w.writerows(rows)


def build(write: bool = True) -> Report:
    d = _harness()

    R.md("# ccsds_tm — certification evidence")
    R.md()
    R.md("## 1. The object")
    R.md()
    R.md(
        "CCSDS 131.0-B's TM channel coding as a CONFIGURATION: the inner "
        "code is a `conv_code_t`, the outer code an `rs_code_t`, and neither "
        "`conv` nor `rs` knows what CCSDS is. What this component owns is "
        "everything the standard adds around them — the sync marker, the "
        "pseudo-randomiser, the dual basis symbols travel in, the "
        "interleaver, and the frame assembler where the four stages meet and "
        "disagree about what they cover."
    )
    R.md()
    R.md("Design and API, not restated here:")
    R.md()
    R.md(
        "- `native/inc/ccsds_tm/ccsds_tm.h`, `ccsds_tm_rs.h`, "
        "`ccsds_tm_frame.h` — the SSOT for every claim below\n"
        "- `native/tests/test_ccsds_tm_{asm,conv,rand,rs,frame}.c` — the C "
        "certification, five files because the published oracles are five "
        "separate documents\n"
        "- [The FEC Receive Half](../../../../../docs/design/fec-receive.md) "
        "— the chain this feeds\n"
        "- `native/validation/ccsds_tm_certify.c` — the sweeps below"
    )
    R.md()
    R.md("### Claim coverage — every prose claim in the header")
    R.md()
    R.md(
        "The campaign's order is header first, and this component came out "
        "of that inventory better than any before it: **every public entry "
        "point is already pinned against a published value**, because it was "
        'built that way — `ccsds_tm.h` opens by insisting that "every '
        "kernel here is falsified by a published vector, not by a round "
        'trip". So the table below has no `absent` rows, and the inventory '
        "produced no new C sections."
    )
    R.md()
    R.md(
        "It produced something else instead. Reading the header against the "
        "code turned up one claim that is not merely unpinned but **false** "
        "(F1), and one that is true but incomplete in a way a caller acts on "
        "(F2). Both are prose, which is exactly the class no gate in this "
        "repository can see."
    )
    R.md()
    R.table(
        ["#", "claim in the headers", "C section", "here"],
        [
            [
                "C1",
                "the ASM is `0x1ACFFC1D`, MSB-first from figure 9-1",
                "asm §1",
                "—",
            ],
            [
                "C2",
                "`asm_find` reports the FIRST offset under threshold, not "
                "the best",
                "asm §6",
                "§2.3",
            ],
            [
                "C3",
                "a complemented stream is found and SAID to be complemented",
                "asm §3",
                "§2.3",
            ],
            [
                "C4",
                "`max_errors` is the whole of the trade",
                "asm §4",
                "**§2.2, §2.3**",
            ],
            [
                "C5",
                "it does not invent a marker in random data",
                "asm §5",
                "**§2.2**",
            ],
            ["C6", "a miss leaves the caller's hit untouched", "asm §7", "—"],
            [
                "C7",
                "the default randomiser is 10.4.1's degree-17, period 131071",
                "rand §7",
                "§2.4",
            ],
            [
                "C8",
                "the legacy randomiser is 10.4.2's degree-8, period 255",
                "rand §1, §3",
                "§2.4",
            ],
            [
                "C9",
                "each is its own inverse, across a period boundary",
                "rand §4",
                "—",
            ],
            [
                "C10",
                "each call restarts at the preset (10.4.3)",
                "rand §5",
                "—",
            ],
            [
                "C11",
                "B-6 demoted the legacy sequence over spectral lines at 1/255",
                "—",
                "**§2.4**",
            ],
            [
                "C12",
                "`ccsds_tm_randomise` applies an 8-stage generator, period "
                "255",
                "—",
                "**F1 — false**",
            ],
            [
                "C13",
                "the inner code is `G1 = 171`, `G2 = 133` with G2 inverted",
                "conv §1, §2",
                "—",
            ],
            [
                "C14",
                "the symbol sequence is continuous across frames (3.3.2)",
                "conv §3, frame §9",
                "—",
            ],
            [
                "C15",
                "the field is `x^8 + x^7 + x^2 + x + 1`, roots are powers of "
                "`a^11`",
                "rs §1",
                "—",
            ],
            [
                "C16",
                "symbols travel in the dual basis, and it IS a dual basis",
                "rs §2, §3, §4",
                "—",
            ],
            [
                "C17",
                "`E = 16` corrected, `E + 1` refused with the buffer "
                "untouched",
                "rs §11",
                "—",
            ],
            [
                "C18",
                "depth 1 is the absence of interleaving (4.3.5.1)",
                "rs §9",
                "§2.1",
            ],
            [
                "C19",
                "the information section comes back in the order it entered",
                "rs §10, §12",
                "§2.1",
            ],
            [
                "C20",
                "a burst of `B` is `ceil(B/depth)` per codeword",
                "rs §13",
                "**§2.1**",
            ],
            ["C21", "depth outside `{1,2,3,4,5,8}` is refused", "rs §12", "—"],
            [
                "C22",
                "the outer code and the randomiser stop at the marker; the "
                "inner code reaches over it",
                "frame §1, §2, §3, §4",
                "F5",
            ],
            [
                "C23",
                "a frame that is not exactly `223 * I` octets is refused, "
                "not padded",
                "frame §8, §13",
                "—",
            ],
            [
                "C24",
                "the description and the encoder produce the same bits",
                "frame §14, §15",
                "—",
            ],
        ],
    )
    R.md()
    R.md(
        "`—` in the last column means the C section is the whole evidence "
        "and needs no help; **bold** marks what this report adds."
    )
    R.md()

    characterise(d)
    review(d)
    limits(d)

    leg = _spectrum(d, "legacy")
    t8 = _pd(d, 0.0, 8)
    R.executive(
        "ccsds_tm",
        source=(
            "Generated by `validate.py` in this folder. `ccsds_tm` has no "
            "Python binding, so every number is measured by "
            "`native/validation/ccsds_tm_certify.c` and rendered here. The "
            "one model is the binomial false-alarm rate §2.2 reads the "
            "marker's H0 tail against; the interleaver's edge and the "
            "randomiser's spectrum are measured outright. Re-run to "
            "regenerate."
        ),
        takeaways=[
            f"**The interleaver's guarantee is exact and has no tail.** A "
            f"contiguous burst of `depth * {E}` symbols is always corrected "
            f"and `depth * {E} + 1` always is not — 200 of 200 either way, "
            f"at every allowed depth. Size the depth from the longest burst "
            f"the channel produces, not from a probability (§2.1).",
            f"**The legacy randomiser really does put a "
            f"{float(leg['peak_db']):.0f} dB line at 1/255.** B-6's stated "
            f"reason for demoting it is measurable with this tree's own PSD, "
            f"and it is the largest number in this report. On constant data "
            f"`CCSDS_TM_RAND_LEGACY` emits a carrier-like line where the "
            f"default emits nothing (§2.4).",
            f"**A looser ASM threshold is a worse detector, not a more "
            f"sensitive one.** At `max_errors = 8` the marker is found at "
            f"its right offset only {t8[1] / t8[0]:.2f} of the time with NO "
            f"channel errors, because 96 bits of preceding stream win the "
            f"race first. `t = 4` is the setting that survives both tails; "
            f"`t = 6` if the link is bad (§2.2, §2.3, F2).",
            "**Choose `max_errors` from the search window, not the marker "
            "length.** The false-alarm rate is per bit searched, so the "
            "detection ceiling moves with how much stream precedes the "
            "marker — a property of the caller's synchroniser that no "
            "interface here exposes (F2).",
            "**A docblock claimed the wrong randomiser, in three separate "
            "facts, and no gate in this repository could see it.** The code "
            "was "
            "right and the file's own summary was right; one docblock was "
            "left behind by the B-6 adoption, and reading the header against "
            "the "
            "code is the only thing that finds that class. Fixed here (F1).",
            "**The identity claims are all in C, and that is the design.** "
            "Every published value this component configures — the marker, "
            "Annex G, both dual-basis matrices, both randomiser prefixes — "
            "is a C assertion that fails, not a report row that reads "
            "(F4, F5).",
        ],
    )
    _write_csv(d, write)
    R.summary(
        "\n- Raw sweeps: `data/burst.csv`, `data/asm_fa.csv`, "
        "`data/asm_pd.csv`, `data/spectrum.csv`"
    )
    if write:
        R.emit(HERE / "results.md")
    return R


if __name__ == "__main__":
    sys.exit(cli(build, HERE))
