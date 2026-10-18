"""Render the parallel receivers' measurement record from its runs.

The record (``docs/design/async-dsss-parallel-measurements.md``) quotes two
runs of shipped tools, at one commit, and keeps both beside this file:

- the lock validator, ``native/validation/async_dsss_receiver_lock.c``, its
  full sweep with ``--emit``: ``lock.csv``, one row a trial, and
  ``lock-meta.json``, the run's stamp. ``measure`` takes both, from the
  binary ``make build`` built;
- the C benches of the three objects, ``make bench BENCH_ARGS="--c-only acq
  async_dsss_receiver async_dsss_pool"``: ``bench.json``, the rows the
  record uses, with the snapshot's commit and machine and the machine's
  load either side of the bench. ``bench`` waits for a quiet machine, runs
  it and keeps the rows.

A third run draws the record's figures: the same validator's lifecycle
stories, ``--telemetry DIR --emit``, three receivers each through an
arrival trial of the sweep, three fades and a departure. ``stories`` runs
it into ``build/lifecycle-stories/``, keeps its rows (``stories.csv``) and
stamp (``stories-meta.json``), and draws each capture with
``scripts/plot_capture.py --time --shade stim.on`` into ``<story>.png``
here. The captures stay in ``build/``; the figures are what is committed.

Every table in the record is this file's output, and so is every number
the design page (``docs/design/async-dsss-parallel.md``) quotes from it:
both pages hold them between ``<!-- async-dsss-parallel:<block>:start -->``
and ``:end`` markers, so a cell cannot be edited by hand, or left behind by
a re-run, without ``--check`` failing. The blocks and the
``--write``/``--check`` machinery are ``../_record_blocks.py``'s. Data from
a dirty tree, or two runs at different commits, is refused.

Usage::

    python record.py measure            # the validator's sweep -> lock.*
    python record.py bench              # the benches, quiet -> bench.json
    python record.py stories            # the lifecycle stories -> *.png
    python record.py                    # print the blocks
    python record.py --write            # rewrite them in both pages
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
import re
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
DESIGN = DATA.parent / "async-dsss-parallel.md"
SCRIPT = "docs/design/async-dsss-parallel-measurements/record.py"
SOURCE = ROOT / "native/validation/async_dsss_receiver_lock.c"
VALIDATOR = ROOT / "build/native/validation/validate_async_dsss_receiver_lock"
BENCH_ARGS = "--c-only acq async_dsss_receiver async_dsss_pool"
STORIES_DIR = ROOT / "build/lifecycle-stories"
#: Each story figure's panels, top down: the receiver's own symbol-lock
#: metric and decision; the carrier loop's command and flag; the code
#: loop's command, its own rate and flag; the demodulator's two flags; and
#: the stimulus the shading is drawn from. Full names: a suffix such as
#: ``car.nco`` also matches the demodulator's.
STORY_PROBES = (
    "rx.sym.lock",
    "rx.sym.locked",
    "rx.car.nco",
    "rx.car.locked",
    "rx.code.nco",
    "rx.code.rate",
    "rx.code.locked",
    "rx.demod.car.locked",
    "rx.demod.sync.locked",
    "stim.on",
)

#: The design's sample rate: 3.069 Mcps, two samples a chip.
FS = 6.138e6
#: One code epoch, the feed block: 2,046 samples, ms.
EPOCH_MS = 2046 / FS * 1e3
#: The dropout lengths the hold is reported at, ms; the design page quotes
#: three of them.
HOLD_MS = (2.0, 5.0, 10.0, 15.0, 20.0, 25.0, 30.0)
DESIGN_HOLD_MS = (5.0, 10.0, 20.0)
#: The refine seed's error, |Hz|, binned for the lock it led to.
SEED_BINS = ((0.0, 100.0), (100.0, 200.0), (200.0, 300.0), (300.0, 1e9))
#: A machine is quiet for the bench below this 1-minute load average; the
#: bench waits up to QUIET_WAIT_S for it, then runs and records the load.
QUIET_LOAD, QUIET_WAIT_S = 2.0, 900.0

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


def _load() -> list[float]:
    return [round(v, 2) for v in os.getloadavg()]


#: The stamp schema a record writes. Schema 2 stamps carry the toolchain; a
#: schema 2 stamp without one is refused (load), so a stamp cannot silently
#: lose the field the validator's numbers depend on. Schema 1 stamps (the
#: committed data before this field) render the toolchain as "—".
STAMP_SCHEMA = 2


#: The fields a schema-2 toolchain stamp carries, each non-empty.
TOOLCHAIN_FIELDS = ("compiler", "libc", "build_type", "c_flags", "native")


def _cmake_cache(build: Path) -> dict[str, str]:
    """`build/CMakeCache.txt` as name -> value, the `:TYPE` suffix dropped."""
    cache: dict[str, str] = {}
    text = (build / "CMakeCache.txt").read_text(encoding="utf-8")
    for line in text.splitlines():
        if line and not line.startswith(("#", "//")) and "=" in line:
            key, _, value = line.partition("=")
            cache[key.split(":")[0]] = value
    return cache


def _libc() -> str:
    """The running C library, e.g. "glibc 2.41"; "unknown" where none is
    named.

    `os.confstr` is POSIX-only: Windows has no `os.confstr`, and macOS has it
    but refuses the GNU name (ValueError). It reports the C library running
    now, unlike `platform.libc_ver()`, which reads the version the
    interpreter was linked against.
    """
    try:
        return os.confstr("CS_GNU_LIBC_VERSION") or "unknown"
    except (AttributeError, ValueError, OSError):
        return "unknown"


def _toolchain(build: Path = ROOT / "build") -> dict[str, str]:
    r"""The toolchain the build used, read from what CMake recorded.

    The validator's numbers are toolchain-bound (the same commit gives
    different rows under another compiler or C library), so a stamp records
    the compiler the build was configured with, the C library, the build
    type and the flags that type compiles with, and whether `DOPPLER_NATIVE`
    added `-march=native` (CMakeLists.txt; it moves the numbers). A re-run
    on another box then reads as "a different toolchain", not as drift.

    The compiler and its version come from the compiler CMake detected
    (`CMakeFiles/<cmake version>/CMakeCCompiler.cmake`), not from running
    `cc --version` at stamp time, which can name a compiler the build never
    used. The flags are `CMAKE_C_FLAGS` plus `CMAKE_C_FLAGS_<BUILD_TYPE>`:
    the release flags alone carry `-O3`.

    Examples
    --------
    >>> import tempfile
    >>> b = Path(tempfile.mkdtemp())
    >>> _ = (b / "CMakeCache.txt").write_text(
    ...     "CMAKE_CACHE_MAJOR_VERSION:INTERNAL=3\n"
    ...     "CMAKE_CACHE_MINOR_VERSION:INTERNAL=31\n"
    ...     "CMAKE_CACHE_PATCH_VERSION:INTERNAL=6\n"
    ...     "CMAKE_C_COMPILER:FILEPATH=/usr/bin/cc\n"
    ...     "CMAKE_BUILD_TYPE:STRING=Release\n"
    ...     "CMAKE_C_FLAGS:STRING=\n"
    ...     "CMAKE_C_FLAGS_RELEASE:STRING=-O3 -DNDEBUG\n"
    ...     "DOPPLER_NATIVE:BOOL=OFF\n",
    ...     encoding="utf-8",
    ... )
    >>> d = b / "CMakeFiles" / "3.31.6"
    >>> d.mkdir(parents=True)
    >>> ids = 'set(CMAKE_C_COMPILER_ID "GNU")\n'
    >>> ver = 'set(CMAKE_C_COMPILER_VERSION "14.2.0")\n'
    >>> f = d / "CMakeCCompiler.cmake"
    >>> _ = f.write_text(ids + ver, encoding="utf-8")
    >>> tc = _toolchain(b)
    >>> tc["compiler"], tc["build_type"], tc["c_flags"], tc["native"]
    ('GNU 14.2.0', 'Release', '-O3 -DNDEBUG', 'OFF')
    """
    cache = _cmake_cache(build)
    cmake = ".".join(
        cache[f"CMAKE_CACHE_{part}_VERSION"]
        for part in ("MAJOR", "MINOR", "PATCH")
    )
    detected = dict(
        re.findall(
            r'^set\((CMAKE_C_COMPILER_(?:ID|VERSION)) "([^"]*)"\)',
            (build / "CMakeFiles" / cmake / "CMakeCCompiler.cmake").read_text(
                encoding="utf-8"
            ),
            re.MULTILINE,
        )
    )
    build_type = cache.get("CMAKE_BUILD_TYPE") or "default"
    flags = " ".join(
        f
        for f in (
            cache.get("CMAKE_C_FLAGS", "").strip(),
            cache.get(f"CMAKE_C_FLAGS_{build_type.upper()}", "").strip(),
        )
        if f
    )
    return {
        "compiler": f"{detected['CMAKE_C_COMPILER_ID']} "
        f"{detected['CMAKE_C_COMPILER_VERSION']}",
        "compiler_path": cache.get("CMAKE_C_COMPILER", "cc"),
        "libc": _libc(),
        "build_type": build_type,
        "c_flags": flags or "default",
        "native": cache.get("DOPPLER_NATIVE") or "OFF",
    }


def _tc(stamp: dict[str, Any]) -> str:
    """A stamp's toolchain as one cell; "—" for a stamp that predates it."""
    tc = stamp.get("toolchain")
    if tc is None:
        return "—"
    return (
        f"{tc['compiler']}; {tc['libc']}; build {tc['build_type']}, "
        f"flags {tc['c_flags']}, native {tc['native']}"
    )


