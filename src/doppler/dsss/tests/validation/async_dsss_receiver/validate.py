"""Certify `async_dsss_receiver` -- one continuous asynchronous DSSS receiver.

Run:  make validate-py VALIDATORS=<this file>   (this report)
      make validate-async-receiver-sweep        (re-run the full sweeps into
                                                 data/, then this)
      make validate          (regenerates every report)
      make validate-check    (fails if the committed report is stale)

**C measures; Python renders and asserts.** Every number here is one of three
C harnesses' on one stimulus (`native/validation/
async_dsss_receiver_common.h`): `async_dsss_receiver_lock.c` (arrival,
EVM, dropout, false lock), `async_dsss_receiver_ber.c` (the error rate
against the BPSK bound) and `async_dsss_receiver_loss.c` (where that
loss goes, stage by stage). All three drive the shipped continuous-DSSS synth
(`wfm_synth`) with the harness's own data bits, the shipped `awgn` sized
from C/N0, the receiver in the specification configuration. This file
parses their CSV blocks, characterises, reviews and asserts the limits
through the same `Report` every other certified object uses -- the
`conv` shape.

**Two runs, two roles.** The limits are asserted on each harness's
regression subset, `--check --emit`, re-run on every push (about 3 s and
8 s), so the evidence and the gate are one run and cannot disagree. The
full sweeps are committed and rendered: `data/sweep.txt` -- twenty arrival
and dropout draws at 10 and 5 dB, five quality draws at each Es/N0, five
false-lock draws -- in section 2.5, and `data/ber_sweep.txt` -- the error
rate at 5 to 8 dB, 200 errors a point -- in section 2.6, and
`data/loss_sweep.txt` -- the loss by stage at 5 and 8 dB -- in section 2.7.
The loss harness asserts its own gates in ctest (`--check`); this report
renders its sweep and asserts nothing on it. The findings and the executive
summary quote them, computed from those files, never typed.
`make validate-async-receiver-sweep` refreshes all three on purpose.

**Scope: BPSK.** The receiver accepts m = 2, 4 or 8; the owner scoped this
certification to BPSK (2026-10-19), and the test library spreads BPSK only,
so m = 4 and 8 are not certified here.
"""

from __future__ import annotations

import csv
import math
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

#: This report's asserted bounds. CHECK_TRACK_MS and DROP_WATCH_MS are the
#: harness's own (it asserts them in --check and the sweep alike). The EVM
#: allowance is not restated here: the harness computes it per Es/N0 and
#: emits it with each quality row (`loss_min_db`, `loss_max_db`), and its
#: model's two lines in `# evm_model`, so the report reads the bound the
#: harness asserted (section 4).
CHECK_TRACK_MS = 100.0
DROP_WATCH_MS = 80.0

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
    names: tuple[str, ...] = (
        "arrival",
        "dropout",
        "quality",
        "falselock",
        "evm_model",
    ),
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


def _plot(q: Rows) -> None:
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    es = [float(r["esn0_db"]) for r in q]
    fig, ax = plt.subplots(figsize=(6.0, 4.0))
    ax.plot(
        es,
        [float(r["ideal_db"]) for r in q],
        "k--",
        lw=1,
        label="the meter's ideal (dp_ber_evm_ideal_db)",
    )
    ax.plot(
        es,
        [float(r["evm_db"]) for r in q],
        "o-",
        label="AsyncDsssReceiver, locked",
    )
    ax.set_xlabel("Es/N0 (dB)")
    ax.set_ylabel("EVM (dB, own hard decisions)")
    ax.set_title("BPSK EVM against Es/N0, specification operating point")
    ax.grid(alpha=0.3)
    ax.legend()
    fig.tight_layout()
    fig.savefig(HERE / "evm.png", dpi=110)
    plt.close(fig)


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


def _losses(sw: Blocks, lo: float, hi: float) -> list[float]:
    return [
        float(r["loss_db"])
        for r in sw["quality"]
        if r["settled"] and lo <= float(r["esn0_db"]) <= hi
    ]


