"""Certify `async_dsss_pool` -- N asynchronous DSSS receivers on one stream.

Run:  make validate-py VALIDATORS=<this file>   (this report)
      make validate-async-pool-sweep            (re-run the full sweep into
                                                 data/, then this)
      make validate          (regenerates every report)
      make validate-check    (fails if the committed report is stale)

**C measures; Python renders and asserts.** The pool's controller -- the
one search, the duplicate, ghost and loss releases, the seeds -- is C, and
its contract is `native/inc/doppler/async_dsss_pool/async_dsss_pool_core.h`.
Every claim in that header is mapped in section 1 onto the C test that pins
it, each seen to fail under a sabotage of the code it guards (#3267's claim
inventory, then #3384's C tests). The measurement is one C harness,
`native/validation/async_dsss_pool_ghosts.c`, on the use-case scene
(`native/tests/dp_air_test.h`) at the 415.1-B-1 point: FORCED (a ghost on a
strong emitter's sidelobe, seeded by hand at k image spacings), SECOND (a
distinct emitter on a strong one's carrier) and ALONE (one emitter, a pool
of four searching). This file reads its CSV blocks.

**Two runs, two roles.** The limits are asserted on the harness's
regression subset, `--check --emit`, re-run on every push (about 17 s), and
on the full sweep's committed record, `data/forced.csv`,
`data/second.csv` and `data/alone.csv` (twenty draws a cell, both
carriers), whose per-draw rows every rate in section 2 is computed from.
`make validate-async-pool-sweep` re-runs the sweep on purpose, in shards,
after the pool, the receiver, the harness or the toolchain changes.

**The Python face** reaches the pool's construction, its refusals, its
blob and its reports, and section 2.4 drives them through the binding. The
controller's rules are reachable only through the C state (its rings, its
counters, a receiver seeded by hand), so they are C-ONLY here, each with
its C test.
"""

from __future__ import annotations

import csv
import os
import statistics
import subprocess
import sys
from pathlib import Path
from typing import Union

import numpy as np

from doppler.ccsds import ccsds415_chip_rate
from doppler.dsss import Ccsds415Pool
from doppler.tests._repo import build_dir, exe, repo_root
from doppler.tests._validation_common import (
    Report,
    cli,
    harness_blocks,
    wilson,
)
from doppler.wfm import Gold

HERE = Path(__file__).resolve().parent
DATA = HERE / "data"
ROOT = repo_root(__file__)
HARNESS = exe(
    build_dir(__file__) / "native/validation/validate_async_dsss_pool_ghosts"
)

R = Report()

Rows = list[dict[str, Union[float, str]]]
Blocks = dict[str, Rows]

#: The record's blocks, one file each. `runs` and `ratio` (FORCED's
#: flag-down runs and every sample's prompt ratio) are the ghost-rule
#: design's inputs (doppler#3263), not this report's, and are not kept.
RECORD = ("forced", "second", "alone")

#: The machine the committed record (data/*.csv) was taken on. FORCED's
#: scene is chaotic in floating point: an ulp-level difference grows into a
#: different ending, so its per-draw outcomes are this machine's sample, not
#: a property of the pool (doppler#3409: on x86_64 the same sweep flips 40 of
#: 200 FORCED endings, and every limit holds on both).
RECORD_BOX = (
    "aarch64 (Qualcomm, WSL2), GCC 15.2.0, Release, DOPPLER_NATIVE off"
)

#: The harness's image tolerance, Hz (`GH_IMAGE_TOL_HZ`): the ghost rule's
#: ASYNC_DSSS_POOL_GHOST_EPS_HZ plus three of its STILL_HZ.
GHOST_EPS_HZ = 10.0
GHOST_STILL_HZ = 10.0
IMAGE_TOL_HZ = GHOST_EPS_HZ + 3.0 * GHOST_STILL_HZ

#: ALONE's epsilon: the 99th percentile of the per-tenure carrier error,
#: stable when at least 299 on-phase tenures feed it (0.99^N <= 0.05).
EPS_Q = 0.99
EPS_MIN_N = 299

#: The full sweep's cells, as the harness runs them (GH_DRAWS, FORCED's
#: k = -2..2 and SECOND's b_lvls, both carriers), so the record's
#: population is derived, never read off the record it checks.
DRAWS = 20
CARRIERS_GHZ = (2.2, 2.8)
FORCED_K = (-2, -1, 0, 1, 2)
SECOND_DBHZ = (38.8, 42.0, 44.0, 46.0, 50.0)
#: --check's SECOND levels (2.8 GHz, one draw each).
CHECK_DBHZ = (38.8, 50.0)


def _cells(rows: Rows, col: str, values: tuple) -> bool:
    """Every (carrier, value) cell holds exactly DRAWS rows, and no other
    cell exists."""
    got: dict[tuple[float, float], int] = {}
    for r in rows:
        key = (float(r["fc_ghz"]), float(r[col]))
        got[key] = got.get(key, 0) + 1
    want = {(fc, float(v)): DRAWS for fc in CARRIERS_GHZ for v in values}
    return got == want


#: FORCED's outcomes, as the harness codes them.
OUTCOME = {0: "held", 1: "released by the ghost rule", 2: "freed as lost"}


# ── the measurement ──────────────────────────────────────────────────────
def _check() -> Blocks:
    """The regression subset, as ctest runs it, with its CSV blocks."""
    return harness_blocks(
        HARNESS, "async_dsss_pool", ROOT, ("--check", "--emit")
    )


