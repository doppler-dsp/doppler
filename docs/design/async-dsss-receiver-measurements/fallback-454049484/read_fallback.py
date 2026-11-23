"""The hand-over ring's fallback sweep, read: the evidence it was deleted on.

Reads ``fixed.txt`` and ``unfixed.txt`` beside this file (two runs of
``validate_async_dsss_receiver_refine --fallback-sweep`` at 454049484, the
last commit that had the fallback; ``STATUS`` names how each was built) and
prints, for each, the sweep's own counts line and how many geometries
reached the fallback, by sample rate and by symbol rate.

The sweep: CODE7 at 1 Mcps, seeded at phase 0 / 0 Hz, fed complex unit
noise in one call; 8 sample rates x 5 symbol rates x 3
refine_max_error_db x 11 seeds = 1,320 geometries. A geometry reached the
fallback when the refine decided further past its estimate than the
hand-over ring held. With the refine Dll's output sized for two epochs
(#3281) none did; with it at one epoch's dumps, which dropped a code
period when the epoch ended on a call's edge, 423 did.

Nothing here computes a statistic beyond counting the rows.

Usage:  python -I read_fallback.py
"""

from collections import Counter
from pathlib import Path

HERE = Path(__file__).resolve().parent


def main() -> None:
    for name in ("fixed", "unfixed"):
        lines = (HERE / f"{name}.txt").read_text(encoding="utf-8")
        rows = [ln for ln in lines.splitlines() if "reached:" in ln]
        summary = [ln for ln in lines.splitlines() if "fallback sweep:" in ln]
        print(f"{name}: {summary[0].strip() if summary else 'no summary'}")
        if not rows:
            continue
        by_fs = Counter(
            r.split("reached: ")[1].split(" samples")[0] for r in rows
        )
        by_sym = Counter(r.split("chip, ")[1].split(" sym/s")[0] for r in rows)
        print("  by samples a chip:", dict(sorted(by_fs.items())))
        print("  by symbol rate:", dict(sorted(by_sym.items())))


if __name__ == "__main__":
    main()