def _validator_stamp() -> dict[str, Any]:
    """The built validator, refused when absent or older than its source,
    and the stamp of a run about to start."""
    if not VALIDATOR.exists():
        raise SystemExit(
            f"{VALIDATOR.name} is not built: make build "
            f"BUILD_TARGET={VALIDATOR.name}"
        )
    if VALIDATOR.stat().st_mtime < SOURCE.stat().st_mtime:
        raise SystemExit(f"{VALIDATOR.name} is older than its source: rebuild")
    # The tree as the run found it, before anything is written: the run's
    # outputs are themselves tracked, so read after the write it would call
    # its own run dirty.
    return {
        "stamp_schema": STAMP_SCHEMA,
        "date_utc": datetime.datetime.now(datetime.timezone.utc).strftime(
            "%Y-%m-%d %H:%M"
        ),
        "machine": platform.node(),
        "cpu_model": _cpu_model(),
        "cpu_threads": os.cpu_count(),
        "commit": _git("rev-parse", "--short=9", "HEAD"),
        "dirty": bool(_git("status", "--porcelain", "--untracked-files=no")),
        "load_before": _load(),
        "toolchain": _toolchain(),
    }


def measure() -> int:
    """Run the built validator's full sweep; write lock.csv and its stamp."""
    meta = _validator_stamp()
    t0 = time.perf_counter()
    run = subprocess.run(
        [str(VALIDATOR), "--emit"], capture_output=True, text=True
    )
    if run.returncode != 0:
        sys.stderr.write(run.stderr)
        return run.returncode
    meta["load_after"] = _load()
    meta["wall_s"] = round(time.perf_counter() - t0, 1)
    (DATA / "lock.csv").write_text(run.stdout, encoding="utf-8")
    (DATA / "lock-meta.json").write_text(
        json.dumps(meta, indent=1) + "\n", encoding="utf-8"
    )
    print(f"measure: {VALIDATOR.name} --emit in {meta['wall_s']} s")
    return 0


