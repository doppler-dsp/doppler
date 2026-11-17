"""The receiver at the 415.1-B-1 point: its runs, rendered from their data.

The pull-in validator (``native/validation/async_dsss_receiver_pullin.c``)
with ``--emit`` prints four sections a run is kept as:

- ``pullin``: one row a draw -- the hand-over, the lock times, the flags'
  changes over the data, the error rate, and the loops' stress against
  the truth beside the design's baselines (design §4.8, "The two
  stresses");
- ``hist``: per cell, the bin counts of every decision's statistic (lock
  statistics on data blocks; each search and refine test over its
  threshold) and of the loops' errors, computed in C over the cell's
  draws (``dp_hist_t``, ``native/tests/dp_stats_test.h``);
- ``rule``: the five lock rules each cell's flags were decided by;
- ``transition``: every change of the symbol- and code-lock flags after
  each was sustained, with what it is attributed by: the flag's statistic
  at the change against its rule, the carrier loop's stress over the
  second before (largest error, rms, steps past the fold) and the code
  loop's largest error, and the symbols' EVM over +/-0.5 s around it.

Nothing here computes a statistic of the receiver: a figure draws the
counts the run binned, a table counts the rows it printed, and a trace
is the capture plotter (``scripts/plot_capture.py``) on a capture the
validator wrote, its reference lines read from the capture's rules file
and the run's own baseline columns.

Usage (development, until the record run is taken)::

    python record_415.py shape DIR OUT FC ESN0 RATE CW
    python record_415.py windows DIR OUT FC ESN0 RATE
    python record_415.py handover DIR OUT
    python record_415.py bn DIR [DIR ...] OUT.md

``DIR`` holds the shard outputs ``s*.csv`` of one slice of the sweep;
``OUT`` receives the cell's figures and its attribution table; ``windows``
draws one cell under both carrier windows on shared axes.
"""

from __future__ import annotations

import csv
import io
import json
import re
import subprocess
import sys
from collections import Counter, defaultdict
from pathlib import Path
from typing import Any

DATA = Path(__file__).resolve().parent
ROOT = DATA.parents[2]
VALIDATOR = (
    ROOT / "build/native/validation/validate_async_dsss_receiver_pullin"
)

#: A cell: carrier GHz, Es/N0 dB, ramp magnitude Hz/s, carrier window.
Cell = tuple[str, str, str, str]
#: A run's sections, by name, each its rows.
Data = dict[str, list[dict[str, str]]]
#: One section's rows.
Rows = list[dict[str, str]]

#: The lock statistics drawn against their rules, in the receiver's order:
#: (the histogram's quantity, its rule in the ``rule`` section).
LOCK_STATS = (
    ("sym.lock", "sym"),
    ("car.lock", "car"),
    ("code.lock", "code"),
    ("demod.lock", "demod"),
    ("demod.sync.lock", "demod_sync"),
)
#: The tests drawn over their own thresholds (1 is the decision).
TESTS = ("acq.stat/gate", "ref.stat/thr", "ref_sq.stat/thr")
#: Per cell, the share of settled tracking blocks off the emitter's code
#: (the rows' own ``off_code_frac``), stated on every figure of the cell.
OFF_SHARE: dict[Cell, str] = {}
#: The carrier detector's fold, degrees of carrier phase (design §4.8).
FOLD_DEG = 90.0
#: The excursion levels short of the fold a change's second before is read
#: against (the validator's PI_EXC_DEG; its rows' ``exc<N>_win_frac``).
EXC_DEG = (30, 45, 60)


def csv_blocks(text: str) -> dict[str, list[dict[str, str]]]:
    """The validator's ``--emit`` output: ``# <name>`` then a CSV block."""
    out: dict[str, list[dict[str, str]]] = {}
    for chunk in text.split("# ")[1:]:
        name, _, body = chunk.partition("\n")
        out[name.strip()] = list(csv.DictReader(io.StringIO(body)))
    return out


def merge_shards(texts: list[str]) -> dict[str, list[dict[str, str]]]:
    """The shards of one run as one: rows concatenated, a cell's bin
    counts summed across shards, a cell's rules taken once (every shard of
    a cell runs the same configuration, so they agree; a disagreement is
    refused)."""
    rows: dict[str, list[dict[str, str]]] = defaultdict(list)
    hist: dict[tuple[str, ...], int] = Counter()
    rules: dict[tuple[str, ...], dict[str, str]] = {}
    for text in texts:
        b = csv_blocks(text)
        rows["pullin"] += b.get("pullin", [])
        rows["transition"] += b.get("transition", [])
        for r in b.get("hist", []):
            key = tuple(r[k] for k in r if k != "count")
            hist[key] += int(r["count"])
        for r in b.get("rule", []):
            key = (*cell_of(r), r["rule"])
            if key in rules and rules[key] != r:
                raise SystemExit(f"shards disagree on the rule {key}")
            rules[key] = r
    hist_rows = [
        dict(
            zip(
                (
                    "fc_ghz",
                    "esn0_db",
                    "rate_hz_s",
                    "carrier_window",
                    "draws",
                    "quantity",
                    "lo",
                    "hi",
                ),
                k,
            ),
            count=str(n),
        )
        for k, n in hist.items()
    ]
    return {**rows, "hist": hist_rows, "rule": list(rules.values())}


def cell_of(r: dict[str, str]) -> Cell:
    """A row's cell, the rate as its magnitude (a draw's sign is its
    seed's)."""
    return (
        f"{float(r['fc_ghz']):.1f}",
        f"{float(r['esn0_db']):.1f}",
        f"{abs(float(r['rate_hz_s'])):.0f}",
        r["carrier_window"],
    )


def cell_label(cell: Cell, n: int, false: int = 0) -> str:
    """What every figure states: its cell, its draws (and the false locks
    left out of it), the front end."""
    fc, es, rate, cw = cell
    dyn = "static" if float(rate) == 0 else f"+/-{rate} Hz/s"
    fl = f" ({false} false lock(s) apart)" if false else ""
    if OFF_SHARE.get(cell):
        fl += f"; {OFF_SHARE[cell]} of tracking off the emitter's code, apart"
    return (
        f"{fc} GHz, {es} dB Es/N0, {dyn}, {cw} window; {n} draws{fl}; "
        "front end spc 4, C/N0 at the antenna"
    )


