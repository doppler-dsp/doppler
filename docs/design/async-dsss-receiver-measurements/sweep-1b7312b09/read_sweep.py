#!/usr/bin/env python3
"""The half-bandwidth sweep on carrier_nda, read against its predictions.

Reads the rows of ``validate_costas_pullin --half-bw`` (``shard-*.csv``) and
its ``--design`` output (``design.txt``) from one directory, and prints the
read against the four predictions fixed on docs/design/async-dsss-receiver.md
before the run ("The sweep, on carrier_nda at M = 2"):

1. slips: at or below a configuration's worst-start edge, 0 of 60 draws a
   sign cross two or more lock points; past it, some do, from the first grid
   point beyond the edge;
2. lock points crossed: a draw starting 3 sigma or more from the null
   crosses the number its own start predicts on the mean loop;
3. lock time: inside the edge, such a draw locks by the point's budget;
4. the control (10 B_L) never locks inside 20 s.

and two readings beside them: the configured loop's measured noise
bandwidth (B_eff design x (tail sd / derived sigma)^2, over the locked
draws inside the edge) and the .nco pin.

Usage::

    python3 read_sweep.py DIR
"""

import csv
import glob
import math
import statistics as st
import sys
from collections import defaultdict

Key = tuple[str, str]
Row = dict[str, str]


def design(path: str) -> tuple[dict[Key, dict], dict[Key, dict]]:
    """The design_sweep and design_budget tables, keyed (cell, den)."""
    cfg: dict[Key, dict] = {}
    bud: dict[Key, dict] = {}
    sec = None
    grid: list[float] = []
    with open(path, encoding="utf-8") as fh:
        lines = [ln.strip() for ln in fh]
    for line in lines:
        if line.startswith("#"):
            sec = line.split(":")[0][2:]
            continue
        if not line or "," not in line or line.startswith("cell,"):
            if line.startswith("cell,den,0"):
                grid = [float(x) for x in line.split(",")[2:]]
            continue
        f = line.split(",")
        if sec == "design_sweep":
            cfg[(f[0], f[1])] = {
                "bn": float(f[2]),
                "sigma": float(f[7]),
                "edge": float(f[8]),
            }
        elif sec == "design_budget":
            vals = f[2 : 2 + len(grid)]
            bud[(f[0], f[1])] = {
                g: (float(v) if v != "nan" else math.nan)
                for g, v in zip(grid, vals, strict=True)
            }
    return cfg, bud


def rows(d: str) -> list[Row]:
    """Every shard's rows, the comment lines skipped."""
    out: list[Row] = []
    for p in sorted(glob.glob(f"{d}/shard-*.csv")):
        with open(p, encoding="utf-8") as fh:
            body = [ln for ln in fh if not ln.startswith("#")]
        out += list(csv.DictReader(body))
    return out


def slips(r: Row) -> bool:
    """A slip: two or more lock points crossed."""
    return abs(int(r["n_meas"])) >= 2


def far(r: Row) -> bool:
    """A start 3 sigma or more from the null."""
    return float(r["null_deg"]) >= 3 * float(r["sigma_deg"])


