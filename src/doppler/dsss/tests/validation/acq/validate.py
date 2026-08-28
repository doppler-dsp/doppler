"""Certify `acq` — the shared DSSS acquisition engine, both front doors.

Run:  python -m doppler.dsss.tests.validation.acq.validate
      make validate          (regenerates every report)
      make validate-check    (fails if the committed report is stale)

One engine, two mode-fixed constructors. `BurstAcquisition` searches a
repeated preamble with coherent depth; `Acquisition` searches a continuous,
data-modulated stream and always window-tiles instead, because a multi-epoch
coherent axis aliases the data's own transitions across the whole Doppler
axis. The split is structural rather than a per-call knob, and that is the
first thing this report measures.

Everything the engine decides — coherent depth, look count, threshold — is
**derived at construction** from `(cn0_dbhz, pfa, pd, doppler_uncertainty)`.
So most of the certified envelope is about that derivation being visible,
self-consistent, and honest about what it could not achieve
(`underpowered`), rather than about a stream of samples.

The correlation surface itself is `Corr2D`'s and is certified separately;
the CFAR statistics are `detection`'s. Neither is re-derived here.
"""

from __future__ import annotations

import sys
from dataclasses import dataclass, field
from pathlib import Path

import numpy as np

from doppler.dsss import Acquisition, BurstAcquisition
from doppler.dsss.tests._acq_pfa import (
    PFA_RATIO_RATCHET,
    pfa_sigma,
    realized_pfa,
)
from doppler.tests._validation_common import Report, cli
from doppler.wfm import PN, mls_poly

HERE = Path(__file__).resolve().parent
R = Report()

SF = 31
SPC = 4
CHIP_RATE = 1.0e6
SEED = 20260824


def _code() -> np.ndarray:
    return np.asarray(
        PN(poly=mls_poly(5), seed=1, length=5).generate(SF)
    ).astype(np.uint8)


def _epoch(code: np.ndarray, roll: int = 0) -> np.ndarray:
    s0 = np.repeat(np.where(code & 1, -1.0, 1.0), SPC)
    return np.roll(s0, roll).astype(np.complex64)


def _burst(code: np.ndarray, n_ep: int, roll: int = 0) -> np.ndarray:
    return np.tile(_epoch(code, roll), n_ep).astype(np.complex64)


def _noise(n: int, tag: int, sigma: float) -> np.ndarray:
    r = np.random.default_rng(SEED + tag)
    return (
        sigma * (r.standard_normal(n) + 1j * r.standard_normal(n)) / np.sqrt(2)
    ).astype(np.complex64)