def hist_of(
    data: Data, cell: Cell, quantity: str
) -> tuple[list[float], list[int], int, int]:
    """A cell's bins of one quantity ON the emitter -- blocks whose code is
    within half a chip of the truth, of draws that are not false locks; a
    receiver elsewhere tracks something else, another population, its
    share stated beside the figure: edges, counts, under, over."""
    rows = [
        r
        for r in data["hist"]
        if cell_of(r) == cell
        and r["quantity"] == quantity
        and r["draws"] == "on"
    ]
    inner = sorted(
        (r for r in rows if r["lo"] != "-inf" and r["hi"] != "inf"),
        key=lambda r: float(r["lo"]),
    )
    under = sum(int(r["count"]) for r in rows if r["lo"] == "-inf")
    over = sum(int(r["count"]) for r in rows if r["hi"] == "inf")
    edges = [float(r["lo"]) for r in inner] + [float(inner[-1]["hi"])]
    return edges, [int(r["count"]) for r in inner], under, over


def rule_of(data: Data, cell: Cell, name: str) -> dict[str, str]:
    """A cell's lock rule by name."""
    for r in data["rule"]:
        if cell_of(r) == cell and r["rule"] == name:
            return r
    raise SystemExit(f"no rule {name} for {cell}")


def _bars(
    ax: Any, edges: list[float], counts: list[int], color: str = "C0"
) -> None:
    ax.stairs(counts, edges, fill=True, color=color, alpha=0.6)


def fig_stats(data: Data, cell: Cell, n: int, out: Path, nfl: int = 0) -> None:
    """Every decision's statistic against its rule (lock statistics, data
    blocks from 1 s after the hand-over) or its own threshold (each test),
    log counts so the tail that makes a toggle shows."""
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    names = [q for q, _ in LOCK_STATS] + list(TESTS)
    fig, axes = plt.subplots(2, 4, figsize=(13, 5.6))
    for ax, q in zip(axes.flat, names):
        edges, counts, under, over = hist_of(data, cell, q)
        total = sum(counts) + under + over
        _bars(ax, edges, counts)
        ax.set_yscale("log")
        if q in dict(LOCK_STATS):
            r = rule_of(data, cell, dict(LOCK_STATS)[q])
            up, down = float(r["up_thresh"]), float(r["down_thresh"])
            ax.axvline(up, color="C2", ls="--", lw=1, label=f"up {up:g}")
            ax.axvline(down, color="C3", ls=":", lw=1, label=f"down {down:g}")
            below = sum(
                c for c, lo in zip(counts, edges) if lo + 1e-12 < down
            ) + (under if edges[0] < down else 0)
            ax.set_title(
                f"{q}: {total} values, {below} under down",
                fontsize=8,
            )
        else:
            ax.axvline(1.0, color="C3", ls="--", lw=1, label="threshold")
            over_thr = sum(c for c, lo in zip(counts, edges) if lo >= 1.0)
            note = (
                " (the squared test decides first)"
                if q == "ref.stat/thr" and total == 0
                else ""
            )
            ax.set_title(
                f"{q}: {total} tests, {over_thr + over} over{note}",
                fontsize=8,
            )
        ax.legend(fontsize=6)
        ax.tick_params(labelsize=7)
    fig.suptitle(
        "Test statistics against their thresholds (lock statistics: data "
        "blocks only, the 275-symbol code-only windows excluded) -- "
        + cell_label(cell, n, nfl),
        fontsize=9,
    )
    fig.tight_layout()
    fig.savefig(out, dpi=110)
    plt.close(fig)


