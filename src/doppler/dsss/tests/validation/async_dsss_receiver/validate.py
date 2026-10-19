"""Certify `async_dsss_receiver` -- one continuous asynchronous DSSS receiver.

Run:  python -m doppler.dsss.tests.validation.async_dsss_receiver.validate
      make validate          (regenerates every report)
      make validate-check    (fails if the committed report is stale)

**C measures; Python renders and asserts.** Every number here is
`native/validation/async_dsss_receiver_lock.c`'s: the shipped
continuous-DSSS synth (`wfm_synth`) with the harness's own data bits, the
shipped `awgn` sized from C/N0, the receiver at the binding's defaults at
the SPEC operating point. This file parses its CSV blocks, characterises,
reviews and asserts the limits through the same `Report` every other
certified object uses -- the `conv` / `async_dsss_pool` shape.

**The run is the harness's regression subset, `--check --emit`** -- the
same downselect `ctest` runs, about 3 s -- so the evidence and the gate are
one run and cannot disagree. Its full sweep (`make validate-c`: twenty
arrival and dropout draws at 10 and 5 dB, five quality draws at each
Es/N0, five false-lock draws) is the parallel-measurements record's,
refreshed on purpose.

**Scope: BPSK.** The receiver accepts m = 2, 4 or 8; the owner scoped this
certification to BPSK (2026-10-19), and the test library spreads BPSK only,
so m = 4 and 8 are not certified here.
"""

from __future__ import annotations

import csv
import subprocess
import sys
from pathlib import Path

from doppler.tests._repo import build_dir, exe, repo_root
from doppler.tests._validation_common import Report, cli

HERE = Path(__file__).resolve().parent
DATA = HERE / "data"
ROOT = repo_root(__file__)
HARNESS = exe(
    build_dir(__file__) / "native/validation/validate_async_dsss_receiver_lock"
)

#: The harness's own constants, restated only to label the report; the
#: asserted bounds below are this report's, chosen off every toolchain's
#: edge (the harness asserts its own, the same in --check and the sweep).
CHECK_TRACK_MS = 100.0
DROP_WATCH_MS = 80.0
EVM_SLACK_DB = 1.5
SYM_RATE = 2700.0

R = Report()

Rows = list[dict[str, float | str]]


def _harness() -> dict[str, Rows]:
    """Run the harness's regression subset with `--emit`; parse its blocks.

    A missing binary is a hard failure rather than a skip: this report has
    no measurement of its own, and a skipped one is indistinguishable from
    a passing one in a log. `make build` builds it.
    """
    if not HARNESS.exists():
        raise SystemExit(
            f"async_dsss_receiver: {HARNESS.relative_to(ROOT)} is not built "
            f"-- run `make build` first. Every number in this report is the "
            f"C harness's."
        )
    run = subprocess.run(
        [str(HARNESS), "--check", "--emit"],
        capture_output=True,
        text=True,
        check=False,
    )
    blocks = _parse(run.stdout)
    for name in ("arrival", "dropout", "quality", "falselock"):
        if not blocks.get(name):
            raise SystemExit(
                f"async_dsss_receiver: the harness emitted no `# {name}` "
                f"block (exit {run.returncode})"
            )
    return blocks


def _num(v: str) -> float | str:
    try:
        return float(v)
    except ValueError:
        return v


def _parse(out: str) -> dict[str, Rows]:
    """One run's stdout into its `# <name>` CSV blocks."""
    blocks: dict[str, Rows] = {}
    name, header = "", []
    for line in out.splitlines():
        line = line.strip()
        if line.startswith("# "):
            name, header = line[2:].split()[0], []
            blocks[name] = []
        elif not line or not name:
            name = "" if not line else name
        elif not header:
            header = line.split(",")
        else:
            blocks[name].append(
                {k: _num(v) for k, v in zip(header, line.split(","))}
            )
    return blocks


def _write_csv(d: dict[str, Rows]) -> None:
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
    evm = [float(r["evm_db"]) for r in q]
    fig, ax = plt.subplots(figsize=(6.0, 4.0))
    lo, hi = min(es) - 1.0, max(es) + 1.0
    ax.plot([lo, hi], [-lo, -hi], "k--", lw=1, label="-Es/N0 (ideal)")
    ax.plot(es, evm, "o-", label="AsyncDsssReceiver, locked")
    ax.set_xlabel("Es/N0 (dB)")
    ax.set_ylabel("EVM (dB, own hard decisions)")
    ax.set_title("BPSK EVM against Es/N0, SPEC operating point")
    ax.grid(alpha=0.3)
    ax.legend()
    fig.tight_layout()
    fig.savefig(HERE / "evm.png", dpi=110)
    plt.close(fig)


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
        "data at 2,700 sym/s; the searching flavor at the binding's "
        "defaults, its own search over +/-1/T_E (+/-3 kHz), its design "
        "C/N0 the true one. **BPSK only**: m = 4 and 8 are accepted by the "
        "object and not certified here (the owner's scope, 2026-10-19)."
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
                "`differential` does not change the symbols",
                "`_test_differential_does_not_change_the_symbols`",
            ],
        ],
    )
    R.md()


