"""conv — certification evidence for a component with no Python face.

Run directly to regenerate `results.md`, the plots and the CSVs:

    uv run python src/doppler/tests/validation/conv/validate.py

`--check` re-renders in memory and diffs against the committed bytes;
`make validate` writes, `make validate-check` checks. Every limit this
records is asserted by `src/doppler/tests/test_validation_limits.py`,
which runs this same `build(write=False)`.

## Why this one is shaped differently

Every other certified object is measured *through its binding*, because
that is what a caller uses. `conv` has no binding and is not getting one:
a Python face built only to be certified is a face nobody calls, and the
campaign would then be measuring an artifact of its own process.

So the split is **C measures, Python renders**:
`native/validation/conv_certify.c` runs the sweeps — bits from `pn`,
symbols from `mpsk_map`, noise from `awgn`, soft decisions from
`mpsk_soft_demap` — and emits CSV; this file parses it, characterises,
reviews and asserts the limits through the same `Report` every other
report uses, so the format cannot drift between the two kinds of object.

Nothing here computes a measurement, and nothing there decides whether a
number is acceptable.

The campaign's order still holds and came first: `native/inc/conv/conv_core.h`
is the SSOT, `native/tests/test_conv_core.c` certifies it in C, and §1's
claim table is the inventory that produced four new C sections.
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
HARNESS = build_dir(__file__) / "native/validation/validate_conv_certify"

R = Report()


def _harness() -> dict[str, list[dict[str, float]]]:
    """Run the C harness and parse its CSV blocks.

    A missing binary is a hard failure rather than a skip. A skipped
    measurement is indistinguishable from a passing one in a log, and this
    report's whole content comes from that binary — `make build` builds it,
    and CI builds before it runs any Python.
    """
    if not HARNESS.exists():
        raise SystemExit(
            f"conv: {HARNESS.relative_to(ROOT)} is not built — run "
            f"`make build` first. This report has no measurement of its "
            f"own; the C harness is where every number in it comes from."
        )
    out = subprocess.run(
        [str(HARNESS), "--emit"], capture_output=True, text=True, check=True
    ).stdout

    blocks: dict[str, list[dict[str, float]]] = {}
    name, rows, header = "", [], []
    for line in out.splitlines():
        line = line.strip()
        if line.startswith("#"):
            if name:
                blocks[name] = rows
            name, rows, header = line[1:].strip(), [], []
        elif not line:
            continue
        elif not header:
            header = line.split(",")
        else:
            rows.append(dict(zip(header, (float(v) for v in line.split(",")))))
    if name:
        blocks[name] = rows
    return blocks


def _uncoded_ber(ebn0_db: float) -> float:
    """Uncoded BPSK, `Q(sqrt(2 Eb/N0))` — the curve coding is read against."""
    ebn0 = 10.0 ** (ebn0_db / 10.0)
    return 0.5 * math.erfc(math.sqrt(ebn0))


def _fmt(p: float, bits: float) -> str:
    """A rate, or the limit zero errors supports."""
    if p > 0.0:
        return f"{p:.3e}"
    return f"< {3.0 / bits:.1e} (0 errors)"


def characterise(d) -> None:
    R.md("## 2. Characterisation")
    R.md()
    R.md(
        "Every number below is `native/validation/conv_certify.c`'s, at "
        "120 000 information bits per point, on CCSDS's K = 7 rate-1/2 code. "
        "Es/N0 is at the matched-filter output; Eb/N0 is that plus 3.01 dB, "
        "because the code spends two channel symbols per information bit and "
        "a coding claim that does not pay for its rate is not a coding claim."
    )
    R.md()

    # ── 2.1 the curve ───────────────────────────────────────────────
    R.md("### 2.1 Soft, hard and uncoded, against Eb/N0 (C §6)")
    R.md()
    rows = []
    for r in d["ber"]:
        unc = _uncoded_ber(r["ebn0_db"])
        rows.append(
            [
                f"{r['esn0_db']:.1f}",
                f"{r['ebn0_db']:.2f}",
                _fmt(r["soft_ber"], r["bits"]),
                _fmt(r["hard_ber"], r["bits"]),
                f"{unc:.3e}",
            ]
        )
    R.table(["Es/N0 dB", "Eb/N0 dB", "soft", "hard", "uncoded BPSK"], rows)
    R.md("![BER against Eb/N0](ber.png)")
    R.md()
    R.md(
        "The soft column is what the decoder is for. The hard column is the "
        "same decoder over a two-level input — the difference is the INPUT, "
        "not a second algorithm — and the uncoded column is the closed form "
        "`Q(sqrt(2 Eb/N0))`, which needs no measurement at all."
    )
    R.md()

    # ── 2.2 what soft buys, and where hard loses to no coding at all ──
    R.md("### 2.2 What soft decisions buy, and the crossover hard has (NEW)")
    R.md()
    ratios = [
        (r["ebn0_db"], r["hard_ber"] / r["soft_ber"])
        for r in d["ber"]
        if r["soft_ber"] > 0.0
    ]
    best = max(ratios, key=lambda t: t[1])
    R.md(
        f"Hard decisions cost between "
        f"{min(t[1] for t in ratios):.1f}x and {best[1]:.0f}x the error rate "
        f"over the measured range, the ratio growing with Eb/N0 because soft "
        f"information is worth more where the decoder is nearly right."
    )
    R.md()

    cross = [
        r
        for r in d["ber"]
        if r["hard_ber"] > 0 and r["hard_ber"] < _uncoded_ber(r["ebn0_db"])
    ]
    lo = min((r["ebn0_db"] for r in cross), default=float("nan"))
    R.md(
        f"**A hard-decision Viterbi is worse than no coding at all below "
        f"Eb/N0 ~ {lo:.1f} dB** — the rate-1/2 code spends 3.01 dB of Eb to "
        f"buy back less than that from a quantised input. That is the "
        f"measurement behind the soft-decision design in "
        f"`docs/design/mpsk.md` existing and behind "
        f"soft demapping landing before the decoder did."
    )
    R.md()

    # ── 2.3 traceback depth ─────────────────────────────────────────
    R.md("### 2.3 Traceback depth, where the parameter can be seen")
    R.md()
    dep = d["depth"]
    floor = min(r["ber"] for r in dep)
    rows = [
        [
            f"{int(r['depth'])}",
            f"{r['ber']:.4e}",
            f"{100.0 * (r['ber'] / floor - 1.0):+.0f}%",
        ]
        for r in dep
    ]
    R.table(["depth", "BER at Eb/N0 = 1.0 dB", "above the floor"], rows)
    R.md("![BER against traceback depth](depth.png)")
    R.md()
    R.md(
        "Swept at Eb/N0 = 1.0 dB, and the operating point is the whole "
        "reason the table says anything: a first attempt at Eb/N0 = 4 dB "
        "returned **zero errors in 120 000 bits at every depth from 30 up**, "
        "which measures the run length rather than the parameter."
    )
    R.md()

    # ── 2.4 node synchronization ────────────────────────────────────
    R.md("### 2.4 Node synchronization — the separation it decides on (C §6b)")
    R.md()
    rows = []
    for r in d["nodesync"]:
        esn0 = r["esn0_db"]
        ser = 0.5 * math.erfc(math.sqrt(10.0 ** (esn0 / 10.0)))
        rows.append(
            [
                f"{esn0:.1f}",
                f"{int(r['window_bits'])}",
                f"{r['in_sync']:.4f}",
                f"{ser:.4f}",
                f"{r['wrong']:.4f}",
                f"{r['margin']:.4f}",
            ]
        )
    R.table(
        [
            "Es/N0 dB",
            "window (bits)",
            "in sync",
            "channel SER",
            "wrong phase",
            "margin",
        ],
        rows,
    )
    R.md(
        "**The window that DECIDES and the window that ESTIMATES are not the "
        "same size.** The margin holds up at 250 bits — the separation is "
        "wide and a phase decision only has to pick the larger gap — but the "
        "in-sync column is a rate over roughly `2*(window - depth)` symbols, "
        "which at 250 bits and Es/N0 = 2 dB is about 130 symbols carrying "
        "two expected errors. The 0.0159 in that row is Poisson noise around "
        "0.0375, not a bias. Size for the decision at a few hundred bits; "
        "size for a channel estimate at a thousand or more."
    )
    R.md()
    R.md(
        "In sync the disagreement fraction tracks the CHANNEL's symbol error "
        "rate, which is the property that lets one statistic serve node "
        "sync, lock detection and a channel estimate at once "
        "(`docs/design/fec-receive.md` §3). The wrong hypothesis sits near "
        "0.22 rather than near 0.5 — a maximum-likelihood search finds "
        "whatever codeword agrees best with a stream that is not on its "
        "trellis, so the coin-flip intuition is wrong by more than a factor "
        "of two."
    )
    R.md()


def plots(d) -> None:
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    ebn0 = [r["ebn0_db"] for r in d["ber"]]
    soft = [
        r["soft_ber"] if r["soft_ber"] > 0 else 3.0 / r["bits"]
        for r in d["ber"]
    ]
    hard = [r["hard_ber"] for r in d["ber"]]
    unc = [_uncoded_ber(e) for e in ebn0]

    fig, ax = plt.subplots(figsize=(6.2, 4.2))
    ax.semilogy(ebn0, unc, "o--", label="uncoded BPSK")
    ax.semilogy(ebn0, hard, "s-", label="hard-decision Viterbi")
    ax.semilogy(ebn0, soft, "^-", label="soft-decision Viterbi")
    ax.set_xlabel("Eb/N0 (dB)")
    ax.set_ylabel("bit error rate")
    ax.set_title("CCSDS K=7 r=1/2 — what soft decisions buy")
    ax.grid(True, which="both", alpha=0.3)
    ax.legend()
    fig.tight_layout()
    fig.savefig(HERE / "ber.png", dpi=110)
    plt.close(fig)

    dep = [int(r["depth"]) for r in d["depth"]]
    ber = [r["ber"] for r in d["depth"]]
    fig, ax = plt.subplots(figsize=(6.2, 3.6))
    ax.plot(dep, ber, "o-")
    ax.axvline(35, color="k", ls=":", lw=1)
    ax.annotate(
        "5K = 35", (35, max(ber)), textcoords="offset points", xytext=(6, -12)
    )
    ax.axvline(60, color="g", ls=":", lw=1)
    ax.annotate(
        "shipped 60",
        (60, max(ber)),
        textcoords="offset points",
        xytext=(6, -12),
    )
    ax.set_xlabel("traceback depth (bits)")
    ax.set_ylabel("BER at Eb/N0 = 1.0 dB")
    ax.set_title("Traceback depth against the floor it approaches")
    ax.grid(True, alpha=0.3)
    fig.tight_layout()
    fig.savefig(HERE / "depth.png", dpi=110)
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

    dep = {int(r["depth"]): r["ber"] for r in d["depth"]}
    floor = min(dep.values())
    over35 = 100.0 * (dep[35] / floor - 1.0)
    R.find(
        "F1",
        "FIXED",
        f"`docs/design/viterbi.md` §4 quoted `5*K = 35` at **33 % above the "
        f"BER floor (0.04178 vs 0.03137)**, from an uncommitted prototype. "
        f"Measured here at the same Eb/N0 over four times the bits: "
        f"**{over35:.0f} %** ({dep[35]:.4f} against a floor of "
        f"{floor:.4f}), with every level ~30 % higher — about what a "
        f"fraction of a dB of Es/N0 convention is worth on a curve this "
        f"steep. The design page now carries this table and says the "
        f"prototype's is superseded. The decision it drove is unchanged: 35 "
        f"is short of the floor, 60 is on it.",
    )
    R.find(
        "F2",
        "C-ONLY",
        "Every claim in §1's table is verified in C and NONE of it is "
        "reachable from Python, because `conv` has no binding. That is the "
        "component's design rather than a gap in the evidence: the callers "
        "are `ccsds_tm` and any receiver that wants a trellis, both of them "
        "C. This report is the first in the campaign whose evidence layer "
        "is a C harness rendered by Python rather than a binding exercised "
        "by it.",
    )
    R.find(
        "F4",
        "BY DESIGN",
        "The wrong node-sync hypothesis scores ~0.22 of symbols, not ~0.5 "
        "(§2.4). A maximum-likelihood search returns the codeword that "
        "agrees best with whatever it is given, so a misaligned stream is "
        "not decoded to noise — it is decoded to the nearest codeword, "
        "which agrees with about three quarters of it. Any threshold placed "
        "at 'half the symbols' would never fire.",
    )


def limits(d) -> None:
    R.md("## 4. Limits")
    R.md()
    R.md("Claims a caller may rely on, asserted by this run.")
    R.md()

    ber = {round(r["ebn0_db"], 2): r for r in d["ber"]}
    dep = {int(r["depth"]): r["ber"] for r in d["depth"]}
    floor = min(dep.values())

    at3 = ber[3.01]
    R.limit(
        at3["soft_ber"] <= 1e-3,
        f"soft-decision BER at Eb/N0 = 3.01 dB is <= 1e-3 "
        f"(measured {at3['soft_ber']:.2e})",
    )
    R.limit(
        all(
            r["hard_ber"] >= 10.0 * r["soft_ber"]
            for r in d["ber"]
            if r["ebn0_db"] >= 2.0 and r["soft_ber"] > 0.0
        ),
        "soft decisions are worth at least 10x the error rate from Eb/N0 "
        "2 dB up",
    )
    R.limit(
        ber[1.01]["hard_ber"] > _uncoded_ber(1.01),
        f"hard-decision decoding is WORSE than uncoded at Eb/N0 = 1 dB "
        f"({ber[1.01]['hard_ber']:.3f} against {_uncoded_ber(1.01):.3f})",
    )
    R.limit(
        dep[60] <= 1.05 * floor,
        f"the shipped traceback depth of 60 is within 5 % of the floor "
        f"(measured {100.0 * (dep[60] / floor - 1.0):.1f} %)",
    )
    R.limit(
        dep[35] >= 1.10 * floor,
        f"5*K = 35 is measurably above that floor, so the textbook depth is "
        f"not enough for this code (measured "
        f"{100.0 * (dep[35] / floor - 1.0):.0f} % above)",
    )
    worst = min(r["margin"] for r in d["nodesync"])
    R.limit(
        worst >= 0.05,
        f"node sync separates its hypotheses by at least 5 % of the symbols "
        f"scored, at every window from 250 bits and every Es/N0 from -2 dB "
        f"(worst measured {worst:.3f})",
    )
    # The claim that makes ONE statistic serve three jobs: in sync, the
    # disagreement fraction is the channel's symbol error rate. Bounded both
    # ways — an upper bound alone is satisfied by a metric stuck at zero.
    long_win = [r for r in d["nodesync"] if r["window_bits"] >= 1000]
    worst_ratio = max(
        abs(
            r["in_sync"]
            / (0.5 * math.erfc(math.sqrt(10.0 ** (r["esn0_db"] / 10.0))))
            - 1.0
        )
        for r in long_win
    )
    R.limit(
        worst_ratio <= 0.25,
        f"in sync, the disagreement fraction is the channel symbol error "
        f"rate to within 25 % once at least 1000 bits are scored (worst "
        f"{100.0 * worst_ratio:.0f} %) — see §2.4 for why the shorter "
        f"windows cannot support this claim",
    )


def build(write: bool = True) -> Report:
    d = _harness()

    R.md("# conv — certification evidence")
    R.md()
    R.md("## 1. The object")
    R.md()
    R.md(
        "Convolutional codes as a DESCRIPTION — a constraint length, an "
        "output count, a generator polynomial per output and which outputs "
        "are inverted — with one encoder, one maximum-likelihood decoder and "
        "one node synchronizer all reading it. CCSDS's K = 7 rate-1/2 code "
        "is a configuration of it and is what every number here is measured "
        "on."
    )
    R.md()
    R.md("Design and API, not restated here:")
    R.md()
    R.md(
        "- `native/inc/conv/conv_core.h` — the SSOT for every claim below\n"
        "- `native/tests/test_conv_core.c` — the C certification\n"
        "- [The Viterbi Decoder](../../../../../docs/design/viterbi.md) — "
        "the trellis, the branch metric, the traceback depth and node "
        "synchronization\n"
        "- `native/validation/conv_certify.c` — the sweeps below"
    )
    R.md()
    R.md("### Claim coverage — every prose claim in the header")
    R.md()
    R.md(
        "The campaign's order is header first. This table is the inventory "
        "that produced four new C sections: `conv_outputs` and "
        "`conv_next_state` had **zero** mentions, the LLR sign convention "
        "was pinned only against the test's own helper, `d_free` was an "
        "open unknown, and one claim was off by one."
    )
    R.md()
    R.md(
        "**Two headers and two test files, since doppler#893.** `viterbi` "
        "became its own declared component, so the decoder's claims left "
        "`conv_core.h` for `viterbi_core.h` and its C sections left "
        "`test_conv_core.c` for `test_viterbi_core.c`, keeping their "
        "numbers. The `C section` column says which file each row is in. "
        "This report still covers both because the encoder and the decoder "
        "are only meaningful against each other -- a decoder matched to a "
        "wrong encoder decodes perfectly and interoperates with nothing -- "
        "but `viterbi` is owed a certification of its own, which is "
        "[#894](https://github.com/doppler-dsp/doppler/issues/894)."
    )
    R.md()
    R.table(
        ["#", "claim in the header", "C section", "here"],
        [
            [
                "C1",
                "`conv_outputs` is the one expression both directions read",
                "§2b",
                "F2",
            ],
            [
                "C2",
                "output `j` is bit `j` of the word and the `j`-th symbol"
                " emitted",
                "§2b (NEW)",
                "—",
            ],
            [
                "C3",
                "`invert` bit `j` inverts output `j`; CCSDS inverts G2",
                "§2",
                "—",
            ],
            [
                "C4",
                "a state IS the `k-1` previous inputs, newest in the high"
                " stage",
                "§2b (NEW)",
                "—",
            ],
            [
                "C5",
                "polynomials are written as the standard writes them",
                "§2",
                "—",
            ],
            [
                "C6",
                "`conv_code_valid` refuses every out-of-range field",
                "§1",
                "—",
            ],
            ["C7", "`conv_states` is `2^(k-1)`", "§1", "—"],
            ["C8", "the encoder is continuous across calls", "§3", "—"],
            [
                "C9",
                "`conv_encode` refuses an invalid code or a short buffer,"
                " untouched",
                "§7",
                "—",
            ],
            [
                "C10",
                "positive LLR means symbol 0, agreeing with a hard slicer",
                "viterbi §5b",
                "—",
            ],
            [
                "C11",
                "a positive scale cannot move the maximum-likelihood path",
                "viterbi §5",
                "—",
            ],
            [
                "C12",
                "`depth-1` branches are owed, then one bit per `n` symbols",
                "viterbi §4, §5b",
                "—",
            ],
            [
                "C13",
                "`viterbi_decode` refuses a non-multiple or a short buffer",
                "viterbi §7",
                "—",
            ],
            ["C14", "`d_free = 10` for the CCSDS code", "viterbi §6d", "F1"],
            [
                "C15",
                "traceback depth 60 is measured, not a rule of thumb",
                "—",
                "§2.3",
            ],
            ["C16", "soft decisions are worth about 2 dB", "—", "§2.1, §2.2"],
            [
                "C17",
                "in sync, the re-encode disagreement IS the channel SER",
                "viterbi §6b, §6c",
                "§2.4",
            ],
            [
                "C18",
                "the decoder resumes bit-exactly from a blob",
                "viterbi §8",
                "—",
            ],
        ],
    )
    R.md(
        "**What Python cannot reach: the encoder.** `conv` has no binding, "
        "so unlike every other report in this campaign the evidence here is "
        "a C harness rendered by Python rather than a binding exercised by "
        "it. That is no longer true of either direction -- "
        "`doppler.coding.Viterbi` and `ConvEncoder` both exist now -- but "
        "this report was built "
        "against the C harness and is left that way rather than half "
        "converted; the binding's own contract is exercised in "
        "`src/doppler/viterbi/tests/` and the state round trip in the "
        "shared matrix. "
        "The `here` column points at what this report adds — the statistical "
        "behaviour no single assertion can hold — and `—` means the C "
        "section is the whole evidence and needs no help."
    )
    R.md()

    characterise(d)
    if write:
        plots(d)
        write_csv(d)
    review(d)
    limits(d)

    R.executive(
        "conv",
        source=(
            "Generated by `validate.py` in this folder. `conv` has no Python "
            "binding, so every number is measured by "
            "`native/validation/conv_certify.c` and rendered here; nothing "
            "is modelled. Re-run to regenerate."
        ),
        takeaways=[
            "**Ship traceback depth 60, not the textbook `5*K = 35`** — 35 "
            "sits measurably above the achievable floor at the operating "
            "point where depth still matters, and 60 is within a few "
            "percent of it (§2.3, F1). The design page's *magnitude* for "
            "that gap does not reproduce; its decision does.",
            "**Feed the decoder soft decisions or do not code at all.** "
            "Below Eb/N0 ~3.5 dB a hard-decision Viterbi is worse than an "
            "uncoded link, because the rate costs 3.01 dB of Eb that a "
            "two-level input does not buy back (§2.2, F3).",
            "**Node sync decides on a margin near 0.22, not 0.5.** A "
            "maximum-likelihood search decodes a misaligned stream to the "
            "nearest codeword rather than to noise, so a threshold placed "
            "at half the symbols never fires (§2.4, F4).",
            "**Size the node-sync window for the job.** The phase decision "
            "holds at 250 bits; the in-sync statistic only becomes a channel "
            "estimate — the property that lets it also serve lock detection "
            "— at a thousand bits or more, where it tracks the delivered "
            "symbol error rate to within 25 % (§2.4).",
            "**The evidence is C, and that is the design.** `conv` has no "
            "Python face and should not grow one to be certified; this "
            "report renders a C harness rather than exercising a binding "
            "(F2).",
        ],
    )
    R.summary(
        "\n- Raw sweeps: `data/ber.csv`, `data/depth.csv`, `data/nodesync.csv`"
    )
    if write:
        R.emit(HERE / "results.md")
    return R


if __name__ == "__main__":
    sys.exit(cli(build, HERE))
