"""The release's final pull-in run, read as d-6fc84cc80's reader reads it.

d-6fc84cc80/read_d.py unchanged but for this paragraph and the usage line,
so the two runs' tables compare row for row. Reads ``pullin_rows.csv`` and
``k1_rows.csv`` beside this file (the run of 9ce208250; see STATUS) and
prints:

1. per cell (carrier / Es/N0 / signed rate): draws, works, lock inside
   20 s, the lock time's median / 90th percentile / worst, the fine
   hand-overs (ho_src 3) and their |error| median / worst, the first
   detection's median / worst, fold crossings before / after the declared
   lock, net pi reversals after it (and how many draws had one), the
   on-code seconds after it, the end-of-capture carrier residual's worst
   in Hz and in units of Rs/2 (1,500 Hz), false locks, the M2M4 median,
   draws whose code flag changed after the declared lock, and Lindsey's
   first-order mean time to a slip at the ruled 20 dB loop SNR;
2. per cell, the pooled error rate of the aligned, unslipped draws, the
   same with the draws that do not work left out, and the per-draw
   median, against BPSK's;
3. the eight named image draws at 10 dB, rate 0: the main run's fine
   hand-over (K = 5) beside the --fine-k1 control;
4. the 10 dB static draws that do not work, and why.

Nothing here computes a statistic of the receiver beyond counting the
rows and ordering them.

Usage:  python -I read_final.py
"""

import csv
import math
import statistics as st
from collections import defaultdict
from pathlib import Path

HERE = Path(__file__).resolve().parent
#: A code period, s: 3,500 / 2,750 samples at 11 MSa/s.
T = {"2.2": 3500 / 11e6, "2.8": 2750 / 11e6}
#: Half the symbol rate, Hz: the M = 2 loop's false-lock spacing.
RS_2 = 1500.0
#: The ruled loop SNR, dB, squaring loss counted (owner, 2026-11-22).
RHO_L_DB = 20.0


def rows(name: str) -> list[dict[str, str]]:
    with open(HERE / name, newline="") as fh:
        return list(csv.DictReader(fh))


def f(x: object) -> float:
    try:
        return float(x)
    except (TypeError, ValueError):
        return math.nan


def clean(v: list[float]) -> list[float]:
    return [x for x in v if not math.isnan(x)]


def med(v: list[float]) -> float:
    v = clean(v)
    return st.median(v) if v else math.nan


def q(v: list[float], p: float) -> float:
    v = sorted(clean(v))
    return v[min(len(v) - 1, math.ceil(p * len(v)) - 1)] if v else math.nan


def mx(v: list[float]) -> float:
    v = clean(v)
    return max(v) if v else math.nan


def log10_mttfs_s(rho_l_db: float, bl_hz: float) -> float:
    """log10 of Lindsey's first-order mean time to a slip, s.

    pi rho' I0(rho')^2 / (2 B_L), with I0(x) ~ e^x / sqrt(2 pi x). The
    M = 2 loop tracks 2 phi, so its phase process sees rho' = rho_L / 4:
    a factor of M^2 for the doubled phase, not the squaring loss again
    (the ruled 20 dB already counts that).
    """
    r = 10 ** (rho_l_db / 10) / 4.0
    ln_i0 = r - 0.5 * math.log(2 * math.pi * r)
    return (math.log(math.pi * r / (2 * bl_hz)) + 2 * ln_i0) / math.log(10)


def bpsk_ber(esn0_db: float) -> float:
    return 0.5 * math.erfc(math.sqrt(10 ** (esn0_db / 10)))


