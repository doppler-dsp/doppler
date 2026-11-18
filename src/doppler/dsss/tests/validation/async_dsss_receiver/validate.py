"""Certify `async_dsss_receiver` -- one continuous asynchronous DSSS receiver.

Run:  make validate-py VALIDATORS=<this file>   (this report)
      make validate-async-receiver-sweep        (re-run the full sweeps into
                                                 data/, then this)
      make validate          (regenerates every report)
      make validate-check    (fails if the committed report is stale)

**C measures; Python renders and asserts.** Every number here is one of three
C harnesses': `async_dsss_receiver_lock.c` (symbol lock through a dropout,
at the 415.1-B-1 point, `native/tests/dp_s415_test.h`),
`async_dsss_receiver_ber.c` (the error rate against the BPSK bound) and
`async_dsss_receiver_loss.c` (where that loss goes, stage by stage); the
last two still run at the retired 3.069 Mcps point
(`native/validation/async_dsss_receiver_common.h`) until each moves. All
three drive the shipped continuous-DSSS synth (`wfm_synth`) with the
harness's own data bits, the shipped `awgn` sized from C/N0, the receiver
in the specification configuration. This file
parses their CSV blocks, characterises, reviews and asserts the limits
through the same `Report` every other certified object uses -- the
`conv` shape.

**Two runs, two roles.** The limits are asserted on each harness's
regression subset, `--check --emit`, re-run on every push (about 3 s and
8 s), so the evidence and the gate are one run and cannot disagree. The
full sweeps are committed and rendered: `data/sweep.txt` -- twenty
settled dropout draws a carrier at 10 and 4 dB -- in section 2.5,
`data/ber_sweep.txt` -- the error rate at 5 to 8 dB, 200 errors a point --
in section 2.6, and
`data/loss_sweep.txt` -- the loss by stage at 5 and 8 dB -- in section 2.7.
The loss harness asserts its own gates in ctest (`--check`); this report
renders its sweep and asserts nothing on it. The findings and the executive
summary quote them, computed from those files, never typed.
`make validate-async-receiver-sweep` refreshes all three on purpose.

**Scope: BPSK.** The receiver accepts m = 2, 4 or 8; the owner scoped this
certification to BPSK (2026-10-19), and the test library spreads BPSK only,
so m = 4 and 8 are not certified here.

**Three kinds of row.** A claim with a source (a header sentence, a bound,
a specification row) is asserted. A claim with a source the receiver does
not meet is a gap, with its issue. A measurement whose requirement nobody
has set yet is printed with its interval, citing the decision it waits on
(doppler#3157), and not asserted: no threshold is fitted to the run it would
gate.
"""

from __future__ import annotations

import csv
import statistics
import subprocess
import sys
from pathlib import Path
from typing import Union

from doppler.tests._repo import build_dir, exe, repo_root
from doppler.tests._validation_common import Report, cli

HERE = Path(__file__).resolve().parent
DATA = HERE / "data"
SWEEP = DATA / "sweep.txt"
BER_SWEEP = DATA / "ber_sweep.txt"
LOSS_SWEEP = DATA / "loss_sweep.txt"
ROOT = repo_root(__file__)
HARNESS = exe(
    build_dir(__file__) / "native/validation/validate_async_dsss_receiver_lock"
)
BER_HARNESS = exe(
    build_dir(__file__) / "native/validation/validate_async_dsss_receiver_ber"
)
LOSS_HARNESS = exe(
    build_dir(__file__) / "native/validation/validate_async_dsss_receiver_loss"
)

R = Report()

#: Spelled with Union, not `|`: a module-level alias is evaluated at import,
#: and the floor Python (3.9) has no `|` between types.
Rows = list[dict[str, Union[float, str]]]
Blocks = dict[str, Rows]

#: Columns kept as text: the dropout's per-length hold profile is a bit
#: string, which float() would read as a 60-digit number.
_TEXT = {"held"}


def _run(args: list[str], harness: Path = HARNESS) -> str:
    """Run a harness; its stdout, or a hard failure that says why.

    A missing binary, or a run whose own assertions failed, is a failure
    rather than a skip: this report has no measurement of its own, and a
    skipped or half-run one is indistinguishable from a passing one in a
    log. `make build` builds both.
    """
    if not harness.exists():
        raise SystemExit(
            f"async_dsss_receiver: {harness.relative_to(ROOT)} is not built "
            f"-- run `make build` first. Every number in this report is the "
            f"C harnesses'."
        )
    run = subprocess.run(
        [str(harness), *args], capture_output=True, text=True, check=False
    )
    if run.returncode != 0:
        raise SystemExit(
            f"async_dsss_receiver: `{harness.name} {' '.join(args)}` failed "
            f"(exit {run.returncode}):\n{run.stderr[-2000:]}"
        )
    return run.stdout