def stories() -> int:
    """Run the lifecycle stories; keep their rows, stamp and figures.

    The figures are drawn by the one capture plotter from the captures this
    run wrote, so a figure and the row beside it cannot come from
    different runs.
    """
    meta = _validator_stamp()
    STORIES_DIR.mkdir(parents=True, exist_ok=True)
    t0 = time.perf_counter()
    run = subprocess.run(
        [str(VALIDATOR), "--telemetry", str(STORIES_DIR), "--emit"],
        capture_output=True,
        text=True,
    )
    if run.returncode != 0:
        sys.stderr.write(run.stderr)
        return run.returncode
    meta["load_after"] = _load()
    meta["wall_s"] = round(time.perf_counter() - t0, 1)
    (DATA / "stories.csv").write_text(run.stdout, encoding="utf-8")
    (DATA / "stories-meta.json").write_text(
        json.dumps(meta, indent=1) + "\n", encoding="utf-8"
    )
    sys.path.insert(0, str(ROOT / "scripts"))
    import plot_capture

    for row in _csv_blocks(run.stdout)["story"]:
        rc = plot_capture.main(
            [
                str(STORIES_DIR / f"{row['name']}.tlm"),
                "--time",
                "--shade",
                "stim.on",
                "-o",
                str(DATA / f"{row['name']}.png"),
                *STORY_PROBES,
            ]
        )
        if rc != 0:
            return rc
    print(f"stories: {VALIDATOR.name} --telemetry in {meta['wall_s']} s")
    return 0


