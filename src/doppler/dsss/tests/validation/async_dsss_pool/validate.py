"""Certify `async_dsss_pool` — the holder of the multi-emitter population.

Run:  python -m doppler.dsss.tests.validation.async_dsss_pool.validate
      make validate          (regenerates every report)
      make validate-check    (fails if the committed report is stale)

**C measures; Python renders and asserts.** The pool has a binding, but a
population of emitters through the channel, arriving and leaving, is not
something a validator should rebuild in numpy beside the shipped C stimulus
that already exists for it: every number here is
`native/validation/async_dsss_pool_soak.c`'s (design sections 12.14–12.17), the
lifecycle soak that certified the pool. This file parses its CSV blocks,
characterises, reviews and asserts the limits through the same `Report`
every other certified object uses — the `conv` shape.

**The run is the soak's own regression subset, `--check --emit`** — the
same downselect `ctest` runs — and not the population soak. A validator
runs on every push, twice (`test_validation_limits` and `make
validate-check`), and the evidence and the gate must be one run so they
cannot disagree; the ten-emitter soak at 20 s costs 3.3 min on twenty
cores (24 CPU-minutes) against CI's Python jobs of 4–7 min each, and the
report's 60 s cost 11. The population runs — ten emitters for 120 s
(section 12.14), on the floored dwell (12.16) and for 600 s (12.17) — are
`make validate-c`'s, evidence for the design page, refreshed on purpose.

What the soak scores, per emitter and per on-air stint: the time from
arrival to a slot holding it and to tracking, the seed's error against the
synth's own clock through the channel, the fraction of blocks tracked with
code and symbol lock while held, releases while on the air (a `lost` one
is a false release), double assignments, the release latency after
departure, and the heap after its warm-up. A slot is an emitter's by BOTH
coordinates — its seed within the searcher's row of the emitter's Doppler
and within a chip of its code phase — never by a count, because the
searcher's false alarms are part of the lifecycle.
"""

from __future__ import annotations

import csv
import subprocess
import sys
from dataclasses import dataclass, field
from pathlib import Path

from doppler.tests._repo import build_dir, repo_root
from doppler.tests._validation_common import Report, cli

HERE = Path(__file__).resolve().parent
DATA = HERE / "data"
ROOT = repo_root(__file__)
HARNESS = (
    build_dir(__file__) / "native/validation/validate_async_dsss_pool_soak"
)

LOST_CONFIRM_S = 2.0
N_SLOTS = 12

R = Report()


@dataclass
class Data:
    """The soak's CSV blocks, per C/N0: the hand-off pool's, and the cell
    pool's (`--cell`, design section 12.27) from a second run of the same
    stimulus -- the flavour is the only difference, so the two are read
    side by side and gated alike."""

    stints: dict[float, list[dict[str, float]]] = field(default_factory=dict)
    totals: dict[float, dict[str, float]] = field(default_factory=dict)
    cell_stints: dict[float, list[dict[str, float]]] = field(
        default_factory=dict
    )
    cell_totals: dict[float, dict[str, float]] = field(default_factory=dict)


def _harness() -> Data:
    """Run the soak's regression subset with `--emit` and parse its CSV.

    A missing binary is a hard failure rather than a skip: this report has
    no measurement of its own, and a skipped one is indistinguishable from
    a passing one in a log. `make build` builds it.
    """
    if not HARNESS.exists():
        raise SystemExit(
            f"async_dsss_pool: {HARNESS.relative_to(ROOT)} is not built — "
            f"run `make build` first. Every number in this report is the "
            f"C soak's."
        )
    d = Data()
    for cell in (False, True):
        _parse(
            subprocess.run(
                [str(HARNESS), "--check", "--emit"]
                + (["--cell"] if cell else []),
                capture_output=True,
                text=True,
                check=False,
            ).stdout,
            d.cell_stints if cell else d.stints,
            d.cell_totals if cell else d.totals,
        )
    if not d.totals or not d.cell_totals:
        raise SystemExit("async_dsss_pool: the soak emitted no totals block")
    return d