def _blocks(
    out: str,
    source: str,
    names: tuple[str, ...] = ("dropout",),
) -> Blocks:
    blocks = _parse(out)
    for name in names:
        if not blocks.get(name):
            raise SystemExit(
                f"async_dsss_receiver: {source} has no `# {name}` block"
            )
    return blocks


#: The error-rate harness's blocks: one row an Es/N0, one row a burst.
BER_BLOCKS = ("ber", "burst")


def _num(k: str, v: str) -> float | str:
    if k in _TEXT:
        return v
    try:
        return float(v)
    except ValueError:
        return v


def _parse(out: str) -> Blocks:
    """One run's stdout into its `# <name>` CSV blocks."""
    blocks: Blocks = {}
    name, header = "", []
    for line in out.splitlines():
        line = line.strip()
        if line.startswith("# "):
            name, header = line[2:].split()[0], []
            blocks[name] = []
        elif not line:
            name = ""
        elif not name:
            continue
        elif not header:
            header = line.split(",")
        else:
            blocks[name].append(
                {k: _num(k, v) for k, v in zip(header, line.split(","))}
            )
    return blocks


def _sweep() -> Blocks:
    if not SWEEP.exists():
        raise SystemExit(
            "async_dsss_receiver: data/sweep.txt is missing -- run this "
            "validator with --sweep"
        )
    return _blocks(SWEEP.read_text(encoding="utf-8"), "data/sweep.txt")


def _ber_sweep() -> Blocks:
    if not BER_SWEEP.exists():
        raise SystemExit(
            "async_dsss_receiver: data/ber_sweep.txt is missing -- run this "
            "validator with --sweep"
        )
    return _blocks(
        BER_SWEEP.read_text(encoding="utf-8"),
        "data/ber_sweep.txt",
        BER_BLOCKS,
    )


def _loss_sweep() -> Blocks:
    if not LOSS_SWEEP.exists():
        raise SystemExit(
            "async_dsss_receiver: data/loss_sweep.txt is missing -- run this "
            "validator with --sweep"
        )
    return _blocks(
        LOSS_SWEEP.read_text(encoding="utf-8"),
        "data/loss_sweep.txt",
        ("loss",),
    )


#: The loss harness's stages, in the order they compose, and what each adds.
LOSS_STAGES = (
    ("S0", "the ideal despread: exact carrier, code and symbol edges"),
    ("S1", "ideal sums over the receiver's K partials a code period"),
    ("S1d", "the shipped Dll at K, held at truth"),
    ("S2", "+ the shipped RateConverter, at oracle timing"),
    ("S3", "+ the shipped MpskReceiver, the receiver's parameters"),
    ("S4", "the whole receiver: its search, refine and hand-over"),
)


