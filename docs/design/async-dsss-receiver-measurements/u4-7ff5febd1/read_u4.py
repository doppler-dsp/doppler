"""The carrier lock's count and the design sections, read.

Reads ``design.txt`` and ``rule.csv`` beside this file (the run of
7ff5febd1; ``STATUS`` names the commands) and prints:

1. ``# carrier_lock``: per carrier, the count the floor's receiver is built
   with and one fewer, the false-alarm rate measured on noise against Pfa
   1e-3 and the miss rate measured at the floor against 1 - Pd (0.01),
   each with its Wilson 95% interval as the validator printed it, and the
   loop's measured phase variance against the model's;
2. the carrier rule's rows from ``rule.csv`` (the threshold the cells were
   built with), beside the ``# carrier_lock`` threshold at the count;
3. ``# pass``, ``# search`` and ``# design`` as committed, one line a row.

Nothing here computes a statistic beyond what the validator printed: the
verdict columns compare its printed interval ends with the design rates.

Usage:  python -I read_u4.py
"""

import csv
import io
from pathlib import Path

HERE = Path(__file__).resolve().parent


def sections(path: Path) -> dict[str, list[dict[str, str]]]:
    """Split the validator's output into its ``# name`` CSV sections."""
    out: dict[str, list[str]] = {}
    name = None
    for line in path.read_text(encoding="utf-8").splitlines():
        if line.startswith("# "):
            name = line[2:].strip()
            out[name] = []
        elif name and line.strip():
            out[name].append(line)
    return {
        k: list(csv.DictReader(io.StringIO("\n".join(v))))
        for k, v in out.items()
        if v and "," in v[0]
    }


def main() -> None:
    d = sections(HERE / "design.txt")
    with open(HERE / "rule.csv", newline="", encoding="utf-8") as fh:
        rules = list(csv.DictReader(fh))

    print("1. The carrier lock at the floor (Es/N0 4 dB)")
    print(
        f"{'fc':>4} {'n':>3} {'Pfa meas [95%]':>30} {'holds 1e-3':>10}"
        f" {'miss meas [95%]':>30} {'<= 0.01':>8} {'var meas/model':>15}"
    )
    built: dict[str, str] = {}
    for r in d["carrier_lock"]:
        pfa, pd = float(r["pfa"]), float(r["pd"])
        e_ok = float(r["exceed_lo"]) <= pfa <= float(r["exceed_hi"])
        m_ok = float(r["miss_lo"]) <= 1.0 - pd
        vr = float(r["phase_var_meas"]) / float(r["phase_var_model"])
        built.setdefault(r["fc_ghz"], r["thresh"])
        print(
            f"{r['fc_ghz']:>4} {r['n_noncoh']:>3} "
            f"{r['exceed_rate']:>10} [{r['exceed_lo']}, {r['exceed_hi']}]"
            f" {e_ok!s:>10} "
            f"{r['miss_rate']:>10} [{r['miss_lo']}, {r['miss_hi']}]"
            f" {m_ok!s:>8} "
            f"{vr:>15.3f}"
        )

    print("\n2. The carrier rule the cells were built with")
    for r in rules:
        if r["rule"] != "car":
            continue
        print(
            f"  {r['fc_ghz']} GHz {r['esn0_db']:>4} dB"
            f" {r['rate_hz_s']:>3} Hz/s:"
            f" up {r['up_thresh']} (carrier_lock at the count:"
            f" {built.get(r['fc_ghz'])}), n_up {r['n_up']}, n_down"
            f" {r['n_down']}"
        )

    for name in ("pass", "search", "design"):
        print(f"\n3. # {name}")
        for r in d[name]:
            print("  " + ", ".join(f"{k}={v}" for k, v in r.items()))


if __name__ == "__main__":
    main()