def _parse(
    out: str,
    stints: dict[float, list[dict[str, float]]],
    totals: dict[float, dict[str, float]],
) -> None:
    """One run's stdout into its `# stints` / `# totals` blocks."""
    name, cn0, header, rows = "", 0.0, [], []

    def flush() -> None:
        if name == "stints":
            stints[cn0] = rows
        elif name == "totals" and rows:
            totals[cn0] = rows[0]

    for line in out.splitlines():
        line = line.strip()
        if line.startswith("# "):
            flush()
            kind, _, tail = line[2:].partition(" ")
            name, cn0, header, rows = kind, float(tail.split("=")[1]), [], []
        elif not line:
            # A blank line ends a block: the human tables follow it.
            flush()
            name, header, rows = "", [], []
        elif not name:
            continue
        elif not header:
            header = line.split(",")
        else:
            rows.append(dict(zip(header, (float(v) for v in line.split(",")))))
    flush()


def _write_csv(d: Data) -> None:
    DATA.mkdir(exist_ok=True)
    for cn0, rows in d.stints.items():
        with (DATA / f"stints_{cn0:.0f}.csv").open("w", newline="") as fh:
            w = csv.DictWriter(fh, fieldnames=list(rows[0]))
            w.writeheader()
            w.writerows(rows)
    for cn0, rows in d.cell_stints.items():
        with (DATA / f"stints_{cn0:.0f}_cell.csv").open("w", newline="") as fh:
            w = csv.DictWriter(fh, fieldnames=list(rows[0]))
            w.writeheader()
            w.writerows(rows)
    with (DATA / "totals.csv").open("w", newline="") as fh:
        rows = [d.totals[c] for c in sorted(d.totals, reverse=True)] + [
            d.cell_totals[c] for c in sorted(d.cell_totals, reverse=True)
        ]
        w = csv.DictWriter(fh, fieldnames=list(rows[0]))
        w.writeheader()
        w.writerows(rows)


def _cn0s(d: Data) -> list[float]:
    return sorted(d.totals, reverse=True)


def section_object() -> None:
    R.md("## 1. The object — one holder for the population")
    R.md()
    R.md(
        "The design is [`docs/design/async-dsss-receiver.md`]"
        "(../../../../../../docs/design/async-dsss-receiver.md): section "
        "8.2 for the pool's shape, sections 12.13 to 12.17 for what was "
        "built and measured."
    )
    R.md()
    R.md(
        "`AsyncDsssPool` (design section 8.2) composes one continuous "
        "`Acquisition` "
        "with the block coherence its code-only window buys, `n_slots` "
        "hand-off `AsyncDsssReceiver`s created idle, the assigned table and "
        "the event log by attachment, behind one `push()` per block: feed "
        "the searcher; refresh the table from every locked loop; drop every "
        "peak within a chip of a live row's code phase as that emitter's "
        "own; seed each survivor into a free slot or count it dropped; feed "
        "every receiver across the threads; release every receiver that "
        "reports lost or has held its slot past the on-air cap. Nothing "
        "about the waveform or the population is baked in, and nothing "
        "allocates per push once a block size has been seen."
    )
    R.md()
    R.md(
        "The header's claims and where each is pinned in "
        "`native/tests/test_async_dsss_pool_core.c`, the order "
        "`docs/dev/contributing/validation.md` requires:"
    )
    R.md()
    R.table(
        ["claim (header)", "C test", "verdict"],
        [
            [
                "one emitter takes exactly one slot however many dwells hit "
                "it",
                "`_test_one_emitter_lifecycle`",
                "pinned",
            ],
            [
                "the seed is at the emitter's row, within the searcher's cell",
                "`_test_one_emitter_lifecycle`",
                "pinned",
            ],
            [
                "off the air it is lost and released; back on it is a new "
                "detection into a free slot",
                "`_test_one_emitter_lifecycle`",
                "pinned",
            ],
            [
                "two emitters two rows apart hold one slot each, nothing "
                "dropped; a one-slot pool counts the drop",
                "`_test_two_emitters_and_a_full_pool`",
                "pinned",
            ],
            [
                "a peak within a chip of a live row's code phase is that "
                "emitter's own at any Doppler",
                "`_test_same_phase_is_the_live_emitters`",
                "pinned",
            ],
            [
                "the table keys on locked loops only",
                "`_test_table_holds_the_locked_doppler`",
                "pinned",
            ],
            [
                "every transition reaches the log, in order",
                "`_test_event_log`",
                "pinned",
            ],
            [
                "a slot held past the cap is released `on_time`; reset logs "
                "nothing; the refine floor reaches every slot",
                "`_test_on_time_release_reset_and_the_floor`",
                "pinned",
            ],
            [
                "across threads the records are bit-identical",
                "`_test_two_emitters_and_a_full_pool`",
                "pinned",
            ],
            [
                "a mid-stream split resumes bit for bit; the envelope and a "
                "foreign slot count are rejected",
                "`_test_state_roundtrip`",
                "pinned",
            ],
            [
                "nothing allocates per push once a block size has been seen",
                "the soak's heap watch (§2.4)",
                "pinned by the harness; see F5",
            ],
            [
                "`CellAsyncDsssPool` is created only on a searcher a cell "
                "receiver can take (D > 1, a row inside the carrier loop's "
                "pull-in) and has no refine to floor",
                "`_test_cell_pool_refusals`",
                "pinned",
            ],
            [
                "the cell pool assigns once, tracks with both flags on the "
                "emitter's phase, releases and re-assigns; two threads give "
                "one thread's records",
                "`_test_cell_pool_lifecycle`",
                "pinned",
            ],
            [
                "the cell pool's blob resumes bit for bit and is keyed by "
                "the flavour",
                "`_test_cell_pool_state_roundtrip`",
                "pinned",
            ],
        ],
    )