def section_loss(lsw: Blocks) -> None:
    rows = lsw["loss"]
    k_rx = int(next(r["k"] for r in rows if r["stage"] == "S1d"))
    es_pts = sorted({float(r["esn0_db"]) for r in rows})

    def at(es: float, stage: str, k: int | None = None) -> dict:
        for r in rows:
            if (
                float(r["esn0_db"]) == es
                and r["stage"] == stage
                and (k is None or int(r["k"]) == k)
            ):
                return r
        raise SystemExit(f"async_dsss_receiver: no {stage} row at {es} dB")

    R.md("### 2.7 Where the loss goes (`data/loss_sweep.txt`)")
    R.md()
    R.md(
        "`native/validation/async_dsss_receiver_loss.c`, on the stimulus "
        "above at known truth (the synth's carrier, its chips from sample 0, "
        "its symbol edges): more and more of the shipped chain, each stage "
        "scored as a BER-equivalent loss, the Es/N0 the bound needs for the "
        "stage's rate subtracted from the Es/N0 applied. Every stage but S4 "
        "is scored on the same symbols and noise, so a step between stages "
        "carries less counting noise than either loss. S4 is the whole "
        "receiver on the same bursts, scored over its own settled window, "
        "like section 2.6."
    )
    R.md()
    head = ["stage"]
    for es in es_pts:
        head += [f"loss at {es:.0f} dB", "step"]
    body = []
    for stage, what in LOSS_STAGES:
        line = [f"{stage}: {what}"]
        for es in es_pts:
            r = at(es, stage, k_rx if stage == "S1" else None)
            prev = {
                "S0": None,
                "S1": "S0",
                "S1d": "S1",
                "S2": "S1d",
                "S3": "S2",
                "S4": "S3",
            }[stage]
            loss = float(r["loss_db"])
            if prev is None:
                step = ""
            else:
                p = at(es, prev, k_rx if prev == "S1" else None)
                step = f"{loss - float(p['loss_db']):+.2f} dB"
            n_bad = int(r["unsettled"])
            line += [
                f"{loss:.2f} dB ({int(r['errors']):,} errors)"
                + (f", {n_bad} burst(s) not settled" if n_bad else ""),
                step,
            ]
        body.append(line)
    R.table(head, body)
    R.md()
    ks = sorted({int(r["k"]) for r in rows if r["stage"] == "S1"})
    R.md(
        "The cut alone, against K: ideal sums at known timing, the same "
        "bursts. A partial that straddles a symbol edge carries both symbols, "
        f"and the receiver runs at K = {k_rx}."
    )
    R.md()
    R.table(
        ["K a code period"] + [f"S1 loss at {es:.0f} dB" for es in es_pts],
        [
            [f"{k}" + (" (the receiver)" if k == k_rx else "")]
            + [f"{float(at(es, 'S1', k)['loss_db']):.2f} dB" for es in es_pts]
            for k in ks
        ],
    )
    R.md()


def _write_csv(d: Blocks) -> None:
    DATA.mkdir(exist_ok=True)
    for name, rows in d.items():
        if not rows:
            continue
        with (DATA / f"{name}.csv").open("w", newline="") as fh:
            w = csv.DictWriter(fh, fieldnames=list(rows[0]))
            w.writeheader()
            w.writerows(rows)


def _plot_ber(rows: Rows) -> None:
    """The committed sweep's error rate, with its 99% interval, against the
    BPSK bound -- the bound drawn by the library's own `ber_theory_ber`."""
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    import numpy as np

    from doppler.ber import ber_theory_ber

    es = np.array([float(r["esn0_db"]) for r in rows])
    p = np.array([float(r["ber"]) for r in rows])
    lo = np.array([float(r["ber_lo"]) for r in rows])
    hi = np.array([float(r["ber_hi"]) for r in rows])
    grid = np.linspace(es.min() - 0.5, es.max() + 0.5, 81)
    fig, ax = plt.subplots(figsize=(6.0, 4.0))
    ax.semilogy(
        grid,
        [ber_theory_ber(2, 10.0 ** (g / 10.0)) for g in grid],
        "k--",
        lw=1,
        label="BPSK bound, Q(sqrt(2 Es/N0))",
    )
    ax.errorbar(
        es,
        p,
        yerr=[p - lo, hi - p],
        fmt="o-",
        capsize=3,
        label="AsyncDsssReceiver, 99% interval",
    )
    ax.set_xlabel("Es/N0 (dB)")
    ax.set_ylabel("bit error rate")
    ax.set_title(
        "BPSK error rate against the bound, specification operating point"
    )
    ax.grid(alpha=0.3, which="both")
    ax.legend()
    fig.tight_layout()
    fig.savefig(HERE / "ber.png", dpi=110)
    plt.close(fig)


# -- summaries of the committed sweep, the numbers the prose quotes ------


def _es(rows: Rows) -> list[float]:
    return sorted({float(r["esn0_db"]) for r in rows}, reverse=True)


def _at(rows: Rows, es: float) -> Rows:
    return [r for r in rows if float(r["esn0_db"]) == es]


def _span(xs: list[float], unit: str = "dB") -> str:
    return f"{min(xs):+.2f} to {max(xs):+.2f} {unit}"


