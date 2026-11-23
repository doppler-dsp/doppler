"""The long-dropout row, read: how long a held receiver rides back on.

The run (6b54f4a54, a clean tree; see STATUS):
validate_async_dsss_receiver_lock --longdrop --emit in 10 shards. At the
415.1-B-1 point, 10 dB, both carriers, the release armed at 2.0 s: a
settled trial's signal is cut for 0.1 / 0.2 / 0.5 / 1 / 2 s inside the
frame, then returned and watched for 1 s. Each length reruns the trial
from the same seeds. Per row: the Dll's chip-phase error against the
truth at the cut and at the return (drift_chip, the hold's), and the
outcome (0 reacquired, 1 released, 2 neither: tracking, not back on the
emitter by the watch's end).

Printed:

1. Per carrier and length: settled trials, reacquired, released,
   neither, and the drift at return (median and range).
2. Per trial: its drift rate (drift over the dropout, chips/s) at each
   length, so a rate that is the trial's own, fixed through the hold,
   reads the same across lengths.
3. The largest |drift| that reacquired and the smallest that did not.

Counts and orders rows only.

Usage:  python -I read_ld.py
"""

import csv
import statistics as st
from collections import Counter, defaultdict
from pathlib import Path

HERE = Path(__file__).resolve().parent


def main() -> None:
    with (HERE / "longdrop.csv").open(newline="", encoding="utf-8") as fh:
        rows = [r for r in csv.DictReader(fh) if r["settled"] == "1"]
    by = defaultdict(list)
    for r in rows:
        by[(r["fc_ghz"], float(r["drop_s"]))].append(r)
    print(
        "| fc / dropout s | settled | reacquired | released | neither | "
        "drift at return, chips: median [min, max] |"
    )
    for key in sorted(by):
        c = by[key]
        o = Counter(r["outcome"] for r in c)
        d = [float(r["drift_chip"]) for r in c]
        print(
            f"| {key[0]} / {key[1]:.1f} | {len(c)} | {o['0']} | {o['1']} | "
            f"{o['2']} | {st.median(d):+.3f} [{min(d):+.3f}, "
            f"{max(d):+.3f}] |"
        )
    print("\nper trial, drift rate chips/s at each length (outcome):")
    tr = defaultdict(dict)
    for r in rows:
        tr[(r["fc_ghz"], int(r["seed"]))][float(r["drop_s"])] = r
    for key in sorted(tr):
        cells = " ".join(
            f"{d:.1f}:{float(r['drift_chip']) / d:+.2f}({r['outcome']})"
            for d, r in sorted(tr[key].items())
        )
        print(f"  {key[0]} seed {key[1]:2d}: {cells}")
    back = [abs(float(r["drift_chip"])) for r in rows if r["outcome"] == "0"]
    lost = [abs(float(r["drift_chip"])) for r in rows if r["outcome"] == "2"]
    print(
        f"\nlargest |drift| reacquired {max(back):.3f} chip; smallest "
        f"|drift| not back (neither) {min(lost):.3f} chip"
    )


if __name__ == "__main__":
    main()