def _quality_misses(sw: Blocks, below: float) -> tuple[int, int]:
    rows = [r for r in sw["quality"] if float(r["esn0_db"]) < below]
    return sum(1 for r in rows if not r["settled"]), len(rows)


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
        "[`docs/design/async-dsss-parallel.md`]"
        "(../../../../../../docs/design/async-dsss-parallel.md)."
    )
    R.md()
    R.md(
        "**Operating point (the specification).** A 1,023-chip Gold code "
        "(CCSDS #365) at "
        "3.069 Mcps, two samples a chip (6.138 MSa/s), asynchronous BPSK "
        "data at 2,700 sym/s. The searching flavor, at the binding's "
        "defaults except two: its design C/N0 is the true one, and its "
        "search covers +/-1/T_E (+/-3 kHz). **BPSK only**: m = 4 and 8 are "
        "accepted by the object and not certified here (the owner's scope, "
        "2026-10-19)."
    )
    R.md()
    R.md(
        "The header's lock and lifecycle claims, each pinned in "
        "`native/tests/test_async_dsss_receiver_core.c` and each seen to "
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
                "decodes at the Es/N0 floor",
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
        "Sections 2.1 to 2.4 are the `--check` run the limits read; 2.5 and "
        "2.6 are the committed full sweeps."
    )
    R.md()

    R.md("### 2.1 Arrival: from a signal's first sample to symbol lock")
    R.md()
    R.md(
        "Each draw: the carrier uniform over +/-1/T_E, the arrival at a "
        "uniform point of up to 20 ms of noise, mid-data. Times are from "
        "the arrival, to one 2,046-sample block (0.33 ms)."
    )
    R.md()
    R.table(
        [
            "Es/N0",
            "carrier",
            "tracking",
            "seed error",
            "symbol lock",
            "locked at 2 s",
        ],
        [
            [
                f"{r['esn0_db']:.0f} dB",
                f"{r['carrier_hz']:+.0f} Hz",
                f"{r['t_track_ms']:.1f} ms",
                f"{r['seed_err_hz']:+.0f} Hz",
                f"{r['t_lock_ms']:.1f} ms",
                "yes" if r["locked_end"] else "no",
            ]
            for r in d["arrival"]
        ],
    )
    R.md()

    R.md("### 2.2 Quality: EVM and lock, BPSK, against Es/N0")
    R.md()
    R.md(
        "Each draw: the signal from sample 0, fed until symbol lock first "
        "comes up, 0.2 s more, then **every** symbol steps() returns over "
        "1 s (2,700 symbols), never gated on the flag. EVM is over that "
        "whole window, against the stream's own hard decisions "
        "(`dp_test_evm_db_hard_range`, m = 2). The meter scales the window "
        "to unit total power, so its reading on an ideal stream is not "
        "-Es/N0 but `dp_ber_evm_ideal_db()`; the **loss** is the EVM over "
        "that."
    )
    R.md()
    R.table(
        [
            "Es/N0",
            "symbol lock after",
            "symbols",
            "EVM",
            "meter's ideal",
            "loss",
            "window in lock",
        ],
        [
            [
                f"{r['esn0_db']:.0f} dB",
                f"{r['t_lock_ms']:.0f} ms",
                f"{r['n_sym']:.0f}",
                f"{r['evm_db']:.2f} dB",
                f"{r['ideal_db']:.2f} dB",
                f"{r['loss_db']:+.2f} dB",
                f"{100.0 * float(r['lock_frac']):.1f}%",
            ]
            for r in d["quality"]
        ],
    )
    R.md()
    R.md("![EVM against Es/N0](evm.png)")
    R.md()

    R.md("### 2.3 Dropout: symbol lock when the signal goes")
    R.md()
    R.md(
        "A draw settled in tracking with symbol lock held, then cut at a "
        "random sample, mid-data; the noise runs on. `held` is the longest "
        "dropout lock rode out; the re-lock times are from the signal's "
        "return."
    )
    R.md()
    R.table(
        [
            "Es/N0",
            "lock falls, gone for good",
            "longest dropout held",
            "back after 50 ms out",
            "back after 200 ms out",
        ],
        [
            [
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

    R.md("### 2.4 False lock: a receiver seeded onto noise")
    R.md()
    R.md(
        "A receiver handed a false alarm -- seeded onto noise alone, sized "
        "for the Es/N0 it expected -- and never released, so its loops "
        "run on noise for the whole 2 s."
    )
    R.md()
    R.table(
        ["design Es/N0", "symbol lock up", "declared", "code lock up"],
        [
            [
                f"{r['esn0_db']:.0f} dB",
                f"{r['up']:.0f} of {r['blocks']:.0f} blocks",
                f"{r['declares']:.0f} times",
                f"{100.0 * float(r['code_frac']):.2f}% of blocks",
            ]
            for r in d["falselock"]
        ],
    )
    R.md()

    R.md("### 2.5 The full sweep (`data/sweep.txt`)")
    R.md()
    R.md(
        "The same measurements over more draws, committed and rendered, "
        "never re-run on a push: five quality draws at each Es/N0, twenty "
        "arrival draws and up to forty dropout draws at 10 and 5 dB, five "
        "false-lock draws at each."
    )
    R.md()
    rows = []
    for es in _es(sw["quality"]):
        q = _at(sw["quality"], es)
        ok = [r for r in q if r["settled"]]
        rows.append(
            [
                f"{es:.0f} dB",
                f"{len(ok)} of {len(q)}",
                _span([float(r["loss_db"]) for r in ok]) if ok else "--",
                (
                    f"{100.0 * min(float(r['lock_frac']) for r in ok):.1f}%"
                    if ok
                    else "--"
                ),
            ]
        )
    R.table(
        ["Es/N0", "locked within 4 s", "loss", "least window in lock"],
        rows,
    )
    R.md()
    rows = []
    for es in _es(sw["arrival"]):
        a = _at(sw["arrival"], es)
        trk = [
            float(r["t_track_ms"])
            for r in a
            if r["t_track_ms"] == r["t_track_ms"]
        ]
        lk = [
            float(r["t_lock_ms"])
            for r in a
            if r["t_lock_ms"] == r["t_lock_ms"]
        ]
        dr = _at(sw["dropout"], es)
        rows.append(
            [
                f"{es:.0f} dB",
                f"{sum(1 for r in a if r['locked_end'])} of {len(a)}",
                (
                    f"{statistics.median(trk):.0f} / {max(trk):.0f} ms"
                    if trk
                    else "--"
                ),
                (
                    f"{statistics.median(lk):.0f} / {max(lk):.0f} ms"
                    if lk
                    else "--"
                ),
                f"{sum(1 for r in dr if not r['settled'])} of {len(dr)}",
            ]
        )
    R.table(
        [
            "Es/N0",
            "arrivals locked at 2 s",
            "tracking, median / worst",
            "symbol lock, median / worst",
            "dropout draws not settled in 4 s",
        ],
        rows,
    )
    R.md()
    fl = sw["falselock"]
    R.md(
        f"False lock over the sweep: {sum(int(r['up']) for r in fl)} of "
        f"{sum(int(r['blocks']) for r in fl)} blocks with symbol lock up, "
        f"{sum(int(r['declares']) for r in fl)} declarations, in "
        f"{len(fl)} receivers on noise."
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
        f"That is F3, a known defect, so their count is a ratchet: the "
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
        "and its loss is over that meter's ideal (`dp_ber_evm_ideal_db()`), "
        "as in 2.2. The blind M2M4 reads "
        + _span([float(r["m2m4_db"]) - float(r["esn0_db"]) for r in ber])
        + " from the applied Es/N0 (F5)."
    )
    R.md()


def review(sw: Blocks, bsw: Blocks) -> None:
    R.md("## 3. Review — findings, with verdicts")
    R.md()
    R.find(
        "F1",
        "GAP",
        "`steps_max_out()` returns 0, which is not a bound: a call that "
        "completes a carried code period emits that whole period's symbols, "
        "so at more than one symbol per period an `out` sized to the call's "
        "`x_len` drops symbols silently. At the specification's operating "
        "point (0.9 symbols a period) it never binds. Tracked as #2469.",
    )
    R.find(
        "F2",
        "GAP",
        "`differential` is inert here: it reaches MpskReceiver's `bits()` "
        "demap, and this object returns soft symbols only "
        "(`_test_differential_does_not_change_the_symbols`). Remove it or "
        "add `bits()`: #2471.",
    )
    miss, n_q = _quality_misses(sw, 10.0)
    a5 = _at(sw["arrival"], 5.0)
    d5, d10 = _at(sw["dropout"], 5.0), _at(sw["dropout"], 10.0)
    miss10 = sum(1 for r in d10 if not r["settled"])
    R.find(
        "F3",
        "CONFIRMED",
        f"A first hand-off can land off the carrier and never pull in. With "
        f"`reacquire` off (the binding's default, and this harness's), the "
        f"full sweep has {miss} of its {n_q} quality draws under 10 dB with "
        f"no symbol lock within 4 s, "
        f"{sum(1 for r in a5 if not r['locked_end'])} of {len(a5)} arrivals "
        f"at 5 dB unlocked at 2 s, and dropout draws that never settled in "
        f"{sum(1 for r in d5 if not r['settled'])} of {len(d5)} at 5 dB and "
        f"{miss10} of {len(d10)} at 10 dB (section 2.5). "
        + (
            "A 10 dB draw that misses is #2409's case: the refine handed "
            "over outside the carrier loops' pull-in, code lock came up and "
            "symbol lock never did. "
            if miss10
            else "None missed at the 10 dB design point in this sweep; "
            "#2409 holds one that did, replayed from an earlier sweep: the "
            "refine handed over outside the carrier loops' pull-in, code "
            "lock came up and symbol lock never did. "
        )
        + "`reacquire` recovers a missed hand-off by searching again; the "
        "specification example locks every draw that way. The hand-off "
        "accuracy is "
        "#2409.",
    )
    R.find(
        "F4",
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
        "F5",
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
    q = {float(r["esn0_db"]): r for r in d["quality"]}
    m = d["evm_model"][0]
    margin = float(m["margin_db"])
    bounds = ", ".join(
        f"{float(q[es]['loss_min_db']):+.2f} to "
        f"{float(q[es]['loss_max_db']):+.2f} dB at {es:.0f} dB"
        for es in sorted(q, reverse=True)
        if float(q[es]["loss_max_db"]) == float(q[es]["loss_max_db"])
    )
    n_sym = int(float(q[20.0]["n_sym"]))
    # One sigma of an EVM over n symbols, in dB: 10/ln(10)/sqrt(n).
    sigma_db = 10.0 / math.log(10.0) / math.sqrt(n_sym)
    R.md(
        f"The EVM bound is two-sided and set per Es/N0, as a loss over the "
        f"meter's ideal. A locked receiver's error is the ideal's scaled by "
        f"its implementation loss, plus a floor: EVM² = k·ideal + floor in "
        f"linear power, so its loss is 10·log10(k + floor/ideal). The band "
        f"lies between two such lines fitted to the full sweep's draws at "
        f"10, 15 and 20 dB: the upper (k {float(m['k_hi_db']):.2f} dB, "
        f"floor {float(m['floor_hi_db']):.1f} dB) through its worst, the "
        f"lower (k {float(m['k_lo_db']):.2f} dB, floor "
        f"{float(m['floor_lo_db']):.1f} dB) under its least, each widened by "
        f"{margin:.2f} dB, the EVM estimator's own counting noise over "
        f"{n_sym} symbols ({margin / sigma_db:.1f} sigma of "
        f"{sigma_db:.2f} dB): {bounds}. An EVM under the band reads better "
        f"than the receiver's implementation loss allows: a meter that "
        f"flatters it, as the Dll's per-period output normalization did "
        f"until #2495, and the lower line is what sees that. Below 10 dB a "
        f"draw need not lock, and no EVM limit is claimed."
    )
    R.md()
    for r in d["arrival"]:
        if float(r["esn0_db"]) >= 10.0:
            R.limit(
                0.0 < float(r["t_track_ms"]) <= CHECK_TRACK_MS,
                f"[{r['esn0_db']:.0f} dB, carrier {r['carrier_hz']:+.0f} Hz] "
                f"a signal is tracked within {CHECK_TRACK_MS:.0f} ms of "
                f"arriving",
            )
            R.limit(
                bool(r["locked_end"]),
                f"[{r['esn0_db']:.0f} dB, carrier {r['carrier_hz']:+.0f} Hz] "
                f"it holds symbol lock 2 s after arriving",
            )
    for r in d["quality"]:
        es = float(r["esn0_db"])
        if es < 10.0:
            continue
        tag = f"[{es:.0f} dB]"
        R.limit(bool(r["settled"]), f"{tag} the receiver reaches symbol lock")
        lo, hi = float(r["loss_min_db"]), float(r["loss_max_db"])
        R.limit(
            lo <= float(r["loss_db"]) <= hi,
            f"{tag} a locked receiver's BPSK EVM is within {lo:+.2f} to "
            f"{hi:+.2f} dB of the meter's ideal",
        )
        R.limit(
            float(r["lock_frac"]) == 1.0,
            f"{tag} symbol lock holds through 1 s of locked operation",
        )
    for r in d["dropout"]:
        if r["settled"]:
            R.limit(
                0.0 < float(r["drop_ms"]) <= DROP_WATCH_MS,
                f"[{r['esn0_db']:.0f} dB] a signal gone for good drops "
                f"symbol lock within {DROP_WATCH_MS:.0f} ms",
            )
    for r in d["falselock"]:
        R.limit(
            float(r["declares"]) == 0.0,
            f"[{r['esn0_db']:.0f} dB design] a receiver on noise never "
            f"declares symbol lock in 2 s",
        )
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
            f"settle ({n_u} here; F3, a ratchet that may only shrink)",
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
    mid = _losses(sw, 5.0, 15.0)
    top = _losses(sw, 20.0, 20.0)
    a10 = _at(sw["arrival"], 10.0)
    trk = [
        float(r["t_track_ms"])
        for r in a10
        if r["t_track_ms"] == r["t_track_ms"]
    ]
    lk = [
        float(r["t_lock_ms"]) for r in a10 if r["t_lock_ms"] == r["t_lock_ms"]
    ]
    drops = [float(r["drop_ms"]) for r in sw["dropout"] if r["settled"]]
    d10, d5 = _at(sw["dropout"], 10.0), _at(sw["dropout"], 5.0)
    fl = sw["falselock"]
    return [
        f"**Budget about 1 dB for the error rate, once settled.** From "
        f"{min(es):.0f} to {max(es):.0f} dB the BER of the bursts that "
        f"settled ({n_set} of {len(bursts)}) sits {min(loss):.2f} to "
        f"{max(loss):.2f} dB from the BPSK bound; where it goes is "
        f"section 2.7 (2.6). "
        + (
            f"The other {len(bursts) - n_set} never settled (F3)."
            if n_set < len(bursts)
            else "Every burst settled."
        ),
        f"**Budget about 1 dB of EVM over the meter's ideal from 5 to 15 "
        f"dB.** Over the full sweep a locked receiver's BPSK EVM is "
        f"{_span(mid)} over `dp_ber_evm_ideal_db()` there, the same "
        f"implementation loss the error rate shows; at 20 dB {_span(top)}, "
        f"where an implementation floor joins it (section 2.5).",
        f"**Lock is quick at 10 dB:** tracked {statistics.median(trk):.0f} ms "
        f"after a signal arrives (worst {max(trk):.0f} ms over "
        f"{len(trk)} draws), symbol lock {statistics.median(lk):.0f} ms "
        f"(worst {max(lk):.0f} ms) (2.5).",
        f"**Turn `reacquire` on.** A first hand-off can miss the carrier "
        f"and, without it, never recover: "
        f"{_quality_misses(sw, 10.0)[0]} of the "
        f"{_quality_misses(sw, 10.0)[1]} quality draws under 10 dB never "
        f"locked, and {sum(1 for r in d10 if not r['settled'])} of "
        f"{len(d10)} dropout draws at 10 dB and "
        f"{sum(1 for r in d5 if not r['settled'])} of {len(d5)} at 5 dB "
        f"never settled (F3, #2409).",
        f"**The lock flag means something:** {len(fl)} receivers on noise "
        f"declared it {sum(int(r['declares']) for r in fl)} times, and a "
        f"signal that leaves drops it within {max(drops):.0f} ms (2.5).",
        "**Size `out` with care above one symbol a code period:** "
        "`steps_max_out()` is not yet a bound (F1, #2469). At the "
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
        _plot(d["quality"])
        _plot_ber(bsw["ber"])
    characterise(d, sw, bsw)
    section_loss(lsw)
    review(sw, bsw)
    limits(d, sw)
    R.executive("AsyncDsssReceiver", _takeaways(sw, bsw))
    R.summary(
        "\n- Raw run: `data/arrival.csv`, `data/quality.csv`, "
        "`data/dropout.csv`, `data/falselock.csv`, `data/ber.csv`, "
        "`data/burst.csv`, `data/evm_model.csv` (the EVM band's two "
        "lines) -- the two harnesses' `--check --emit` blocks; "
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