def section_object() -> None:
    R.md("## 1. The object — one continuous asynchronous DSSS receiver")
    R.md()
    R.md(
        "The design is [`docs/design/async-dsss-receiver.md`]"
        "(../../../../../../docs/design/async-dsss-receiver.md); the "
        "contract is the header, `native/inc/doppler/async_dsss_receiver/"
        "async_dsss_receiver_core.h`. The parallel design it serves is "
        "[`docs/design/async-dsss-receiver.md`]"
        "(../../../../../../docs/design/async-dsss-receiver.md)."
    )
    R.md()
    R.md(
        "**Operating point: the 415.1-B-1 point** "
        "(`native/tests/dp_s415_test.h`, the design page's "
        "specification): a 1,023-chip Gold code at F_fwd x 31/(221 x 96) "
        "chips a second over 2.2 to 2.8 GHz, 3,000 sym/s with a 275-symbol "
        "code-only window opening every 1,000-symbol frame, 11 MSa/s, "
        "+/-1,500 Hz, a 4 dB Es/N0 floor; the receiver as built, its design "
        "C/N0 the true one. The dropout (section 2.3) runs there. The "
        "error-rate and loss harnesses (sections 2.6 and 2.7) still run at "
        "the retired 3.069 Mcps point, two samples a chip and 2,700 sym/s, "
        "until each moves. **BPSK only**: m = 4 and 8 are accepted by the "
        "object and not certified here (the owner's scope, 2026-10-19)."
    )
    R.md()
    R.md(
        "The header's lock and lifecycle claims, each pinned in "
        "`native/tests/async_dsss_receiver/test_async_dsss_receiver_core.c` "
        "and each seen to "
        "fail under a sabotage of the code it guards, the order "
        "`docs/dev/contributing/validation.md` requires:"
    )
    R.md()
    R.table(
        ["claim (header)", "C test"],
        [
            [
                "symbol lock: an EMA of cos 2phi over 30 symbols; 30 above "
                "0.5 declare, 15 below 0.3 drop",
                "`_test_symbol_lock_detector_symbol_by_symbol`",
            ],
            [
                "both flags down for longer than `lost_confirm_s` -> lost; "
                "not a sample early, not a call late",
                "`_test_lost_after_switch_off`",
            ],
            [
                "the plain clock restarts when a flag comes back",
                "`_test_plain_clock_restarts_on_a_flag`",
            ],
            [
                "one flag down is a degrade, not a release",
                "`_test_one_flag_down_is_a_degrade`",
            ],
            [
                "a hand-off that never locks is released, on time",
                "`_test_a_handoff_that_never_locks_is_released`",
            ],
            [
                "`reacquire` releases on symbol lock alone, back to searching",
                "`_test_reacquire_is_symbol_lock_alone`, "
                "`_test_reacquire_after_switch_off`",
            ],
            [
                "a refined Doppler at or past fs/2 is refused at the "
                "hand-over, and not left readable",
                "`_test_handover_holds_the_refined_doppler_to_the_domain`",
            ],
            [
                "steps() never writes past `max_out`; the excess is "
                "dropped, not deferred",
                "`_test_output_capacity`",
            ],
            [
                "decodes at the Es/N0 floor (the 3.069 Mcps point's claim; "
                "`_test_awgn_esn0_floor` is red under true noise, the "
                "design page's open findings)",
                "`_test_awgn_esn0_floor`, "
                "`_test_spec_combined_scenario_at_spec_floor`",
            ],
            [
                "`differential` acts in a demap this object does not offer: "
                "steps()' symbols are the same either way (`@param "
                "differential`, #2471)",
                "`_test_differential_does_not_change_the_symbols`",
            ],
        ],
    )
    R.md()