def characterise(d: Data) -> None:
    R.md("## 2. Characterisation")
    R.md()
    t0 = d.totals[_cn0s(d)[0]]
    R.md(
        f"Every number below is the soak's regression subset: "
        f"{t0['n_emit']:.0f} emitters from the shipped continuous-DSSS synth "
        f"with the frame's window, each through the shipped "
        f"`doppler_channel` at its own Doppler within ±20 ppm of 2.5 GHz, "
        f"summed with the shipped awgn, for {t0['duration_s']:.0f} s at "
        f"{_cn0s(d)[0]:.0f} dB-Hz — the always-on one released for its "
        f"on-time and re-acquired, the other leaving, released, returning "
        f"and re-acquired; one pool at the operating point of section 6.1 "
        f"(D = 154, ±50 kHz, sixteen peaks, twelve slots, the carrier told, "
        f"a {LOST_CONFIRM_S:.0f} s release interval, {t0['threads']:.0f} "
        f"threads), fed one epoch at a time with the event log attached. "
        f"The population — ten emitters arriving and leaving for 120 s at "
        f"45 and 40 dB-Hz, and for 600 s — is the design's soak "
        f"(sections 12.14, 12.16 and 12.17), which this run is the "
        f"per-push subset of."
    )
    R.md()
    R.md("### 2.1 The lifecycle, per run")
    R.md()
    rows = []
    for c in _cn0s(d):
        t = d.totals[c]
        rows.append(
            [
                f"{c:.0f}",
                f"{t['stints']:.0f} ({t['scored']:.0f})",
                f"{t['missed']:.0f}",
                f"{t['false_rel']:.0f}",
                f"{t['dbl_locked']:.0f}",
                f"{t['late_rel']:.0f} / {t['over_rel']:.0f}",
                f"{t['on_time_rel']:.0f}",
                f"{t['false_alarms']:.0f}",
                f"{t['dropped']:.0f}",
                f"{t['max_assigned']:.0f} of {t['n_slots']:.0f}",
            ]
        )
    R.table(
        [
            "C/N0 dB-Hz",
            "stints (scored)",
            "missed",
            "false releases",
            "double assignments (blocks)",
            "released past 2 intervals / past 1",
            "on-time releases",
            "seeds matching no emitter",
            "hits dropped",
            "most assigned",
        ],
        rows,
    )
    R.md("### 2.2 Time to a slot, and the seed")
    R.md()
    rows = []
    for c in _cn0s(d):
        st = [s for s in d.stints[c] if s["t_held_s"] >= 0.0]
        held = [s["t_held_s"] for s in st]
        trk = [s["t_track_s"] for s in st if s["t_track_s"] >= 0.0]
        hz = [abs(s["seed_err_hz"]) for s in st if s["seeds"] > 0]
        ch = [abs(s["seed_err_chip"]) for s in st if s["seeds"] > 0]
        rows.append(
            [
                f"{c:.0f}",
                f"{min(held):.2f} / {sum(held) / len(held):.2f} / "
                f"{max(held):.2f}",
                f"{min(trk):.2f} / {sum(trk) / len(trk):.2f} / {max(trk):.2f}",
                f"{max(hz):.0f}",
                f"{max(ch):.2f}",
            ]
        )
    R.table(
        [
            "C/N0 dB-Hz",
            "arrival → held, min / mean / max s",
            "arrival → tracking s",
            "seed error, worst Hz",
            "seed error, worst chip",
        ],
        rows,
    )
    R.md(
        "At 45 dB-Hz the emitter's own data blocks seed it in the first block "
        "or two — section 12.7's smeared copy is over the gate there, "
        "hundreds of "
        "Hz off, and the receiver pulls in through its refine; at 40 dB-Hz "
        "(the design's soak, section 12.14) the copy is under the gate more "
        "often than not and the window is waited for, within the frame the "
        "design allows. The seed's Doppler is therefore scored within one "
        "native tile, its chip phase within a chip."
    )
    R.md()
    R.md("### 2.3 Tracking, and the release")
    R.md()
    rows = []
    for c in _cn0s(d):
        t = d.totals[c]
        rel = [
            s["t_rel_s"]
            for s in d.stints[c]
            if s["t_rel_s"] >= 0.0 and not s["rel_on_time"]
        ]
        rows.append(
            [
                f"{c:.0f}",
                f"{t['held_of_on']:.4f}",
                f"{t['trk_of_held']:.4f}",
                f"{t['sym_of_held']:.4f}",
                f"{min(rel):.2f} / {sum(rel) / len(rel):.2f} / {max(rel):.2f} "
                f"({len(rel)})"
                if rel
                else "—",
                f"{t['clock_restarts']:.0f}",
            ]
        )
    R.table(
        [
            "C/N0 dB-Hz",
            "held, of on-air blocks after first tracking",
            "tracking with code lock, of held",
            "with symbol lock, of held",
            "departure → release, min / mean / max s (n)",
            "release clocks restarted",
        ],
        rows,
    )
    R.md("### 2.4 The duration requirement")
    R.md()
    rows = []
    for c in _cn0s(d):
        t = d.totals[c]
        rows.append(
            [
                f"{c:.0f}",
                f"{t['heap_base_mib']:.1f}",
                f"{t['heap_step_max_kib']:.1f}",
                f"{t['heap_growth_kib']:+.1f}",
                f"{t['rss_base_mib']:.1f} → {t['rss_end_mib']:.1f}",
            ]
        )
    R.table(
        [
            "C/N0 dB-Hz",
            "heap after warm-up MiB",
            "largest first-use step KiB",
            "growth since the last first use KiB",
            "resident high-water mark MiB",
        ],
        rows,
    )
    R.md(
        "The heap is sampled once a second after a warm-up and re-based at "
        "every slot's first tracking — a receiver builds its chains on its "
        "first seed and hand-over, a first-use step kept apart from growth "
        "with time (section 5.1). The searcher's block and surface at D = 154 "
        "are "
        "the half gigabyte."
    )
    R.md()


