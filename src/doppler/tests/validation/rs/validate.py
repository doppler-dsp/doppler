"""rs — certification evidence for a component with no Python face.

Run directly to regenerate `results.md`, the plots and the CSVs:

    uv run python src/doppler/tests/validation/rs/validate.py

`--check` re-renders in memory and diffs against the committed bytes;
`make validate` writes, `make validate-check` checks. Every limit this
records is asserted by `src/doppler/tests/test_validation_limits.py`,
which runs this same `build(write=False)`.

## Why this one is shaped differently

`rs` has no binding and is not getting one: a Python face built only to be
certified is a face nobody calls, and the campaign would then be measuring
an artifact of its own process. So the split is `conv`'s — **C measures,
Python renders**: `native/validation/rs_certify.c` runs the sweeps and
emits CSV; this file parses it, characterises, reviews and asserts the
limits through the same `Report` every other report uses, so the format
cannot drift between the two kinds of object.

Nothing here computes a measurement, and nothing there decides whether a
number is acceptable.

The campaign's order still holds and came first: `native/inc/rs/rs_core.h`
is the SSOT, `native/tests/test_rs_core.c` certifies it in C, and §1's
claim table is the inventory that produced seven new C sections and caught
a header claim that was not merely unpinned but false.
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
HARNESS = build_dir(__file__) / "native/validation/validate_rs_certify"

R = Report()

#: The code rate of the (255,223) code the channel sweep runs.
RATE = 223.0 / 255.0

#: Es/N0 to Eb/N0, in dB: a coding claim that does not pay for its rate is
#: not a coding claim.
RATE_DB = -10.0 * math.log10(RATE)


def _harness() -> dict[str, list[dict[str, float | str]]]:
    """Run the C harness and parse its CSV blocks.

    A missing binary is a hard failure rather than a skip. A skipped
    measurement is indistinguishable from a passing one in a log, and this
    report's whole content comes from that binary — `make build` builds it,
    and CI builds before it runs any Python.

    Unlike `conv`'s parser this one keeps a non-numeric field as a string:
    the sphere block is keyed by a code NAME, because "which code" is the
    independent variable and an index would make the report unreadable.
    That name contains the comma in `RS(255,223)`, so the rows are read with
    the `csv` module against the harness's quoting rather than split by hand.
    """
    if not HARNESS.exists():
        raise SystemExit(
            f"rs: {HARNESS.relative_to(ROOT)} is not built — run "
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


def _sphere_model(row: dict) -> float:
    """The probability a word far past `E` lands in some codeword's sphere.

    A received word beyond the guaranteed radius is, to a decoder, an
    arbitrary point of the space. It is miscorrected exactly when it sits
    inside one of the `q^k` decoding spheres, and those spheres are
    disjoint, so the probability is the fraction of the space they fill::

        P = q^k * V(E) / q^n = V(E) / q^(n-k),
        V(E) = sum_{i<=E} C(n, i) (q-1)^i

    This is the exact form. The `1/E!` a textbook prints is its large-field
    limit — substitute `C(n,E) ~ n^E/E!` and `n ~ q` — and §2.2 measures how
    far the two part company once the field is small.
    """
    q = 2 ** int(row["symbol_bits"])
    n = q - 1
    e = int(row["e"])
    vol = sum(math.comb(n, i) * (q - 1) ** i for i in range(e + 1))
    return vol / q ** int(row["nroots"])


def _uncoded_ber(ebn0_db: float) -> float:
    """Uncoded BPSK, `Q(sqrt(2 Eb/N0))` — the curve coding is read against."""
    ebn0 = 10.0 ** (ebn0_db / 10.0)
    return 0.5 * math.erfc(math.sqrt(ebn0))


def _uncoded_symbol_ser(ebn0_db: float, bits: int = 8) -> float:
    """An uncoded byte is broken if ANY of its bits is.

    The comparison the report needs is symbol against symbol: `rs` counts
    symbols, and quoting its output against an uncoded BIT error rate would
    flatter it by a factor of eight.
    """
    return 1.0 - (1.0 - _uncoded_ber(ebn0_db)) ** bits


def _rate(count: float, trials: float) -> str:
    """A measured rate, or the bound zero events supports."""
    if count > 0:
        return f"{count / trials:.4f}"
    return f"< {3.0 / trials:.1e}"


def _p(value: float) -> str:
    """A probability, in whichever notation does not read as zero."""
    return f"{value:.4f}" if value >= 1e-3 else f"{value:.1e}"


def _rule_error(row: dict) -> float:
    """How far the textbook `1/E!` sits above the exact form, as a fraction."""
    return 1.0 / math.factorial(int(row["e"])) / _sphere_model(row) - 1.0


def _small_field(d) -> tuple[float, float, float, float]:
    """RS(15,11)'s measured rate, its model, the textbook rule, the error.

    Read from the two points well past the radius rather than from all
    three: at exactly `E+1` errors the received word is not yet an
    arbitrary point of the space, which is the model's own assumption and
    is visible as a low point in §2.1 at every code.
    """
    pts = [
        r
        for r in d["sphere"]
        if int(r["symbol_bits"]) == 4 and int(r["errors"]) > int(r["e"]) + 1
    ]
    meas = sum(r["miscorrected"] for r in pts) / sum(r["trials"] for r in pts)
    model = _sphere_model(pts[0])
    rule = 1.0 / math.factorial(int(pts[0]["e"]))
    return meas, model, rule, rule / model - 1.0


def _by_code(d) -> dict[str, list[dict]]:
    """The sphere block, grouped by code name, order preserved."""
    out: dict[str, list[dict]] = {}
    for r in d["sphere"]:
        out.setdefault(str(r["code"]), []).append(r)
    return out


def characterise(d) -> None:
    R.md("## 2. Characterisation")
    R.md()
    R.md(
        "Every number below is `native/validation/rs_certify.c`'s, at 6000 "
        "trials per (code, error count) point and 200 codewords per channel "
        "point. The sphere sweep places its own error patterns, with the "
        "generator `test_rs_core.c` places them with; the channel sweep is "
        "the library's — bits from `pn`, symbols from `mpsk_map`, noise from "
        "`awgn`, decisions from `mpsk_soft_demap`."
    )
    R.md()

    codes = _by_code(d)

    # ── 2.1 the radius, and what is past it ─────────────────────────
    R.md("### 2.1 Past the guaranteed radius (C §4, §4b, §5)")
    R.md()
    rows = []
    for name, pts in codes.items():
        for j, r in enumerate(pts):
            rows.append(
                [
                    name if j == 0 else "",
                    f"{int(r['errors'])}",
                    _rate(r["corrected"], r["trials"]),
                    _rate(r["refused"], r["trials"]),
                    _rate(r["miscorrected"], r["trials"]),
                ]
            )
    R.table(["code", "errors", "corrected", "refused", "miscorrected"], rows)
    R.md()
    R.md(
        "The first row of each code is the guaranteed radius, and it is "
        "there as an anchor rather than as news: `E` errors are corrected in "
        "every one of 6000 trials at every configuration, which is what C "
        "§4 and §4b assert by construction. Everything below it is the "
        "header's careful sentence made measurable — *a refusal is not the "
        "same claim as more than `E` errors*."
    )
    R.md()
    total = sum(int(r["trials"]) for r in d["sphere"])
    bad = sum(int(r["noncodeword"]) for r in d["sphere"])
    R.md(
        f"Across all {total} decodes the harness ran, the number that "
        f"returned something which was neither a refusal nor a codeword is "
        f"**{bad}**. C §5 pins that as a property of the key equation over "
        f"error patterns from 0 to `n`; this is the same claim at "
        f"{total // 1000}k samples and it is the reason the two columns "
        f"beside `corrected` account for every remaining outcome."
    )
    R.md()

    # ── 2.2 the rate is a property of E ─────────────────────────────
    R.md("### 2.2 The miscorrection rate belongs to E, not to the damage")
    R.md()
    rows = []
    for name, pts in codes.items():
        past = [r for r in pts if int(r["errors"]) > int(r["e"])]
        model = _sphere_model(pts[0])
        e = int(pts[0]["e"])
        rows.append(
            [
                name,
                f"{e}",
                *(_rate(r["miscorrected"], r["trials"]) for r in past),
                _p(model),
                _p(1.0 / math.factorial(e)),
            ]
        )
    R.table(
        [
            "code",
            "E",
            "E+1 errors",
            "E+2",
            "4E+1",
            "sphere model",
            "1/E!",
        ],
        rows,
    )
    R.md("![Miscorrection against E](miscorrect.png)")
    R.md()
    R.md("**Two readings, and the second is the one a link budget needs.**")
    R.md()
    R.md(
        "First, the rate barely moves as the damage grows. From `E+1` errors "
        "to `4E+1` it changes by under a quarter at every code that "
        "miscorrects measurably, against a range of error counts that spans "
        "a factor of four. A word past the radius is, to the decoder, very "
        "nearly an arbitrary point of the space; whether it lands in some "
        "codeword's sphere is a question about the spheres, not about how "
        "far it travelled to get there. **So a link cannot escape a silent "
        "failure by being much worse than `E`** — see F3."
    )
    R.md()
    R.md(
        "The residual movement has a direction, and it is the model's own "
        "assumption showing: the `E+1` column is systematically the lowest "
        "of the three, because a word one symbol outside the radius is not "
        "yet arbitrary — it still remembers where it came from. By `4E+1` "
        "it does not, and the measurement sits on the model."
    )
    R.md()
    meas, model, rule, err = _small_field(d)
    ccsds = _rule_error(codes["RS(255,223) E=16"][0])
    small_e = _rule_error(codes["RS(255,251) E=2"][0])
    R.md(
        "Second, that model is exact and `1/E!` is not. The probability is "
        "the fraction of the space the decoding spheres fill, "
        "`V(E) / q^(n-k)`; `1/E!` is what is left after substituting "
        "`C(n,E) ~ n^E/E!` and `n ~ q`, and **both substitutions cost "
        "something.** The rule runs high on two independent axes, and the "
        "lower panel above is the only place either is visible:"
    )
    R.md()
    R.md(
        f"- **With E**, because `C(n,E) ~ n^E/E!` decays as `E` grows "
        f"against `n`. At `q = 256` the rule is {100 * small_e:.0f} % high "
        f"at `E = 2` and **{100 * ccsds:.0f} % high at CCSDS's `E = 16`** — "
        f"the configuration the tree actually ships.\n"
        f"- **With a small field**, because `n ~ q` is `255/256` at `J = 8` "
        f"and `15/16` at `J = 4`. RS(15,11) measures {meas:.3f} against a "
        f"model of {model:.3f} and a textbook {rule:.2f}: "
        f"{100 * err:.0f} % high at an `E` where the first axis costs only "
        f"{100 * small_e:.0f} % (F4)."
    )
    R.md()
    R.md(
        "Neither error changes a decision on its own — a factor of two on a "
        "probability of 2.6e-14 is still never — but the rule is quoted as "
        "though it were the answer, and it is the kind of number that gets "
        "carried into a budget where the exponent is smaller."
    )
    R.md()
    unseen = sorted(
        _sphere_model(pts[0])
        for pts in codes.values()
        if all(r["miscorrected"] == 0 for r in pts)
    )
    R.md(
        f"The rows that read `< 5.0e-4` are the honest limit of a per-push "
        f"sweep: the model puts them at {_p(unseen[-1])} and "
        f"{_p(unseen[0])}, and no Monte-Carlo that has to finish in three "
        f"seconds will ever see the second one. What 6000 clean trials do "
        f"establish is the claim a caller acts on — that at `E >= 8` a "
        f"failure past the radius is a **refusal**."
    )
    R.md()

    # ── 2.3 the two shapes of one code ──────────────────────────────
    R.md("### 2.3 One code, two shapes — the header's central claim (NEW)")
    R.md()
    a = codes["RS(255,223) E=16"]
    b = codes["RS(255,223) CCSDS-shaped E=16"]
    rows = [
        [
            f"{int(x['errors'])}",
            f"{int(x['corrected'])}/{int(x['trials'])}",
            f"{int(y['corrected'])}/{int(y['trials'])}",
            f"{int(x['refused'])}",
            f"{int(y['refused'])}",
        ]
        for x, y in zip(a, b)
    ]
    R.table(
        [
            "errors",
            "textbook corrected",
            "CCSDS-shaped corrected",
            "textbook refused",
            "CCSDS-shaped refused",
        ],
        rows,
    )
    R.md()
    R.md(
        "`0x1D`/`j0 = 1`/`s = 1` against `0x87`/`j0 = 112`/`s = 11`: a "
        "different field, a different first root and a stride of eleven, "
        "delivering outcome for outcome the same result. That is the file's "
        "opening claim — *the arithmetic is identical and only the table "
        "changes* — and no assertion can make it, because each shape decodes "
        "its own encoder perfectly whether or not the other exists. It takes "
        "two codes measured side by side."
    )
    R.md()
    R.md(
        "It is also the claim that fails LOUDLY when it fails: the two "
        "factors `docs/design/reed-solomon.md` §4 calls out — the `j0` "
        "substitution before Berlekamp-Massey and the `X^-(j0-1)` in Forney "
        "— are 1 for the textbook row and neither is 1 for the CCSDS row. A "
        "decoder that omitted them would sit at 6000/6000 in the left column "
        "and 0/6000 in the right."
    )
    R.md()

    # ── 2.4 on a channel ────────────────────────────────────────────
    R.md("### 2.4 On a real channel: where the outer code starts paying")
    R.md()
    rows = []
    for r in d["channel"]:
        ebn0 = r["esn0_db"] + RATE_DB
        rows.append(
            [
                f"{r['esn0_db']:.1f}",
                f"{ebn0:.2f}",
                f"{r['sym_err_in'] / r['sym_total']:.4f}",
                f"{r['sym_err_out'] / r['sym_total']:.2e}",
                f"{_uncoded_symbol_ser(ebn0):.4f}",
                f"{int(r['cw_good'])}/200",
                f"{int(r['cw_refused'])}",
            ]
        )
    R.table(
        [
            "Es/N0 dB",
            "Eb/N0 dB",
            "symbol SER in",
            "symbol SER out",
            "uncoded SER",
            "codewords good",
            "refused",
        ],
        rows,
    )
    R.md("![The channel the code is bought for](channel.png)")
    R.md()
    R.md(
        f"RS(255,223) over BPSK, hard decisions, 200 codewords a point. The "
        f"code spends {RATE_DB:.2f} dB of Eb on its rate, so the uncoded "
        f"column is read at the SAME Eb/N0 rather than the same Es/N0 — the "
        f"comparison a caller is actually choosing between."
    )
    R.md()
    cross = [
        r
        for r in d["channel"]
        if r["sym_err_out"] / r["sym_total"]
        > _uncoded_symbol_ser(r["esn0_db"] + RATE_DB)
    ]
    worst = max(r["esn0_db"] for r in cross)
    R.md(
        f"**Below Es/N0 ~ {worst:.1f} dB the outer code delivers a WORSE "
        f"symbol error rate than no coding at all**, for the same reason a "
        f"hard-decision Viterbi does in `conv`'s §2.2: the rate is paid up "
        f"front and a code that cannot clear the channel buys nothing back. "
        f"The knee is about a decibel wide, and one decibel past it the "
        f"output is clean."
    )
    R.md()
    R.md(
        "**Below the knee the value is the REFUSAL, not the correction.** At "
        "Es/N0 = 4.0 dB the code repairs about 2 % of the broken symbols and "
        "refuses 193 of 200 codewords: almost nothing is fixed, and almost "
        "everything wrong is *known* to be wrong. That is the number "
        "`ccsds_tm_frame_rx_t` reports counts for, and it is why a receiver "
        "that treats a refusal as a failure and a correction as a success is "
        "reading the outer code backwards at exactly the SNR where it "
        "matters."
    )
    R.md()
    mis = sum(int(r["cw_miscorrected"]) for r in d["channel"])
    R.md(
        f"Silent failures on this channel: **{mis}** in "
        f"{len(d['channel']) * 200} codewords. At `E = 16` the sphere model "
        f"puts a miscorrection at "
        f"{_p(_sphere_model(codes['RS(255,223) E=16'][0]))}, so a run that "
        f"produced one would be evidence of a defect rather than of bad "
        f"luck (§2.2)."
    )
    R.md()


def plots(d) -> None:
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    codes = _by_code(d)

    # ── measured miscorrection against E ────────────────────────────
    es, meas, model, fact, bounds, be = [], [], [], [], [], []
    for pts in codes.values():
        if int(pts[0]["symbol_bits"]) != 8:
            continue
        e = int(pts[0]["e"])
        past = [r for r in pts if int(r["errors"]) > e]
        hits = sum(r["miscorrected"] for r in past)
        trials = sum(r["trials"] for r in past)
        if hits > 0:
            es.append(e)
            meas.append(hits / trials)
        else:
            be.append(e)
            bounds.append(3.0 / trials)
        model.append((e, _sphere_model(pts[0])))
        fact.append((e, 1.0 / math.factorial(e)))

    sm_meas, sm_model, sm_rule, _ = _small_field(d)

    # Two panels, because one cannot carry both claims. The top spans
    # fourteen decades, where a 37 % disagreement is a line width; the
    # bottom divides that span out and is the only place the small field's
    # departure from 1/E! is visible at all.
    fig, (ax, bx) = plt.subplots(
        2, 1, figsize=(6.2, 5.6), sharex=True, height_ratios=[2, 1]
    )
    mx = sorted(set(model))
    ax.semilogy(
        [p[0] for p in mx],
        [p[1] for p in mx],
        "k-",
        label="sphere model, q = 256",
    )
    fx = sorted(set(fact))
    ax.semilogy(
        [p[0] for p in fx], [p[1] for p in fx], "k--", alpha=0.5, label="1/E!"
    )
    ax.semilogy(es, meas, "o", ms=8, label="measured, q = 256")
    if be:
        ax.semilogy(be, bounds, "v", ms=8, label="0 events, 3/N bound")
    ax.semilogy([2], [sm_meas], "s", ms=8, label="measured, q = 16")
    ax.set_ylabel("P(miscorrect | beyond the radius)")
    ax.set_title("A failure past E is silent with probability V(E)/q^(n-k)")
    ax.grid(True, which="both", alpha=0.3)
    ax.legend(fontsize=8)

    by_e = dict(model)
    bx.axhline(1.0, color="k", lw=1)
    bx.plot(
        [p[0] for p in fx],
        [p[1] / by_e[p[0]] for p in fx],
        "k--",
        alpha=0.5,
        label="1/E! over the model",
    )
    bx.plot(es, [m / by_e[e] for e, m in zip(es, meas)], "o", ms=8)
    bx.plot([2], [sm_meas / sm_model], "s", ms=8)
    bx.plot([2], [sm_rule / sm_model], "d", ms=8, label="1/E! at q = 16")
    bx.set_xlabel("E, correctable symbols")
    bx.set_ylabel("÷ sphere model")
    bx.set_ylim(0.8, 1.6)
    bx.grid(True, alpha=0.3)
    bx.legend(fontsize=8)
    fig.tight_layout()
    fig.savefig(HERE / "miscorrect.png", dpi=110)
    plt.close(fig)

    # ── the channel ─────────────────────────────────────────────────
    ebn0 = [r["esn0_db"] + RATE_DB for r in d["channel"]]
    sin = [r["sym_err_in"] / r["sym_total"] for r in d["channel"]]
    unc = [_uncoded_symbol_ser(e) for e in ebn0]
    good = [r["cw_good"] / 200.0 for r in d["channel"]]

    # Zero errors is not a measurement of a rate, so it is not drawn as one:
    # the points where the output was clean become the bound 3/N supports,
    # with their own marker. Drawing them on the curve at a substituted
    # floor would show a plateau the data does not have.
    seen = [
        (e, r["sym_err_out"] / r["sym_total"])
        for e, r in zip(ebn0, d["channel"])
        if r["sym_err_out"] > 0
    ]
    clean = [
        (e, 3.0 / r["sym_total"])
        for e, r in zip(ebn0, d["channel"])
        if r["sym_err_out"] == 0
    ]

    fig, (ax, bx) = plt.subplots(2, 1, figsize=(6.2, 6.0), sharex=True)
    ax.semilogy(ebn0, unc, "o--", label="uncoded symbol SER")
    ax.semilogy(ebn0, sin, "s-", label="channel symbol SER (in)")
    ax.semilogy(
        [p[0] for p in seen],
        [p[1] for p in seen],
        "^-",
        label="after RS(255,223) (out)",
    )
    if clean:
        ax.semilogy(
            [p[0] for p in clean],
            [p[1] for p in clean],
            "v",
            mfc="none",
            ms=8,
            label="0 errors, 3/N bound",
        )
    ax.set_ylabel("symbol error rate")
    ax.set_title("RS(255,223) over BPSK — the rate is paid before the knee")
    ax.grid(True, which="both", alpha=0.3)
    ax.legend(fontsize=8)

    bx.plot(ebn0, good, "o-")
    bx.set_xlabel("Eb/N0 (dB)")
    bx.set_ylabel("codewords delivered exact")
    bx.set_ylim(-0.05, 1.05)
    bx.grid(True, alpha=0.3)
    fig.tight_layout()
    fig.savefig(HERE / "channel.png", dpi=110)
    plt.close(fig)


def write_csv(d) -> None:
    DATA.mkdir(exist_ok=True)
    for name, rows in d.items():
        if not rows:
            continue
        with (DATA / f"{name}.csv").open("w", newline="") as fh:
            w = csv.DictWriter(fh, fieldnames=list(rows[0]))
            w.writeheader()
            w.writerows(rows)


def review(d) -> None:
    R.md("## 3. Review")
    R.md()

    R.find(
        "F1",
        "FIXED",
        "`rs_core.h` offered RS(204,188) as a configuration to point the "
        "file at — *point this at RS(204,188) for DVB* — and it cannot be "
        "done. `n` is `2^J - 1` by construction, so DVB's code is a "
        "SHORTENED RS(255,239): the mother code with 51 leading zeros the "
        "sender never transmits. `docs/design/reed-solomon.md` said the same "
        "thing in §1 while its own §7 listed shortened codes as not "
        "implemented, tracking the virtual fill as "
        "[gh-813](https://github.com/doppler-dsp/doppler/issues/813); the "
        "two halves of one page disagreed. Both now name the mother code and "
        "say why the shortened one is a different question. This is what "
        "step 1 is for: the claim was not unpinned, it was false, and no "
        "amount of testing the code would have found it.",
    )
    R.find(
        "F2",
        "C-ONLY",
        "Every claim in §1's table is verified in C and none of it is "
        "reachable from Python, because `rs` has no binding. That is the "
        "component's design rather than a gap in the evidence: the caller is "
        "`ccsds_tm`, and it is C. `rs` is the second component certified on "
        "the C-only track `conv` opened, and the first to use it for a "
        "component whose interesting behaviour is a probability rather than "
        "a curve.",
    )
    R.find(
        "F3",
        "BY DESIGN",
        "The miscorrection rate does not fall as the damage grows (§2.2): "
        "at `4E+1` errors it is within a quarter of its value at `E+1`, "
        "across a four-fold range of error counts. This reads as a "
        "defect because the intuition is that a badly broken word is "
        "obviously broken, and the intuition is wrong — a word past the "
        "radius is an arbitrary point of the space, and whether it lands in "
        "a decoding sphere is a question about the spheres. The consequence "
        "is the one the header states: a decode reports a COUNT and not a "
        "verdict, because a caller cannot infer 'many errors' from a "
        "correction, and the protection is accounting one layer up.",
    )
    meas, model, rule, err = _small_field(d)
    codes = _by_code(d)
    ccsds = _rule_error(codes["RS(255,223) E=16"][0])
    R.find(
        "F4",
        "FIXED",
        f"`test_rs_core.c`'s refusal gate carried its rationale as *with "
        f"probability ~1/E! for random errors ... at E = 2 it is a coin "
        f"toss*, and cited `1/16! ~ 5e-14` for the CCSDS code. `1/E!` is "
        f"what is left of the exact `V(E)/q^(n-k)` after two "
        f"approximations, and each costs more than the comment implies: it "
        f"is {100 * err:.0f} % high at RS(15,11), the configuration the "
        f"comment used as its example (the rate there is {meas:.3f}, "
        f"against {rule:.2f} from the rule and {model:.3f} from the exact "
        f"form the measurement lands on), and {100 * ccsds:.0f} % high at "
        f"`E = 16`, the configuration the tree ships — 5e-14 against an "
        f"exact 2.6e-14. The gate's own floor was never in danger; it sits "
        f"at 32 of 64, which the exact model puts more than two standard "
        f"deviations away. What was wrong was the number a reader takes "
        f"away, at both ends of the table. The comment now carries the "
        f"exact form and points here for the measurement (§2.2).",
    )


def limits(d) -> None:
    R.md("## 4. Limits")
    R.md()
    R.md("Claims a caller may rely on, asserted by this run.")
    R.md()

    codes = _by_code(d)
    at_e = [r for r in d["sphere"] if int(r["errors"]) == int(r["e"])]
    past = [r for r in d["sphere"] if int(r["errors"]) > int(r["e"])]

    R.limit(
        all(r["corrected"] == r["trials"] for r in at_e),
        f"E symbol errors are corrected in every trial, at all "
        f"{len(at_e)} configurations "
        f"({sum(int(r['trials']) for r in at_e)} words, zero failures)",
    )
    R.limit(
        all(r["noncodeword"] == 0 for r in d["sphere"]),
        f"a decode never returns a non-codeword — "
        f"{sum(int(r['trials']) for r in d['sphere'])} decodes across every "
        f"code and every error count, zero third outcomes",
    )

    # Measurable only where the model puts the rate above the noise floor of
    # 6000 trials; the two large-E codes are bounded instead, below.
    meas = [
        (r, _sphere_model(r))
        for r in past
        if r["miscorrected"] > 30 and int(r["symbol_bits"]) == 8
    ]
    worst = max(
        abs(r["miscorrected"] / r["trials"] / m - 1.0) for r, m in meas
    )
    R.limit(
        worst <= 0.15,
        f"where it is measurable, the miscorrection rate is the sphere "
        f"model V(E)/q^(n-k) to within 15 % (worst {100 * worst:.0f} %, over "
        f"{len(meas)} points at q = 256)",
    )

    # Bounded BOTH ways on purpose. "Agrees with the model" alone is also
    # satisfied by a rule that happens to sit nearby, and the point of this
    # limit is that the textbook rule does NOT.
    sm_rate, sm_model, sm_rule, _ = _small_field(d)
    R.limit(
        abs(sm_rate / sm_model - 1.0) <= 0.15
        and abs(sm_rate / sm_rule - 1.0) >= 0.20,
        f"the same model holds on the small field where 1/E! does not: "
        f"RS(15,11) measures {sm_rate:.3f} against a model of "
        f"{sm_model:.3f} and a textbook 1/E! of {sm_rule:.3f}",
    )

    flat = []
    for pts in codes.values():
        p = [r for r in pts if int(r["errors"]) > int(r["e"])]
        if p[0]["miscorrected"] < 100:
            continue
        near = p[0]["miscorrected"] / p[0]["trials"]
        far = p[-1]["miscorrected"] / p[-1]["trials"]
        flat.append(abs(far / near - 1.0))
    # 30 %, not the 20 % this measures: the residual is SYSTEMATIC (§2.2 —
    # the E+1 point is low at every code because the word is not yet
    # arbitrary), so a threshold set on the measurement would be a threshold
    # set on one sweep's noise around a real effect. What the claim defends
    # is that the rate does not COLLAPSE with distance.
    R.limit(
        max(flat) <= 0.30,
        f"the miscorrection rate is flat in the error count — at 4E+1 "
        f"errors it is within 30 % of its value at E+1 (worst "
        f"{100 * max(flat):.0f} %), so being far past the radius does not "
        f"make a failure detectable",
    )

    big = [r for r in past if int(r["e"]) >= 8]
    R.limit(
        all(r["miscorrected"] == 0 for r in big),
        f"at E >= 8 a failure past the radius is a REFUSAL: zero silent "
        f"miscorrections in {sum(int(r['trials']) for r in big)} words "
        f"beyond the radius, over three codes",
    )

    a = codes["RS(255,223) E=16"]
    b = codes["RS(255,223) CCSDS-shaped E=16"]
    R.limit(
        all(
            x["corrected"] == y["corrected"] and x["refused"] == y["refused"]
            for x, y in zip(a, b)
        ),
        "the textbook and CCSDS-shaped (255,223) codes deliver outcome for "
        "outcome the same result — a different field, first root and stride "
        "change the table and not the arithmetic",
    )

    ch = {round(r["esn0_db"], 1): r for r in d["channel"]}
    clean = [r for r in d["channel"] if r["esn0_db"] >= 5.5]
    R.limit(
        all(r["cw_good"] == 200 and r["sym_err_out"] == 0 for r in clean),
        f"from Es/N0 = 5.5 dB (Eb/N0 = {5.5 + RATE_DB:.2f} dB) every "
        f"codeword is delivered byte-exact — 600 of 600, zero residual "
        f"symbol errors",
    )
    low = ch[4.0]
    R.limit(
        low["sym_err_out"] / low["sym_total"]
        > _uncoded_symbol_ser(4.0 + RATE_DB),
        f"at Es/N0 = 4.0 dB the coded symbol error rate is WORSE than "
        f"uncoded at the same Eb/N0 "
        f"({low['sym_err_out'] / low['sym_total']:.4f} against "
        f"{_uncoded_symbol_ser(4.0 + RATE_DB):.4f}) — the rate is paid "
        f"before the knee",
    )
    R.limit(
        low["cw_refused"] >= 180,
        f"and it says so: {int(low['cw_refused'])} of 200 codewords are "
        f"REFUSED there, so what the outer code delivers below the knee is "
        f"detection rather than repair",
    )
    R.limit(
        all(r["cw_miscorrected"] == 0 for r in d["channel"]),
        "no codeword is ever silently miscorrected on the measured channel "
        "(0 of 1400), which is what E = 16 buys",
    )


def build(write: bool = True) -> Report:
    d = _harness()

    R.md("# rs — certification evidence")
    R.md()
    R.md("## 1. The object")
    R.md()
    R.md(
        "A Reed-Solomon code as a DESCRIPTION — a symbol width, a field "
        "polynomial, a parity count, a first root and a root stride — with "
        "one encoder, one syndrome routine and one correcting decoder all "
        "reading it. CCSDS 131.0-B-3's (255,223) is a configuration of it, "
        "and this report measures that configuration beside a textbook one "
        "to show the difference is the table."
    )
    R.md()
    R.md("Design and API, not restated here:")
    R.md()
    R.md(
        "- `native/inc/rs/rs_core.h` — the SSOT for every claim below\n"
        "- `native/tests/test_rs_core.c` — the C certification\n"
        "- [Reed-Solomon](../../../../../docs/design/reed-solomon.md) — the "
        "algebra, the two offsets a textbook omits, and what a decode "
        "refusal does and does not mean\n"
        "- `native/validation/rs_certify.c` — the sweeps below"
    )
    R.md()
    R.md("### Claim coverage — every prose claim in the header")
    R.md()
    R.md(
        "The campaign's order is header first. This table is the inventory "
        "that produced seven new C sections — the derived sizes and the "
        "declared range had **zero** assertions, the parity was pinned only "
        "against the syndromes that share its convention, and 'carries no "
        "running state' was pinned by nothing at all — and one claim that "
        "was not unpinned but false (C2)."
    )
    R.md()
    R.table(
        ["#", "claim in `rs_core.h`", "C section", "here"],
        [
            [
                "C1",
                "one description; encoder, checker and decoder cannot"
                " disagree",
                "§2, §2b, §3b",
                "§2.3",
            ],
            [
                "C2",
                "point it at any code a caller brings — DVB, RS(15,11)",
                "§1b",
                "F1",
            ],
            [
                "C3",
                "`field_poly` must be primitive, and `rs_init` refuses",
                "§1, §1b",
                "—",
            ],
            [
                "C4",
                "`gcd(root_stride, n) = 1`, or the code corrects fewer errors",
                "§1, §1c (NEW)",
                "—",
            ],
            [
                "C5",
                "symbols are packed one per byte, top bits clear at `J < 8`",
                "§3c (NEW)",
                "—",
            ],
            [
                "C6",
                "`k` information then `nroots` parity; index `i` carries"
                " `x^(n-1-i)`",
                "§2b (NEW), §3b (NEW)",
                "—",
            ],
            ["C7", "the conventional basis throughout", "— (ccsds_tm's)", "—"],
            [
                "C8",
                "the declared range: `J` in 2..8, `nroots` in 2..64",
                "§1, §1b (NEW)",
                "—",
            ],
            [
                "C9",
                "`n = 2^J-1`, `k = n-nroots`, `E = nroots/2`",
                "§1b (NEW)",
                "—",
            ],
            [
                "C10",
                "it carries no running state; every function takes it `const`",
                "§8 (NEW)",
                "—",
            ],
            [
                "C11",
                "`rs_code_valid` checks the ranges, evenness and the stride",
                "§1, §1b",
                "—",
            ],
            [
                "C12",
                "...and deliberately does NOT check primitivity",
                "§1b (NEW)",
                "—",
            ],
            [
                "C13",
                "`gen[i]` is the coefficient of `x^i`, monic, degree `2E`",
                "§2",
                "—",
            ],
            [
                "C14",
                "exposed because standards publish it (Annex G)",
                "`test_ccsds_tm_rs.c`",
                "—",
            ],
            ["C15", "systematic: the information is not touched", "§3", "—"],
            [
                "C16",
                "the parity IS `info(x)*x^nroots mod g(x)`, high order first",
                "§2b (NEW)",
                "—",
            ],
            [
                "C17",
                "`S_m = C(a^(s*(j0+m)))`, all zero IS being a codeword",
                "§3, §3b (NEW)",
                "—",
            ],
            [
                "C18",
                "corrects up to `E` symbol errors, in place",
                "§4, §4b (NEW), §7",
                "§2.1",
            ],
            [
                "C19",
                "it refuses or returns a codeword — no third outcome",
                "§5",
                "§2.1",
            ],
            [
                "C20",
                "a refusal is not the claim 'more than E'; it can miscorrect",
                "§4",
                "§2.2, F3",
            ],
            ["C21", "a refused word is left untouched", "§5", "—"],
            [
                "C22",
                "returns the count repaired, 0 for a clean word, -1 to refuse",
                "§4, §4b, §6",
                "§2.1",
            ],
        ],
    )
    R.md(
        "**What Python cannot reach: all of it.** `rs` has no binding, so "
        "the evidence here is a C harness rendered by Python rather than a "
        "binding exercised by it. The `here` column points at what this "
        "report adds — the behaviour that is a probability rather than an "
        "assertion — and `—` means the C section is the whole evidence and "
        "needs no help."
    )
    R.md()

    characterise(d)
    if write:
        plots(d)
        write_csv(d)
    review(d)
    limits(d)

    codes = _by_code(d)
    _, sm_model, _, sm_err = _small_field(d)
    worst_e = _sphere_model(codes["RS(255,253) E=1"][0])
    best_e = _sphere_model(codes["RS(255,223) E=16"][0])

    R.executive(
        "rs",
        source=(
            "Generated by `validate.py` in this folder. `rs` has no Python "
            "binding, so every number is measured by "
            "`native/validation/rs_certify.c` and rendered here; nothing is "
            "modelled except the two closed forms it is compared against. "
            "Re-run to regenerate."
        ),
        takeaways=[
            f"**A failure past `E` is silent with probability "
            f"`V(E)/q^(n-k)`, and that number does not depend on how bad "
            f"the damage is.** At `E = 1` almost every uncorrectable word is "
            f"miscorrected ({worst_e:.2f}); at `E = 16` it is "
            f"{_p(best_e)}. Choose the parity count for the detection you "
            f"need, not only for the correction (§2.2, F3).",
            f"**Do not read `1/E!` off a textbook — use `V(E)/q^(n-k)`.** "
            f"The rule is high by {100 * sm_err:.0f} % on a small field "
            f"(RS(15,11), exact value {sm_model:.3f}) and by "
            f"{100 * _rule_error(codes['RS(255,223) E=16'][0]):.0f} % at "
            f"CCSDS's `E = 16`, which is the configuration this tree ships "
            f"(§2.2, F4).",
            "**Below the knee the outer code's product is detection, not "
            "repair.** At Es/N0 = 4.0 dB RS(255,223) fixes about 2 % of the "
            "broken symbols, delivers a worse symbol error rate than no "
            "coding at all, and refuses 193 of 200 codewords — which is the "
            "useful output at that SNR (§2.4).",
            "**One decibel is the whole knee.** From Es/N0 = 4.5 dB to "
            "5.5 dB the code goes from breaking even to delivering every "
            "codeword byte-exact with zero residual symbol errors (§2.4).",
            "**The CCSDS shape is not a special case.** The same code with "
            "`j0 = 112` and `s = 11` over a different field delivers outcome "
            "for outcome the same result as the textbook one, which is the "
            "measurement behind `rs` and `ccsds_tm` being separate files "
            "(§2.3).",
            "**The evidence is C, and that is the design.** `rs` has no "
            "Python face and should not grow one to be certified (F2).",
        ],
    )
    R.summary("\n- Raw sweeps: `data/sphere.csv`, `data/channel.csv`")
    if write:
        R.emit(HERE / "results.md")
    return R


if __name__ == "__main__":
    sys.exit(cli(build, HERE))