def characterise(d: Blocks, sw: Blocks, bsw: Blocks) -> None:
    R.md("## 2. Characterisation")
    R.md()
    R.md(
        "Section 2.3 is the `--check` run; 2.5 to 2.7 are the committed "
        "full sweeps. 2.1, 2.2 and 2.4 say where the 3.069 Mcps rows went."
    )
    R.md()

    R.md("### 2.1 Arrival: measured at the use case's cells")
    R.md()
    R.md(
        "How soon a new signal is acquired is measured where the use case "
        "is: the pull-in validator's cells "
        "(`native/validation/async_dsss_receiver_pullin.c`), at the "
        "415.1-B-1 point with the ruled dynamics. 415.1-B-1 section 7.4.1's "
        "acquisition requirement -- PN lock, the receiver's code lock, at "
        "probability 0.9 within 20 s -- is asserted there in every cell of "
        "the receiver as built. This harness's 3.069 Mcps arrival rows "
        "retired with that point. How soon the receiver must be DECODING "
        "after arrival is a requirement nobody has set (doppler#3157)."
    )
    R.md()

    R.md("### 2.2 Quality: retired with the 3.069 Mcps point")
    R.md()
    R.md(
        "The EVM rows, and the band they were held to, retired with the "
        "3.069 Mcps point: the band was two lines fitted to the full "
        "sweep's own draws, a threshold derived from the run it gated. Its "
        "successor, an error rate within an allowed loss of the BPSK bound "
        "at the floor, waits on that loss (doppler#3157); the error rate "
        "itself is section 2.6."
    )
    R.md()

    R.md("### 2.3 Dropout: symbol lock when the signal goes (415.1-B-1)")
    R.md()
    R.md(
        "A draw settled in tracking with symbol lock held at the 415.1-B-1 "
        "point, then cut at a random sample, mid-data (the symbols either "
        "side are data, none in a frame's code-only window); the noise runs "
        "on. `held` is the longest dropout lock rode out; the re-lock times "
        "are from the signal's return. **Printed, not asserted**: how soon a "
        "receiver must drop symbol lock once its signal is gone has no "
        "specification row, and waits on the owner (doppler#3157)."
    )
    R.md()
    R.table(
        [
            "carrier",
            "Es/N0",
            "lock falls, gone for good",
            "longest dropout held",
            "back after 50 ms out",
            "back after 200 ms out",
        ],
        [
            [
                f"{r['fc_ghz']:.1f} GHz",
                f"{r['esn0_db']:.0f} dB",
                f"{r['drop_ms']:.1f} ms",
                f"{r['hold_ms']:.1f} ms",
                f"{r['back_50_ms']:.1f} ms",
                f"{r['back_200_ms']:.1f} ms",
            ]
            for r in d["dropout"]
            if r["settled"]
        ],
    )
    R.md()

    R.md("### 2.4 False lock: waits on the lock budget")
    R.md()
    R.md(
        "The 3.069 Mcps false-lock rows retired with that point. Their "
        "successor -- on noise alone, no lock flag declares within the "
        "receiver's lock budget -- needs the budget, which waits on the "
        "owner (doppler#3143, doppler#3157)."
    )
    R.md()

    R.md("### 2.5 The full sweep (`data/sweep.txt`)")
    R.md()
    R.md(
        "The dropout over more draws, committed and rendered, never re-run "
        "on a push: up to twenty settled draws a carrier and Es/N0, at 10 "
        "and 4 dB, from at most forty tried. Printed, not asserted "
        "(doppler#3157)."
    )
    R.md()
    rows = []
    for fc in sorted({float(r["fc_ghz"]) for r in sw["dropout"]}):
        for es in _es(sw["dropout"]):
            dr = [
                r for r in _at(sw["dropout"], es) if float(r["fc_ghz"]) == fc
            ]
            ok = [r for r in dr if r["settled"]]
            drops = [float(r["drop_ms"]) for r in ok if r["drop_ms"] >= 0]
            rows.append(
                [
                    f"{fc:.1f} GHz",
                    f"{es:.0f} dB",
                    f"{len(ok)} of {len(dr)}",
                    (
                        f"{statistics.median(drops):.1f} / {max(drops):.1f} ms"
                        if drops
                        else "--"
                    ),
                    f"{sum(1 for r in ok if r['drop_ms'] < 0)}",
                ]
            )
    R.table(
        [
            "carrier",
            "Es/N0",
            "draws settled",
            "lock falls after a loss, median / worst",
            "held through the watch",
        ],
        rows,
    )
    R.md()

    ber = bsw["ber"]
    bursts = bsw["burst"]
    conf = 100.0 * float(ber[0]["conf"])
    R.md("### 2.6 Error rate against the BPSK bound (`data/ber_sweep.txt`)")
    R.md()
    R.md(
        "`native/validation/async_dsss_receiver_ber.c`, on the stimulus "
        "above: bursts of 6 s from the signal's first sample, the carrier "
        "uniform over +/-1/T_E, the harness's own i.i.d. bits. Every symbol "
        "the receiver emits is kept. The measurement is "
        "`native/tests/dp_ber_test.h`'s. The window starts at the later of "
        "the analytic budget for the demodulator's two loops, read from the "
        "receiver, and its symbol lock sustained. The lag and the BPSK phase "
        "are detected against a marker of truth past that, and a second "
        "detection at the burst's end must agree (a burst that slipped is "
        "counted, never folded in). Bursts run until 200 errors (inverse "
        f"binomial sampling), so the {conf:.0f}% interval is exact. The "
        "**loss** is the Es/N0 the bound needs for the measured rate, "
        "subtracted from the Es/N0 applied, **over the bursts that settled**: "
        "a burst with no settled window is not scored, and is held to a rule "
        "of its own (below)."
    )
    R.md()
    R.table(
        [
            "Es/N0",
            "BER",
            f"{conf:.0f}% interval",
            "bound",
            f"loss ({conf:.0f}% interval)",
            "bursts settled",
            "symbols",
            "EVM loss",
            "M2M4",
        ],
        [
            [
                f"{float(r['esn0_db']):.0f} dB",
                f"{float(r['ber']):.2e}",
                f"{float(r['ber_lo']):.2e} to {float(r['ber_hi']):.2e}",
                f"{float(r['theory']):.2e}",
                f"{float(r['loss_db']):.2f} dB "
                f"({float(r['loss_lo_db']):.2f} to "
                f"{float(r['loss_hi_db']):.2f})",
                f"{int(r['bursts']) - int(r['unsettled'])} of "
                f"{int(r['bursts'])}",
                f"{int(r['symbols']):,}",
                f"{float(r['evm_db']) - float(r['evm_ideal_db']):+.2f} dB",
                f"{float(r['m2m4_db']):.2f} dB",
            ]
            for r in ber
        ],
    )
    R.md()
    R.md("![BER against the bound](ber.png)")
    R.md()
    n_unset = sum(1 for r in bursts if not r["settled"])
    # A burst that never tracked has no hand-over (NaN): the harness's floor
    # fails it, so none reaches this list.
    unset = [
        abs(float(r["handover_err_hz"]))
        for r in bursts
        if not r["settled"] and r["handover_err_hz"] == r["handover_err_hz"]
    ]
    kept = [abs(float(r["handover_err_hz"])) for r in bursts if r["settled"]]
    slips = sum(int(r["slipped"]) for r in ber)
    pullin = float(ber[0]["pullin_hz"])
    past = [e for e in kept if e > pullin]
    far = [e for e in kept if unset and e >= min(unset)]
    ratchet = ", ".join(
        f"{int(r['unsettled'])} of at most {int(r['unsettled_max'])} at "
        f"{float(r['esn0_db']):.0f} dB"
        for r in ber
    )
    R.md(
        f"The loss is over {len(kept)} of the {len(bursts)} bursts: the ones "
        f"that settled. "
        + (
            "None slipped."
            if slips == 0
            else f"{slips} slipped, and the harness fails them."
        )
        + f" {n_unset} had no settled window and are not scored: symbol lock "
        f"was not sustained, or came too late in the burst to leave one. "
        f"That is {R.ref('handoff_can_miss_the_carrier')}, a known defect, so "
        "their count is a ratchet: the "
        f"committed count at the sweep's seeds plus one for another "
        f"toolchain's draws, which may only shrink ({ratchet})."
        + (
            f" Each was handed over {min(unset):.0f} to {max(unset):.0f} Hz "
            f"off its carrier (the refine's carrier against the truth), but "
            f"no line in Hz separates them from the bursts that settled: "
            f"{len(past)} of the {len(kept)} that settled were handed over "
            f"past {pullin:.0f} Hz and {len(far)} past {min(unset):.0f} Hz, "
            f"up to {max(kept):.0f} Hz. Where the searching path's pull-in "
            f"ends is #2409's question. The harness keeps one floor: inside "
            f"{pullin:.0f} Hz, the pre-despread loop's reliable pull-in "
            f"(twice `ASYNC_DSSS_RX_CARRIER_PULLIN_HZ`), the hand-over cannot "
            f"be why "
            f"a burst did not settle, and none did "
            f"({sum(int(r['unsettled_inside']) for r in ber)})."
            if unset
            else ""
        )
        + " The EVM is `dp_ber_report()`'s cross-check over the same windows, "
        "and its loss is over that meter's ideal (`dp_ber_evm_ideal_db()`). "
        "The blind M2M4 reads "
        + _span([float(r["m2m4_db"]) - float(r["esn0_db"]) for r in ber])
        + f" from the applied Es/N0 ({R.ref('evm_agrees_with_error_rate')})."
    )
    R.md()