def _record(name: str) -> Rows:
    path = DATA / f"{name}.csv"
    if not path.exists():
        raise SystemExit(
            f"async_dsss_pool: data/{name}.csv is missing -- run "
            f"`make validate-async-pool-sweep`"
        )
    with path.open(encoding="utf-8", newline="") as fh:
        rows: Rows = []
        for r in csv.DictReader(fh):
            row: dict[str, float | str] = {}
            for k, v in r.items():
                try:
                    row[k] = float(v)
                except ValueError:
                    row[k] = v
            rows.append(row)
        return rows


def sweep() -> None:
    """Run the full sweep in shards and write the record.

    Each shard is the harness with `--emit --shard i/n`; its blocks are
    merged by name, in shard order, so a block's rows read as one run's.
    A shard whose own assertions fail fails the sweep: the record is only
    written from a run that passed.
    """
    n = max(1, min(8, (os.cpu_count() or 2) // 2))
    if not HARNESS.exists():
        raise SystemExit(
            f"async_dsss_pool: {HARNESS.relative_to(ROOT)} is not built -- "
            f"run `make build` first"
        )
    procs = [
        subprocess.Popen(
            [str(HARNESS), "--emit", "--shard", f"{i}/{n}"],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
        for i in range(n)
    ]
    outs = [p.communicate() for p in procs]
    for i, (p, (_, err)) in enumerate(zip(procs, outs)):
        if p.returncode != 0:
            raise SystemExit(
                f"async_dsss_pool: shard {i}/{n} failed (exit "
                f"{p.returncode}):\n{err[-2000:]}"
            )
    write_record([out for out, _ in outs])


def write_record(outs: list[str]) -> None:
    """Merge the shards' blocks by name, in shard order, into `data/`."""
    merged: dict[str, tuple[str, list[str]]] = {}
    for out in outs:
        name, header = "", ""
        for line in out.splitlines():
            line = line.strip()
            if line.startswith("# "):
                name, header = line[2:].split()[0], ""
            elif not line or not name:
                continue
            elif not header:
                header = line
                merged.setdefault(name, (header, []))
            else:
                merged[name][1].append(line)
    DATA.mkdir(exist_ok=True)
    for name in RECORD:
        header, rows = merged[name]
        (DATA / f"{name}.csv").write_text(
            "\n".join([header, *rows]) + "\n", encoding="utf-8"
        )


# ── helpers ──────────────────────────────────────────────────────────────
def _spacing(fc_ghz: float) -> float:
    """The pool's image spacing at a carrier, Hz: chip_rate / (2 code_len)
    (the pool's `lattice_hz`)."""
    return ccsds415_chip_rate(fc_ghz * 1e9) / (2.0 * 1023.0)


def _on_image(df_hz: float, fc_ghz: float) -> bool:
    """The harness's `gh_on_image()`: a whole nonzero number of image
    spacings off, within the image tolerance."""
    s = _spacing(fc_ghz)
    k = round(df_hz / s)
    return k != 0 and abs(df_hz - k * s) <= IMAGE_TOL_HZ


def _rate(k: int, n: int) -> str:
    lo, hi = wilson(k, n)
    return f"{k} of {n} [{lo:.2f}, {hi:.2f}]"


def _scored(rows: Rows) -> Rows:
    return [r for r in rows if int(r["outcome"]) >= 0]


#: A distinct emitter decodes when its error rate is under this: the
#: decoder's working region, two orders under a coin.
DECODES = 1e-2


def _decodes(r: dict[str, float | str]) -> bool:
    ber = float(r["b_ber"])
    return ber == ber and ber < DECODES


def _carriers(rows: Rows) -> list[float]:
    return sorted({float(r["fc_ghz"]) for r in rows})


# ── 1. the object ────────────────────────────────────────────────────────
#: The header's claims, each with the C test that pins it and where this
#: report reads it (`—`: the C test closes it; the binding cannot reach it).
CLAIMS = [
    (
        "one search, while any receiver is idle; it stops once all hold "
        "and restarts on the period one is free, reset at its first",
        "`_test_search_runs_once`, `_test_search_restarts_and_names_survive`",
        "—",
    ),
    (
        "the search's tiles across the workers; the receivers in "
        "parallel; one thread and N give the same bytes",
        "`_test_threads_change_nothing`, `_test_create_domain`",
        "2.4",
    ),
    (
        "a receiver call is exactly one code period (its floor), in groups "
        "of ASYNC_DSSS_POOL_PERIODS_PER_JOIN; the carry goes to the next "
        "push",
        "`_test_fractional_period_phase`, `_test_reset_clears_the_carry`",
        "—",
    ),
    (
        "any blocking of one stream gives the same receivers, releases "
        "and symbols",
        "`_test_chunk_invariance`",
        "2.4",
    ),
    (
        "a release or a seed comes at most a group's periods less one "
        "after the period that called for it",
        "`_test_release_latency`",
        "—",
    ),
    (
        "the lost are reset to idle; with `reacquire` a receiver releases "
        "itself and is never lost",
        "`_test_lost_is_released`, `_test_reacquire_releases_itself`",
        "—",
    ),
    (
        "duplicates: within one chip, circularly; the keeper by state, "
        "then time in it, then seed C/N0, then index",
        "`_test_duplicates_keep_the_first`, "
        "`_test_duplicate_zone_is_one_chip_circular`",
        "2.3",
    ),
    (
        "ghosts: a whole nonzero k of image spacings, mean within EPS, "
        "spread below STILL, code lock below LOCK_FRAC, both tracking the "
        "window; neither test alone",
        "`_test_ghost_test_edges`, `_test_ghost_needs_both_tests`, "
        "`_test_ghost_is_released`",
        "2.1, 4",
    ),
    (
        "the ghost's parent is the stronger holder by seed C/N0",
        "`_test_ghost_parent_is_the_stronger`",
        "—",
    ),
    (
        "a k = 0 ghost is not this rule's: it leaves by the loss release, "
        "and is held while it decodes",
        "`_test_ghost_is_released`, "
        "`_test_shipped_pool_frees_a_dropped_ghost`",
        "2.1",
    ),
    (
        "a released ghost goes idle; a re-seed of its cell is a rate",
        "`_test_ghost_is_released` (idle)",
        "3",
    ),
    (
        "the search's picks go to the idle receivers strongest first, in "
        "index order; its pick count is the receivers still idle (one "
        "while none is)",
        "`_test_seeds_go_strongest_first_in_index_order`",
        "2.2",
    ),
    (
        "every holder's phase becomes the search's exclusions",
        "`_test_two_emitters_two_holders`, `_test_duplicates_keep_the_first`",
        "—",
    ),
    (
        "status() as of the last whole period: a released receiver never "
        "reads lost",
        "`_test_lost_is_released`, `_test_release_latency`",
        "—",
    ),
    (
        "create(): 1 to MAX receivers, its arguments to every receiver, "
        "each receiver's search one thread; NULL on a refusal",
        "`_test_create_domain`",
        "2.4",
    ),
    (
        "`refine` passes to every receiver; off is refused under a deep "
        "search",
        "`_test_refine_passes_through`",
        "—",
    ),
    (
        "Ccsds415Pool is create() with dp_ccsds_415_rx()'s table, and "
        "refuses with its cause",
        "`_test_the_ccsds415_pool_is_the_point`, "
        "`_test_the_ccsds415_pool_refuses_with_its_cause`",
        "2.4",
    ),
    (
        "reset(): receivers idle, exclusions, clock, counts, search "
        "periods and ghost history to 0",
        "`_test_reset_and_threads`, `_test_reset_and_restore_edges`",
        "—",
    ),
    (
        "the blob carries the clock, counts, threads, carry, ghost rings, "
        "every receiver and the exclusions; symbols do not travel",
        "`_test_state_round_trip`",
        "2.4",
    ),
    (
        "set_state() refuses another object, version or receiver count, a "
        "carry of a group, and a ghost history control() cannot leave; "
        "whole or nothing",
        "`_test_refused_restore_changes_nothing`, "
        "`_test_reset_and_restore_edges`, "
        "`_test_nested_trace_version_refused`",
        "2.4",
    ),
    (
        "status() past the pool is a zeroed record, NO_RECEIVER; "
        "symbols_max_out() 0 before the first push, after reset and "
        "set_state; symbols() empty for idle, refining and lost",
        "`_test_reset_and_restore_edges`, "
        "`_test_seeds_go_strongest_first_in_index_order`",
        "2.4",
    ),
    (
        "set_threads(): results unchanged, refuses negative; telemetry one "
        "producer, whole or nothing, names survive a controller reset",
        "`_test_reset_and_threads`, `_test_telemetry_one_producer`, "
        "`_test_search_restarts_and_names_survive`",
        "—",
    ),
    (
        "receiver(): NULL past the pool; a lock retuned through the borrow "
        "lasts until the next seed or hand-over",
        "`_test_borrowed_lock_lasts_one_chain`",
        "—",
    ),
]


def section_object() -> None:
    R.md("## 1. The object — N asynchronous DSSS receivers on one stream")
    R.md()
    R.md(
        "The design is [`docs/design/async-dsss-receiver.md`]"
        "(../../../../../../docs/design/async-dsss-receiver.md), sections "
        "5 to 8; the contract is the header, `native/inc/doppler/"
        "async_dsss_pool/async_dsss_pool_core.h`. The receiver each slot "
        "holds is certified in its own report, "
        "[`async_dsss_receiver/results.md`](../async_dsss_receiver/"
        "results.md); this one certifies what the pool adds: one search "
        "for N receivers, and the controller that seeds and releases them."
    )
    R.md()
    R.md(
        "**Operating point: the 415.1-B-1 point** "
        "(`native/tests/dp_s415_test.h`): a 1,023-chip Gold code at "
        "F_fwd x 31/(221 x 96) chips a second at 2.2 and 2.8 GHz, 3,000 "
        "sym/s, 11 MSa/s, the pool designed at the 4 dB Es/N0 floor "
        "(38.8 dB-Hz), its search +/-1,500 Hz, through the use-case scene "
        "`native/tests/dp_air_test.h`. BPSK only, as the receiver's."
    )
    R.md()
    R.md(
        "The header's claims, each pinned in `native/tests/async_dsss_pool/"
        "test_async_dsss_pool_core.c` and each seen to fail under a "
        "sabotage of the code it guards (the inventory is #3267's; the C "
        "tests for its gaps are #3384's). `here` names the section of this "
        "report that also reads it:"
    )
    R.md()
    R.table(["claim (header)", "C test", "here"], [list(c) for c in CLAIMS])
    R.md()
    R.md(
        "Two header figures are a measurement rather than a test claim: "
        "the group size, ASYNC_DSSS_POOL_PERIODS_PER_JOIN = 16, is one "
        f"host's `make bench-ab` (the review's finding "
        f"{R.ref('group_size_one_host')}), and the state version is held "
        "by `make lint`'s `state-versions-check`."
    )
    R.md()


# ── 2. characterisation ──────────────────────────────────────────────────
def section_forced(rows: Rows) -> None:
    R.md(
        "### 2.1 A forced sidelobe ghost, by image (harness FORCED, "
        "`_test_ghost_is_released`)"
    )
    R.md()
    R.md(
        "A 60 dB-Hz emitter A; once receiver 0 tracks it with code lock, "
        "receiver 1 is seeded by hand on A's 167-chip Gold sidelobe and on "
        "A's carrier plus k image spacings, then run 10 s with "
        "`lost_confirm_s` 2.0. Each draw ends released by the ghost rule, "
        "freed by the loss release, or held. `on an image` counts the "
        "draws whose ghost ended a whole nonzero number of spacings off A "
        "(within the harness's 40 Hz), where the rule can act; the fine "
        "hand-over carries the rest to A's own carrier, k = 0 "
        "(the design page, section 8.2)."
    )
    R.md()
    R.md(
        f"**The record is one machine's sample**, taken on {RECORD_BOX}. "
        "FORCED's scene is chaotic in floating point: on x86_64 the same "
        "sweep ends 40 of these 200 draws differently, while every limit in "
        "section 4 holds on both machines (#3409). So each count below is "
        "this machine's, and its interval, not its exact value, is what "
        "carries over. No limit reads a FORCED per-draw outcome."
    )
    R.md()
    out = []
    for fc in _carriers(rows):
        for k in sorted({int(r["k"]) for r in rows}):
            cell = [
                r
                for r in rows
                if float(r["fc_ghz"]) == fc and int(r["k"]) == k
            ]
            n = len(cell)
            cnt = {
                o: sum(int(r["outcome"]) == o for r in cell) for o in (1, 2, 0)
            }
            img = sum(_on_image(float(r["df_end_hz"]), fc) for r in cell)
            out.append(
                [
                    f"{fc:.1f}",
                    f"{k:+d}",
                    _rate(cnt[1], n),
                    _rate(cnt[2], n),
                    _rate(cnt[0], n),
                    f"{img} of {n}",
                ]
            )
    R.table(
        [
            "GHz",
            "k",
            "released by the ghost rule",
            "freed as lost",
            "held",
            "on an image",
        ],
        out,
    )
    R.md()


def section_second(rows: Rows) -> None:
    R.md(
        "### 2.2 A distinct emitter on a strong one's carrier (harness "
        "SECOND, `_test_seeds_go_strongest_first_in_index_order`)"
    )
    R.md()
    R.md(
        "A at 70 dB-Hz and a distinct emitter B within 5 Hz of A's carrier, "
        "at a drawn code offset and at B's level, run until both receivers' "
        "rings have been full for a second. Receiver 1 is seeded on B by "
        "hand once receiver 0 tracks A, unless the pool's own search has "
        "already taken B (`seeded by the pool`), which it does more often "
        "the stronger B is. `if k = 0 were admitted` runs the ghost rule's "
        "same tests on the same rings with k = 0 allowed "
        "(`dp__async_dsss_pool_ghost_of_any_k()`): the cost of the choice "
        "the header declares. B's error rate is against its own payload "
        "while both tracked, read by the harness's own best-lag meter (one "
        "lag and one sign over the run, a declared deviation from the "
        "shipped `dp_ber_sync()`, #3396): a run with a slip reads high, so "
        "a count near the line may move under the shipped meter, not "
        "re-measured here. `decodes` counts "
        "the draws under "
        f"{DECODES:.0e}. Held is not decoding: A shares B's code and "
        "carrier, and A's data flips inside most code periods, so A leaks "
        "into B's despread at about 1/1023 of its power at any lag. At 31 "
        "dB below A, B is held and decodes nothing; with A at 48.8 dB-Hz "
        "instead, 10 dB above, the same seed-1 draw at 38.8 dB-Hz decodes "
        "at 0.019 with code lock throughout (a one-off build, not this "
        "record)."
    )
    R.md()
    out = []
    for fc in _carriers(rows):
        for b in sorted({float(r["b_dbhz"]) for r in rows}):
            cell = [
                r
                for r in rows
                if float(r["fc_ghz"]) == fc and float(r["b_dbhz"]) == b
            ]
            sc = _scored(cell)
            n = len(sc)
            ber = [float(r["b_ber"]) for r in sc if r["b_ber"] == r["b_ber"]]
            out.append(
                [
                    f"{fc:.1f}",
                    f"{b:.1f}",
                    f"{n} of {len(cell)}",
                    _rate(sum(int(r["outcome"]) == 1 for r in sc), n),
                    _rate(sum(int(r["any_k"]) for r in sc), n),
                    _rate(sum(int(r["by_pool"]) for r in sc), n),
                    _rate(sum(_decodes(r) for r in sc), n),
                    f"{statistics.median(ber):.1e}" if ber else "—",
                ]
            )
    R.table(
        [
            "GHz",
            "B, dB-Hz",
            "scored",
            "released by the rule",
            "if k = 0 were admitted",
            "seeded by the pool",
            "decodes",
            "B's median BER",
        ],
        out,
    )
    R.md()


def _eps(cell: Rows) -> tuple[float, int]:
    v = sorted(
        float(r["eps_hz"])
        for r in cell
        if int(r["off_phase"]) == 0 and r["eps_hz"] == r["eps_hz"]
    )
    if not v:
        return float("nan"), 0
    k = int(np.ceil(EPS_Q * len(v)))
    return v[max(k, 1) - 1], len(v)


def section_alone(rows: Rows) -> None:
    R.md(
        "### 2.3 One emitter, a pool of four searching (harness ALONE, "
        "`_test_duplicates_keep_the_first`)"
    )
    R.md()
    R.md(
        "One emitter at a drawn Doppler, a pool of four, 4 s. A tenure is "
        "a receiver's run in tracking; one ever more than a chip from the "
        "emitter's code phase is off phase, a receiver the pool took on "
        "its own with no parent to be a ghost of. ε is the 99th "
        "percentile, nearest rank, of the on-phase tenures' carrier error "
        "from one second in (the pool scene's resolution, "
        "`docs/design/pool-constellation-scene.md`); it is stable at "
        f"{EPS_MIN_N} tenures or more."
    )
    R.md()
    out = []
    for fc in _carriers(rows):
        for lv in sorted({float(r["cn0_dbhz"]) for r in rows}):
            cell = [
                r
                for r in rows
                if float(r["fc_ghz"]) == fc and float(r["cn0_dbhz"]) == lv
            ]
            off = [r for r in cell if int(r["off_phase"])]
            ended = {
                e: sum(r["ended"] == e for r in off)
                for e in ("lost", "duplicate", "ghost", "open", "other")
            }
            q, n = _eps(cell)
            out.append(
                [
                    f"{fc:.1f}",
                    f"{lv:.2f}",
                    str(len(cell)),
                    str(len(off)),
                    ", ".join(f"{e} {c}" for e, c in ended.items() if c)
                    or "—",
                    f"{q:.2f} Hz over {n}"
                    + ("" if n >= EPS_MIN_N else " (too few)"),
                ]
            )
    R.table(
        [
            "GHz",
            "C/N0, dB-Hz",
            "tenures",
            "off phase",
            "off-phase endings",
            "ε",
        ],
        out,
    )
    R.md()


def _python_face() -> dict[str, bool]:
    """The binding's reach, one claim a key: each driven and its control."""
    code = np.asarray(Gold().generate(1023), np.uint8)
    got: dict[str, bool] = {}
    for fc in (2.2e9, 2.8e9):
        p = Ccsds415Pool(code, n_receivers=2, carrier_freq_hz=fc, threads=1)
        got[f"builds_{fc / 1e9:.1f}"] = (
            p.n_receivers == 2 and p.status(0).state == 3
        )
    p = Ccsds415Pool(code, n_receivers=2, carrier_freq_hz=2.8e9, threads=1)

    def refuses(kw: dict, cause: str) -> bool:
        try:
            Ccsds415Pool(code, **kw)
        except ValueError as e:
            return cause in str(e)
        return False

    got["refuses_n"] = (
        refuses({"n_receivers": 17}, "n_receivers must be 1 to 16")
        and Ccsds415Pool(code, n_receivers=16, threads=1).n_receivers == 16
    )
    got["refuses_threads"] = (
        refuses({"threads": -1}, "threads must be >= 0")
        and Ccsds415Pool(code, threads=0).threads >= 1
    )
    st = p.status(2)
    got["past_the_pool"] = st.state == -1 and st.doppler_hz == 0.0
    got["max_out_zero"] = p.symbols_max_out() == 0 and not len(p.symbols(0))
    # A group of noise, then the blob: a second pool restored from it
    # resumes to the same bytes, one thread or four.
    rng = np.random.default_rng(1)
    n = 3 * 44000 + 1234
    x = (rng.standard_normal(n) + 1j * rng.standard_normal(n)).astype(
        np.complex64
    )
    p.push(x[: n // 2])
    q = Ccsds415Pool(code, n_receivers=2, carrier_freq_hz=2.8e9, threads=4)
    q.set_state(p.get_state())
    p.push(x[n // 2 :])
    q.push(x[n // 2 :])
    got["resumes"] = p.get_state() == q.get_state() and p.samples == n
    bad = bytearray(p.get_state())
    bad[0] ^= 0xFF
    try:
        q.set_state(bytes(bad))
        got["refuses_blob"] = False
    except ValueError:
        got["refuses_blob"] = True
    got["refuses_blob"] &= q.get_state() == p.get_state()
    return got


def section_python(face: dict[str, bool]) -> None:
    R.md("### 2.4 The Python face (`doppler.dsss.Ccsds415Pool`)")
    R.md()
    R.md(
        "Driven through the binding, each with its control: both carriers "
        "build; 17 receivers and -1 threads are refused naming the cause, "
        "while 16 and 0 build; a receiver past the pool reads NO_RECEIVER "
        "with a zeroed record; `symbols_max_out()` is 0 before the first "
        "push; a pool restored from another's blob mid-stream, at four "
        "threads instead of one, ends on the same bytes; a blob with its "
        "magic clobbered is refused and leaves the pool as it was. The "
        "rows are section 4's."
    )
    R.md()


# ── 3. review ────────────────────────────────────────────────────────────
def review(forced: Rows, second: Rows, alone: Rows) -> None:
    R.md("## 3. Review — findings, with verdicts")
    R.md()
    sc = _scored(second)
    pooled = sum(int(r["by_pool"]) for r in sc)
    R.find(
        "second_read_the_pools_own_seed",
        "FIXED",
        f"The SECOND arm waited for receiver 1 to be idle before seeding it "
        f"on B by hand, and called a draw that never got there 'receiver 0 "
        f"never tracked A', a failed check. From about 44 dB-Hz the pool's "
        f"own search takes B first, so receiver 1 was never idle: in every "
        f"such draw receiver 0 held A and receiver 1 held B, both within "
        f"0.2 chip with code lock, and no release had fired. The pool was "
        f"right and the meter misread it; the full sweep had been red on "
        f"it since the arm was written (`run-pc-8df79b4cc`'s shards). The "
        f"harness now scores those draws from the pool's own seed "
        f"({pooled} of {len(sc)} scored here, section 2.2) and fails only "
        f"when receiver 0 does not hold A, pinned in `--check` by a "
        f"seed-8 draw at 50 dB-Hz that goes red with the fix removed.",
    )
    k0 = [r for r in forced if int(r["k"]) == 0]
    held0 = sum(int(r["outcome"]) == 0 for r in k0)
    held = sum(int(r["outcome"]) == 0 for r in forced)
    R.find(
        "k0_ghost_held_on_its_parent",
        "CONFIRMED",
        f"A sidelobe ghost the fine hand-over carries onto its parent's own "
        f"carrier is left by the ghost rule, as the header declares, and "
        f"leaves only by the loss release once both its flags are down: "
        f"while it decodes the parent's data it is held. {held} of "
        f"{len(forced)} forced ghosts were still held at 10 s, {held0} of "
        f"{len(k0)} at k = 0 (section 2.1). This is the declared near-far "
        f"limit; the rule that closes it is #3263's.",
    )
    ak = [r for r in sc if int(r["any_k"])]
    R.find(
        "k0_admitted_would_release_b",
        "BY DESIGN",
        f"Admitting k = 0 would release {len(ak)} of {len(sc)} distinct "
        f"emitters on a strong one's carrier (section 2.2's "
        f"counterfactual), {sum(map(_decodes, ak))} of them decoding: the "
        f"rest sit 24 to 31 dB under a 70 dB-Hz neighbour, past the near-far "
        f"limit, where B decodes nothing anyway. Against that, "
        f"{R.ref('k0_ghost_held_on_its_parent')}'s ghosts hold a slot. "
        f"Leaving k = 0 alone is the owner's decision (2026-11-23, "
        f"async-dsss-receiver.md, section 8); these numbers are the trade's "
        f"measured cost, and the rule that would take it is #3263's.",
    )
    R.find(
        "ghost_reseed_is_a_rate",
        "GAP",
        "After a ghost release the search may seed the same cell again: "
        "its sidelobe rule models one clean code period, and a data "
        "transition in the parent's epoch lifts the sidelobes above it. "
        "The header states this as a rate; no harness measures it yet "
        "(#3263).",
    )
    R.find(
        "ghost_constants_not_derived",
        "GAP",
        "The ghost rule's constants (a 0.2 s window, EPS and STILL 10 Hz, "
        "a code-lock fraction of 0.2) are #2529's design values, and the "
        "harness's 40 Hz image tolerance is a 3-sigma coverage choice on "
        "them. None is derived from the carrier loop, which the card's "
        "principle 4 asks for. ALONE's ε (section 2.3) is the measured "
        "carrier resolution that a derivation would start from. The rule's "
        "redesign is #3263.",
    )
    lv = sorted({float(r["b_dbhz"]) for r in sc})

    def dec(b: float) -> str:
        c = [r for r in sc if float(r["b_dbhz"]) == b]
        return f"{sum(map(_decodes, c))} of {len(c)}"

    R.find(
        "near_far_short_of_spec",
        "CONFIRMED",
        f"The near-far range falls short of the specification's 20 dB. On "
        f"a strong emitter's carrier, a distinct one decodes in "
        f"{dec(lv[-1])} draws at {70.0 - lv[-1]:.0f} dB below it, "
        f"{dec(lv[-2])} at {70.0 - lv[-2]:.0f} dB and {dec(lv[0])} at "
        f"{70.0 - lv[0]:.0f} dB (section 2.2), nearly all of them held. The "
        f"deep search, which the pool shares, takes a weaker emitter only "
        f"to about 14 dB below (`near-far-alone-9de3b8120`, "
        f"async-dsss-receiver.md, section 7.2). #3067 carries the search's "
        f"side and #3263 the pool's.",
    )
    R.find(
        "group_size_one_host",
        "GAP",
        "ASYNC_DSSS_POOL_PERIODS_PER_JOIN = 16 is one host's measurement: "
        "`make bench-ab A=967275d78`, the push row at K = 9 and 16 threads, "
        "54 to 43.5 ns a sample at 2.2 GHz. Neither the header nor a "
        "benchmark derives it, and the wake-up cost it trades against is "
        "per host (#2778).",
    )
    R.find(
        "rows_sized_by_their_own_rule",
        "FIXED",
        "Each receiver's symbol row is sized from the receiver's own bound: "
        "a push's calls, each of one code period, times the most such a "
        "call can produce (the receiver's store, "
        "`dp_async_dsss_receiver_steps_max_out()` at a code period), and "
        "every call's `consumed` is checked, so a refused period aborts a "
        "Debug build rather than going unfed. A pool's row is a lone "
        "receiver's stream through a hand-over "
        "(`_test_a_pool_row_is_a_lone_receivers_stream`). Until then the "
        "pool sized rows by its own rule and did not read `consumed` "
        "(#3398).",
    )
    R.find(
        "controller_rules_c_only",
        "C-ONLY",
        "The controller's rules (duplicates, the ghost rule's tests, the "
        "seed order, the release latency, reset and the blob's forgeries) "
        "need the C state, its rings and a receiver seeded by hand, which "
        "the binding does not reach. Each is certified in the C test "
        "section 1 names.",
    )


# ── 4. limits ────────────────────────────────────────────────────────────
def limits(
    d: Blocks, forced: Rows, second: Rows, face: dict[str, bool]
) -> None:
    R.md("## 4. Limits — claims a caller may rely on")
    R.md()
    R.md(
        "The `--check` rows are re-run on every push (2.8 GHz, one draw a "
        "cell); the record's rows are the full sweep's, committed in "
        "`data/`, and asserted as they stand."
    )
    R.md()
    by_k = {int(r["k"]): r for r in d["forced"]}
    r0 = by_k[0]
    R.limit(
        int(r0["outcome"]) != 1,
        "[check] a forced ghost on its parent's own carrier (k = 0) is not "
        "released by the ghost rule",
        by="`--check` FORCED k = 0",
        control="the refine-off k = 2 row, released",
    )
    off = d["refine_off"][0]
    R.limit(
        int(off["outcome"]) == 1 and round(float(off["spacings_off"])) == 2,
        f"[check] with the refine off, a k = 2 sidelobe ghost stays on its "
        f"image ({float(off['spacings_off']):.2f} spacings off A) and the "
        f"ghost rule releases it",
        by="`--check` refine-off k = 2",
        control="the k = 0 row, not released",
    )
    r2 = by_k[2]
    R.limit(
        int(r2["outcome"]) != 1
        or _on_image(float(r2["df_end_hz"]), float(r2["fc_ghz"])),
        "[check] the shipped pool's forced k = 2 ghost is released only "
        "if it ended on a carrier image",
        by="`--check` FORCED k = 2",
    )
    R.limit(
        sorted(float(r["b_dbhz"]) for r in d["second"]) == list(CHECK_DBHZ)
        # the control: the same rows less one must not pass
        and sorted(float(r["b_dbhz"]) for r in d["second"][1:])
        != list(CHECK_DBHZ),
        f"[check] SECOND ran one draw at each of "
        f"{', '.join(f'{b:.1f}' for b in CHECK_DBHZ)} dB-Hz",
        by="`--check` SECOND",
        control="the same rows less one, refused",
    )
    for r in d["second"]:
        b = float(r["b_dbhz"])
        # "Not released" is what outcome 0 shows; whether B is a working
        # link is its error rate, which 31 dB under a 70 dB-Hz neighbour it
        # is not (section 2.2). Where it is within 20 dB, it must decode.
        R.limit(
            int(r["outcome"]) == 0,
            f"[check] a distinct emitter at {b:.1f} dB-Hz on a 70 dB-Hz "
            f"one's carrier is not released by the ghost rule",
            by=f"`--check` SECOND, B at {b:.1f} dB-Hz",
        )
        if 70.0 - b <= 20.0:
            far = [x for x in d["second"] if 70.0 - float(x["b_dbhz"]) > 30.0]
            R.limit(
                int(r["outcome"]) == 0
                and _decodes(r)
                # the control: 31 dB under, the same meter must say no
                and len(far) == 1
                and not _decodes(far[0]),
                f"[check] {70.0 - b:.0f} dB under the strong emitter, B is "
                f"held and decodes (BER {float(r['b_ber']):.1e} < "
                f"{DECODES:.0e})",
                by=f"`--check` SECOND, B at {b:.1f} dB-Hz",
                control="B at 38.8 dB-Hz, 31 dB under: held, and does not "
                "decode",
            )
    bp = d["second_by_pool"][0]
    R.limit(
        int(bp["by_pool"]) == 1 and int(bp["outcome"]) >= 0,
        "[check] a distinct emitter the pool's own search takes is scored "
        "on that seed, with receiver 0 on the stronger emitter",
        by="`--check` SECOND seed 8, B at 50 dB-Hz",
        red="drop the pool-seeded branch: the draw reads as unscored",
    )
    R.limit(
        _cells(forced, "k", FORCED_K)
        and _cells(second, "b_dbhz", SECOND_DBHZ)
        # the control: a record with one shard's row gone must not pass
        and not _cells(forced[1:], "k", FORCED_K),
        f"[record] the full sweep is whole: {DRAWS} draws in every carrier "
        f"and k ({len(CARRIERS_GHZ) * len(FORCED_K) * DRAWS} FORCED rows) "
        f"and every carrier and level "
        f"({len(CARRIERS_GHZ) * len(SECOND_DBHZ) * DRAWS} SECOND rows)",
        by="data/forced.csv, data/second.csv",
        control="the record less one row, refused",
    )
    rel = [r for r in forced if int(r["outcome"]) == 1]
    R.limit(
        len(rel) > 0
        and all(
            _on_image(float(r["df_end_hz"]), float(r["fc_ghz"])) for r in rel
        ),
        f"[record] every ghost-rule release of a forced ghost ended a whole "
        f"nonzero number of image spacings off its parent ({len(rel)} of "
        f"{len(rel)})",
        by="data/forced.csv",
    )
    k0 = [r for r in forced if int(r["k"]) == 0]
    R.limit(
        len(k0) == len(CARRIERS_GHZ) * DRAWS
        and not any(int(r["outcome"]) == 1 for r in k0),
        f"[record] no forced k = 0 ghost is released by the ghost rule "
        f"(0 of {len(k0)})",
        by="data/forced.csv",
    )
    sc = _scored(second)
    srel = [r for r in sc if int(r["outcome"]) == 1]
    R.limit(
        len(srel) > 0
        and all(
            _on_image(float(r["df_end_hz"]), float(r["fc_ghz"])) for r in srel
        ),
        f"[record] the ghost rule releases a distinct emitter's receiver "
        f"only off that emitter's carrier, on an image ({len(srel)} "
        f"release(s))",
        by="data/second.csv",
    )
    R.limit(
        len(sc) == len(second),
        f"[record] receiver 0 holds the stronger emitter and receiver 1 "
        f"holds B or nothing in every SECOND draw ({len(sc)} of "
        f"{len(second)} scored)",
        by="data/second.csv",
    )
    for key, claim in (
        ("builds_2.2", "Ccsds415Pool builds at 2.2 GHz, its receivers idle"),
        ("builds_2.8", "Ccsds415Pool builds at 2.8 GHz, its receivers idle"),
        (
            "refuses_n",
            "17 receivers are refused, naming the range; 16 build",
        ),
        ("refuses_threads", "-1 threads is refused; 0 builds"),
        (
            "past_the_pool",
            "a receiver past the pool reads NO_RECEIVER, a zeroed record",
        ),
        (
            "max_out_zero",
            "symbols_max_out() is 0 and symbols() empty before the first push",
        ),
        (
            "resumes",
            "a pool restored from another's blob mid-stream, at 4 threads "
            "instead of 1, ends on the same bytes",
        ),
        (
            "refuses_blob",
            "a blob with its magic clobbered is refused, the pool unchanged",
        ),
    ):
        R.limit(face[key], f"[python] {claim}", by="section 2.4")


def _takeaways(forced: Rows, second: Rows, alone: Rows) -> list[str]:
    held = sum(int(r["outcome"]) == 0 for r in forced)
    sc = _scored(second)
    ak = [r for r in sc if int(r["any_k"])]
    top = [r for r in sc if float(r["b_dbhz"]) == 50.0]
    low = [r for r in sc if float(r["b_dbhz"]) == 38.8]
    return [
        f"**One search feeds N receivers, one emitter each.** No distinct "
        f"emitter on a strong one's carrier is released by the ghost rule "
        f"except off its carrier, and receiver 0 holds the stronger in "
        f"every one of {len(sc)} draws (2.2, section 4).",
        f"**Held is not decoding near a strong emitter:** 20 dB under a "
        f"70 dB-Hz neighbour, {sum(map(_decodes, top))} of {len(top)} "
        f"decode; at the floor, 31 dB under, {sum(map(_decodes, low))} of "
        f"{len(low)} ({R.ref('near_far_short_of_spec')}, #3067).",
        f"**A ghost the hand-over puts on its parent's carrier is held "
        f"while it decodes:** {held} of {len(forced)} forced ghosts at 10 s "
        f"({R.ref('k0_ghost_held_on_its_parent')}, #3263). Size the pool "
        f"with a spare slot near a strong emitter.",
        f"**Admitting k = 0 would cost {sum(map(_decodes, ak))} decoding "
        f"emitters of {len(ak)} it releases** "
        f"({R.ref('k0_admitted_would_release_b')}): the trade #3263 weighs.",
    ]


def build(write: bool = True) -> Report:
    global R
    R = Report(write=write)
    R.md("# AsyncDsssPool — validation report")
    R.md()
    d = _check()
    for name in ("forced", "refine_off", "second", "second_by_pool", "alone"):
        if not d.get(name):
            raise SystemExit(
                f"async_dsss_pool: the --check run has no `# {name}` block"
            )
    forced, second, alone = (_record(n) for n in RECORD)
    face = _python_face()
    section_object()
    R.md("## 2. Characterisation")
    R.md()
    section_forced(forced)
    section_second(second)
    section_alone(alone)
    section_python(face)
    review(forced, second, alone)
    limits(d, forced, second, face)
    R.executive("AsyncDsssPool", _takeaways(forced, second, alone))
    R.summary(
        "\n- Raw run: `data/forced.csv`, `data/second.csv` and "
        "`data/alone.csv`, the full sweep's per-draw rows "
        "(`validate_async_dsss_pool_ghosts --emit`, in shards, by "
        "`make validate-async-pool-sweep`); the limits' `--check` rows are "
        "re-run on every push and not kept."
    )
    R.emit(HERE / "results.md")
    return R


if __name__ == "__main__":
    if "--sweep" in sys.argv:
        sys.argv.remove("--sweep")
        sweep()
    sys.exit(cli(build, HERE))
