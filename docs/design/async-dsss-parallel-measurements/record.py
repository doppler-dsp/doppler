"""Render the parallel receivers' measurement record from its runs.

The record (``docs/design/async-dsss-parallel-measurements.md``) quotes two
runs of shipped tools, at one commit, and keeps both beside this file:

- the lock validator, ``native/validation/async_dsss_receiver_lock.c``, its
  full sweep with ``--emit``: ``lock.csv``, one row a trial, and
  ``lock-meta.json``, the run's stamp. ``measure`` takes both, from the
  binary ``make build`` built;
- the C benches of the three objects, ``make bench BENCH_ARGS="--c-only acq
  async_dsss_receiver async_dsss_pool"``: ``bench.json``, the rows the
  record uses, with the snapshot's commit and machine. ``take-bench`` keeps
  them from the snapshot ``jm bench`` saved under ``benchmarks/history/``.

Every table in the record is this file's output, between
``<!-- async-dsss-parallel:<block>:start -->`` and ``:end`` markers, so a
cell cannot be edited by hand or drift from the data; the blocks and the
``--write``/``--check`` machinery are ``../_record_blocks.py``'s. Data from a
dirty tree, or two runs at different commits, is refused.

Usage::

    python record.py measure            # the validator's sweep -> lock.*
    python record.py take-bench <snap>  # a bench snapshot's rows -> bench.json
    python record.py                    # print the blocks
    python record.py --write            # rewrite them in the record
    python record.py --check            # fail when they differ (the gate)

``--check`` is a line of the Makefile's ``DOCS_CHECK_PRE_CMDS``.
"""

from __future__ import annotations

import csv
import datetime
import io
import json
import os
import platform
import statistics
import subprocess
import sys
import time
from pathlib import Path
from typing import Any

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
import _record_blocks as blocks
from _record_blocks import table

DATA = Path(__file__).resolve().parent
ROOT = DATA.parents[2]
RECORD = DATA.parent / "async-dsss-parallel-measurements.md"
SCRIPT = "docs/design/async-dsss-parallel-measurements/record.py"
SOURCE = ROOT / "native/validation/async_dsss_receiver_lock.c"
VALIDATOR = ROOT / "build/native/validation/validate_async_dsss_receiver_lock"
BENCH_CMD = (
    'make bench BENCH_ARGS="--c-only acq async_dsss_receiver async_dsss_pool"'
)

#: The design's sample rate: 3.069 Mcps, two samples a chip.
FS = 6.138e6
#: One code epoch, the feed block: 2,046 samples, ms.
EPOCH_MS = 2046 / FS * 1e3
#: The dropout lengths the hold is reported at, ms.
HOLD_MS = (2.0, 5.0, 10.0, 15.0, 20.0, 25.0, 30.0)
#: The refine seed's error, |Hz|, binned for the lock it led to.
SEED_BINS = ((0.0, 100.0), (100.0, 200.0), (200.0, 300.0), (300.0, 1e9))

#: The bench rows the record uses, by name up to the auto-sizer's `_nc`
#: suffix, and each one's own sample rate (the rate its row was timed at).
ROWS: dict[str, tuple[str, float]] = {
    "track": ("async_dsss_receiver::steps[cell,sf=1023,5Mcps]", 10e6),
    "tile": ("acq::native_nc", FS),
    "span": ("acq::op5M_U5k_nc", 10e6),
    "deep": ("acq::op5M_U5k_D154_t1_nc", 10e6),
    "deep_t4": ("acq::op5M_U5k_D154_t4_nc", 10e6),
    "pool": ("async_dsss_pool::push[12 slots, serial]", 10e6),
    "pool_all": ("async_dsss_pool::push[12 slots, all cores]", 10e6),
    "u50_5_t1": ("acq::op5M_U50k_D154_t1_nc", 10e6),
    "u50_5_t8": ("acq::op5M_U50k_D154_t8_nc", 10e6),
    "u50_2_t1": ("acq::op2M_U50k_D61_t1_nc", 4e6),
    "u50_2_t4": ("acq::op2M_U50k_D61_t4_nc", 4e6),
}


def _git(*args: str) -> str:
    return subprocess.run(
        ["git", "-C", str(ROOT), *args],
        capture_output=True,
        text=True,
        check=True,
    ).stdout.strip()


def _cpu_model() -> str:
    try:
        with open("/proc/cpuinfo", encoding="utf-8") as f:
            for line in f:
                if line.startswith("model name"):
                    return line.split(":", 1)[1].strip()
    except OSError:
        pass
    return platform.processor() or "unknown"