def bench() -> int:
    """Wait for a quiet machine, run the benches, keep the record's rows.

    The bench's own load is recorded either side of it, so a row timed
    while the machine was busy says so in the record's stamp.
    """
    t0 = time.monotonic()
    while os.getloadavg()[0] > QUIET_LOAD:
        if time.monotonic() - t0 > QUIET_WAIT_S:
            print(f"bench: load still {_load()[0]} after {QUIET_WAIT_S} s")
            break
        time.sleep(15)
    before, start = _load(), time.time()
    rc = subprocess.run(
        ["make", "bench", f"BENCH_ARGS={BENCH_ARGS}"], cwd=ROOT
    ).returncode
    if rc != 0:
        return rc
    after = _load()
    snaps = [
        p
        for p in (ROOT / "benchmarks/history").glob("*-c.json")
        if p.stat().st_mtime >= start
    ]
    if len(snaps) != 1:
        raise SystemExit(f"bench: {len(snaps)} new C snapshots, not one")
    return take_bench(snaps[0], before, after)


def take_bench(snapshot: Path, before: list[float], after: list[float]) -> int:
    """Keep the record's rows of a `jm bench` snapshot in bench.json."""
    snap = json.loads(snapshot.read_text(encoding="utf-8"))
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
        "stamp_schema": STAMP_SCHEMA,
        "datetime": snap["datetime"],
        "machine_info": snap["machine_info"],
        "commit_info": snap["commit_info"],
        "load_before": before,
        "load_after": after,
        "toolchain": _toolchain(),
        "benchmarks": rows,
    }
    (DATA / "bench.json").write_text(
        json.dumps(out, indent=1) + "\n", encoding="utf-8"
    )
    print(f"bench: {len(rows)} rows kept from {snapshot.name}")
    return 0


def _csv_blocks(text: str) -> dict[str, list[dict[str, str]]]:
    """The validator's `--emit` output: `# <name>` then a CSV block."""
    out: dict[str, list[dict[str, str]]] = {}
    for chunk in text.split("# ")[1:]:
        name, _, body = chunk.partition("\n")
        out[name.strip()] = list(csv.DictReader(io.StringIO(body)))
    return out


#: The fields a story shares with the sweep's arrival trial of its draws.
STORY_TRIAL = ("carrier_hz", "delay_s", "t_lock_ms")


def _stories(lock: dict[str, Any]) -> tuple[list[dict[str, str]], Any]:
    """The lifecycle stories' rows and stamp, each held to the sweep.

    A story is its arrival trial run on, so it may come from a later
    commit than the sweep only while it still IS that trial: the same
    carrier, arrival and symbol-lock time, in the same formats. A receiver
    that changed in between fails here, and the record is re-run whole.
    """
    smeta = json.loads((DATA / "stories-meta.json").read_text("utf-8"))
    _require_toolchain("stories-meta.json", smeta)
    rows = _csv_blocks((DATA / "stories.csv").read_text("utf-8"))["story"]
    if smeta["dirty"]:
        raise SystemExit("record data from a dirty tree is refused")
    trials = {(r["esn0_db"], r["seed"]): r for r in lock["arrival"]}
    for s in rows:
        t = trials.get((s["esn0_db"], s["seed"]))
        if t is None or any(s[k] != t[k] for k in STORY_TRIAL):
            raise SystemExit(
                f"story {s['name']} is not the sweep's {_db(s['esn0_db'])} "
                f"seed {s['seed']} arrival trial: re-run measure, bench and "
                "stories at one commit"
            )
        if not (DATA / f"{s['name']}.png").exists():
            raise SystemExit(f"{s['name']}.png is missing: record.py stories")
    return rows, smeta