def _parity(d: Data) -> None:
    """Section 2.5: the cell pool beside the hand-off pool on the same
    stimulus -- the lifecycle counts and the cost, per C/N0."""
    R.md("### 2.5 The cell flavour, beside the hand-off flavour")
    R.md()
    R.md(
        "`CellAsyncDsssPool` (design section 12.27) runs the same soak on "
        "cell receivers: no refine, every slot's `Dll` held from the seed "
        "and corrected on the searcher's own block timing. The same "
        "stimulus, the same gates; the receivers' flavour is the only "
        "difference. `push()` seconds are the population's cost on the "
        "run's threads."
    )
    R.md()
    rows = []
    for c in _cn0s(d):
        for label, t in (
            ("hand-off", d.totals[c]),
            ("cell", d.cell_totals[c]),
        ):
            rows.append(
                [
                    f"{c:.0f}",
                    label,
                    f"{t['stints']:.0f}",
                    f"{t['missed']:.0f}",
                    f"{t['false_rel']:.0f}",
                    f"{t['late_rel']:.0f}",
                    f"{t['dbl_locked']:.0f}",
                    f"{t['dropped']:.0f}",
                    f"{t['held_of_on']:.3f}",
                    f"{t['trk_of_held']:.3f}",
                    f"{t['sym_of_held']:.3f}",
                    f"{t['push_s']:.2f}",
                ]
            )
    R.table(
        [
            "C/N0",
            "flavour",
            "stints",
            "missed",
            "false",
            "late",
            "double",
            "dropped",
            "held of on",
            "code lock of held",
            "symbol lock of held",
            "push() s",
        ],
        rows,
    )
    R.md()