def measure() -> int:
    """Run the built validator's full sweep; write lock.csv and its stamp."""
    if not VALIDATOR.exists():
        raise SystemExit(
            f"{VALIDATOR.name} is not built: make build "
            f"BUILD_TARGET={VALIDATOR.name}"
        )
    if VALIDATOR.stat().st_mtime < SOURCE.stat().st_mtime:
        raise SystemExit(f"{VALIDATOR.name} is older than its source: rebuild")
    load = os.getloadavg()
    t0 = time.perf_counter()
    run = subprocess.run(
        [str(VALIDATOR), "--emit"], capture_output=True, text=True
    )
    if run.returncode != 0:
        sys.stderr.write(run.stderr)
        return run.returncode
    (DATA / "lock.csv").write_text(run.stdout, encoding="utf-8")
    meta = {
        "date_utc": datetime.datetime.now(datetime.timezone.utc).strftime(
            "%Y-%m-%d %H:%M"
        ),
        "machine": platform.node(),
        "cpu_model": _cpu_model(),
        "cpu_threads": os.cpu_count(),
        "commit": _git("rev-parse", "--short=9", "HEAD"),
        "dirty": bool(_git("status", "--porcelain", "--untracked-files=no")),
        "load_before": [round(v, 2) for v in load],
        "load_after": [round(v, 2) for v in os.getloadavg()],
        "wall_s": round(time.perf_counter() - t0, 1),
    }
    (DATA / "lock-meta.json").write_text(
        json.dumps(meta, indent=1) + "\n", encoding="utf-8"
    )
    print(f"measure: {VALIDATOR.name} --emit in {meta['wall_s']} s")
    return 0


def take_bench(snapshot: str) -> int:
    """Keep the record's rows of a `jm bench` snapshot in bench.json."""
    snap = json.loads(Path(snapshot).read_text(encoding="utf-8"))
    rows = []
    for key, (prefix, _) in ROWS.items():
        hit = [r for r in snap["benchmarks"] if r["name"].startswith(prefix)]
        if len(hit) != 1:
            raise SystemExit(f"{key}: {len(hit)} rows match {prefix!r}")
        r = hit[0]
        keep = ("min", "max", "mean", "median", "iterations", "rounds")
        rows.append(
            {"name": r["name"], "stats": {k: r["stats"][k] for k in keep}}
        )
    out = {
        "datetime": snap["datetime"],
        "machine_info": snap["machine_info"],
        "commit_info": snap["commit_info"],
        "benchmarks": rows,
    }
    (DATA / "bench.json").write_text(
        json.dumps(out, indent=1) + "\n", encoding="utf-8"
    )
    print(f"take-bench: {len(rows)} rows kept from {snapshot}")
    return 0


def _csv_blocks(text: str) -> dict[str, list[dict[str, str]]]:
    """The validator's `--emit` output: `# <name>` then a CSV block."""
    out: dict[str, list[dict[str, str]]] = {}
    for chunk in text.split("# ")[1:]:
        name, _, body = chunk.partition("\n")
        out[name.strip()] = list(csv.DictReader(io.StringIO(body)))
    return out


def load() -> tuple[dict[str, Any], dict[str, Any], dict[str, Any]]:
    """The lock rows, their stamp and the bench rows, held to one clean
    commit."""
    meta = json.loads((DATA / "lock-meta.json").read_text(encoding="utf-8"))
    bench = json.loads((DATA / "bench.json").read_text(encoding="utf-8"))
    lock = _csv_blocks((DATA / "lock.csv").read_text(encoding="utf-8"))
    if meta["dirty"] or bench["commit_info"]["dirty"]:
        raise SystemExit("record data from a dirty tree is refused")
    if not bench["commit_info"]["id"].startswith(meta["commit"]):
        raise SystemExit(
            f"the validator ran at {meta['commit']}, the benches at "
            f"{bench['commit_info']['id'][:9]}: one commit, or no record"
        )
    return lock, meta, bench


def _f(v: str) -> float:
    return float(v)


def _ms(v: float) -> str:
    return f"{v:.1f} ms"


def _spread(vals: list[float], fmt: str = ".1f", unit: str = " ms") -> str:
    """min / median / max, or the one value, or a dash."""
    if not vals:
        return "—"
    lo, med, hi = min(vals), statistics.median(vals), max(vals)
    return f"{lo:{fmt}} / {med:{fmt}} / {hi:{fmt}}{unit}"


def _frac(k: int, n: int) -> str:
    """`k of n` with its 95% Wilson interval: a rate from twenty trials is
    a range, and the record says how wide."""
    if n == 0:
        return "—"
    from scipy.stats import binomtest

    ci = binomtest(k, n).proportion_ci(confidence_level=0.95, method="wilson")
    return f"{k} of {n} ({ci.low:.2f}–{ci.high:.2f})"