def _require_toolchain(name: str, stamp: dict[str, Any]) -> None:
    """Refuse a schema-2 stamp that lacks the toolchain it must carry.

    A stamp of schema 1 predates the field and renders "—" (_tc); a stamp
    that claims schema 2 without its toolchain is a writer that dropped it,
    and --check fails on it rather than printing a row no reader can date.
    """
    if stamp.get("stamp_schema", 1) < 2:
        return
    tc = stamp.get("toolchain")
    # Falsy, not absent: `"toolchain": null` or an empty field is the same
    # dropped field, and would render "—" or a blank cell silently.
    if not isinstance(tc, dict) or not all(
        tc.get(k) for k in TOOLCHAIN_FIELDS
    ):
        raise SystemExit(
            f"{name}: a schema-{stamp['stamp_schema']} stamp without its "
            f"full toolchain ({', '.join(TOOLCHAIN_FIELDS)}): the writer "
            "dropped a field; re-run the tool"
        )


def load() -> tuple[dict[str, Any], dict[str, Any], dict[str, Any]]:
    """The lock rows, their stamp and the bench rows, held to one clean
    commit. The lifecycle stories ride along, held to the sweep instead
    (_stories): their rows as `lock["story"]`, their stamp as
    `meta["stories"]`."""
    meta = json.loads((DATA / "lock-meta.json").read_text(encoding="utf-8"))
    bench_ = json.loads((DATA / "bench.json").read_text(encoding="utf-8"))
    _require_toolchain("lock-meta.json", meta)
    _require_toolchain("bench.json", bench_)
    lock = _csv_blocks((DATA / "lock.csv").read_text(encoding="utf-8"))
    if meta["dirty"] or bench_["commit_info"]["dirty"]:
        raise SystemExit("record data from a dirty tree is refused")
    if not bench_["commit_info"]["id"].startswith(meta["commit"]):
        raise SystemExit(
            f"the validator ran at {meta['commit']}, the benches at "
            f"{bench_['commit_info']['id'][:9]}: one commit, or no record"
        )
    lock["story"], meta["stories"] = _stories(lock)
    return lock, meta, bench_


def _f(v: str) -> float:
    return float(v)


def _num(v: str) -> bool:
    """A measured value: not NaN, not the validator's -1 for none."""
    return v != "nan" and _f(v) >= 0


def _spread(vals: list[float], fmt: str = ".1f", unit: str = " ms") -> str:
    """min / median / max, or a dash."""
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


def _range(vals: list[float], fmt: str = ".2f") -> str:
    return f"{min(vals):{fmt}}–{max(vals):{fmt}}"


def _esn0s(rows: list[dict[str, str]]) -> list[str]:
    seen: list[str] = []
    for r in rows:
        if r["esn0_db"] not in seen:
            seen.append(r["esn0_db"])
    return seen


def _db(esn0: str) -> str:
    return f"{float(esn0):g} dB"


def _settled(lock: Any, e: str) -> tuple[list[Any], list[Any]]:
    d = [r for r in lock["dropout"] if r["esn0_db"] == e]
    return d, [r for r in d if r["settled"] == "1"]


def held_at(profile: str, ms: float) -> bool:
    """Whether a dropout of `ms` held, on the trial's one-epoch profile;
    past its end the lock fell inside the dropout, by construction."""
    j = max(1, round(ms / EPOCH_MS))
    return j <= len(profile) and profile[j - 1] == "1"


def stamp(lock: Any, meta: dict[str, Any], bench_: dict[str, Any]) -> str:
    mi = bench_["machine_info"]
    sm = meta["stories"]
    rows = [
        [
            "lock validator, full sweep",
            f"{meta['date_utc']} UTC, {meta['wall_s']:.0f} s",
            f"`{meta['machine']}`: {meta['cpu_model']} "
            f"({meta['cpu_threads']} threads)",
            f"{meta['load_before'][0]:.1f} / {meta['load_after'][0]:.1f}",
            f"`{meta['commit']}`, clean",
            _tc(meta),
            "`make build`, then `record.py measure`",
        ],
        [
            "C benches",
            bench_["datetime"][:16].replace("T", " ") + " UTC",
            f"`{mi['node']}`: {mi['cpu']['brand_raw']} "
            f"({mi['cpu']['count']} threads)",
            f"{bench_['load_before'][0]:.1f} / {bench_['load_after'][0]:.1f}",
            f"`{bench_['commit_info']['id'][:9]}`, clean",
            _tc(bench_),
            f'`make bench BENCH_ARGS="{BENCH_ARGS}"` once the load fell '
            f"under {QUIET_LOAD:g}, rows kept by `record.py`",
        ],
        [
            "lifecycle stories (§3.2)",
            f"{sm['date_utc']} UTC, {sm['wall_s']:.0f} s",
            f"`{sm['machine']}`: {sm['cpu_model']} "
            f"({sm['cpu_threads']} threads)",
            f"{sm['load_before'][0]:.1f} / {sm['load_after'][0]:.1f}",
            f"`{sm['commit']}`, clean",
            _tc(sm),
            "`make build`, then `record.py stories`; each story is the "
            "sweep's arrival trial of its draws, checked",
        ],
    ]
    return table(
        [
            "run",
            "when",
            "machine",
            "load before / after",
            "commit",
            "toolchain",
            "command",
        ],
        rows,
    )