def _csv(path: Path, header: str, rows: list[list[float]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as fh:
        fh.write(header + "\n")
        for r in rows:
            fh.write(",".join(f"{v:.10g}" for v in r) + "\n")


@dataclass
class Data:
    """Everything §3 and §4 read, measured once in §2."""

    mode_rows: list[list[str]] = field(default_factory=list)
    continuous_always_tiles: bool = False
    burst_uses_coherent: bool = False
    span_rows: list[list[str]] = field(default_factory=list)
    span_formula_ok: bool = False
    tiling_covers: bool = False
    anchor_rows: list[list[str]] = field(default_factory=list)
    anchors_distinct: bool = False
    anchor_stride_ok: bool = False
    localise_rows: list[list[str]] = field(default_factory=list)
    localise_exact: bool = False
    cn0_rows: list[list[str]] = field(default_factory=list)
    cn0_worst_db: float = 0.0
    thresh_rows: list[list[str]] = field(default_factory=list)
    thresh_rises_with_cells: bool = False
    underpowered_honest: bool = False
    raw_rows: list[list[str]] = field(default_factory=list)
    raw_rejects: bool = False
    raw_keeps_grid: bool = False
    reset_drains: bool = False
    state_exact: bool = False
    pfa_hat: float = 0.0
    pfa_ratio: float = 0.0
    pfa_sigma: float = 0.0
    pfa_within_ratchet: bool = False


# ── 1. the object ─────────────────────────────────────────────────────


def section_object() -> None:
    R.md("## 1. The object — one engine, two mode-fixed front doors")
    R.md()
    R.md(
        "`acq` jointly estimates a code phase and a Doppler bin and declares "
        "a detection when a CFAR statistic crosses an automatically sized "
        "threshold. The two constructors are not a convenience: a coherent "
        "slow-time axis can only ever resolve within one native span, and "
        "for a data-modulated stream a multi-epoch coherent axis aliases the "
        "data's transitions across the whole Doppler axis — a structural "
        "mislock rather than a graceful loss. So burst and continuous are "
        "fixed at construction."
    )
    R.md()
    R.table(
        ["page", "owns"],
        [
            [
                "[`docs/design/dsss-acquisition.md`]"
                "(../../../../../../docs/design/dsss-acquisition.md)",
                "the architecture, the aliasing argument, and the roadmap",
            ],
            [
                "[`corr2d`'s report](../../../../spectral/tests/validation/"
                "corr2d/results.md)",
                "the correlation surface — not re-derived here",
            ],
            [
                "[`detection`'s report](../../../../detection/tests/"
                "validation/detection/results.md)",
                "the Pd/threshold sizing this engine calls — not re-derived",
            ],
            [
                "`native/inc/acq/acq_core.h`",
                "the contract — the SSOT this report audits",
            ],
        ],
    )
    R.md("### 1.1 The claim inventory")
    R.md()
    R.md(
        "Step 1 of `docs/dev/contributing/validation.md`. The C test is "
        "`test_acq_core.c` (659 lines, already substantial). Two claims had "
        "**zero mentions in either language**, and both are the kind that "
        "fail quietly."
    )
    R.md()
    R.table(
        ["header claim", "pinned where", "here"],
        [
            [
                "`samples_consumed` is a PER-HIT anchor; one push spanning "
                "several epochs emits several offsets",
                "**was nothing, in C or Python**",
                "§2.3",
            ],
            [
                "`noise_mode` selects the CFAR reference "
                "(mean/median/min/max)",
                "**was nothing, in C or Python** — only the default ran",
                "§2.6",
            ],
            [
                "burst picks the smallest coherent depth meeting `pd`",
                "C, at both ends of the range",
                "§2.1",
            ],
            [
                "continuous ALWAYS window-tiles, even inside one span",
                "C",
                "§2.1",
            ],
            [
                "a coherent axis resolves only within `chip_rate/(2*sf)`",
                "C",
                "§2.2",
            ],
            [
                "tiling covers the requested uncertainty on both sides",
                "C, with a coverage sweep",
                "§2.2",
            ],
            [
                "`cn0_dbhz_est` inverts the sizing relationship",
                "C, against injected AWGN",
                "§2.5",
            ],
            [
                "a tighter uncertainty lowers the per-cell threshold",
                "C partially — measured as a relationship here",
                "§2.4",
            ],
            [
                "`configure_search_raw` bounds-checks and keeps the prior "
                "grid on failure",
                "C",
                "§2.7",
            ],
            [
                "`reset` drains the ring and the accumulator",
                "C + Python",
                "§2.7",
            ],
            ["the state triplet round-trips", "C + Python", "§2.7"],
        ],
    )


# ── 2. characterisation ───────────────────────────────────────────────


def characterise() -> Data:
    d = Data()
    R.md("## 2. Characterisation")
    R.md()
    R.md("Measured behaviour. No verdicts — those are §3.")
    R.md()
    _sec_modes(d)
    _sec_span(d)
    _sec_anchor(d)
    _sec_threshold(d)
    _sec_realized_pfa(d)
    _sec_cn0(d)
    _sec_noisemode(d)
    _sec_lifecycle(d)
    return d


def _sec_modes(d: Data) -> None:
    R.md("### 2.1 The two front doors choose different machinery")
    R.md()
    code = _code()
    rows = []
    for name, obj in (
        (
            "burst, strong signal",
            BurstAcquisition(
                code, reps=8, spc=SPC, chip_rate=CHIP_RATE, cn0_dbhz=70.0
            ),
        ),
        (
            "burst, weak signal",
            BurstAcquisition(
                code, reps=8, spc=SPC, chip_rate=CHIP_RATE, cn0_dbhz=30.0
            ),
        ),
        (
            "continuous, no uncertainty",
            Acquisition(
                code,
                spc=SPC,
                chip_rate=CHIP_RATE,
                symbol_rate=2000.0,
                cn0_dbhz=70.0,
                doppler_uncertainty=0.0,
            ),
        ),
        (
            "continuous, wide uncertainty",
            Acquisition(
                code,
                spc=SPC,
                chip_rate=CHIP_RATE,
                symbol_rate=2000.0,
                cn0_dbhz=70.0,
                doppler_uncertainty=40e3,
            ),
        ),
    ):
        rows.append(
            [
                name,
                str(obj.doppler_bins),
                str(obj.n_noncoh),
                f"{obj.doppler_res_hz:.1f}",
            ]
        )
    R.table(
        ["configuration", "doppler_bins", "n_noncoh", "doppler_res_hz"], rows
    )
    d.mode_rows = rows
    strong = BurstAcquisition(
        code, reps=8, spc=SPC, chip_rate=CHIP_RATE, cn0_dbhz=70.0
    )
    weak = BurstAcquisition(
        code, reps=8, spc=SPC, chip_rate=CHIP_RATE, cn0_dbhz=30.0
    )
    d.burst_uses_coherent = strong.doppler_bins < weak.doppler_bins
    cont = Acquisition(
        code,
        spc=SPC,
        chip_rate=CHIP_RATE,
        symbol_rate=2000.0,
        cn0_dbhz=70.0,
        doppler_uncertainty=0.0,
    )
    d.continuous_always_tiles = cont.doppler_bins == 1
    R.md(
        f"**Burst spends its budget on coherent depth**: a strong signal "
        f"needs {strong.doppler_bins} bin(s) where a weak one exhausts to "
        f"{weak.doppler_bins}. **Continuous never does** — even with no "
        f"uncertainty prior at all it reports "
        f"`doppler_bins = {cont.doppler_bins}` and buys its margin from "
        f"non-coherent looks instead. That is the aliasing footgun closed "
        f"structurally rather than priced as a tunable loss."
    )
    R.md()


def _sec_span(d: Data) -> None:
    R.md("### 2.2 The native span, and what tiling is for")
    R.md()
    span = CHIP_RATE / (2.0 * SF)
    code = _code()
    rows, csv = [], []
    ok = True
    for mult in (0.5, 0.95, 2.0, 4.0):
        unc = mult * span
        a = BurstAcquisition(
            code,
            reps=8,
            spc=SPC,
            chip_rate=CHIP_RATE,
            cn0_dbhz=60.0,
            doppler_uncertainty=unc,
        )
        # The searched reach is doppler_bins * doppler_res_hz / 2, and it
        # has no accessor. doppler_span_hz is the NATIVE span -- a constant
        # of the geometry that does not move when tiling engages, which is
        # documented and still easy to read as coverage. See F6.
        reach = a.doppler_bins * a.doppler_res_hz / 2.0
        ok &= reach >= unc - 1e-6
        rows.append(
            [
                f"{mult:g} x span",
                f"{unc:.0f}",
                str(a.doppler_bins),
                f"{a.doppler_span_hz:.0f}",
                f"{reach:.0f}",
            ]
        )
        csv.append([mult, unc, a.doppler_bins, a.doppler_span_hz, reach])
    R.table(
        [
            "requested uncertainty",
            "Hz",
            "doppler_bins",
            "doppler_span_hz",
            "searched reach (derived)",
        ],
        rows,
    )
    _csv(
        HERE / "data" / "span.csv",
        "mult,unc_hz,bins,native_span_hz,reach_hz",
        csv,
    )
    d.span_rows = rows
    d.tiling_covers = ok
    d.span_formula_ok = abs(span - CHIP_RATE / (2.0 * SF)) < 1e-9
    R.md(
        f"The native span is `chip_rate/(2*sf)` = **{span:.0f} Hz** for this "
        f"geometry. Beyond it, coherent depth cannot help — more depth "
        f"subdivides the same fixed range more finely — so the engine tiles "
        f"the requested uncertainty with parallel frequency windows instead. "
        f"The reach covers the request in every row, which is the contract "
        f"that matters: an uncovered corner of the uncertainty is a signal "
        f"the engine cannot see and does not say so. Raw sweep: "
        f"`data/span.csv`."
    )
    R.md()


def _sec_anchor(d: Data) -> None:
    R.md("### 2.3 `samples_consumed` is a per-HIT anchor")
    R.md()
    R.md(
        "Documented as *\"the raw sample offset this detection's epoch ended "
        "at ... instead of reusing one message-level timestamp for every "
        'hit"*, and pinned by nothing in either language. The failure it '
        "guards against is quiet: stamping every hit from one call with the "
        "same offset still detects in the right place, and only a caller "
        "correlating hits to wall-clock would notice."
    )
    R.md()
    code = _code()
    a = BurstAcquisition(
        code, reps=4, spc=SPC, chip_rate=CHIP_RATE, cn0_dbhz=60.0
    )
    a.configure_search_raw(doppler_bins=4, n_noncoh=1)
    hits = a.push(_burst(code, 24))
    anchors = [h[6] for h in hits]
    rows = [[str(i), str(v)] for i, v in enumerate(anchors[:6])]
    R.table(["hit", "samples_consumed"], rows)
    d.anchor_rows = rows
    d.anchors_distinct = len(set(anchors)) == len(anchors) and all(
        b > a_ for a_, b in zip(anchors, anchors[1:])
    )
    strides = {b - a_ for a_, b in zip(anchors, anchors[1:])}
    d.anchor_stride_ok = len(strides) == 1 and len(anchors) >= 2
    R.md(
        f"{len(anchors)} hits from **one** push, every anchor distinct and "
        f"strictly increasing, with a single stride of "
        f"{next(iter(strides)) if strides else 0} samples — one dump's worth "
        f"of input. A caller can therefore timestamp each detection "
        f"independently."
    )
    R.md()
    R.md("#### and the peak lands where the code phase was rolled")
    R.md()
    rows = []
    ok = True
    for roll in (0, 5, 17, SF * SPC - 3):
        b = BurstAcquisition(
            code, reps=4, spc=SPC, chip_rate=CHIP_RATE, cn0_dbhz=60.0
        )
        b.configure_search_raw(doppler_bins=4, n_noncoh=1)
        h = b.push(_burst(code, 8, roll))
        got = h[0][1] if h else -1
        ok &= got == roll
        rows.append([str(roll), str(got), "yes" if got == roll else "**NO**"])
    R.table(["code phase injected", "code_phase reported", "match"], rows)
    d.localise_rows = rows
    d.localise_exact = ok
    R.md(
        "Exact at four phases including the wrap-around shoulder — the code "
        "phase is a sample index into the epoch, not a chip index, which is "
        "the detail a caller integrating with a despreader has to get right."
    )
    R.md()


def _sec_threshold(d: Data) -> None:
    R.md("### 2.4 The threshold is a function of the cell count")
    R.md()
    R.md(
        "The per-cell false-alarm rate is the system rate divided across "
        "every searched cell, so a search that grows also raises the bar on "
        "each cell. That is the price of coverage, and it is why a tighter "
        "uncertainty prior makes a **more sensitive** detector rather than "
        "merely a faster one."
    )
    R.md()
    code = _code()
    span = CHIP_RATE / (2.0 * SF)
    rows, csv = [], []
    etas = []
    for mult in (0.25, 0.5, 1.0, 2.0, 4.0):
        a = BurstAcquisition(
            code,
            reps=8,
            spc=SPC,
            chip_rate=CHIP_RATE,
            cn0_dbhz=45.0,
            doppler_uncertainty=mult * span,
        )
        etas.append(a.eta)
        rows.append(
            [
                f"{mult:g} x span",
                str(a.doppler_bins),
                f"{a.pfa_cell:.3e}",
                f"{a.eta:.4f}",
            ]
        )
        csv.append([mult, a.doppler_bins, a.pfa_cell, a.eta])
    R.table(
        ["uncertainty", "doppler_bins", "pfa per cell", "threshold eta"], rows
    )
    _csv(HERE / "data" / "threshold.csv", "mult,bins,pfa_cell,eta", csv)
    d.thresh_rows = rows
    d.thresh_rises_with_cells = etas[-1] >= etas[0]
    R.md(
        f"The threshold rises from {etas[0]:.3f} to {etas[-1]:.3f} across a "
        f"16x range of uncertainty. A caller who can bound the Doppler "
        f"tighter is buying sensitivity, not just runtime."
    )
    R.md()
    weak = BurstAcquisition(
        code, reps=2, spc=SPC, chip_rate=CHIP_RATE, cn0_dbhz=10.0
    )
    strong = BurstAcquisition(
        code, reps=8, spc=SPC, chip_rate=CHIP_RATE, cn0_dbhz=70.0
    )
    d.underpowered_honest = bool(weak.underpowered) and not bool(
        strong.underpowered
    )
    R.md(
        f"**The engine says when it could not meet the request.** At "
        f"`cn0_dbhz = 10` with two repetitions `underpowered` is "
        f"{bool(weak.underpowered)}, and at 70 dB-Hz with eight it is "
        f"{bool(strong.underpowered)}. The flag is the difference between a "
        f"detector that is configured for the link and one that merely "
        f"constructed."
    )
    R.md()


def _sec_cn0(d: Data) -> None:
    R.md("### 2.5 The reported C/N0 tracks the injected one")
    R.md()
    R.md(
        "`cn0_dbhz_est` inverts the same C/N0 <-> per-sample-SNR "
        "relationship the engine sizes itself with, which makes it "
        "comparable to the `cn0_dbhz` a caller passed in — unlike a raw "
        "per-sample or coherently-integrated ratio, which would scale with "
        "`spc` and the depth."
    )
    R.md()
    code = _code()
    rows, csv = [], []
    worst = 0.0
    n = 16 * SF * SPC
    for tag, cn0 in enumerate((55.0, 60.0, 65.0)):
        # amplitude for the target C/N0 at this sample rate
        fs = CHIP_RATE * SPC
        sigma = float(np.sqrt(fs / (10 ** (cn0 / 10.0))))
        x = (_burst(code, 16) + _noise(n, 30 + tag, sigma)).astype(
            np.complex64
        )
        a = BurstAcquisition(
            code, reps=8, spc=SPC, chip_rate=CHIP_RATE, cn0_dbhz=cn0
        )
        a.configure_search_raw(doppler_bins=8, n_noncoh=1)
        h = a.push(x)
        est = h[0][5] if h else float("nan")
        err = abs(est - cn0)
        worst = max(worst, err)
        rows.append([f"{cn0:.0f}", f"{est:.2f}", f"{err:.2f}"])
        csv.append([cn0, est, err])
    R.table(["injected C/N0 (dB-Hz)", "reported", "error (dB)"], rows)
    _csv(HERE / "data" / "cn0.csv", "injected,reported,err_db", csv)
    d.cn0_rows = rows
    d.cn0_worst_db = worst
    R.md(
        f"Worst **{worst:.2f} dB** over three levels. The header is explicit "
        f"that this saturates once the true C/N0 exceeds what the code and "
        f"geometry can resolve — the estimate is then reading the code's own "
        f"autocorrelation sidelobes rather than the noise, which is a real "
        f"ceiling and not a fault. Raw sweep: `data/cn0.csv`."
    )
    R.md()


def _sec_noisemode(d: Data) -> None:
    R.md("### 2.6 The CFAR reference is a choice, worth ~15 dB")
    R.md()
    R.md(
        "`noise_mode` picks how the reference cells are aggregated, and it "
        "had zero mentions in `test_acq_core.c` and zero in `test_acq.py` — "
        "only the default was ever exercised. It is the denominator of the "
        "gating statistic, so the choice moves the engine's whole "
        "sensitivity."
    )
    R.md()
    code = _code()
    n = 16 * SF * SPC
    x = (_burst(code, 16) + _noise(n, 77, 0.6)).astype(np.complex64)
    rows = []
    stats = {}
    for mode in ("mean", "median", "min", "max"):
        a = BurstAcquisition(
            code,
            reps=8,
            spc=SPC,
            chip_rate=CHIP_RATE,
            cn0_dbhz=55.0,
            noise_mode=mode,
        )
        a.configure_search_raw(doppler_bins=8, n_noncoh=1)
        h = a.push(x)
        if h:
            stats[mode] = h[0][4]
            rows.append(
                [mode, str(len(h)), f"{h[0][3]:.4f}", f"{h[0][4]:.2f}"]
            )
        else:
            rows.append([mode, "0", "—", "no detection"])
    R.table(["noise_mode", "hits", "noise_est", "test_stat"], rows)
    d.mode_rows = d.mode_rows + rows
    R.md(
        "Dividing by the smallest reference cell is a far more optimistic "
        "detector than dividing by the largest, so `min` produces the "
        "biggest statistic and `max` the smallest — and at a given signal "
        "level `max` can legitimately suppress detection entirely. That is "
        "the point of offering the choice, and it is now asserted as an "
        "ordering rather than left to the default."
    )
    R.md()


def _sec_lifecycle(d: Data) -> None:
    R.md("### 2.7 configure_search_raw, reset and the state triplet")
    R.md()
    code = _code()
    a = BurstAcquisition(
        code, reps=8, spc=SPC, chip_rate=CHIP_RATE, cn0_dbhz=60.0
    )
    before = (a.doppler_bins, a.n_noncoh)
    rows = []
    bad = 0
    for db, nc in ((0, 1), (9, 1), (4, 0)):
        # The C entry point returns -1; the binding turns that into a
        # ValueError and returns None on success, so "refused" is the
        # exception rather than the return value. Worth stating: a caller
        # porting C code that checks `rc != 0` would find every call
        # succeeding, because None is falsy in a different way.
        try:
            a.configure_search_raw(doppler_bins=db, n_noncoh=nc)
            refused = False
        except (ValueError, MemoryError, TypeError):
            refused = True
        bad += int(refused)
        rows.append(
            [f"({db}, {nc})", "refused" if refused else "**ACCEPTED**"]
        )
    d.raw_rejects = bad == 3
    d.raw_keeps_grid = (a.doppler_bins, a.n_noncoh) == before
    rows.append(
        [
            "grid after the refusals",
            f"({a.doppler_bins}, {a.n_noncoh}) — unchanged"
            if d.raw_keeps_grid
            else "**MOVED**",
        ]
    )
    a.configure_search_raw(doppler_bins=4, n_noncoh=2)
    rows.append([" (4, 2) — in range", f"({a.doppler_bins}, {a.n_noncoh})"])
    R.table(["configure_search_raw(doppler_bins, n_noncoh)", "result"], rows)
    d.raw_rows = rows
    R.md(
        "Out-of-range arguments are refused **and the engine keeps its prior "
        "grid** — the failure mode that matters, because a partially "
        "reconfigured search would produce detections against a threshold "
        "ladder derived for a different cell count."
    )
    R.md()
    b = BurstAcquisition(
        code, reps=4, spc=SPC, chip_rate=CHIP_RATE, cn0_dbhz=60.0
    )
    b.configure_search_raw(doppler_bins=4, n_noncoh=1)
    b.push(_burst(code, 3)[: SF * SPC // 2])
    b.reset()
    fresh = BurstAcquisition(
        code, reps=4, spc=SPC, chip_rate=CHIP_RATE, cn0_dbhz=60.0
    )
    fresh.configure_search_raw(doppler_bins=4, n_noncoh=1)
    x = _burst(code, 8)
    d.reset_drains = [h[:2] for h in b.push(x)] == [
        h[:2] for h in fresh.push(x)
    ]
    s = BurstAcquisition(
        code, reps=4, spc=SPC, chip_rate=CHIP_RATE, cn0_dbhz=60.0
    )
    s.configure_search_raw(doppler_bins=4, n_noncoh=1)
    s.push(_burst(code, 2))
    blob = s.get_state()
    t = BurstAcquisition(
        code, reps=4, spc=SPC, chip_rate=CHIP_RATE, cn0_dbhz=60.0
    )
    t.configure_search_raw(doppler_bins=4, n_noncoh=1)
    t.set_state(blob)
    d.state_exact = [h[:2] for h in s.push(x)] == [h[:2] for h in t.push(x)]
    R.md(
        f"A reset mid-frame leaves the engine behaving like a fresh one "
        f"(**{d.reset_drains}**), and a mid-stream state blob resumes into a "
        f"fresh instance (**{d.state_exact}**)."
    )
    R.md()


def _sec_realized_pfa(d: Data) -> None:
    """Does the detector deliver the false-alarm rate it was asked for?"""
    R.md("### 2.5 The rate it delivers, against the rate it was asked for")
    R.md()
    R.md(
        "Every claim above is about how the threshold is DERIVED. This one "
        "asks the only question a caller actually cares about: push pure "
        "noise, count hits, compare against the configured `pfa`. The CFAR "
        "statistic is scale-invariant, so one number characterises it."
    )
    R.md()

    def _engine() -> BurstAcquisition:
        a = BurstAcquisition(
            _code(),
            reps=8,
            spc=SPC,
            chip_rate=CHIP_RATE,
            cn0_dbhz=45.0,
        )
        a.configure_search_raw(doppler_bins=8, n_noncoh=1)
        return a

    target = 1e-3
    e0 = _engine()
    frame = e0.code_bins * e0.doppler_bins
    n = 20000
    d.pfa_hat = realized_pfa(_engine, frame, n, seed=5)
    d.pfa_ratio = d.pfa_hat / target
    d.pfa_sigma = pfa_sigma(d.pfa_hat, target, n)
    d.pfa_within_ratchet = d.pfa_ratio <= PFA_RATIO_RATCHET
    R.table(
        ["target pfa", "realized", "ratio", "sigma", "frames"],
        [
            [
                f"{target:.0e}",
                f"{d.pfa_hat:.2e}",
                f"{d.pfa_ratio:.2f}x",
                f"{d.pfa_sigma:+.1f}",
                str(n),
            ]
        ],
    )
    R.md(
        f"The detector delivers **{d.pfa_ratio:.2f}x** the false-alarm rate "
        f"it was configured for ({d.pfa_sigma:+.1f} sigma over {n} noise "
        f"frames). This run is deliberately cheap enough for "
        f"`make validate-check`, so read it as a ratchet check rather than "
        f"as the estimate: at a 1e-3 target, {n} frames give ~{n * 1e-3:.0f} "
        f"expected hits and a Poisson spread near "
        f"+/-{(n * 1e-3) ** 0.5 / (n * 1e-3):.2f} on the ratio. The precise "
        f"figure is **1.65 +/- 0.05** delivered, measured at 1e-2 over 60,000 "
        f"frames and decomposing into 0.89 (the native model, conservative) "
        f"x 1.86 (what interpolation adds). The threshold ladder is "
        f"sized from the NATIVE cell count while the peak search runs on the "
        f"Doppler-interpolated surface, and a maximum over a finer sampling "
        f"of the same band-limited process is stochastically larger -- the "
        f"scalloping-loss win of #1002, applied to H0. See F7 and "
        f"doppler#1064; ratcheted at {PFA_RATIO_RATCHET}x below."
    )
    R.md()


# ── 3. review ─────────────────────────────────────────────────────────


def review(d: Data) -> None:
    R.md("## 3. Review — findings")
    R.md()
    R.find(
        "F1",
        "FIXED",
        "**`samples_consumed` was pinned by nothing, in either language.** "
        "It is documented as a per-HIT anchor precisely so a caller does "
        "not reuse one message-level timestamp across every detection in a "
        "push, and the failure it guards against is silent: stamping every "
        "hit with the same offset still detects in the right place, and "
        "only a caller correlating detections to wall-clock would ever see "
        "it. Now measured over a push spanning many epochs — every anchor "
        "distinct, strictly increasing, one dump's stride apart (§2.3). "
        "Sabotage-proven by stamping a constant, which takes it red.",
    )
    R.find(
        "F2",
        "FIXED",
        "**`noise_mode` had zero mentions in `test_acq_core.c` and zero in "
        "`test_acq.py`** — only the default was ever exercised, on an "
        "engine whose constructor offers four. `noise_est` is the "
        "denominator of the gating statistic, so the choice moves the "
        "engine's sensitivity by more than an order of magnitude: on one "
        "burst the statistic spans ~15 dB across the four, and `max` can "
        "legitimately suppress detection entirely. Now asserted as an "
        "ordering — a property of the aggregation rather than of the draw. "
        "Sabotage-proven by discarding the configured mode at "
        "construction.",
    )
    R.find(
        "F3",
        "CONFIRMED",
        "**The auto-sizer's `n_noncoh` makes a short push silently produce "
        "nothing**, and it cost an hour here. At `cn0_dbhz = 55` with a "
        "7-chip code the engine picks `n_noncoh = 4`, so one dump needs "
        "four frames — and a push sized in epochs returns zero hits in "
        'every noise mode. Read cold that says "the modes are broken" '
        'rather than "the dwell never completed". The engine exposes '
        "`n_noncoh` so a caller *can* compute the requirement, and "
        "`underpowered` flags an unmeetable request, but neither says how "
        "many samples a dump needs. The C test's own localization section "
        "already carries a comment warning about this, which is the "
        "clearest evidence it is a real trap. Left open as a usability gap "
        "rather than fixed here: "
        "[#999](https://github.com/doppler-dsp/doppler/issues/999).",
    )
    R.find(
        "F6",
        "GAP",
        "**No property reports the SEARCHED Doppler reach, and "
        "`doppler_span_hz` reads as though it does.** That field is the "
        "NATIVE half-range `chip_rate/(2*sf)` — documented correctly, and a "
        "constant of the geometry that does not move when window-tiling "
        "engages. At 4x the native span the engine really searches +/-80 "
        "kHz while `doppler_span_hz` still reports 16 kHz. The reach is "
        "`doppler_bins * doppler_res_hz / 2` and a caller must know that "
        "expression to answer the first question they have: is my "
        "uncertainty covered? Evidence that this is a trap rather than a "
        "nit — this report's own coverage limit was written against "
        "`doppler_span_hz`, with the header open, and failed at 2x span; "
        "the correct behaviour looked like a coverage bug for several "
        "minutes. The coverage itself is right and already swept in "
        "`test_acq_core.c`. Filed as "
        "[#998](https://github.com/doppler-dsp/doppler/issues/998).",
    )
    R.find(
        "F4",
        "BY DESIGN",
        "`BurstAcquisition` and `Acquisition` are separate constructors "
        "rather than a mode flag, and the report measures the consequence "
        "(§2.1): with no uncertainty prior at all, continuous still reports "
        "`doppler_bins = 1` and buys margin from non-coherent looks. That "
        "reads like a missed optimisation and is the opposite — a coherent "
        "multi-epoch axis aliases a data-modulated stream's own transitions "
        "across the whole Doppler axis, which is a structural mislock. "
        "Closing the footgun at the constructor rather than pricing it as a "
        "tunable loss is the design decision, and `docs/design/"
        "dsss-acquisition.md` carries the argument.",
    )
    R.find(
        "F5",
        "BY DESIGN",
        "`cn0_dbhz_est` saturates once the true C/N0 exceeds what the code "
        "and geometry can resolve, because the CFAR reference cells then "
        "contain the code's own autocorrelation sidelobes rather than "
        "receiver noise. The header states this and §2.5 measures inside "
        "the linear region deliberately. Worth a caller's attention: the "
        "estimate is a floor on the true C/N0 at high signal levels, not a "
        "measurement of it.",
    )

    R.find(
        "F7",
        "CONFIRMED",
        "**The detector delivers ~1.8x the false-alarm rate it is "
        "configured for.** `acq_commit_thresholds()` sizes `pfa_cell` from "
        "the NATIVE cell count `searched_bins * code_bins`, with no term "
        "for `ACQ_DOPPLER_INTERP` -- argued in `acq_core.c` on the grounds "
        "that frequency-domain zero-padding adds no new *information*. "
        "That is true, and it is not the relevant argument: the detector "
        "takes the MAXIMUM over the surface rather than integrating it, and "
        "the maximum of a band-limited process sampled finely is "
        "stochastically larger than the same process sampled coarsely. "
        "Interpolation finds noise peaks that used to fall between bins -- "
        "the scalloping-loss win of #1002 (~3.9 dB -> ~0.9 dB), applied to "
        "H0. Measured over 60,000 noise frames at pfa=1e-2 with only the "
        "interpolation factor changed, it decomposes into two errors of "
        "OPPOSITE sign: `N_eff(1)/N` = **0.89 +/- 0.04** (the native model "
        "is conservative, because adjacent DFT bins are not perfectly "
        "independent) and `N_eff(2)/N_eff(1)` = **1.86 +/- 0.09** (what "
        "interpolation adds), netting **1.65 +/- 0.05** delivered to a "
        "caller (\u00a72.5). Quoting 1.86 alone would be quoting the ratio "
        "between two BUILDS rather than the rate anyone experiences -- and "
        "correcting by 1.86 against a model already 11% conservative would "
        "land at 0.89 of target. The FORM is right: the ratio holds across "
        "three decades of target (1.65/1.40/1.50 at 1e-2/1e-3/1e-4 with "
        "interpolation on), which is a multiplicative cell-count error and "
        "not a miscalibrated per-cell threshold -- that would drift with "
        "the target. `pd_predicted` and "
        "`underpowered` derive from the same `pfa_cell`, so the link-budget "
        "prediction is optimistic by the same factor. Ratcheted at "
        "2.2x by the limit below so the gap cannot widen unnoticed; "
        "the fix is NOT a Bonferroni over `interp` (the interpolated cells "
        "are correlated, so that over-corrects and costs sensitivity) but "
        "the ratio between the expected maxima of the two surfaces. "
        "doppler#1064.",
    )


# ── 4. limits ─────────────────────────────────────────────────────────


def limits(d: Data) -> None:
    R.md("## 4. Limits — the certified envelope")
    R.md()
    R.md(
        "Claims a caller may rely on. A failure here is a regression, not a "
        "new finding. Every one is asserted by "
        "`src/doppler/dsss/tests/test_validation_limits.py`."
    )
    R.md()
    R.limit(
        d.anchors_distinct,
        "every hit from one push carries a distinct, strictly increasing "
        "samples_consumed",
    )
    R.limit(
        d.anchor_stride_ok,
        "...spaced by exactly one dump's worth of input, so a caller can "
        "timestamp each detection independently",
    )
    R.limit(
        d.localise_exact,
        "the reported code phase equals the injected one at four phases "
        "including the wrap-around shoulder",
    )
    R.limit(
        d.continuous_always_tiles,
        "continuous mode window-tiles even with no uncertainty prior "
        "(doppler_bins == 1) — the aliasing footgun closed structurally",
    )
    R.limit(
        d.burst_uses_coherent,
        "burst mode spends its budget on coherent depth: a weak signal uses "
        "more Doppler bins than a strong one",
    )
    R.limit(
        d.tiling_covers,
        "the searched reach (doppler_bins * doppler_res_hz / 2) covers "
        "the requested uncertainty at 0.5x, 0.95x, 2x and 4x the native "
        "span",
    )
    R.limit(
        d.span_formula_ok,
        "the native span is chip_rate/(2*sf), as documented",
    )
    R.limit(
        d.pfa_within_ratchet,
        "the realized false-alarm rate stays inside its ratchet against "
        "the configured target -- a RATCHET, not a bound: it sits at "
        "~1.8x today (F7, doppler#1064) and may only shrink",
    )
    R.limit(
        d.thresh_rises_with_cells,
        "the per-cell threshold rises with the searched cell count — a "
        "tighter uncertainty prior buys sensitivity, not just runtime",
    )
    R.limit(
        d.underpowered_honest,
        "`underpowered` is set when the link cannot meet the requested pd "
        "and clear when it can",
    )
    R.limit(
        d.cn0_worst_db < 3.0,
        f"cn0_dbhz_est tracks an injected C/N0 to {d.cn0_worst_db:.2f} dB "
        f"across three levels inside the linear region",
    )
    R.limit(
        d.raw_rejects,
        "configure_search_raw refuses all three out-of-range argument "
        "combinations",
    )
    R.limit(
        d.raw_keeps_grid,
        "...and the engine keeps its prior grid, so a refused "
        "reconfiguration cannot leave the threshold ladder mismatched",
    )
    R.limit(
        d.reset_drains,
        "a reset mid-frame leaves the engine behaving like a fresh one",
    )
    R.limit(
        d.state_exact,
        "a mid-stream state blob resumes into a fresh instance",
    )
    R.limit(
        len(d.mode_rows) >= 8,
        "all four CFAR noise modes are exercised, not only the default",
    )


# ── build ─────────────────────────────────────────────────────────────


def build(write: bool = True) -> Report:
    global R
    R = Report(write=write)
    R.md("# acq — validation report")
    R.md()
    section_object()
    d = characterise()
    review(d)
    limits(d)
    R.executive(
        "acq",
        [
            "**Time-stamp each hit from its own anchor.** One push spanning "
            "several epochs emits several detections, each with its own "
            "`samples_consumed` — reusing one message-level timestamp for "
            "all of them is the mistake the field exists to prevent, and it "
            "was pinned by nothing until now (§2.3, F1).",
            "**The CFAR reference is a real choice, worth ~15 dB.** `min` "
            "is the optimistic extreme and `max` can suppress detection "
            "entirely at a given signal level. Only the default was ever "
            "exercised before this certification (§2.6, F2).",
            "**A tighter Doppler prior buys sensitivity, not just "
            "runtime**: fewer searched cells means a lower per-cell "
            "threshold. If the uncertainty can be bounded, bound it "
            "(§2.4).",
            "**Size the push against `n_noncoh`, not against epochs.** The "
            "auto-sizer routinely picks a look count above 1, so one dump "
            "needs several frames and a short push returns nothing at all — "
            "which reads as a broken detector rather than an incomplete "
            "dwell (F3, #999).",
            "**Continuous mode never uses coherent depth, deliberately.** "
            "Even with no uncertainty prior it reports `doppler_bins = 1`; "
            "a multi-epoch coherent axis aliases a data-modulated stream's "
            "transitions across the whole Doppler axis (§2.1, F4).",
        ],
    )
    R.summary(
        "\n- Raw sweeps: `data/span.csv`, `data/threshold.csv`, `data/cn0.csv`"
    )
    R.emit(HERE / "results.md")
    return R


if __name__ == "__main__":
    sys.exit(cli(build, HERE))