def characterise(d: dict[str, Rows]) -> None:
    R.md("## 2. Characterisation")
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
        "1 s (2,700 symbols), never gated on the flag. EVM is against the "
        "stream's own hard decisions (`dp_test_evm_db_hard_m`, m = 2); a "
        "locked receiver at the matched-filter output reads it near "
        "-Es/N0."
    )
    R.md()
    R.table(
        [
            "Es/N0",
            "symbol lock after",
            "symbols",
            "EVM",
            "EVM + Es/N0",
            "window in lock",
        ],
        [
            [
                f"{r['esn0_db']:.0f} dB",
                f"{r['t_lock_ms']:.0f} ms",
                f"{r['n_sym']:.0f}",
                f"{r['evm_db']:.2f} dB",
                f"{r['evm_db'] + r['esn0_db']:+.2f} dB",
                f"{100.0 * r['lock_frac']:.1f}%",
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
                f"{100.0 * r['frac']:.2f}% of blocks",
                f"{r['declares']:.0f} times",
                f"{100.0 * r['code_frac']:.2f}% of blocks",
            ]
            for r in d["falselock"]
        ],
    )
    R.md()


def review() -> None:
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
    R.find(
        "F3",
        "CONFIRMED",
        "At 5 and 7 dB a first hand-off can land off the carrier and never "
        "pull in: with `reacquire` off (the binding's default, and this "
        "harness's), 3 of the sweep's 10 quality draws at those Es/N0 had "
        "no symbol lock within 4 s. `reacquire` recovers it -- the SPEC "
        "example locks 24 of 24 that way -- so a caller below 10 dB turns "
        "it on. The hand-off "
        "accuracy is #2409.",
    )
    R.find(
        "F4",
        "C-ONLY",
        "The lock detector's hysteresis, the release clock's edges and the "
        "hand-over domain are certified symbol by symbol and sample by "
        "sample in the C tests of section 1; the binding exposes only the "
        "flags, which section 2 reads.",
    )


def limits(d: dict[str, Rows]) -> None:
    R.md("## 4. Limits — claims a caller may rely on")
    R.md()
    for r in d["arrival"]:
        if r["esn0_db"] >= 10.0:
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
        R.limit(
            abs(float(r["evm_db"]) + es) <= EVM_SLACK_DB,
            f"{tag} a locked receiver's BPSK EVM is within "
            f"{EVM_SLACK_DB} dB of -Es/N0",
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


def build(write: bool = True) -> Report:
    global R
    R = Report(write=write)
    R.md("# AsyncDsssReceiver — validation report")
    R.md()
    section_object()
    d = _harness()
    if write:
        _write_csv(d)
        _plot(d["quality"])
    characterise(d)
    review()
    limits(d)
    R.executive(
        "AsyncDsssReceiver",
        [
            "**Design to EVM = -Es/N0 from 5 to 15 dB.** A locked receiver's "
            "BPSK EVM reads within about 0.5 dB of -Es/N0 there (section "
            "2.2); at 20 dB it sits about 1.2 dB above, an implementation "
            "floor near -25 dB.",
            "**Lock is fast at 10 dB and above:** tracked in about 45 ms of "
            "a signal's arrival and symbol-locked within a second, then "
            "held (2.1, 2.2).",
            "**Below 10 dB, turn `reacquire` on.** A first hand-off can "
            "miss the carrier and, without it, never recover (F3, #2409).",
            "**The lock flag means something:** a receiver on noise never "
            "declares it (2.4), and a signal that leaves drops it within "
            "tens of ms (2.3).",
            "**Size `out` with care above one symbol a code period:** "
            "`steps_max_out()` is not yet a bound (F1, #2469). At SPEC it "
            "never binds.",
        ],
    )
    R.summary(
        "\n- Raw run: `data/arrival.csv`, `data/quality.csv`, "
        "`data/dropout.csv`, `data/falselock.csv` -- the harness's "
        "`--check --emit` blocks"
    )
    R.emit(HERE / "results.md")
    return R


if __name__ == "__main__":
    sys.exit(cli(build, HERE))