def arrival(lock: Any, meta: Any, bench_: Any) -> str:
    rows = []
    for e in _esn0s(lock["arrival"]):
        a = [r for r in lock["arrival"] if r["esn0_db"] == e]
        trk = [_f(r["t_track_ms"]) for r in a if _num(r["t_track_ms"])]
        lk = [_f(r["t_lock_ms"]) for r in a if _num(r["t_lock_ms"])]
        rows.append(
            [
                _db(e),
                f"{_frac(len(trk), len(a))}; {_spread(trk)}",
                f"{_frac(len(lk), len(a))}; {_spread(lk)}",
                _frac(sum(r["locked_end"] == "1" for r in a), len(a)),
                _range([_f(r["flips"]) for r in a]),
            ]
        )
    return table(
        [
            "Es/N0",
            "tracking: draws; min / median / max after arrival",
            "symbol lock: draws; min / median / max",
            "locked 2 s after arrival",
            "data flips",
        ],
        rows,
    )


def seed(lock: Any, meta: Any, bench_: Any) -> str:
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
            lk = [_f(r["t_lock_ms"]) for r in a if _num(r["t_lock_ms"])]
            med = f", median {statistics.median(lk):.0f} ms" if lk else ""
            cells.append(f"{_frac(len(lk), len(a))}{med}" if a else "—")
        rows.append([name, *cells])
    return table(
        ["refine seed error", *(f"locked, {_db(e)}" for e in es)], rows
    )


def seed_edge(lock: Any, meta: Any, bench_: Any) -> str:
    """The refine seed errors at the edge of the carrier loop's bound.

    From the sweep's arrival trials at each C/N0: the largest seed error that
    locked, how many draws at 250 Hz or more locked, and the largest seed
    error whether it locked or not. The prose names this block for its
    numbers; they are rendered here, so a re-run moves them with the data.
    """

    def one(rows: list[dict[str, str]]) -> str:
        r = max(rows, key=lambda r: abs(_f(r["seed_err_hz"])))
        return f"{abs(_f(r['seed_err_hz'])):.1f} Hz (seed {r['seed']})"

    es = _esn0s(lock["arrival"])
    locked_row = ["largest seed error that locked"]
    big_row = ["draws at ≥ 250 Hz that locked"]
    worst_row = ["largest seed error, locked or not"]
    for e in es:
        a = [
            r
            for r in lock["arrival"]
            if r["esn0_db"] == e and r["seed_err_hz"] != "nan"
        ]
        lk = [r for r in a if _num(r["t_lock_ms"])]
        big = [r for r in a if abs(_f(r["seed_err_hz"])) >= 250.0]
        big_lk = [r for r in big if _num(r["t_lock_ms"])]
        locked_row.append(one(lk) if lk else "—")
        big_row.append(f"{len(big_lk)} of {len(big)}")
        worst_row.append(one(a) if a else "—")
    return table(
        [
            "refine seed error, the sweep's arrival trials",
            *(_db(e) for e in es),
        ],
        [locked_row, big_row, worst_row],
    )