def main(d: str) -> None:
    cfg, bud = design(f"{d}/design.txt")
    R = rows(d)
    pts: dict[tuple, list[Row]] = defaultdict(list)
    for r in R:
        key = (r["cell"], r["den"], float(r["frac"]), int(r["sign"]))
        pts[key].append(r)
    steps = sorted({k[2] for k in pts if k[2] < 10})
    print(f"rows {len(R)}, points {len(pts)}")

    print("\n## 1. Slips: draws crossing >= 2 lock points, by step")
    print("cell,den,edge_worst," + ",".join(f"{g:.2f}" for g in steps))
    refuted = []
    for key in sorted(cfg):
        c = cfg[key]
        line = [key[0], key[1], f"{c['edge']:.3f}"]
        first_past = None
        for g in steps:
            rs = pts.get((*key, g, 1), []) + pts.get((*key, g, -1), [])
            n, s = len(rs), sum(slips(r) for r in rs)
            line.append(f"{s}/{n}" if n else "-")
            if not n:
                continue
            if g <= c["edge"] and s:
                refuted.append((key, g, "inside the edge", s, n))
            if g > c["edge"] and first_past is None:
                first_past = (g, s, n)
        if first_past and first_past[1] == 0:
            refuted.append((key, first_past[0], "none past it", 0, 0))
        print(",".join(line))
    half = 0
    for k in cfg:
        rs = pts.get((*k, 0.5, 1), []) + pts.get((*k, 0.5, -1), [])
        half += any(slips(r) for r in rs)
    print(f"the owner's one half (0.50 B_L): slips at {half} of {len(cfg)}")
    print("refuted (1):", refuted or "none")

    print("\n## 2. Lock points crossed, starts >= 3 sigma from the null")
    # As written the prediction holds at every step; read inside the edge
    # and past it apart as well, since past it the mean loop spins and the
    # noise picks where it stops.
    agg = {"inside": [0, 0], "past": [0, 0]}
    bad: dict[Key, int] = defaultdict(int)
    for r in R:
        if float(r["frac"]) >= 10 or not far(r):
            continue
        key = (r["cell"], r["den"])
        side = "inside" if float(r["frac"]) <= cfg[key]["edge"] else "past"
        agg[side][1] += 1
        if int(r["n_meas"]) == int(r["n_pred"]):
            agg[side][0] += 1
        elif side == "inside":
            bad[key] += 1
    a = agg["inside"][0] + agg["past"][0]
    t = agg["inside"][1] + agg["past"][1]
    print(f"as written, every step: agree {a} of {t}")
    for side in ("inside", "past"):
        print(f"  {side} the edge: agree {agg[side][0]} of {agg[side][1]}")
    print(f"  disagreeing inside the edge: {dict(bad) or 'none'}")

    print("\n## 3. Lock time inside the edge, starts >= 3 sigma from null")
    print("cell,den,draws,by_budget,worst_over_budget_s,median_t_lock_s")
    late = 0
    for key in sorted(cfg):
        c, b = cfg[key], bud[key]
        n = ok = 0
        worst = 0.0
        tl: list[float] = []
        for (cell, den, g, _sg), rs in pts.items():
            if (cell, den) != key or g >= 10 or g > c["edge"]:
                continue
            budget = b.get(g, math.nan)
            if math.isnan(budget):
                continue
            for r in rs:
                if not far(r):
                    continue
                n += 1
                tlock = float(r["t_lock_s"])
                tl.append(tlock)
                if tlock <= budget:
                    ok += 1
                else:
                    worst = max(worst, tlock - budget)
        late += n - ok
        med = st.median(tl) if tl else math.nan
        print(f"{key[0]},{key[1]},{n},{ok},{worst:.4f},{med:.4f}")
    print(f"late in all: {late}")

    print("\n## 4. The control, 10 B_L: locked inside 20 s")
    ctl = [r for r in R if float(r["frac"]) >= 10]
    lk = sum(r["capped"] == "0" for r in ctl)
    print(f"{lk} of {len(ctl)} locked (predicted 0)")

    print("\n## The configured loop's bandwidth, from the jitter")
    print("cell,den,draws,sd_tail_rms_deg,sigma_deg,beff_over_design,ci95")
    for key in sorted(cfg):
        c = cfg[key]
        sd = [
            float(r["sd_tail_deg"])
            for (cell, den, g, _sg), rs in pts.items()
            if (cell, den) == key and g < 10 and g <= c["edge"]
            for r in rs
            if r["sd_tail_deg"] != "nan"
        ]
        if len(sd) < 2:
            continue
        rr = [x * x / c["sigma"] ** 2 for x in sd]
        ratio = st.fmean(rr)
        se = st.stdev(rr) / math.sqrt(len(rr))
        rms = math.sqrt(st.fmean(x * x for x in sd))
        print(
            f"{key[0]},{key[1]},{len(sd)},{rms:.3f},{c['sigma']:.3f},"
            f"{ratio:.3f},[{ratio - 1.96 * se:.3f},{ratio + 1.96 * se:.3f}]"
        )

    print("\n## The .nco pin")
    locked = [r for r in R if r["capped"] == "0"]
    worst_pin = max(float(r["pin_err_cyc"]) for r in locked)
    least_late = min(float(r["pin_late_cyc"]) for r in locked)
    over = [
        int(r["updates"]) for r in locked if float(r["pin_err_cyc"]) > 1e-3
    ]
    print(
        f"locked draws: worst {worst_pin:.2e} cyc, one update late at "
        f"least {least_late:.2e}; under 1e-3 on every draw of up to "
        f"{min(over) - 1 if over else 'any'} updates"
    )
    capped = [r for r in R if r["capped"] == "1"]
    if capped:
        worst_cap = max(float(r["pin_err_cyc"]) for r in capped)
        print(
            f"capped draws (all 20 s): worst {worst_cap:.2e} cyc, the float "
            "probe's rounding summed over up to 80k updates"
        )


if __name__ == "__main__":
    main(sys.argv[1])