def review(sw: Blocks, bsw: Blocks) -> None:
    R.md("## 3. Review — findings, with verdicts")
    R.md()
    R.find(
        "steps_max_out_not_a_bound",
        "GAP",
        "`steps_max_out()` returns 0, which is not a bound: a call that "
        "completes a carried code period emits that whole period's symbols, "
        "so at more than one symbol per period an `out` sized to the call's "
        "`x_len` drops symbols silently. At the specification's operating "
        "point (0.9 symbols a period) it never binds. Tracked as #2469.",
    )
    R.find(
        "differential_is_inert",
        "GAP",
        "`differential` is inert here: it reaches MpskReceiver's `bits()` "
        "demap, and this object returns soft symbols only "
        "(`_test_differential_does_not_change_the_symbols`). Remove it or "
        "add `bits()`: #2471.",
    )
    unset = sum(1 for r in bsw["burst"] if not r["settled"])
    R.find(
        "handoff_can_miss_the_carrier",
        "CONFIRMED",
        f"A first hand-off can land off the carrier and never pull in. With "
        f"`reacquire` off (the binding's default, and the error-rate "
        f"harness's), {unset} of the {len(bsw['burst'])} bursts of the "
        f"3.069 Mcps error-rate sweep never settled (section 2.6): the "
        f"refine handed over outside the carrier loops' pull-in, code lock "
        f"came up and symbol lock never did. `reacquire` recovers a missed "
        f"hand-off by searching again. The hand-off accuracy is #2409; "
        f"`reacquire` as the default is #3092.",
    )
    R.find(
        "lock_and_release_c_only",
        "C-ONLY",
        "The lock detector's hysteresis, the release clock's edges and the "
        "hand-over domain are certified symbol by symbol and sample by "
        "sample in the C tests of section 1; the binding exposes only the "
        "flags, which section 2 reads.",
    )
    ber = bsw["ber"]
    loss = [float(r["loss_db"]) for r in ber]
    gap = [float(r["m2m4_db"]) - float(r["esn0_db"]) for r in ber]
    evm = [float(r["evm_db"]) - float(r["evm_ideal_db"]) for r in ber]
    agree = float(ber[0]["m2m4_agree_db"])
    bursts = bsw["burst"]
    n_set = sum(1 for r in bursts if r["settled"])
    R.find(
        "evm_agrees_with_error_rate",
        "FIXED",
        f"Over the {n_set} of {len(bursts)} bursts that settled, the error "
        f"rate sits {min(loss):.2f} to {max(loss):.2f} dB from the BPSK "
        f"bound (section 2.6), and the EVM over the same windows reads "
        f"{min(evm):+.2f} to {max(evm):+.2f} dB over its ideal. Until "
        f"#2495 the two disagreed: the Dll divided each code period's "
        f"partials by that period's own signal-plus-noise magnitude, which "
        f"compressed the in-phase error, so the EVM read near its ideal and "
        f"the blind M2M4 2.4 to 2.8 dB above the applied Es/N0 while the "
        f"error rate sat 0.9 dB from the bound. The Dll divides by a slowly "
        f"tracked level now, its error circular, and the blind M2M4 reads "
        f"{min(gap):+.2f} to {max(gap):+.2f} dB from the applied Es/N0, "
        f"inside `dp_ber_report()`'s {agree:.1f} dB "
        f"(`DP_BER_M2M4_AGREE_DB`) at every point. Where the loss goes, "
        f"stage by stage, is section 2.7: the cut into four partials a code "
        f"period, the demodulator's loops and the front end's tracking.",
    )