def _range(vals: list[float]) -> str:
    return f"{min(vals):.2f}–{max(vals):.2f}"


def _esn0s(rows: list[dict[str, str]]) -> list[str]:
    seen: list[str] = []
    for r in rows:
        if r["esn0_db"] not in seen:
            seen.append(r["esn0_db"])
    return seen


def _db(esn0: str) -> str:
    return f"{float(esn0):g} dB"


def stamp(lock: Any, meta: dict[str, Any], bench: dict[str, Any]) -> str:
    mi = bench["machine_info"]
    rows = [
        [
            "lock validator, full sweep",
            f"{meta['date_utc']} UTC, {meta['wall_s']:.0f} s",
            f"`{meta['machine']}`: {meta['cpu_model']} "
            f"({meta['cpu_threads']} threads), load "
            f"{meta['load_before'][0]:.1f}",
            f"`{meta['commit']}`, clean",
            "`make build`, then `record.py measure`",
        ],
        [
            "C benches",
            bench["datetime"][:16].replace("T", " ") + " UTC",
            f"`{mi['node']}`: {mi['cpu']['brand_raw']} "
            f"({mi['cpu']['count']} threads)",
            f"`{bench['commit_info']['id'][:9]}`, clean",
            f"`{BENCH_CMD}`, then `record.py take-bench`",
        ],
    ]
    return table(["run", "when", "machine", "commit", "command"], rows)


def dropout(lock: Any, meta: Any, bench: Any) -> str:
    rows = []
    for e in _esn0s(lock["dropout"]):
        d = [r for r in lock["dropout"] if r["esn0_db"] == e]
        s = [r for r in d if r["settled"] == "1"]
        back = {
            f: [
                _f(r[f"back_{f}_ms"])
                for r in s
                if r[f"fell_{f}"] == "1" and _f(r[f"back_{f}_ms"]) >= 0
            ]
            for f in ("50", "200")
        }
        fell = {f: sum(r[f"fell_{f}"] == "1" for r in s) for f in back}
        rows.append(
            [
                _db(e),
                _frac(len(s), len(d)),
                _spread([_f(r["drop_ms"]) for r in s]),
                f"{fell['50']} of {len(s)}; {_spread(back['50'])}",
                f"{fell['200']} of {len(s)}; {_spread(back['200'])}",
                _range([_f(r["flips"]) for r in d]),
            ]
        )
    return table(
        [
            "Es/N0",
            "draws with symbol lock at the cut",
            "lock falls after a loss, min / median / max",
            "a 50 ms dropout: fell; back after the signal",
            "a 200 ms dropout: fell; back after the signal",
            "data flips at the cut",
        ],
        rows,
    )


def held_at(profile: str, ms: float) -> bool:
    """Whether a dropout of `ms` held, on the trial's one-epoch profile;
    past its end the lock fell inside the dropout, by construction."""
    j = max(1, round(ms / EPOCH_MS))
    return j <= len(profile) and profile[j - 1] == "1"


def hold(lock: Any, meta: Any, bench: Any) -> str:
    rows = []
    for e in _esn0s(lock["dropout"]):
        d = [r for r in lock["dropout"] if r["esn0_db"] == e]
        s = [r for r in d if r["settled"] == "1"]
        cells = [
            _frac(sum(held_at(r["held"], ms) for r in s), len(s))
            for ms in HOLD_MS
        ]
        odd = sum(r["non_monotonic"] == "1" for r in s)
        rows.append(
            [_db(e), f"{len(d) - len(s)} of {len(d)}", *cells, str(odd)]
        )
    return table(
        [
            "Es/N0",
            "draws excluded: no symbol lock within 4 s",
            *(f"held {ms:g} ms" for ms in HOLD_MS),
            "trials where a longer one held after a shorter fell",
        ],
        rows,
    )


def arrival(lock: Any, meta: Any, bench: Any) -> str:
    rows = []
    for e in _esn0s(lock["arrival"]):
        a = [r for r in lock["arrival"] if r["esn0_db"] == e]
        trk = [_f(r["t_track_ms"]) for r in a if r["t_track_ms"] != "nan"]
        lk = [_f(r["t_lock_ms"]) for r in a if r["t_lock_ms"] != "nan"]
        m4 = [_f(r["m2m4_db"]) for r in a if r["m2m4_db"] != "nan"]
        rows.append(
            [
                _db(e),
                f"{len(trk)} of {len(a)}; {_spread(trk)}",
                f"{_frac(len(lk), len(a))}; {_spread(lk)}",
                _frac(sum(r["locked_end"] == "1" for r in a), len(a)),
                f"{statistics.median(m4):.1f} dB" if m4 else "—",
                _range([_f(r["flips"]) for r in a]),
            ]
        )
    return table(
        [
            "Es/N0",
            "tracking, after arrival: min / median / max",
            "symbol lock, after arrival",
            "locked at 2 s",
            "SNR of its symbols (m2m4), median",
            "data flips",
        ],
        rows,
    )