def dropout(lock: Any, meta: Any, bench_: Any) -> str:
    rows = []
    for e in _esn0s(lock["dropout"]):
        d, s = _settled(lock, e)
        cells = []
        for f in ("50", "200"):
            fell = [r for r in s if r[f"fell_{f}"] == "1"]
            back = [
                _f(r[f"back_{f}_ms"]) for r in fell if _num(r[f"back_{f}_ms"])
            ]
            cells.append(
                f"fell {_frac(len(fell), len(s))}; back "
                f"{_frac(len(back), len(fell))}; {_spread(back)}"
            )
        rows.append(
            [
                _db(e),
                _range([_f(r["carrier_hz"]) for r in d], "+.0f") + " Hz",
                _frac(len(s), len(d)),
                _spread([_f(r["drop_ms"]) for r in s if _num(r["drop_ms"])]),
                *cells,
                _range([_f(r["flips"]) for r in d]),
            ]
        )
    return table(
        [
            "Es/N0",
            "carriers drawn",
            "draws settled: lock held 200 epochs within 4 s, up at the cut",
            "lock falls after a loss, min / median / max",
            "a 50 ms dropout: fell; back within 300 ms; min / median / max",
            "a 200 ms dropout: the same",
            "data flips at the cut",
        ],
        rows,
    )


def hold(lock: Any, meta: Any, bench_: Any) -> str:
    rows = []
    for e in _esn0s(lock["dropout"]):
        d, s = _settled(lock, e)
        cells = [
            _frac(sum(held_at(r["held"], ms) for r in s), len(s))
            for ms in HOLD_MS
        ]
        odd = sum(r["non_monotonic"] == "1" for r in s)
        rows.append([_db(e), _frac(len(d) - len(s), len(d)), *cells, f"{odd}"])
    return table(
        [
            "Es/N0",
            "draws excluded: not settled",
            *(f"held {ms:g} ms" for ms in HOLD_MS),
            "trials where a longer one held after a shorter fell",
        ],
        rows,
    )


def design_fade(lock: Any, meta: Any, bench_: Any) -> str:
    rows = []
    for e in _esn0s(lock["dropout"]):
        d, s = _settled(lock, e)
        drops = [_f(r["drop_ms"]) for r in s if _num(r["drop_ms"])]
        rows.append(
            [
                _db(e),
                f"{len(d) - len(s)} of {len(d)}",
                *(
                    _frac(sum(held_at(r["held"], ms) for r in s), len(s))
                    for ms in DESIGN_HOLD_MS
                ),
                f"{max(drops):.1f} ms",
            ]
        )
    return table(
        [
            "Es/N0",
            "draws not settled, excluded",
            *(f"rides out {ms:g} ms" for ms in DESIGN_HOLD_MS),
            "lets go after a loss, the slowest",
        ],
        rows,
    )


def design_arrival(lock: Any, meta: Any, bench_: Any) -> str:
    rows = []
    for e in _esn0s(lock["arrival"]):
        a = [r for r in lock["arrival"] if r["esn0_db"] == e]
        trk = [_f(r["t_track_ms"]) for r in a if _num(r["t_track_ms"])]
        lk = [_f(r["t_lock_ms"]) for r in a if _num(r["t_lock_ms"])]
        rows.append(
            [
                _db(e),
                f"{statistics.median(trk):.0f} ms (slowest {max(trk):.0f})",
                f"{statistics.median(lk):.0f} ms (slowest {max(lk):.0f})"
                if lk
                else "—",
                _frac(sum(r["locked_end"] == "1" for r in a), len(a)),
            ]
        )
    return table(
        [
            "Es/N0",
            "tracking (the state), median",
            "symbol lock, median of those that lock",
            "locked 2 s after arrival",
        ],
        rows,
    )


def _row(bench_: dict[str, Any], key: str) -> tuple[str, float, float, float]:
    """(name, mean, min, max) ns per sample of one kept row."""
    prefix, _ = ROWS[key]
    (r,) = [b for b in bench_["benchmarks"] if b["name"].startswith(prefix)]
    st = r["stats"]
    per = 1e9 / st["iterations"]
    return r["name"], st["mean"] * per, st["min"] * per, st["max"] * per


def _ns(bench_: dict[str, Any], key: str) -> str:
    _, mean, lo, hi = _row(bench_, key)
    return f"{mean:.1f} ({lo:.1f}–{hi:.1f})"


#: The receiver's states, the bench row each is, and its thread count.
STATES = (
    ("tracking", "track", 1),
    ("searching one tile, one-epoch looks", "tile", 1),
    ("searching ±1/T_E (3 tiles), one-epoch looks", "span", 1),
    ("searching ±1/T_E, slow time over D = 154", "deep", 1),
    ("the same, four threads", "deep_t4", 4),
)