def fig_stress(
    data: Data, cell: Cell, rows: Rows, out: Path, nfl: int = 0
) -> None:
    """The loops' stress: the carrier's and the code's true error, pooled
    over the cell's draws from 1 s after the hand-over, against the
    design's baselines (the ramp's lag and the jitter, from the run's own
    columns) and the detector's fold; the demod's own detector outputs
    beside them (no truth); and the hand-over transient per draw against
    0.49 * 2 pi df T / (1.886 bn)."""
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    def col(k: str) -> list[float]:
        return [float(r[k]) for r in rows if r[k] != "nan"]

    lag = col("base_lag_deg")[0]
    sd = col("base_sd_deg")[0]
    fig, axes = plt.subplots(1, 5, figsize=(16, 3.4))
    edges, counts, under, over = hist_of(data, cell, "car.err_deg")
    _bars(axes[0], edges, counts)
    axes[0].set_yscale("log")
    # The run's own per-draw means and sds, the mean signed with the
    # draw's ramp as the histogram is (C bins the error x sign(ramp)).
    sgn = [-1.0 if float(r["rate_hz_s"]) < 0 else 1.0 for r in rows]
    means = [
        s * float(r["car_mean_deg"])
        for s, r in zip(sgn, rows)
        if r["car_mean_deg"] != "nan"
    ]
    sds = col("car_sd_deg")
    m = sum(means) / len(means)
    msd = sorted(sds)[len(sds) // 2]
    for x, ls, c, lab in (
        (lag, "--", "C3", f"design |lag| {lag:.2f} (noise-free)"),
        (-lag, "--", "C3", None),
        (sd, ":", "C3", f"design sd {sd:.1f}"),
        (-sd, ":", "C3", None),
        (m, "-", "k", f"measured mean {m:+.2f}"),
        (m + msd, ":", "k", f"measured sd {msd:.1f} (median draw)"),
        (m - msd, ":", "k", None),
        (FOLD_DEG, "-", "C3", "fold"),
        (-FOLD_DEG, "-", "C3", None),
    ):
        axes[0].axvline(x, color=c, ls=ls, lw=0.8, label=lab)
    axes[0].set_title(
        f"carrier error x sign(ramp), deg: {sum(counts)} blocks", fontsize=8
    )
    axes[0].legend(fontsize=6)
    edges, counts, under, over = hist_of(data, cell, "code.err_chip")
    _bars(axes[1], edges, counts)
    axes[1].set_yscale("log")
    axes[1].set_title(
        f"code error, chip: {sum(counts)} blocks, {under + over} past +/-0.5",
        fontsize=8,
    )
    for k, q in ((2, "demod.car.e"), (3, "demod.sync.e")):
        edges, counts, under, over = hist_of(data, cell, q)
        _bars(axes[k], edges, counts)
        axes[k].set_yscale("log")
        axes[k].set_title(f"{q} (detector output, no truth)", fontsize=8)
    # The hand-over's 0.5 s against the same draw's steady 0.5 s: at the
    # floor the jitter's own peak is several sigma, so the transient shows
    # only as the excess over it. The design's step peak, per draw, is the
    # marker's color.
    pk = [
        (
            float(r["steady_peak_deg"]),
            float(r["ho_peak_deg"]),
            float(r["base_ho_deg"]),
        )
        for r in rows
        if r["ho_peak_deg"] != "nan" and r["steady_peak_deg"] != "nan"
    ]
    sc = axes[4].scatter(
        [s for s, _, _ in pk],
        [h for _, h, _ in pk],
        c=[b for _, _, b in pk],
        s=12,
        cmap="viridis",
    )
    fig.colorbar(sc, ax=axes[4]).set_label("design step peak, deg", fontsize=7)
    hi = FOLD_DEG * 1.05
    axes[4].plot([0, hi], [0, hi], color="0.5", lw=0.8, label="no excess")
    axes[4].axhline(FOLD_DEG, color="C3", lw=0.8, label="fold")
    axes[4].set_xlabel("steady peak (0.5 s from 1 s after), deg", fontsize=7)
    axes[4].set_ylabel("hand-over peak (its first 0.5 s), deg", fontsize=7)
    axes[4].set_title("hand-over transient, per draw", fontsize=8)
    axes[4].legend(fontsize=6)
    for ax in axes:
        ax.tick_params(labelsize=7)
    fig.suptitle(
        "Loop stress -- " + cell_label(cell, len(rows), nfl), fontsize=9
    )
    fig.tight_layout()
    fig.savefig(out, dpi=110)
    plt.close(fig)


def fig_times(rows: Rows, cell: Cell, out: Path) -> None:
    """Per draw: time to sustained symbol lock, time to the first change
    of each flag, and the hand-over's error."""
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    def col(k: str) -> list[float]:
        return [float(r[k]) for r in rows if r[k] != "nan"]

    fig, axes = plt.subplots(1, 3, figsize=(12, 3.0))
    axes[0].hist(col("t_sym_s"), bins=20)
    axes[0].set_title(
        f"time to sustained symbol lock, s ({len(col('t_sym_s'))} of "
        f"{len(rows)} draws)",
        fontsize=8,
    )
    axes[1].hist(
        [col("sym_tog1_s"), col("code_tog1_s")],
        bins=20,
        label=["symbol lock", "code lock"],
    )
    axes[1].set_title("time to the first change after it, s", fontsize=8)
    axes[1].legend(fontsize=6)
    axes[2].hist([abs(v) for v in col("ho_err_hz")], bins=20)
    axes[2].set_title("|hand-over error|, Hz", fontsize=8)
    for ax in axes:
        ax.tick_params(labelsize=7)
    fig.suptitle("Distributions -- " + cell_label(cell, len(rows)), fontsize=9)
    fig.tight_layout()
    fig.savefig(out, dpi=110)
    plt.close(fig)


def attribution(trans: Rows, cell: Cell) -> list[list[str]]:
    """The cell's changes, pooled over its draws, by what preceded them:
    a crossing of the carrier detector's fold in the second before, or
    none (the statistic's tail on a loop that did not cross); with the
    median largest carrier error, and the symbols' EVM around the change
    against the draw's own. A change on a block off the emitter (or of a
    false lock) is counted apart. The share is read against the rows'
    ``cross_win_frac``: how often the second before ANY settled block
    holds a crossing (``baseline()``)."""
    rows = []
    groups = defaultdict(list)
    for t in trans:
        if cell_of(t) != cell:
            continue
        if t["false_lock"] != "0" or t["on_code"] != "1":
            groups[(t["flag"], "any", "off the emitter")].append(t)
            continue
        slip = int(t["car_slips"]) > 0
        groups[
            (
                t["flag"],
                "up" if t["up"] == "1" else "down",
                "a fold crossing in the second before"
                if slip
                else "no crossing",
            )
        ].append(t)

    def med(v: list[float]) -> float:
        v = sorted(x for x in v if x == x)
        return v[len(v) // 2] if v else float("nan")

    def coasted(ts: Rows, k: str) -> float:
        return sum(t.get(k) == "1" for t in ts) / len(ts)

    def past(ts: Rows, x: float, k: str = "car_max_deg") -> float:
        return sum(float(t.get(k, "nan")) > x for t in ts) / len(ts)

    total = sum(len(v) for v in groups.values())
    for (flag, way, slip), ts in sorted(groups.items()):
        evm = [float(t["evm_db"]) - float(t["draw_evm_db"]) for t in ts]
        rows.append(
            [
                flag,
                way,
                slip,
                f"{len(ts)} of {total}",
                f"{med([float(t['car_max_deg']) for t in ts]):.0f}",
                f"{med([float(t['car_rms_deg']) for t in ts]):.1f}",
                f"{med([float(t['code_max_chip']) for t in ts]):.3f}",
                f"{med(evm):+.2f}",
                *(f"{past(ts, x):.0%}" for x in EXC_DEG),
                *(f"{past(ts, x, 'car_max_mem_deg'):.0%}" for x in EXC_DEG),
                f"{coasted(ts, 'coast_mem'):.0%}",
                f"{coasted(ts, 'coast_1s'):.0%}",
            ]
        )
    return rows


def baseline(rows: Rows, col: str = "cross_win_frac") -> str:
    """The share of settled on-emitter blocks whose second before holds a
    fold crossing (or, by ``col``, a carrier error past an excursion
    level), pooled over the draws (each draw's share, weighted equally:
    the draws are the same length)."""
    v = [float(r[col]) for r in rows if r.get(col, "nan") != "nan"]
    return f"{sum(v) / len(v):.1%}" if v else "n/a"


def baselines(rows: Rows) -> str:
    """The sentence every attribution table is read against."""
    exc = ", ".join(
        f"past {x} deg {baseline(rows, f'exc{x}_win_frac')}" for x in EXC_DEG
    )
    mem = ", ".join(
        f"past {x} deg {baseline(rows, f'exc{x}_mem_frac')}" for x in EXC_DEG
    )
    ms = 1e3 * float(rows[0].get("lock_mem_s", "nan"))
    return (
        "At a settled block on the emitter, the second before holds a fold "
        f"crossing {baseline(rows)} of the time, and a carrier error {exc}; "
        f"the symbol-lock statistic's memory before it ({ms:.0f} ms, 1 / "
        f"lock_alpha symbols) holds a carrier error {mem}, and a coasting "
        f"block {baseline(rows, 'coast_mem_frac')}: the rates a change's "
        "shares are read against."
    )


ATTR_HEAD = (
    "flag",
    "change",
    "carrier loop, 1 s before",
    "changes",
    "largest carrier error, deg (median)",
    "carrier rms, deg (median)",
    "largest code error, chip (median)",
    "EVM around it less the draw's, dB (median)",
    *(f"share with carrier error past {x} deg, 1 s before" for x in EXC_DEG),
    *(
        f"share past {x} deg within the lock statistic's memory before"
        for x in EXC_DEG
    ),
    "share coasting at it or within the lock statistic's memory before",
    "share coasting at it or in the second before",
)


def trace(
    cell: Cell,
    trans: Rows,
    out_dir: Path,
    held: bool = False,
    bn: float | None = None,
) -> Path:
    """A capture of the cell's first failing draw around its first change,
    drawn by the capture plotter: the loops' true errors with the fold and
    the baselines, the lock statistics with their rules (the receiver's
    and the demodulator's), the demodulator's carrier detector, the flags,
    the code-only windows shaded. ``held``: the first change of symbol lock
    with no fold crossing in the second before -- the pre-despread loop
    held, so the change is the demodulator's or its statistic's."""
    fc, es, rate, cw = cell
    order = sorted(
        (
            t
            for t in trans
            if cell_of(t) == cell
            and t["false_lock"] == "0"
            and t["on_code"] == "1"
        ),
        key=lambda t: (int(t["seed"]), float(t["t_s"])),
    )
    held_ones = [
        t for t in order if t["flag"] == "sym" and int(t["car_slips"]) == 0
    ]
    # A cell with no change draws seed 1 over the same 4 s window a
    # change would get, so every cell has its trace.
    first = (held_ones if held else order)[:1] or order[:1]
    seed = int(first[0]["seed"]) if first else 1
    t0 = float(first[0]["t_s"]) if first else 17.0
    cwn = 1 if cw == "code_period" else 0
    cap_dir = ROOT / "build/pullin-captures"
    cap_dir.mkdir(parents=True, exist_ok=True)
    frm, to = max(t0 - 3.0, 0.0), t0 + 1.0
    run = subprocess.run(
        [
            str(VALIDATOR),
            "--telemetry",
            str(cap_dir),
            "--draw",
            f"{fc},{es},{rate},{cwn},{seed}",
            "--from",
            f"{frm}",
            "--to",
            f"{to}",
            "--emit",
            *(["--bn-carrier", f"{bn:g}"] if bn is not None else []),
        ],
        capture_output=True,
        text=True,
    )
    if run.returncode:
        sys.stderr.write(run.stderr + run.stdout)
        raise SystemExit("the capture run failed")
    stem = (
        f"pullin_{float(fc):.1f}GHz_{float(es):.0f}dB_{float(rate):.0f}Hzs_"
        f"{cw}_s{seed}" + (f"_bn{bn:g}" if bn is not None else "")
    )
    tlm = cap_dir / f"{stem}.tlm"
    rules = {
        r["rule"]: r
        for r in csv.DictReader(
            (cap_dir / f"{stem}.rules.csv").open(encoding="utf-8")
        )
    }
    row = csv_blocks(run.stdout)["pullin"][0]
    lag, sd = float(row["base_lag_deg"]), float(row["base_sd_deg"])
    lines = [
        f"err.car_deg={FOLD_DEG}:fold",
        f"err.car_deg={-FOLD_DEG}",
        f"err.car_deg={lag + sd:.2f}:lag + sd",
        f"err.car_deg={-lag - sd:.2f}",
        "err.code_chip=0.5:half the spacing",
        "err.code_chip=-0.5",
    ]
    for probe, name in (
        ("rx.sym.lock", "sym"),
        ("rx.car.lock", "car"),
        ("rx.code.lock", "code"),
        ("rx.demod.lock", "demod"),
        ("rx.demod.sync.lock", "demod_sync"),
    ):
        lines += [
            f"{probe}={rules[name]['up_thresh']}:up",
            f"{probe}={rules[name]['down_thresh']}:down",
        ]
    sys.path.insert(0, str(ROOT / "scripts"))
    import plot_capture

    png = out_dir / f"trace-{stem}.png"
    args = [
        str(tlm),
        "--time",
        "--shade",
        "truth.data",
        "--xlim",
        f"{frm},{to}",
        "-o",
        str(png),
        "err.car_deg",
        "err.code_chip",
        "rx.car.lock",
        "rx.sym.lock",
        "rx.code.lock",
        "rx.demod.lock",
        "rx.demod.sync.lock",
        "rx.demod.car.e",
        "rx.sym.locked",
        "rx.code.locked",
        "rx.had_lock",
        "rx.coast",
        "rx.hold",
        "rx.release",
        "rx.state",
        "rx.ho_src",
        "rx.handover_hz",
        "rx.refused",
        "--mark",
        "rx.state",
        "--names",
        "rx.state=" + ",".join(f"{k}:{v}" for k, v in STATES.items()),
        "--names",
        "rx.ho_src=" + ",".join(f"{k}:{v}" for k, v in SOURCES.items()),
        "--line",
        "rx.release=1:released",
    ]
    for ln in lines:
        args += ["--line", ln]
    if plot_capture.main(args):
        raise SystemExit("the plot failed")
    return png


#: The two carrier windows, each its color on a comparison figure.
WINDOWS = (("partials", "C0"), ("code_period", "C1"))


def fig_windows(
    data: Data, cells: list[Cell], rows: dict[Cell, Rows], out: Path
) -> None:
    """One cell under both carrier windows on shared axes: the carrier's
    true error against each window's own design lines (``base_lag_deg``,
    ``base_sd_deg``) and the fold, the carrier and symbol lock statistics
    against their rules, and the demodulator's carrier detector. Each is
    the fraction of the window's on-emitter blocks, so draws that lose
    blocks to a false lock do not shrink a curve; the counts are in the
    legend."""
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    fig, axes = plt.subplots(1, 4, figsize=(17, 3.8))
    color = dict(WINDOWS)
    for cell in cells:
        cw, c = cell[3], color[cell[3]]
        rs = rows[cell]
        nfl = sum(r["false_lock"] != "0" for r in rs)
        ok = [r for r in rs if r["false_lock"] == "0"]
        cross = sum(
            int(r["car_slips"]) for r in ok if r["t_sym_s"] != "nan"
        )  # in a link that locked: fold_cell()
        lag = float(ok[0]["base_lag_deg"])
        sd = float(ok[0]["base_sd_deg"])
        for ax, q in zip(
            axes, ("car.err_deg", "car.lock", "sym.lock", "demod.car.e")
        ):
            edges, counts, under, over = hist_of(data, cell, q)
            tot = sum(counts) + under + over
            lab = f"{cw}: {tot} blocks"
            if q == "car.err_deg":
                lab += f", {cross} fold crossings, {nfl} false lock(s)"
            elif q in dict(LOCK_STATS):
                down = float(
                    rule_of(data, cell, dict(LOCK_STATS)[q])["down_thresh"]
                )
                below = sum(
                    k for k, lo in zip(counts, edges) if lo + 1e-12 < down
                ) + (under if edges[0] < down else 0)
                lab += f", {below / tot:.1%} under down"
            ax.stairs(
                [k / tot for k in counts], edges, color=c, lw=1.2, label=lab
            )
            if q == "car.err_deg":
                for x, ls, txt in (
                    (sd, ":", f"{cw} design sd {sd:.1f}"),
                    (-sd, ":", None),
                    (lag, "--", f"design |lag| {lag:.2f}"),
                    (-lag, "--", None),
                ):
                    ax.axvline(x, color=c, ls=ls, lw=0.8, label=txt)
    axes[0].axvline(FOLD_DEG, color="C3", lw=0.8, label="fold")
    axes[0].axvline(-FOLD_DEG, color="C3", lw=0.8)
    axes[0].set_title("carrier error x sign(ramp), deg", fontsize=8)
    for ax, (q, name) in zip(
        axes[1:3], (("car.lock", "car"), ("sym.lock", "sym"))
    ):
        r = rule_of(data, cells[0], name)
        up, down = float(r["up_thresh"]), float(r["down_thresh"])
        ax.axvline(up, color="C2", ls="--", lw=1, label=f"up {up:g}")
        ax.axvline(down, color="C3", ls=":", lw=1, label=f"down {down:g}")
        ax.set_title(f"{q} against its rule (data blocks)", fontsize=8)
    axes[3].set_title(
        "demod.car.e: Im((z/|z|)^2), bounded by 1 (no truth)", fontsize=8
    )
    for ax in axes:
        ax.set_yscale("log")
        ax.set_ylabel("fraction of blocks", fontsize=7)
        ax.legend(fontsize=6)
        ax.tick_params(labelsize=7)
    fc, es, rate, _ = cells[0]
    n = len(rows[cells[0]])
    fig.suptitle(
        f"Carrier window, partials beside code_period -- {fc} GHz, {es} dB "
        f"Es/N0, +/-{rate} Hz/s; {n} draws each (same seeds); false locks "
        "and blocks off the emitter's code apart; front end spc 4, C/N0 at "
        "the antenna",
        fontsize=9,
    )
    fig.tight_layout()
    fig.savefig(out, dpi=110)
    plt.close(fig)


def windows(argv: list[str]) -> int:
    """One cell under both carrier windows, from a development run's
    shards: the side-by-side figure, the two attribution tables, and
    every change of the code-lock flag with the code loop's error."""
    d, out = Path(argv[0]), Path(argv[1])
    fc, es, rate = argv[2:5]
    data = merge_shards(
        [p.read_text(encoding="utf-8") for p in sorted(d.glob("s*.csv"))]
    )
    key = (f"{float(fc):.1f}", f"{float(es):.1f}", f"{abs(float(rate)):.0f}")
    cells = [(*key, cw) for cw, _ in WINDOWS]
    rows = {c: [r for r in data["pullin"] if cell_of(r) == c] for c in cells}
    out.mkdir(parents=True, exist_ok=True)
    fig_windows(data, cells, rows, out / "windows.png")
    sys.path.insert(0, str(DATA.parent))
    from _record_blocks import table

    summary = []
    for c in cells:
        rs = rows[c]
        ok = [r for r in rs if r["false_lock"] == "0"]
        summary.append(
            [
                c[3],
                str(len(rs)),
                str(len(rs) - len(ok)),
                str(sum(r["works"] == "1" for r in rs)),
                fold_cell(ok),
                str(sum(int(r["sym_tog"]) for r in ok)),
                str(sum(int(r["code_tog"]) for r in ok)),
                f"{float(ok[0]['base_sd_deg']):.1f}",
                baseline(ok),
            ]
        )
    text = [
        "Carrier window, partials beside code_period: "
        + cell_label(cells[0], len(rows[cells[0]])).replace(
            ", partials window;", ", each window;"
        )
        + " (same seeds).\n",
        table(
            (
                "window",
                "draws",
                "false locks",
                "works",
                "fold crossings (not false locks)",
                "symbol-lock changes",
                "code-lock changes",
                "design sd, deg",
                "settled blocks with a crossing in the second before",
            ),
            summary,
        ),
    ]
    for c in cells:
        text += [
            f"\n## {c[3]}: changes after sustained lock\n",
            baselines([r for r in rows[c] if r["false_lock"] == "0"]) + "\n",
            table(ATTR_HEAD, attribution(data["transition"], c)),
        ]
    code = [
        [
            t["carrier_window"],
            t["seed"],
            "up" if t["up"] == "1" else "down",
            t["t_s"],
            f"{float(t['stat']):.2f}",
            t["down_thresh"],
            t["code_max_chip"],
            t["car_max_deg"],
            t["car_slips"],
            "off the emitter"
            if t["false_lock"] != "0" or t["on_code"] != "1"
            else "on",
        ]
        for c in cells
        for t in data["transition"]
        if cell_of(t) == c and t["flag"] == "code"
    ]
    text += [
        "\n## Every code-lock change\n",
        table(
            (
                "window",
                "seed",
                "change",
                "t, s",
                "code.lock",
                "rule (up = down)",
                "largest code error, 1 s before, chip",
                "largest carrier error, 1 s before, deg",
                "fold crossings, 1 s before",
                "draw",
            ),
            code,
        ),
    ]
    (out / "windows.md").write_text("\n".join(text) + "\n", encoding="utf-8")
    return 0


HEADER = (
    ROOT / "native/inc/doppler/async_dsss_receiver/async_dsss_receiver_core.h"
)


def header_codes(pattern: str) -> dict[int, str]:
    """A coded value's names, read from the receiver's header (one
    declaration): ``pattern`` matches a name and its value."""
    text = HEADER.read_text(encoding="utf-8")
    return {
        int(v): n.lower().replace("_", " ")
        for n, v in re.findall(pattern, text)
    }


#: The lifecycle (async_dsss_rx_state_t) and the hand-over's sources
#: (ASYNC_DSSS_RX_HO_*), by value.
STATES = header_codes(r"\n  ASYNC_DSSS_RX_([A-Z]+)\s*=\s*(\d)")
SOURCES = header_codes(r"#define ASYNC_DSSS_RX_HO_([A-Z]+) (\d)")
#: The hand-over's sources as the rows carry them (a value's string).
HO_SRC = {str(k): v for k, v in SOURCES.items() if k}


def _pct(v: list[float]) -> tuple[float, float, float]:
    """Median, 90th percentile and worst of |v| (nearest rank)."""
    a = sorted(abs(x) for x in v)
    return (
        a[len(a) // 2],
        a[min(len(a) - 1, int(0.9 * len(a)))],
        a[-1],
    )


def on_code(r: dict[str, str]) -> bool:
    """A seed on the emitter's code: within half a chip of it. One past it
    is the search taking something else (a sidelobe, a noise peak), a
    different population, counted apart."""
    return (
        r["seed_code_chip"] != "nan" and abs(float(r["seed_code_chip"])) <= 0.5
    )


def fig_handover(rows: Rows, cell: Cell, out: Path) -> None:
    """How close acquisition brings the receiver before any loop sees it:
    the search's own seed (Doppler, code phase) and the refine's
    hand-over (Doppler by source, the code loop's starting error), the
    hand-over's age, and its Doppler error beside the ramp's drift over
    that age; each against what it is read by, from the run's own
    columns."""
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    def col(k: str, rs: Rows = rows) -> list[float]:
        return [float(r[k]) for r in rs if r[k] != "nan"]

    b = {k: float(rows[0][k]) for k in rows[0] if k.startswith("base_")}
    every = rows
    rows = [r for r in every if on_code(r)]
    off = len(every) - len(rows)
    row, pull = b["base_row_hz"], b["base_pullin_hz"]
    half_s, half_sp = b["base_half_sample_chip"], b["base_half_spacing_chip"]
    fig, axes = plt.subplots(2, 3, figsize=(15, 6.4))

    def lines(ax: Any, xs: list[tuple[float, str, str, str]]) -> None:
        for x, ls, c, lab in xs:
            ax.axvline(x, color=c, ls=ls, lw=0.9, label=lab)
            ax.axvline(-x, color=c, ls=ls, lw=0.9)

    dop = [
        (row / 2, "--", "C3", f"half the search row {row / 2:.1f}"),
        (row / 4, ":", "C3", f"its 2x interpolation {row / 4:.1f}"),
        (pull, "-", "C2", f"carrier pull-in {pull:.0f}"),
    ]
    chips = [
        (half_s, "--", "C3", f"half a search sample {half_s:.3f}"),
        (half_sp, ":", "C2", f"half the Dll spacing {half_sp:.3f}"),
    ]
    ax = axes[0, 0]
    ax.hist(col("seed_err_hz"), bins=30)
    lines(ax, dop)
    ax.set_title("the search's seed: Doppler error, Hz", fontsize=8)
    ax = axes[0, 1]
    ax.hist(col("seed_code_chip"), bins=30)
    lines(ax, chips)
    ax.set_title("the search's seed: code-phase error, chip", fontsize=8)
    ax = axes[0, 2]
    srcs = sorted({r["ho_src"] for r in rows if r["ho_src"] in HO_SRC})
    n = len(col("ho_err_hz"))
    ax.hist(
        [
            col("ho_err_hz", [r for r in rows if r["ho_src"] == k])
            for k in srcs
        ],
        bins=30,
        stacked=True,
        label=[
            f"{HO_SRC[k]}: "
            f"{sum(r['ho_src'] == k for r in rows) / max(n, 1):.0%}"
            for k in srcs
        ],
    )
    lines(ax, dop[2:])
    ax.set_title(
        f"the hand-over: Doppler error by source, Hz (1/(4T) = "
        f"{b['base_unamb_hz']:.0f}, off the axis)",
        fontsize=8,
    )
    ax = axes[1, 0]
    ax.hist(col("ho_code_chip"), bins=30)
    lines(ax, chips)
    ax.set_title(
        "the code loop's start: code error at the first block after the "
        "hand-over, chip",
        fontsize=8,
    )
    ax = axes[1, 1]
    ax.hist([1e3 * v for v in col("ho_age_s")], bins=30)
    ax.set_title("the hand-over's age (seed to hand-over), ms", fontsize=8)
    ax = axes[1, 2]
    pairs = [
        (float(r["rate_hz_s"]) * float(r["ho_age_s"]), float(r["ho_err_hz"]))
        for r in rows
        if r["ho_age_s"] != "nan" and r["ho_err_hz"] != "nan"
    ]
    ax.scatter([d for d, _ in pairs], [e for _, e in pairs], s=8)
    if any(d for d, _ in pairs):
        lo = min(d for d, _ in pairs)
        hi = max(d for d, _ in pairs)
        ax.plot([lo, hi], [-lo, -hi], color="C3", lw=0.8, label="-drift")
        ax.plot(
            [lo, hi], [-lo / 2, -hi / 2], "C3:", lw=0.8, label="-drift / 2"
        )
    ax.set_xlabel("the ramp's drift over the age, Hz", fontsize=7)
    ax.set_ylabel("hand-over Doppler error, Hz", fontsize=7)
    ax.set_title("the hand-over's error beside the drift", fontsize=8)
    for a in axes.flat:
        a.tick_params(labelsize=7)
        if a.get_legend_handles_labels()[0]:
            a.legend(fontsize=6)
    fig.suptitle(
        "Acquisition to hand-over, search-seeded -- "
        + cell_label(cell, len(rows))
        + f"; {off} seed(s) off the emitter's code (> 0.5 chip) apart",
        fontsize=9,
    )
    fig.tight_layout()
    fig.savefig(out, dpi=110)
    plt.close(fig)


def handover(argv: list[str]) -> int:
    """Every cell's acquisition-to-hand-over set, from a development run's
    shards: per cell a figure, and one table of median / 90th percentile /
    worst. The search and the refine run before the carrier window is
    used, so a draw's two windows hand over identically; that is checked,
    and the code-period rows are drawn."""
    d, out = Path(argv[0]), Path(argv[1])
    data = merge_shards(
        [p.read_text(encoding="utf-8") for p in sorted(d.glob("s*.csv"))]
    )
    keys = ("seed_err_hz", "seed_code_chip", "ho_err_hz", "ho_code_chip")
    by: dict[tuple[str, str, str, str], dict[str, str]] = {}
    for r in data["pullin"]:
        k = (r["fc_ghz"], r["esn0_db"], r["rate_hz_s"], r["seed"])
        v = {x: r[x] for x in (*keys, "ho_src", "ho_age_s")}
        if k in by and by[k] != v:
            raise SystemExit(f"the two windows hand over apart at {k}")
        by[k] = v
    out.mkdir(parents=True, exist_ok=True)
    cells = sorted(
        {
            cell_of(r)
            for r in data["pullin"]
            if r["carrier_window"] == "code_period"
        }
    )
    tab = []
    for c in cells:
        rows = [r for r in data["pullin"] if cell_of(r) == c]
        fc, es, rate, _ = c
        fig_handover(rows, c, out / f"handover_{fc}GHz_{es}dB_{rate}Hzs.png")

        every = rows
        rows = [r for r in every if on_code(r)]

        def stat(k: str, fmt: str, rs: Rows = rows) -> str:
            v = [float(r[k]) for r in rs if r[k] != "nan"]
            return " / ".join(fmt.format(x) for x in _pct(v)) if v else "n/a"

        n = len(rows)
        pull = float(rows[0]["base_pullin_hz"])
        past = sum(
            r["ho_err_hz"] != "nan" and abs(float(r["ho_err_hz"])) > pull
            for r in rows
        )
        shares = ", ".join(
            f"{HO_SRC[k]} {sum(r['ho_src'] == k for r in rows) / n:.0%}"
            for k in sorted(HO_SRC)
            if any(r["ho_src"] == k for r in rows)
        )
        tab.append(
            [
                f"{fc} GHz {es} dB "
                + ("static" if float(rate) == 0 else f"+/-{rate} Hz/s"),
                f"{n} (+{len(every) - n} off code)",
                stat("seed_err_hz", "{:.1f}"),
                stat("seed_code_chip", "{:.3f}"),
                stat("ho_err_hz", "{:.1f}"),
                f"{past} of {n} (> {pull:.0f} Hz)",
                shares,
                stat("ho_code_chip", "{:.3f}"),
                stat("ho_age_s", "{:.3f}"),
            ]
        )
    sys.path.insert(0, str(DATA.parent))
    from _record_blocks import table

    (out / "handover.md").write_text(
        "Acquisition to hand-over, search-seeded (the shipped search on the "
        "scene, from cold), against the channel's truth; each |error| as "
        "median / 90th percentile / worst over the seeds on the emitter's "
        "code (within half a chip; the rest are counted apart); front end "
        "spc 4, C/N0 at the antenna.\n\n"
        + table(
            (
                "cell",
                "draws",
                "seed Doppler, Hz",
                "seed code, chip",
                "hand-over Doppler, Hz",
                "past the carrier pull-in",
                "hand-over source",
                "code loop's start, chip",
                "age, s",
            ),
            tab,
        )
        + "\n",
        encoding="utf-8",
    )
    return 0


def loop_snr_db(sd_deg: float) -> float:
    """The carrier loop's SNR on the squared phase, the quantity that
    slips: 1 / (4 var), var the phase error's variance in rad^2."""
    import math

    var = math.radians(sd_deg) ** 2
    return 10.0 * math.log10(1.0 / (4.0 * var))


def bn_cells(d: Path) -> tuple[Data, dict[Cell, Rows]]:
    """A run's sections and its rows by cell."""
    data = merge_shards(
        [p.read_text(encoding="utf-8") for p in sorted(d.glob("s*.csv"))]
    )
    by: dict[Cell, Rows] = defaultdict(list)
    for r in data["pullin"]:
        by[cell_of(r)].append(r)
    return data, by


def fold_cell(ok: Rows) -> str:
    """Fold crossings over the draws that reached symbol lock, then in
    parentheses those of the draws that never did. A draw that never
    locked and was not called false (seed 8 at 2.2 GHz, a data-template
    hand-over 403 Hz off) spins its carrier for the whole run, and its
    count would swamp the cell, so it is shown apart.

    >>> fold_cell(
    ...     [
    ...         {"car_slips": "2", "t_sym_s": "0.3"},
    ...         {"car_slips": "5000", "t_sym_s": "nan"},
    ...     ]
    ... )
    '2 (+5000 never locked)'
    >>> fold_cell([{"car_slips": "0", "t_sym_s": "0.3"}])
    '0'
    """
    locked = sum(int(r["car_slips"]) for r in ok if r["t_sym_s"] != "nan")
    never = [r for r in ok if r["t_sym_s"] == "nan"]
    spun = sum(int(r["car_slips"]) for r in never)
    return f"{locked}" + (f" (+{spun} never locked)" if never else "")


def bn_row(data: Data, cell: Cell, rows: Rows) -> list[str]:
    """One cell at one bandwidth: the columns the comparison reads."""
    import statistics as st

    ok = [r for r in rows if r["false_lock"] == "0"]
    n = len(rows)
    pull = float(rows[0]["base_pullin_hz"])
    outside = [
        r
        for r in rows
        if r["ho_err_hz"] != "nan" and abs(float(r["ho_err_hz"])) > pull
    ]

    def med(k: str, rs: Rows) -> float:
        v = [float(r[k]) for r in rs if r[k] != "nan"]
        return st.median(v) if v else float("nan")

    sgn = [
        (-1.0 if float(r["rate_hz_s"]) < 0 else 1.0) * float(r["car_mean_deg"])
        for r in ok
        if r["car_mean_deg"] != "nan"
    ]
    sd = med("car_sd_deg", ok)
    base = float(rows[0]["base_sd_deg"])
    edges, counts, under, _ = hist_of(data, cell, "car.lock")
    down = float(rule_of(data, cell, "car")["down_thresh"])
    tot = sum(counts) + under
    below = sum(k for k, lo in zip(counts, edges) if lo + 1e-12 < down) + under
    trans = [
        t
        for t in data["transition"]
        if cell_of(t) == cell
        and t["flag"] == "sym"
        and t["up"] == "0"
        and t["false_lock"] == "0"
        and t["on_code"] == "1"
    ]

    def share(x: int) -> str:
        if not trans:
            return "-"
        k = sum(float(t.get("car_max_mem_deg", "nan")) > x for t in trans)
        return f"{k / len(trans):.0%}"

    coast = sum(t.get("coast_mem") == "1" for t in trans)
    t_sym = [float(r["t_sym_s"]) for r in rows if r["t_sym_s"] != "nan"]
    return [
        f"{float(rows[0]['bn_car']):g}",
        f"{sum(r['works'] == '1' for r in rows)}/{n}",
        str(n - len(ok)),
        f"{len(outside)} ({sum(r['works'] == '1' for r in outside)} work)",
        f"{med('t_sym_s', rows):.2f} / "
        + (f"{max(t_sym):.2f}" if t_sym else "n/a")
        + f" ({sum(r['t_sym_s'] == 'nan' for r in rows)} never)",
        (f"{sum(sgn) / len(sgn):+.2f}" if sgn else "n/a")
        + f" / {float(rows[0]['base_lag_deg']):.2f}",
        f"{sd:.1f} / {base:.1f}",
        f"{loop_snr_db(sd):.1f} / {loop_snr_db(base):.1f}",
        fold_cell(ok),
        f"{below / tot:.1%}" if tot else "n/a",
        f"{sum(int(r['sym_tog']) for r in ok) / max(len(ok), 1):.1f}",
        f"{sum(int(r['code_tog']) for r in ok) / max(len(ok), 1):.2f}",
        f"{len(trans)}: {share(45)} / {share(60)} "
        f"(base {baseline(ok, 'exc45_mem_frac')} / "
        f"{baseline(ok, 'exc60_mem_frac')})",
        f"{coast} (base {baseline(ok, 'coast_mem_frac')})",
    ]


def bn(argv: list[str]) -> int:
    """Cells side by side at two carrier-loop bandwidths: each DIR one
    run (its rows' bn_car say which), OUT the table."""
    out = Path(argv[-1])
    runs = [bn_cells(Path(a)) for a in argv[:-1]]
    cells = sorted(set().union(*(set(b) for _, b in runs)))
    sys.path.insert(0, str(DATA.parent))
    from _record_blocks import table

    body = []
    for c in cells:
        for data, by in runs:
            if c in by:
                fc, es, _, cw = c
                body.append([f"{fc} {es} dB {cw}", *bn_row(data, c, by[c])])
    head = (
        "cell",
        "bn",
        "works",
        "false locks",
        "hand-over past the pull-in",
        "t to sustained sym lock, s (median / worst)",
        "carrier mean x sign(ramp) / design lag, deg",
        "carrier sd measured / design, deg (median draw)",
        "loop SNR measured / design, dB (1/(4 var))",
        "fold crossings",
        "car.lock under down",
        "sym-lock changes / draw",
        "code-lock changes / draw",
        "sym drops: past 45 / 60 deg in 10 ms before",
        "sym drops coasting (10 ms)",
    )
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(
        "Carrier-loop bandwidth, side by side (development data; "
        "false locks and blocks off the emitter apart where stated).\n\n"
        + table(head, body)
        + "\n"
        + sizing_note(cells),
        encoding="utf-8",
    )
    return 0


def sizing_note(cells: list[Cell]) -> str:
    """The label every table with a 2.8 GHz 10 dB cell carries until the
    code-lock sizing fix lands: those misses are that, not the carrier.

    >>> sizing_note([("2.8", "10.0", "0", "code_period")])[:12]
    '\\n2.8 GHz 10 '
    >>> sizing_note([("2.2", "10.0", "0", "partials")])
    ''
    """
    if not any(c[0] == "2.8" and float(c[1]) == 10.0 for c in cells):
        return ""
    return (
        "\n2.8 GHz 10 dB: the draws that do not work are the code-lock "
        "sizing (the Dll's look count from the antenna C/N0, not the "
        "despread one), a separate fix, the same at every carrier-loop "
        "bandwidth here; they are not the carrier loop.\n"
    )


def lag(argv: list[str]) -> int:
    """Per cell and window, the carrier error window's end against the
    update that uses it (the run's ``lag.epoch_T`` bins, code periods)
    and the share of updates whose code-period window was the natural
    one (``win_natural_frac``): DIR [DIR ...] OUT.json, the bins kept so
    a model can be evaluated over them."""
    out = Path(argv[-1])
    res = []
    for a in argv[:-1]:
        data, by = bn_cells(Path(a))
        for c, rows in sorted(by.items()):
            edges, counts, _, _ = hist_of(data, c, "lag.epoch_T")
            tot = sum(counts)
            nat = [
                float(r["win_natural_frac"])
                for r in rows
                if r.get("win_natural_frac", "nan") != "nan"
            ]
            res.append(
                {
                    "cell": list(c),
                    "bn": float(rows[0]["bn_car"]),
                    "edges": edges,
                    "counts": counts,
                    "updates": tot,
                    "mean_T": sum(
                        (lo + hi) / 2 * k
                        for lo, hi, k in zip(edges, edges[1:], counts)
                    )
                    / tot
                    if tot
                    else None,
                    "natural_share": sum(nat) / len(nat) if nat else None,
                }
            )
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(res, indent=1), encoding="utf-8")
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    fig, ax = plt.subplots(figsize=(8, 3.6))
    for r in res:
        if not r["updates"]:
            continue
        c = r["cell"]
        ax.stairs(
            [k / r["updates"] for k in r["counts"]],
            r["edges"],
            lw=0.8,
            label=f"{c[0]} GHz {c[1]} dB {c[3]} bn {r['bn']:g}: "
            f"{r['updates']} updates, natural {r['natural_share']:.0%}",
        )
    ax.set_xlabel(
        "the Dll epoch's end before the carrier update, code periods "
        "(code period window: plus the chunks the look-back borrowed)",
        fontsize=7,
    )
    ax.set_ylabel("fraction of updates", fontsize=7)
    ax.set_title(
        "Carrier error window against its update, static, settled "
        "tracking; front end spc 4",
        fontsize=8,
    )
    ax.legend(fontsize=4.5, ncol=2)
    ax.tick_params(labelsize=7)
    fig.tight_layout()
    fig.savefig(out.with_suffix(".png"), dpi=110)
    plt.close(fig)
    return 0


def shape(argv: list[str]) -> int:
    """One cell's full set, from a development run's shards."""
    d, out = Path(argv[0]), Path(argv[1])
    fc, es, rate, cw = argv[2:6]
    cell = (
        f"{float(fc):.1f}",
        f"{float(es):.1f}",
        f"{abs(float(rate)):.0f}",
        cw,
    )
    data = merge_shards(
        [p.read_text(encoding="utf-8") for p in sorted(d.glob("s*.csv"))]
    )
    every = [r for r in data["pullin"] if cell_of(r) == cell]
    rows = [r for r in every if r["false_lock"] == "0"]
    nfl = len(every) - len(rows)
    offs = [
        float(r["off_code_frac"]) for r in rows if r["off_code_frac"] != "nan"
    ]
    if offs and sum(offs):
        OFF_SHARE[cell] = f"{sum(offs) / len(offs):.1%}"
    out.mkdir(parents=True, exist_ok=True)
    fig_stats(data, cell, len(rows), out / "stats.png", nfl)
    fig_stress(data, cell, rows, out / "stress.png", nfl)
    fig_times(every, cell, out / "times.png")
    tab = attribution(data["transition"], cell)
    sys.path.insert(0, str(DATA.parent))
    from _record_blocks import table

    (out / "attribution.md").write_text(
        "Changes after sustained lock, "
        + cell_label(cell, len(rows), nfl)
        + ". "
        + baselines(rows)
        + "\n\n"
        + table(ATTR_HEAD, tab)
        + "\n",
        encoding="utf-8",
    )
    built = float(
        re.search(
            r"#define ASYNC_DSSS_RX_BN_CARRIER ([0-9.]+)",
            HEADER.read_text(encoding="utf-8"),
        ).group(1)
    )
    ran = float(every[0].get("bn_car", built))
    trace(
        cell,
        data["transition"],
        out,
        held=cw == "code_period",
        bn=None if abs(ran - built) < 1e-12 else ran,
    )
    print(json.dumps({"draws": len(rows), "changes": len(data["transition"])}))
    return 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "shape":
        sys.exit(shape(sys.argv[2:]))
    if len(sys.argv) > 1 and sys.argv[1] == "lag":
        sys.exit(lag(sys.argv[2:]))
    if len(sys.argv) > 1 and sys.argv[1] == "bn":
        sys.exit(bn(sys.argv[2:]))
    if len(sys.argv) > 1 and sys.argv[1] == "handover":
        sys.exit(handover(sys.argv[2:]))
    if len(sys.argv) > 1 and sys.argv[1] == "windows":
        sys.exit(windows(sys.argv[2:]))
    sys.exit(__doc__)
