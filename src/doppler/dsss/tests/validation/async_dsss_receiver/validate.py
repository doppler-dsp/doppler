"""Certify `async_dsss_receiver` -- one continuous asynchronous DSSS receiver.

Run:  make validate-py VALIDATORS=<this file>   (this report)
      make validate-async-receiver-sweep        (re-run the full sweep into
                                                 data/sweep.txt, then this)
      make validate          (regenerates every report)
      make validate-check    (fails if the committed report is stale)

**C measures; Python renders and asserts.** Every number here is
`native/validation/async_dsss_receiver_lock.c`'s: the shipped
continuous-DSSS synth (`wfm_synth`) with the harness's own data bits, the
shipped `awgn` sized from C/N0, the receiver in the SPEC configuration.
This file parses its CSV blocks, characterises, reviews and asserts the
limits through the same `Report` every other certified object uses -- the
`conv` / `async_dsss_pool` shape.

**Two runs, two roles.** The limits are asserted on the harness's
regression subset, `--check --emit`, re-run on every push (about 3 s), so
the evidence and the gate are one run and cannot disagree. The full sweep
-- twenty arrival and dropout draws at 10 and 5 dB, five quality draws at
each Es/N0, five false-lock draws -- is committed as `data/sweep.txt` and
rendered in section 2.5; the findings and the executive summary quote it,
computed from that file, never typed. `make validate-async-receiver-sweep`
refreshes it on purpose.

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
ROOT = repo_root(__file__)
HARNESS = exe(
    build_dir(__file__) / "native/validation/validate_async_dsss_receiver_lock"
)

#: This report's asserted bounds. CHECK_TRACK_MS and DROP_WATCH_MS are the
#: harness's own (it asserts them in --check and the sweep alike). The EVM
#: allowance is not restated here: the harness computes it per Es/N0 and
#: emits it with each quality row (`loss_min_db`, `loss_max_db`), so the
#: report reads the bound the harness asserted (section 4).
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


def _run(args: list[str]) -> str:
    """Run the harness; its stdout, or a hard failure that says why.

    A missing binary, or a run whose own assertions failed, is a failure
    rather than a skip: this report has no measurement of its own, and a
    skipped or half-run one is indistinguishable from a passing one in a
    log. `make build` builds it.
    """
    if not HARNESS.exists():
        raise SystemExit(
            f"async_dsss_receiver: {HARNESS.relative_to(ROOT)} is not built "
            f"-- run `make build` first. Every number in this report is the "
            f"C harness's."
        )
    run = subprocess.run(
        [str(HARNESS), *args], capture_output=True, text=True, check=False
    )
    if run.returncode != 0:
        raise SystemExit(
            f"async_dsss_receiver: `{HARNESS.name} {' '.join(args)}` failed "
            f"(exit {run.returncode}):\n{run.stderr[-2000:]}"
        )
    return run.stdout


def _blocks(out: str, source: str) -> Blocks:
    blocks = _parse(out)
    for name in ("arrival", "dropout", "quality", "falselock"):
        if not blocks.get(name):
            raise SystemExit(
                f"async_dsss_receiver: {source} has no `# {name}` block"
            )
    return blocks


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
    ax.set_title("BPSK EVM against Es/N0, SPEC operating point")
    ax.grid(alpha=0.3)
    ax.legend()
    fig.tight_layout()
    fig.savefig(HERE / "evm.png", dpi=110)
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
        "**Operating point (SPEC).** A 1,023-chip Gold code (CCSDS #365) at "
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


def characterise(d: Blocks, sw: Blocks) -> None:
    R.md("## 2. Characterisation")
    R.md()
    R.md(
        "Sections 2.1 to 2.4 are the `--check` run the limits read; 2.5 is "
        "the committed full sweep."
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


def review(sw: Blocks) -> None:
    R.md("## 3. Review — findings, with verdicts")
    R.md()
    R.find(
        "F1",
        "GAP",
        "`steps_max_out()` returns 0, which is not a bound: a call that "
        "completes a carried code period emits that whole period's symbols, "
        "so at more than one symbol per period an `out` sized to the call's "
        "`x_len` drops symbols silently. At SPEC (0.9 symbols a period) it "
        "never binds. Tracked as #2469.",
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
    R.find(
        "F3",
        "CONFIRMED",
        f"A first hand-off can land off the carrier and never pull in, at "
        f"the 10 dB design point as well as below it. With `reacquire` off "
        f"(the binding's default, and this harness's), the full sweep has "
        f"{miss} of its {n_q} quality draws under 10 dB with no symbol lock "
        f"within 4 s, {sum(1 for r in a5 if not r['locked_end'])} of "
        f"{len(a5)} arrivals at 5 dB unlocked at 2 s, and dropout draws that "
        f"never settled in {sum(1 for r in d5 if not r['settled'])} of "
        f"{len(d5)} at 5 dB and {sum(1 for r in d10 if not r['settled'])} "
        f"of {len(d10)} at 10 dB (section 2.5). The 10 dB draw is on #2409: "
        f"the refine handed over outside the carrier loops' pull-in, code "
        f"lock came up and symbol lock never did. `reacquire` recovers a "
        f"missed hand-off by searching again; the SPEC example locks every "
        f"draw that way. The hand-off accuracy is #2409.",
    )
    R.find(
        "F4",
        "C-ONLY",
        "The lock detector's hysteresis, the release clock's edges and the "
        "hand-over domain are certified symbol by symbol and sample by "
        "sample in the C tests of section 1; the binding exposes only the "
        "flags, which section 2 reads.",
    )


def limits(d: Blocks, sw: Blocks) -> None:
    R.md("## 4. Limits — claims a caller may rely on")
    R.md()
    q = {float(r["esn0_db"]): r for r in d["quality"]}
    margin = -float(q[20.0]["loss_min_db"])
    # The floor the harness's model uses, recovered from its 20 dB bound:
    # loss_max = 10*log10(1 + floor/ideal) + margin.
    floor_db = float(q[20.0]["ideal_db"]) + 10.0 * math.log10(
        10.0 ** ((float(q[20.0]["loss_max_db"]) - margin) / 10.0) - 1.0
    )
    bounds = ", ".join(
        f"{float(q[es]['loss_max_db']):+.2f} dB at {es:.0f} dB"
        for es in sorted(q, reverse=True)
        if float(q[es]["loss_max_db"]) == float(q[es]["loss_max_db"])
    )
    w15 = max(_losses(sw, 15.0, 15.0))
    n_sym = int(float(q[20.0]["n_sym"]))
    # One sigma of an EVM over n symbols, in dB: 10/ln(10)/sqrt(n).
    sigma_db = 10.0 / math.log(10.0) / math.sqrt(n_sym)
    R.md(
        f"The EVM bound is two-sided and set per Es/N0, as a loss over the "
        f"meter's ideal. A locked receiver's error is the ideal's plus an "
        f"implementation floor, EVM² = ideal + floor in linear power, so the "
        f"most loss it may show is 10·log10(1 + floor/ideal) plus "
        f"{margin:.2f} dB, the EVM estimator's own counting noise over "
        f"{n_sym} symbols ({margin / sigma_db:.1f} sigma of "
        f"{sigma_db:.2f} dB): "
        f"{bounds}. The floor, {floor_db:.1f} dB, is what the full sweep's "
        f"worst 20 dB draw leaves. An EVM more than {margin:.2f} dB under "
        f"the ideal fails too: that is a meter or harness defect, not a "
        f"better receiver. At 15 dB the model reads high (the sweep's worst "
        f"loss there is {w15:+.2f} dB), so that bound is the looser one. "
        f"Below 10 dB the loss comes from the loops' dynamics rather than a "
        f"floor, and no EVM limit is claimed."
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


def _takeaways(sw: Blocks) -> list[str]:
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
        f"**Design to the meter's ideal EVM from 5 to 15 dB.** Over the "
        f"full sweep a locked receiver's BPSK EVM is {_span(mid)} over "
        f"`dp_ber_evm_ideal_db()` there; at 20 dB {_span(top)}, an "
        f"implementation floor (section 2.5).",
        f"**Lock is quick at 10 dB:** tracked {statistics.median(trk):.0f} ms "
        f"after a signal arrives (worst {max(trk):.0f} ms over "
        f"{len(trk)} draws), symbol lock {statistics.median(lk):.0f} ms "
        f"(worst {max(lk):.0f} ms) (2.5).",
        f"**Turn `reacquire` on.** A first hand-off can miss the carrier "
        f"and, without it, never recover, at 10 dB as well as below: "
        f"{sum(1 for r in d10 if not r['settled'])} of {len(d10)} dropout "
        f"draws at 10 dB and {sum(1 for r in d5 if not r['settled'])} of "
        f"{len(d5)} at 5 dB never settled (F3, #2409).",
        f"**The lock flag means something:** {len(fl)} receivers on noise "
        f"declared it {sum(int(r['declares']) for r in fl)} times, and a "
        f"signal that leaves drops it within {max(drops):.0f} ms (2.5).",
        "**Size `out` with care above one symbol a code period:** "
        "`steps_max_out()` is not yet a bound (F1, #2469). At SPEC it "
        "never binds.",
    ]


def build(write: bool = True) -> Report:
    global R
    R = Report(write=write)
    R.md("# AsyncDsssReceiver — validation report")
    R.md()
    section_object()
    d = _blocks(_run(["--check", "--emit"]), "the --check run")
    sw = _sweep()
    if write:
        _write_csv(d)
        _plot(d["quality"])
    characterise(d, sw)
    review(sw)
    limits(d, sw)
    R.executive("AsyncDsssReceiver", _takeaways(sw))
    R.summary(
        "\n- Raw run: `data/arrival.csv`, `data/quality.csv`, "
        "`data/dropout.csv`, `data/falselock.csv` -- the harness's "
        "`--check --emit` blocks; `data/sweep.txt` -- its full sweep"
    )
    R.emit(HERE / "results.md")
    return R


if __name__ == "__main__":
    if "--sweep" in sys.argv:
        sys.argv.remove("--sweep")
        DATA.mkdir(exist_ok=True)
        SWEEP.write_text(_run(["--emit"]), encoding="utf-8")
    sys.exit(cli(build, HERE))