def cost(lock: Any, meta: Any, bench_: dict[str, Any]) -> str:
    rows = []
    for label, key, threads in STATES:
        name, mean, _, _ = _row(bench_, key)
        rt = f"{mean * 1e-9 * FS:.2f}"
        rows.append(
            [
                label,
                f"`{name}`",
                _ns(bench_, key),
                rt + (" on four threads" if threads > 1 else ""),
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


def design_cost(lock: Any, meta: Any, bench_: dict[str, Any]) -> str:
    rows = [
        [label, f"{_row(bench_, key)[1] * 1e-9 * FS:.2f}"]
        for label, key, threads in STATES
        if threads == 1
    ]
    return table(["receiver state", "× real time, one thread"], rows)


#: The pool as built and its searcher, the bench row each is.
POOL = (
    ("one receiver, tracking", "track"),
    ("the pool, 12 slots, one emitter tracked", "pool"),
    ("the same, across the machine's cores", "pool_all"),
    ("its searcher at ±50 kHz, 21 tiles, D = 154", "u50_5_t1"),
    ("the same, eight threads", "u50_5_t8"),
    ("its searcher at ±50 kHz, 53 tiles, D = 61", "u50_2_t1"),
    ("the same, four threads", "u50_2_t4"),
)


def pool(lock: Any, meta: Any, bench_: dict[str, Any]) -> str:
    rows = []
    for label, key in POOL:
        name, mean, _, _ = _row(bench_, key)
        rate = ROWS[key][1]
        rows.append(
            [
                label,
                f"`{name}`",
                f"{rate / 1e6:g} MSa/s",
                _ns(bench_, key),
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


def design_pool(lock: Any, meta: Any, bench_: dict[str, Any]) -> str:
    rows = []
    for label, key in POOL:
        _, mean, _, _ = _row(bench_, key)
        rate = ROWS[key][1]
        rows.append(
            [label, f"{rate / 1e6:g} MSa/s", f"{mean * 1e-9 * rate:.2f}"]
        )
    return table(["per push", "timed at", "× real time (wall)"], rows)


def stories_block(lock: Any, meta: Any, bench_: Any) -> str:
    """The stories, each against its sweep trial, then their figures."""
    rows = [
        [
            f"`{s['name']}`",
            _db(s["esn0_db"]),
            s["seed"],
            f"{_f(s['carrier_hz']):+.1f} Hz",
            f"{_f(s['delay_s']) * 1e3:.1f} ms",
            (
                f"{_f(s['t_lock_ms']):.1f} ms"
                if _num(s["t_lock_ms"])
                else "never"
            ),
        ]
        for s in lock["story"]
    ]
    figs = [
        f"![{s['name']}](async-dsss-parallel-measurements/{s['name']}.png)"
        for s in lock["story"]
    ]
    head = [
        "story",
        "Es/N0",
        "seed",
        "carrier",
        "arrival",
        "symbol lock after arrival",
    ]
    return "\n\n".join([table(head, rows), *figs])


BLOCKS = {
    "stamp": stamp,
    "stories": stories_block,
    "arrival": arrival,
    "seed": seed,
    "seed-edge": seed_edge,
    "dropout": dropout,
    "hold": hold,
    "cost": cost,
    "pool": pool,
}
DESIGN_BLOCKS = {
    "design-fade": design_fade,
    "design-arrival": design_arrival,
    "design-seed": seed,
    "design-cost": design_cost,
    "design-pool": design_pool,
}


def render() -> dict[Path, dict[str, str]]:
    data = load()
    return {
        RECORD: {name: f(*data) for name, f in BLOCKS.items()},
        DESIGN: {name: f(*data) for name, f in DESIGN_BLOCKS.items()},
    }


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "measure":
        sys.exit(measure())
    if len(sys.argv) > 1 and sys.argv[1] == "bench":
        sys.exit(bench())
    if len(sys.argv) > 1 and sys.argv[1] == "stories":
        sys.exit(stories())
    sys.exit(
        blocks.main_pages(
            "async-dsss-parallel",
            "async-dsss-parallel",
            SCRIPT,
            render,
            __doc__ or "",
        )
    )