def main() -> None:
    R = rows("pullin_rows.csv")
    cells = defaultdict(list)
    for r in R:
        cells[(r["fc_ghz"], r["esn0_db"], r["rate_hz_s"])].append(r)
    order = sorted(cells, key=lambda k: (k[0], f(k[1]), f(k[2])))

    hdr = [
        "cell",
        "n",
        "works",
        "lock20",
        "t_lock med/p90/max s",
        "ho_src 3",
        "fine_err med/max Hz",
        "first det med/max s",
        "slips pre/post",
        "net rev post (draws)",
        "post_s",
        "end_resid max Hz (Rs/2)",
        "false_lock",
        "m2m4 med dB",
        "code_tog>0 after lock",
        "MTTFS at 20 dB",
    ]
    print("| " + " | ".join(hdr) + " |")
    print("|" + "---|" * len(hdr))
    for key in order:
        c = cells[key]
        lk = [r for r in c if r["lock20"] == "1"]
        tl = [f(r["t_lock_s"]) for r in c]
        fe = [abs(f(r["fine_err_hz"])) for r in c if r["ho_src"] == "3"]
        sd = [f(r["seed_t_s"]) for r in c]
        nrev = [abs(int(f(r["net_rev_post"]))) for r in lk]
        erm = mx([abs(f(r["end_resid_hz"])) for r in c])
        bl = f(c[0]["bn_car"]) / T[key[0]]
        print(
            f"| {key[0]}/{key[1]} @{key[2]} | {len(c)} | "
            f"{sum(r['works'] == '1' for r in c)} | {len(lk)} | "
            f"{med(tl):.2f}/{q(tl, 0.9):.2f}/{mx(tl):.2f} | "
            f"{sum(r['ho_src'] == '3' for r in c)} | "
            f"{med(fe):.2f}/{mx(fe):.2f} | {med(sd):.3f}/{mx(sd):.3f} | "
            f"{sum(int(f(r['slips_pre'])) for r in lk)}/"
            f"{sum(int(f(r['slips_post'])) for r in lk)} | "
            f"{sum(nrev)} ({sum(x > 0 for x in nrev)}) | "
            f"{sum(f(r['post_s']) for r in lk):.0f} | "
            f"{erm:.1f} ({erm / RS_2:.3f}) | "
            f"{sum(r['false_lock'] == '1' for r in c)} | "
            f"{med([f(r['m2m4_db']) for r in c]):.1f} | "
            f"{sum(int(f(r['code_tog'])) > 0 for r in lk)} | "
            f"1e{log10_mttfs_s(RHO_L_DB, bl):.0f} s "
            f"(B_L {bl:.1f} Hz) |"
        )

    print("\nerror rate, aligned and unslipped draws:")
    print("| cell | pooled | pooled, works only | per-draw median | BPSK |")
    print("|---|---|---|---|---|")
    for key in order:
        sc = [
            r
            for r in cells[key]
            if r["aligned"] == "1" and r["slipped"] != "1"
        ]

        def pool(rs: list[dict[str, str]]) -> float:
            e = sum(int(r["errors"]) for r in rs)
            n = sum(int(r["scored"]) for r in rs)
            return e / n if n else math.nan

        per = [
            int(r["errors"]) / int(r["scored"])
            for r in sc
            if int(r["scored"]) > 0
        ]
        print(
            f"| {key[0]}/{key[1]} @{key[2]} | {pool(sc):.2e} | "
            f"{pool([r for r in sc if r['works'] == '1']):.2e} | "
            f"{med(per):.2e} | {bpsk_ber(f(key[1])):.2e} |"
        )

    img = {("2.2", s) for s in (4, 5, 13, 46, 50)}
    img |= {("2.8", s) for s in (7, 31, 46)}
    k1 = {(r["fc_ghz"], int(r["seed"])): r for r in rows("k1_rows.csv")}
    print("\nimage draws, 10 dB rate 0: K = 5 (the run) and K = 1 (control)")
    for r in R:
        k = (r["fc_ghz"], int(r["seed"]))
        if k in img and r["esn0_db"] == "10.0" and f(r["rate_hz_s"]) == 0:
            c = k1.get(k, {})
            print(
                f"  {k[0]} GHz seed {k[1]}: seed error "
                f"{f(r['seed_err_hz']):.0f} Hz; K = 5 ho_src {r['ho_src']} "
                f"error {f(r['ho_err_hz']):.2f} Hz; K = 1 ho_src "
                f"{c.get('ho_src')} error {f(c.get('ho_err_hz')):.2f} Hz"
            )

    print("\n10 dB static draws that do not work:")
    for r in R:
        if (
            r["esn0_db"] == "10.0"
            and f(r["rate_hz_s"]) == 0
            and r["works"] != "1"
        ):
            print(
                f"  {r['fc_ghz']} GHz seed {r['seed']}: lock20 "
                f"{r['lock20']} aligned {r['aligned']} slipped "
                f"{r['slipped']} end_unsynced {r['end_unsynced']} errors "
                f"{r['errors']}/{r['scored']} slips pre/post "
                f"{r['slips_pre']}/{r['slips_post']} sym_tog {r['sym_tog']} "
                f"code_tog {r['code_tog']} car_tog {r['car_tog']} t_lock "
                f"{f(r['t_lock_s']):.2f} s"
            )


if __name__ == "__main__":
    main()