def limits(d: Blocks, sw: Blocks) -> None:
    R.md("## 4. Limits — claims a caller may rely on")
    R.md()
    R.md(
        "The 3.069 Mcps point's arrival, EVM, dropout and false-lock limits "
        "retired with it (the owner's ruling). Their successors: "
        "acquisition is asserted on the pull-in validator's cells "
        "(415.1-B-1 section 7.4.1); the dropout time, the allowed loss and "
        "the decoding time wait on the owner, and their rows are printed "
        "(doppler#3157); false lock waits on the lock budget (doppler#3143). "
        "The error-rate limits below still read the 3.069 Mcps harness, "
        "until it moves."
    )
    R.md()
    # The error rate: the bound is the harness's (loss_max_db), read from
    # the row it asserted, and the loss the one its 99% interval's LOWER
    # limit implies, so counting noise cannot flake the claim.
    for r in d["ber"]:
        tag = f"[{float(r['esn0_db']):.0f} dB]"
        R.limit(
            bool(r["ok"]),
            f"{tag} the error rate is a result: settled, aligned, "
            f"{int(r['errors'])} errors, and its EVM, M2M4 and theory "
            f"cross-checks agree",
        )
        R.limit(
            int(r["slipped"]) == 0,
            f"{tag} no burst's alignment moved between its start and its end",
        )
        n_b, n_u = int(r["bursts"]), int(r["unsettled"])
        R.limit(
            float(r["loss_lo_db"]) <= float(r["loss_max_db"]),
            f"{tag} over the bursts that settle ({n_b - n_u} of {n_b} here), "
            f"the error rate is within {float(r['loss_max_db']):.1f} dB of "
            f"the BPSK bound (the loss at the 99% interval's lower limit)",
        )
        R.limit(
            n_u <= int(r["unsettled_max"]),
            f"{tag} at most {int(r['unsettled_max'])} of the bursts fail to "
            f"settle ({n_u} here; {R.ref('handoff_can_miss_the_carrier')}, a "
            "ratchet that may only shrink)",
        )
        R.limit(
            int(r["unsettled_inside"]) == 0,
            f"{tag} no burst handed over inside the carrier loop's reliable "
            f"pull-in, {float(r['pullin_hz']):.0f} Hz, fails to settle",
        )


