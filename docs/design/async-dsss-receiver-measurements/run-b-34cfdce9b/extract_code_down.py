"""The code flag's down runs after the declared lock, from pull-in captures.

Run once over the run's telemetry directories (kept with the run's logs,
not committed: about 165 MB a capture), it writes ``code_down_runs.csv``
beside this file: one row per capture and down run of ``rx.code.locked``
after the draw's declared lock (``t_lock_s`` of its ``# pullin`` row), and
a row of length 0 for a capture with none.

Usage:  python -I extract_code_down.py CAPTURES_DIR
        (CAPTURES_DIR holds one ``<fc>_<esn0>_<rate>_<seed>/`` directory
        and one ``<name>.csv`` row file a draw, as the run wrote them)
"""

import csv
import io
import json
import sys
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent


def t_lock(row_csv: Path) -> float:
    """The draw's declared lock, s, from its ``# pullin`` row."""
    text = row_csv.read_text(encoding="utf-8").split("# pullin\n")[1]
    rows = [line for line in text.splitlines() if line][:2]
    return float(
        next(csv.DictReader(io.StringIO("\n".join(rows))))["t_lock_s"]
    )


def down_runs(capture_dir: Path, tl: float) -> list[float]:
    """Lengths, s, of the runs with ``rx.code.locked`` down from ``tl``."""
    meta_path = next(capture_dir.glob("*.tlm-meta"))
    meta = json.loads(meta_path.read_text(encoding="utf-8"))
    dt = np.dtype([(n, t) for n, t in meta["dtype"]])
    a = np.fromfile(str(meta_path)[: -len("-meta")], dtype=dt)
    c = a[a["probe"] == meta["probes"]["rx.code.locked"]]
    t = c["n"] / meta["fs"]
    up = c["value"] > 0.5
    keep = t >= tl
    t, up = t[keep], up[keep]
    runs, start = [], None
    for ti, ui in zip(t, up, strict=True):
        if not ui and start is None:
            start = ti
        if ui and start is not None:
            runs.append(float(ti - start))
            start = None
    if start is not None:
        runs.append(float(t[-1] - start))
    return runs


def main() -> None:
    root = Path(sys.argv[1])
    out = HERE / "code_down_runs.csv"
    with out.open("w", newline="", encoding="utf-8") as fh:
        w = csv.writer(fh)
        w.writerow(["draw", "t_lock_s", "run", "len_s"])
        for d in sorted(p for p in root.iterdir() if p.is_dir()):
            tl = t_lock(root / f"{d.name}.csv")
            runs = down_runs(d, tl) or [0.0]
            for i, r in enumerate(runs):
                w.writerow([d.name, f"{tl:.4f}", i, f"{r:.4f}"])
    print(f"wrote {out}")


if __name__ == "__main__":
    main()