def review(d: Data) -> None:
    R.md("## 3. Review — findings")
    R.md()
    t45 = d.totals.get(45.0, {})
    R.find(
        "F1",
        "FIXED",
        "The exclusion zone is the code axis alone: a hit within one chip "
        "of a live row's code phase is that emitter's own at any Doppler. "
        "A tracked emitter's data blocks put smeared copies of it at its "
        "own phase hundreds of Hz away, and the one-row zone seeded twelve "
        "receivers onto one emitter in 0.4 s (design section 12.14). The "
        "cost: a second emitter within a chip of a live one is not seen "
        "until the first leaves.",
    )
    R.find(
        "F2",
        "FIXED",
        "The Dll's symbol-aided looks no longer overlap, so a decision "
        "reads each stretch of noise once: the code flag's returns on "
        "noise after a departure fell from about one a second to 0.004 "
        "(section 12.15, #1264). This run's clock restarted "
        f"{t45.get('clock_restarts', 0):.0f} time(s) in "
        f"{t45.get('duration_s', 0):.0f} s.",
    )
    R.find(
        "F3",
        "FIXED",
        "The refine's dwell is floored at `refine_min_blocks` (seven): sized "
        "for detection alone it shrank to two blocks at 45 dB-Hz, where "
        "one hand-over in sixty fell outside the tracking chain's pull-in "
        "(section 12.16, #1265).",
    )
    R.find(
        "F4",
        "BY DESIGN",
        "The searcher's false alarms are part of the lifecycle: at pfa 1e-3 "
        "a noise peak seeds a free slot, refines to nothing, reports "
        "tracking with both flags down and is released one interval later. "
        "The realized rate is about twice the configured one (#1064). Every "
        "expectation is therefore about the emitter's slot by both "
        "coordinates, never a count of slots, and `n_slots` carries that "
        "headroom.",
    )
    R.find(
        "F5",
        "CONFIRMED",
        "Each receiver builds its refine and track chains on its first seed "
        "and hand-over and frees them on reset (#1269) — a per-transition "
        "allocation, not a leak: the heap is flat once every slot has been "
        "used once, and the soak reports the first-use step apart from "
        "growth.",
    )
    R.find(
        "F6",
        "BY DESIGN",
        "The code flag still returns on noise at about 0.004 per second, "
        "and one return inside the interval restarts the release clock "
        "once, so the release is bounded on two intervals — one departure "
        "in a hundred; two returns inside one interval is a 1e-4 event.",
    )
    R.find(
        "F7",
        "FIXED",
        "An emitter on the edge between two searcher tiles read the same in "
        "both to 0.03 dB and was seeded a whole tile off half the time on "
        "the edge; every listed peak is now asked at its row's own "
        "frequency against the block's raw epochs (section 12.18, #1270). "
        "The ten-minute soak scores no stint missed at either C/N0.",
    )
    R.find(
        "F8",
        "FIXED",
        "A departed emitter's receiver ran its loops on noise, swept its "
        "code phase through every live emitter's and followed one 3.8 kHz "
        "off for fourteen seconds, its code flag flickering sixteen times. "
        "Once locked, both flags down now hold both loops at the state "
        "marked with both flags up (section 12.19, #1271): the ten-minute "
        "soak's release is 2.04 s mean with one restart in 183 departures.",
    )
    R.find(
        "F9",
        "CONFIRMED",
        "At 40 dB-Hz a data-block seed 650–800 Hz off reports tracking at "
        "the seed's own frequency and never pulls the carrier in, holding "
        "code lock without the symbol flag until the next aligned hit "
        "seeds a second receiver — two of 187 stints, each a 5–7 s double "
        "(#1273).",
    )
    R.find(
        "F10",
        "GAP",
        "A neighbour within about a kilohertz crossing a departed "
        "receiver's code phase at a chip or two a second is, to that "
        "receiver, its emitter's return; the pool, which knows the "
        "neighbour has a slot, is where it can be told apart — of the "
        "order of 1e-3 per departure with ten emitters, none seen in 181 "
        "(#1275).",
    )
    R.md()


def limits(d: Data) -> None:
    R.md("## 4. Limits — the certified envelope")
    R.md()
    for c in _cn0s(d):
        _limits_for(f"{c:.0f} dB-Hz", d.totals[c], d.stints[c])
    for c in _cn0s(d):
        _limits_for(f"{c:.0f} dB-Hz, cell", d.cell_totals[c], d.cell_stints[c])