def _takeaways(sw: Blocks, bsw: Blocks) -> list[str]:
    loss = [float(r["loss_db"]) for r in bsw["ber"]]
    es = [float(r["esn0_db"]) for r in bsw["ber"]]
    bursts = bsw["burst"]
    n_set = sum(1 for r in bursts if r["settled"])
    dr = [r for r in sw["dropout"] if r["settled"]]
    drops = [float(r["drop_ms"]) for r in dr if r["drop_ms"] >= 0]
    return [
        f"**Budget about 1 dB for the error rate, once settled.** From "
        f"{min(es):.0f} to {max(es):.0f} dB the BER of the bursts that "
        f"settled ({n_set} of {len(bursts)}) sits {min(loss):.2f} to "
        f"{max(loss):.2f} dB from the BPSK bound; where it goes is "
        f"section 2.7 (2.6). "
        + (
            f"The other {len(bursts) - n_set} never settled "
            f"({R.ref('handoff_can_miss_the_carrier')})."
            if n_set < len(bursts)
            else "Every burst settled."
        ),
        f"**Turn `reacquire` on.** A first hand-off can miss the carrier "
        f"and, without it, never recover "
        f"({R.ref('handoff_can_miss_the_carrier')}, #2409, #3092).",
        f"**A signal that leaves drops symbol lock in "
        f"{statistics.median(drops):.0f} ms** at the 415.1-B-1 point "
        f"(median of {len(drops)} settled draws, worst {max(drops):.0f} ms; "
        f"2.5). Printed, not asserted: the requirement is not set "
        f"(doppler#3157).",
        "**Size `out` with care above one symbol a code period:** "
        "`steps_max_out()` is not yet a bound "
        f"({R.ref('steps_max_out_not_a_bound')}, #2469). At the "
        "specification's operating point it never binds.",
    ]


def build(write: bool = True) -> Report:
    global R
    R = Report(write=write)
    R.md("# AsyncDsssReceiver — validation report")
    R.md()
    section_object()
    d = _blocks(_run(["--check", "--emit"]), "the --check run")
    d.update(
        _blocks(
            _run(["--check", "--emit"], BER_HARNESS),
            "the error-rate --check run",
            BER_BLOCKS,
        )
    )
    sw = _sweep()
    bsw = _ber_sweep()
    lsw = _loss_sweep()
    if write:
        _write_csv(d)
        _plot_ber(bsw["ber"])
    characterise(d, sw, bsw)
    section_loss(lsw)
    review(sw, bsw)
    limits(d, sw)
    R.executive("AsyncDsssReceiver", _takeaways(sw, bsw))
    R.summary(
        "\n- Raw run: `data/dropout.csv` and `data/ramp.csv` (the lock "
        "harness), `data/ber.csv`, `data/burst.csv` and `data/spec.csv` "
        "(the error-rate harness) -- the two harnesses' `--check --emit` "
        "blocks; "
        "`data/sweep.txt`, `data/ber_sweep.txt` and `data/loss_sweep.txt` "
        "-- the three harnesses' full sweeps"
    )
    R.emit(HERE / "results.md")
    return R


if __name__ == "__main__":
    if "--sweep" in sys.argv:
        sys.argv.remove("--sweep")
        DATA.mkdir(exist_ok=True)
        SWEEP.write_text(_run(["--emit"]), encoding="utf-8")
        BER_SWEEP.write_text(_run(["--emit"], BER_HARNESS), encoding="utf-8")
        LOSS_SWEEP.write_text(_run(["--emit"], LOSS_HARNESS), encoding="utf-8")
    sys.exit(cli(build, HERE))