def seed(lock: Any, meta: Any, bench: Any) -> str:
    es = _esn0s(lock["arrival"])
    rows = []
    for lo, hi in SEED_BINS:
        name = f"≥ {lo:g} Hz" if hi > 1e8 else f"{lo:g}–{hi:g} Hz"
        cells = []
        for e in es:
            a = [
                r
                for r in lock["arrival"]
                if r["esn0_db"] == e
                and r["seed_err_hz"] != "nan"
                and lo <= abs(_f(r["seed_err_hz"])) < hi
            ]
            lk = [_f(r["t_lock_ms"]) for r in a if r["t_lock_ms"] != "nan"]
            med = f", median {statistics.median(lk):.0f} ms" if lk else ""
            cells.append(f"{_frac(len(lk), len(a))}{med}" if a else "—")
        rows.append([name, *cells])
    return table(
        ["refine seed error", *(f"locked, {_db(e)}" for e in es)], rows
    )


def _row(bench: dict[str, Any], key: str) -> tuple[str, float, float, float]:
    """(name, mean, min, max) ns per sample of one kept row."""
    prefix, _ = ROWS[key]
    (r,) = [b for b in bench["benchmarks"] if b["name"].startswith(prefix)]
    st = r["stats"]
    per = 1e9 / st["iterations"]
    return r["name"], st["mean"] * per, st["min"] * per, st["max"] * per


def _ns(bench: dict[str, Any], key: str) -> str:
    _, mean, lo, hi = _row(bench, key)
    return f"{mean:.1f} ({lo:.1f}–{hi:.1f})"


def cost(lock: Any, meta: Any, bench: dict[str, Any]) -> str:
    states = (
        ("tracking", "track", 1),
        ("searching one tile, one-epoch looks", "tile", 1),
        ("searching ±1/T_E (3 tiles), one-epoch looks", "span", 1),
        ("searching ±1/T_E, slow time over D = 154", "deep", 1),
        ("the same, four threads", "deep_t4", 4),
    )
    rows = []
    for label, key, threads in states:
        name, mean, _, _ = _row(bench, key)
        rt = mean * 1e-9 * FS
        rows.append(
            [
                label,
                f"`{name}`",
                _ns(bench, key),
                f"{rt:.2f}" + (" on four threads" if threads > 1 else ""),
            ]
        )
    return table(
        [
            "receiver state",
            "bench row",
            "ns a sample, mean (min–max)",
            "× real time at 6.138 MSa/s",
        ],
        rows,
    )


def pool(lock: Any, meta: Any, bench: dict[str, Any]) -> str:
    objects = (
        ("one receiver, tracking", "track"),
        ("the pool, 12 slots, one emitter tracked", "pool"),
        ("the same, across the machine's cores", "pool_all"),
        ("its searcher at ±50 kHz, 21 tiles, D = 154", "u50_5_t1"),
        ("the same, eight threads", "u50_5_t8"),
        ("its searcher at ±50 kHz, 53 tiles, D = 61", "u50_2_t1"),
        ("the same, four threads", "u50_2_t4"),
    )
    rows = []
    for label, key in objects:
        name, mean, _, _ = _row(bench, key)
        rate = ROWS[key][1]
        rows.append(
            [
                label,
                f"`{name}`",
                f"{rate / 1e6:g} MSa/s",
                _ns(bench, key),
                f"{mean * 1e-9 * rate:.2f}",
            ]
        )
    return table(
        [
            "per push",
            "bench row",
            "timed at",
            "ns a sample, mean (min–max)",
            "× real time at that rate",
        ],
        rows,
    )


BLOCKS = {
    "stamp": stamp,
    "arrival": arrival,
    "seed": seed,
    "dropout": dropout,
    "hold": hold,
    "cost": cost,
    "pool": pool,
}


def render() -> dict[str, str]:
    data = load()
    return {name: f(*data) for name, f in BLOCKS.items()}


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "measure":
        sys.exit(measure())
    if len(sys.argv) > 2 and sys.argv[1] == "take-bench":
        sys.exit(take_bench(sys.argv[2]))
    sys.exit(
        blocks.main(
            RECORD,
            "async-dsss-parallel",
            "async-dsss-parallel",
            SCRIPT,
            render,
            __doc__ or "",
        )
    )