def _limits_for(
    tag: str, t: dict[str, float], stints: list[dict[str, float]]
) -> None:
    """One flavour's limits at one C/N0: equal for both flavours by design
    (section 12.27) -- a gate the cell pool fails is a defect, never a
    tolerance to widen."""
    R.limit(t["missed"] == 0, f"[{tag}] no emitter on the air is missed")
    R.limit(
        t["false_rel"] == 0,
        f"[{tag}] no emitter is released while on the air by the rule",
    )
    R.limit(
        t["dbl_locked"] == 0,
        f"[{tag}] no emitter is tracked by two receivers at once",
    )
    R.limit(
        t["late_rel"] == 0,
        f"[{tag}] every departed emitter is released within two "
        f"intervals plus half a second",
    )
    R.limit(
        t["max_assigned"] <= N_SLOTS,
        f"[{tag}] the pool never exceeds its slots",
    )
    R.limit(
        t["events"] == t["log_lines"],
        f"[{tag}] every transition the pool counted reached the log",
    )
    R.limit(
        t["heap_growth_kib"] <= 4.0,
        f"[{tag}] the heap does not grow once every slot in use has "
        f"built its chains",
    )
    held = [
        s["t_held_s"]
        for s in stints
        if s["t_held_s"] >= 0.0 and not s["truncated"]
    ]
    R.limit(
        max(held) <= 4950.0 / 2700.0 + 0.25,
        f"[{tag}] every arrival with a slot free is held within a frame "
        f"and the refine",
    )
    R.limit(
        t["trk_of_held"] >= 0.98,
        f"[{tag}] a held emitter tracks with code lock on at least 98% "
        f"of blocks",
    )
    R.limit(
        t["sym_of_held"] >= 0.95,
        f"[{tag}] a held emitter tracks with symbol lock on at least "
        f"95% of blocks",
    )
    both = [
        s["t_track_s"] - s["t_held_s"]
        for s in stints
        if s["t_held_s"] >= 0.0 and s["t_track_s"] >= 0.0
    ]
    R.limit(
        bool(both) and max(both) <= 0.5,
        f"[{tag}] tracking follows the seed within half a second, the "
        f"refine's dwell and the hand-over",
    )
    rel = [
        s["t_rel_s"]
        for s in stints
        if s["t_rel_s"] >= 0.0 and not s["rel_on_time"]
    ]
    R.limit(
        bool(rel) and min(rel) >= LOST_CONFIRM_S,
        f"[{tag}] no departed emitter is released before the interval",
    )
    R.limit(
        t["on_time_rel"] >= 1 and t["reassign"] >= t["on_time_rel"],
        f"[{tag}] a slot held past the cap is released on time and its "
        f"emitter re-acquired into a free slot",
    )
    R.limit(
        t["relocked"] == 0,
        f"[{tag}] a returning emitter is a new detection, never its "
        f"released receiver's re-lock",
    )
    R.limit(
        t["dropped"] == 0 and t["waited"] == 0,
        f"[{tag}] no hit is dropped and no stint waits for a slot",
    )


def build(write: bool = True) -> Report:
    global R
    R = Report(write=write)
    R.md("# async_dsss_pool — validation report")
    R.md()
    section_object()
    d = _harness()
    if write:
        _write_csv(d)
    characterise(d)
    _parity(d)
    review(d)
    limits(d)
    R.executive(
        "AsyncDsssPool",
        [
            "**A slot is an emitter's by both coordinates, never by a "
            "count.** The searcher's false alarms take a free slot for one "
            "release interval each, by design (F4); read the table by seed "
            "Doppler and chip phase, and size the pool with that headroom.",
            "**Expect the seed from the emitter's own data blocks at "
            "45 dB-Hz** — hundreds of Hz off, in the first block or two — "
            "and from its window at 40, within a frame. Both pull in; the "
            "refine's dwell is floored at seven blocks for that (F3).",
            "**The release comes at the interval, with one rare exception.** "
            "A code-flag return on noise at 0.004 per second restarts the "
            "clock once, so bound on two intervals, not one (F6). While the "
            "clock runs the receiver's loops hold, so a departed emitter's "
            "receiver does not wander onto a neighbour (F8).",
            "**Nothing grows with time, once every slot has been used.** "
            "The receivers' first use allocates their chains (F5, #1269); "
            "after that the heap is flat to a page over the run.",
            "**Two emitters within a chip of each other are one** while the "
            "first is live (F1) — the price of a zone that survives the "
            "emitter's own smeared copies at every Doppler.",
        ],
    )
    R.summary(
        "\n- Raw run: "
        + ", ".join(f"`data/stints_{c:.0f}.csv`" for c in _cn0s(d))
        + ", `data/totals.csv` — the soak's `--emit` blocks"
    )
    R.emit(HERE / "results.md")
    return R


if __name__ == "__main__":
    sys.exit(cli(build, HERE))
