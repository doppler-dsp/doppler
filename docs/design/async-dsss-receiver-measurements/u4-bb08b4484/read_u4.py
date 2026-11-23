"""The carrier lock at the delivered bandwidth, and the design sections, read.

Reads ``design.txt`` and ``rule.csv`` beside this file (the run of
bb08b4484; ``STATUS`` names the commands) and prints:

1. ``# carrier_lock``: per carrier, at the floor and the receiver's 4
   segments (detector delay 1.75 updates), the count the receiver is built
   with (34 / 43) and one fewer. For each, the miss rate measured on a
   carrier_nda built from the receiver's own loop, against 1 - Pd (0.01):
   whether the interval's lower end reaches it, and whether the point
   estimate does. Also the loop's measured phase variance against the model
   it is sized at (the delivered bandwidth at delay 1.75; the harness's own
   loop has one update of delay).
2. ``# carrier_lock_pfa``: the false-alarm rate on noise, one 2,000,000-block
   read a carrier at the count, each against a Wilson interval at 95% for
   the two rows together (z 2.2414 a row). The short reads' pool is printed
   as information.
3. ``# carrier_lock_segments``: the count at every segment count tried.
4. The carrier rule's rows from ``rule.csv``, beside ``# carrier_lock``'s
   threshold at the count.
5. ``# pass``, ``# search`` and ``# design`` as committed.

Nothing here computes a statistic beyond what the validator printed: the
verdict columns compare its printed numbers with the design rates.

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

    print("1. The carrier lock at the floor, delivered bandwidth (miss)")
    print(
        f"{'fc':>4} {'n':>3} {'miss [95%]':>30} {'lower end <= 0.01':>18}"
        f" {'point <= 0.01':>14} {'var meas/model':>15}"
    )
    built: dict[str, str] = {}
    for r in d["carrier_lock"]:
        pd = float(r["pd"])
        lo_ok = float(r["miss_lo"]) <= 1.0 - pd
        pt_ok = float(r["miss_rate"]) <= 1.0 - pd
        vr = float(r["phase_var_meas"]) / float(r["phase_var_model"])
        built.setdefault(r["fc_ghz"], r["thresh"])
        print(
            f"{r['fc_ghz']:>4} {r['n_noncoh']:>3} "
            f"{r['miss_rate']:>10} [{r['miss_lo']}, {r['miss_hi']}]"
            f" {lo_ok!s:>18} {pt_ok!s:>14} {vr:>15.3f}"
        )

    print("\n2. The false-alarm rate on noise (asserted on the long reads)")
    for r in d["carrier_lock_pfa"]:
        if r["fc_ghz"] == "pooled_short":
            print(
                f"  the short reads pooled (information): {r['exceed']} of"
                f" {r['blocks']}, {r['rate']}"
            )
            continue
        ok = float(r["lo"]) <= 1e-3 <= float(r["hi"])
        print(
            f"  {r['fc_ghz']} GHz at {r['n_noncoh']}: {r['exceed']} of"
            f" {r['blocks']}, {r['rate']} [{r['lo']}, {r['hi']}]"
            f" (z {r['z']}): holds 1e-3 {ok}"
        )

    print("\n3. The count at every segment count tried")
    for r in d["carrier_lock_segments"]:
        print(
            f"  {r['fc_ghz']} GHz, {r['segments']:>2} segments"
            f" (accepted {r['accepted']}): delay {r['delay']},"
            f" variance {r['phase_var']}, count {r['n_noncoh']}"
        )

    print("\n4. The carrier rule the cells were built with")
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
        print(f"\n5. # {name}")
        for r in d[name]:
            print("  " + ", ".join(f"{k}={v}" for k, v in r.items()))


if __name__ == "__main__":
    main()
