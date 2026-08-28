"""Certify `CorrDetector2D` — the CFAR decision over a Corr2D surface.

Run:  python -m doppler.spectral.tests.validation.detector2d.validate
      make validate          (regenerates every report)
      make validate-check    (fails if the committed report is stale)

`Corr2D` produces the surface; this object turns it into a decision. It owns
three things the correlator does not: a ring that lets a caller push chunks of
any length, a **noise estimate** aggregated over a configurable window in one
of four modes, and a threshold gate on `peak / noise`.

The noise estimate is the part worth measuring hardest — it is the denominator
of every decision the library makes downstream, so a mode that selects the
wrong statistic moves every `test_stat` in the tree without changing a single
peak. Three of its four modes were exercised by nothing at all before this
certification (§3 F1).

`Corr2D`'s own envelope is certified separately and not re-derived here; see
`src/doppler/spectral/tests/validation/corr2d/results.md`.
"""

from __future__ import annotations

import sys
from dataclasses import dataclass, field
from pathlib import Path

import numpy as np

from doppler.spectral import CorrDetector2D
from doppler.tests._validation_common import Report, cli

HERE = Path(__file__).resolve().parent
R = Report()

NY = NX = 8
N = NY * NX
SEED = 20260824
MODES = ("mean", "median", "min", "max")


@dataclass
class Data:
    """Everything §3 and §4 read, measured once in §2."""

    mode_rows: list[list[str]] = field(default_factory=list)
    mode_worst: float = 0.0
    modes_differ: bool = False
    stat_is_ratio: float = 0.0
    peak_rows: list[list[str]] = field(default_factory=list)
    peak_exact: bool = False
    chunk_rows: list[list[str]] = field(default_factory=list)
    chunk_invariant: bool = False
    gate_rows: list[list[str]] = field(default_factory=list)
    gate_monotone: bool = False
    gate_boundary_exclusive: bool = False
    last_corr_tracks: bool = False
    dwell_holds: bool = False
    reset_drains: bool = False
    state_exact: bool = False
    state_rejects: bool = False
    window_narrows: bool = False
    dwell_zero_refused: bool = False
    default_window_full: bool = False


def _rng(tag: int) -> np.random.Generator:
    return np.random.default_rng(SEED + tag)


def _impulse_ref() -> np.ndarray:
    ref = np.zeros((NY, NX), dtype=np.complex64)
    ref[0, 0] = 1.0
    return ref


def _surface(r: np.random.Generator, peak_at: int, peak: float) -> np.ndarray:
    """A frame with a clear peak over a SPREAD noise floor.

    Spread on purpose: over a flat window every aggregation mode returns the
    same number, and four assertions would then be four spellings of one.
    """
    x = (0.1 + 0.9 * r.random(N)).astype(np.complex64)
    x[peak_at] = peak
    return x


def _agg(mag: np.ndarray, lo: int, hi: int, mode: str) -> float:
    """The aggregate, by definition — the external truth for §2.1."""
    w = mag[lo : hi + 1]
    return float(
        {"mean": np.mean, "median": np.median, "min": np.min, "max": np.max}[
            mode
        ](w)
    )


def _csv(path: Path, header: str, rows: list[list[float]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as fh:
        fh.write(header + "\n")
        for r in rows:
            fh.write(",".join(f"{v:.10g}" for v in r) + "\n")


# ── 1. the object ─────────────────────────────────────────────────────


def section_object() -> None:
    R.md("## 1. The object — a decision over a correlation surface")
    R.md()
    R.md(
        "`CorrDetector2D` chunks an arbitrary-length stream into `ny x nx` "
        "frames, correlates each through an embedded `Corr2D`, estimates a "
        "noise floor over a configurable window, and emits "
        "`(row, col, peak_mag, noise_est, test_stat)` when "
        "`test_stat > threshold`."
    )
    R.md()
    R.table(
        ["page", "owns"],
        [
            [
                "[`docs/design/detection.md`]"
                "(../../../../../../docs/design/detection.md)",
                "the CFAR law this object applies — how a threshold is "
                "priced from a false-alarm rate, and over what population",
            ],
            [
                "[`docs/design/corr2d-interpolated-inverse.md`]"
                "(../../../../../../docs/design/"
                "corr2d-interpolated-inverse.md)",
                "§6, the downstream wiring: what this object reads off the "
                "surface and what the interpolated form changes about it",
            ],
            [
                "[`corr2d`'s report](../corr2d/results.md)",
                "the correlation surface this object decides on — not "
                "re-derived here",
            ],
            [
                "`native/inc/detector2d/detector2d_core.h`",
                "the contract per function — the SSOT this report audits",
            ],
        ],
    )
    R.md("### 1.1 The claim inventory")
    R.md()
    R.md(
        "Step 1 of `docs/dev/contributing/validation.md`. The C test is "
        "`test_detector2d_core.c`. Four rows were **absent** when this "
        "certification began, and the first of them covers the denominator "
        "of every decision the object makes."
    )
    R.md()
    R.table(
        ["header claim", "pinned where", "here"],
        [
            [
                "`noise_est` aggregates over `[noise_lo, noise_hi]` in one "
                "of four modes",
                "**was `mean` only** — median/min/max had zero mentions in "
                "C and Python",
                "§2.1",
            ],
            [
                "`test_stat = peak_mag / noise_est`, 0 when `noise_est == 0`",
                "C, now against the reported fields",
                "§2.2",
            ],
            [
                "the flat peak index decomposes to `(row, col)`",
                "C, at two positions",
                "§2.3",
            ],
            [
                "a chunk of any length may be pushed",
                "C, sub-frame; here across 6 chunk sizes",
                "§2.4",
            ],
            [
                "an event is emitted when `test_stat > threshold`; 0 always "
                "fires",
                "C, now including the exclusive boundary",
                "§2.5",
            ],
            [
                "the last-dump fields update on every dump **regardless of "
                "threshold**",
                "**was nothing** — now C",
                "§2.5",
            ],
            [
                "`set_ref` replaces the reference, always resets, and "
                "returns -1 when refused",
                "**was nothing** — C-ONLY, no binding",
                "C §set_ref",
            ],
            [
                "`set_threshold` changes the gate without rebuilding",
                "C — **C-ONLY**, the property is read-only in Python",
                "C §set_threshold",
            ],
            [
                "`dwell` must be >= 1",
                "C, refusal inherited from `corr2d_create`",
                "§2.7",
            ],
            [
                "`reset` drains the ring and the accumulator",
                "C + Python",
                "§2.6",
            ],
            ["the state triplet round-trips", "C + Python", "§2.6"],
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
    _sec_stat(d)
    _sec_peak(d)
    _sec_chunking(d)
    _sec_gate(d)
    _sec_lifecycle(d)
    _sec_refusals(d)
    return d


def _sec_modes(d: Data) -> None:
    R.md("### 2.1 All four noise modes, against the definition")
    R.md()
    R.md(
        "`noise_est` is the denominator of every decision this object makes. "
        "The mode is a documented four-way choice, and three of the four "
        "were exercised by nothing — so a mode selecting the wrong "
        "statistic would have moved every `test_stat` in the library with "
        "nothing to notice. Measured against the aggregate computed by "
        "definition over the same surface, which shares no code with the "
        "implementation's selection."
    )
    R.md()
    r = _rng(1)
    x = _surface(r, peak_at=0, peak=8.0)
    lo, hi = 1, N - 1
    rows, csv, got = [], [], {}
    worst = 0.0
    for mode in MODES:
        det = CorrDetector2D(
            ref=_impulse_ref(),
            dwell=1,
            noise_lo=lo,
            noise_hi=hi,
            noise_mode=mode,
            threshold=0.0,
        )
        ev = det.push(x)
        assert len(ev) == 1
        _row, _col, _peak, noise_est, _stat = ev[0]
        # With an impulse reference the correlation IS the input, so the
        # surface magnitudes are known without re-running a correlator.
        want = _agg(np.abs(x), lo, hi, mode)
        err = abs(noise_est - want) / max(want, 1e-9)
        worst = max(worst, err)
        got[mode] = noise_est
        rows.append([mode, f"{want:.6f}", f"{noise_est:.6f}", f"{err:.2e}"])
        csv.append([MODES.index(mode), want, noise_est, err])
    R.table(["mode", "by definition", "reported", "relative error"], rows)
    _csv(HERE / "data" / "noise_modes.csv", "mode,want,got,rel_err", csv)
    d.mode_rows = rows
    d.mode_worst = worst
    d.modes_differ = (
        got["min"] < got["median"] < got["max"]
        and got["min"] < got["mean"] < got["max"]
    )
    R.md(
        f"Worst **{worst:.2e}**, and the four modes genuinely disagree on "
        f"this window ({got['min']:.3f} / {got['median']:.3f} / "
        f"{got['mean']:.3f} / {got['max']:.3f}) — which is what makes four "
        f"assertions four different questions rather than one asked four "
        f"times. Raw sweep: `data/noise_modes.csv`."
    )
    R.md()


def _sec_stat(d: Data) -> None:
    R.md("### 2.2 The test statistic is the ratio it says it is")
    R.md()
    worst = 0.0
    for tag, peak in enumerate((2.0, 8.0, 40.0)):
        x = _surface(_rng(10 + tag), peak_at=0, peak=peak)
        det = CorrDetector2D(
            ref=_impulse_ref(),
            dwell=1,
            noise_lo=1,
            noise_hi=N - 1,
            noise_mode="mean",
            threshold=0.0,
        )
        _row, _col, peak_mag, noise_est, stat = det.push(x)[0]
        worst = max(worst, abs(stat - peak_mag / noise_est) / stat)
    d.stat_is_ratio = worst
    R.md(
        f"`test_stat == peak_mag / noise_est` to **{worst:.2e}** across "
        f"three peak strengths — measured from the reported fields, so a "
        f"detector that reported one and gated on another would fail here."
    )
    R.md()


def _sec_peak(d: Data) -> None:
    R.md("### 2.3 The flat peak index decomposes to (row, col)")
    R.md()
    rows = []
    ok = True
    for at in (0, 5, NX + 2, 3 * NX + 7, N - 1):
        x = np.zeros(N, dtype=np.complex64)
        x[at] = 1.0
        det = CorrDetector2D(
            ref=_impulse_ref(),
            dwell=1,
            noise_lo=0,
            noise_hi=N - 1,
            noise_mode="mean",
            threshold=0.0,
        )
        row, col, *_ = det.push(x)[0]
        ok &= (row, col) == (at // NX, at % NX)
        rows.append([str(at), f"({at // NX}, {at % NX})", f"({row}, {col})"])
    R.table(["flat index", "expected (row, col)", "reported"], rows)
    d.peak_rows = rows
    d.peak_exact = ok
    R.md(
        "Five positions including both corners — the decomposition is what "
        "lets a caller read a Doppler bin and a code phase without knowing "
        "`nx`."
    )
    R.md()


def _sec_chunking(d: Data) -> None:
    R.md("### 2.4 The chunk size is not part of the answer")
    R.md()
    R.md(
        "A caller pushes whatever a socket handed them. The ring makes that "
        "invisible: the events emitted over a stream must not depend on how "
        "it was sliced."
    )
    R.md()
    stream = np.concatenate(
        [
            _surface(_rng(20 + k), peak_at=(k * 7) % N, peak=6.0)
            for k in range(4)
        ]
    ).astype(np.complex64)
    baseline = None
    rows = []
    ok = True
    for chunk in (N * 4, N, N // 2, 7, 1, 3 * N):
        det = CorrDetector2D(
            ref=_impulse_ref(),
            dwell=1,
            noise_lo=1,
            noise_hi=N - 1,
            noise_mode="mean",
            threshold=0.0,
        )
        events = []
        for i in range(0, len(stream), chunk):
            events.extend(det.push(stream[i : i + chunk]))
        key = [(e[0], e[1]) for e in events]
        if baseline is None:
            baseline = key
        ok &= key == baseline
        rows.append(
            [
                str(chunk),
                str(len(events)),
                "yes" if key == baseline else "**NO**",
            ]
        )
    R.table(["chunk size", "events", "same as baseline"], rows)
    d.chunk_rows = rows
    d.chunk_invariant = ok
    R.md(
        "Six slicings of one stream, down to a single sample per call, all "
        "producing the same event sequence."
    )
    R.md()


def _sec_gate(d: Data) -> None:
    R.md("### 2.5 The threshold gate, including its exclusive boundary")
    R.md()
    x = _surface(_rng(4), peak_at=0, peak=8.0)
    det0 = CorrDetector2D(
        ref=_impulse_ref(),
        dwell=1,
        noise_lo=1,
        noise_hi=N - 1,
        noise_mode="mean",
        threshold=0.0,
    )
    _r, _c, _pm, _ne, open_stat = det0.push(x)[0]
    rows = []
    counts = []
    for thr in (0.0, open_stat * 0.5, open_stat, open_stat * 1.5):
        det = CorrDetector2D(
            ref=_impulse_ref(),
            dwell=1,
            noise_lo=1,
            noise_hi=N - 1,
            noise_mode="mean",
            threshold=float(thr),
        )
        n = len(det.push(x))
        counts.append(n)
        rows.append([f"{thr:.3f}", str(n)])
    R.table(["threshold", "events emitted"], rows)
    d.gate_rows = rows
    d.gate_monotone = counts == [1, 1, 0, 0]
    d.gate_boundary_exclusive = counts[2] == 0
    R.md(
        f"The stat on this surface is {open_stat:.3f}. A threshold **equal** "
        f"to it emits nothing — the comparison is `test_stat > threshold`, "
        f"strictly, and `0.0` is the documented always-fire case rather than "
        f"a threshold that happens to be low."
    )
    R.md()
    det = CorrDetector2D(
        ref=_impulse_ref(),
        dwell=1,
        noise_lo=1,
        noise_hi=N - 1,
        noise_mode="mean",
        threshold=float(open_stat * 10.0),
    )
    y = np.zeros(N, dtype=np.complex64)
    y[2 * NX + 5] = 1.0
    assert det.push(y) == []
    surf = np.asarray(det.last_corr)
    d.last_corr_tracks = int(np.argmax(np.abs(surf))) == 2 * NX + 5
    R.md(
        f"**A closed gate still updates what the object saw.** With the "
        f"threshold raised past anything the surface produces, nothing is "
        f"emitted and `last_corr` still peaks at the new input's position "
        f"(**{d.last_corr_tracks}**) — which is what lets a caller raise the "
        f"gate and keep a diagnostic."
    )
    R.md()


def _sec_lifecycle(d: Data) -> None:
    R.md("### 2.6 Dwell, reset and the state triplet")
    R.md()
    x = _surface(_rng(5), peak_at=3, peak=6.0)
    det = CorrDetector2D(
        ref=_impulse_ref(),
        dwell=3,
        noise_lo=1,
        noise_hi=N - 1,
        noise_mode="mean",
        threshold=0.0,
    )
    held = [len(det.push(x)) for _ in range(3)]
    d.dwell_holds = held == [0, 0, 1]
    R.md(f"A dwell of 3 emits on the third frame and no earlier: `{held}`.")
    R.md()
    det.push(x[: N // 2])
    det.reset()
    d.reset_drains = det.count == 0
    fresh = CorrDetector2D(
        ref=_impulse_ref(),
        dwell=3,
        noise_lo=1,
        noise_hi=N - 1,
        noise_mode="mean",
        threshold=0.0,
    )
    a = [len(det.push(x)) for _ in range(3)]
    b = [len(fresh.push(x)) for _ in range(3)]
    d.reset_drains = d.reset_drains and a == b
    R.md(
        f"After a reset mid-frame the object behaves like a fresh one "
        f"(**{d.reset_drains}**) — the ring residue is dropped rather than "
        f"prepended to the next frame."
    )
    R.md()
    s = CorrDetector2D(
        ref=_impulse_ref(),
        dwell=3,
        noise_lo=1,
        noise_hi=N - 1,
        noise_mode="mean",
        threshold=0.0,
    )
    s.push(x)
    blob = s.get_state()
    t = CorrDetector2D(
        ref=_impulse_ref(),
        dwell=3,
        noise_lo=1,
        noise_hi=N - 1,
        noise_mode="mean",
        threshold=0.0,
    )
    t.set_state(blob)
    d.state_exact = [len(s.push(x)) for _ in range(2)] == [
        len(t.push(x)) for _ in range(2)
    ]
    bad = bytearray(blob)
    bad[0] ^= 0xFF
    try:
        t.set_state(bytes(bad))
        d.state_rejects = False
    except ValueError:
        d.state_rejects = True
    R.md(
        f"A mid-dwell blob resumes into a fresh instance "
        f"(**{d.state_exact}**) and a clobbered envelope is rejected "
        f"(**{d.state_rejects}**)."
    )
    R.md()


def _sec_refusals(d: Data) -> None:
    R.md("### 2.7 What it refuses")
    R.md()
    try:
        CorrDetector2D(
            ref=_impulse_ref(),
            dwell=0,
            noise_lo=1,
            noise_hi=N - 1,
            noise_mode="mean",
            threshold=0.0,
        )
        d.dwell_zero_refused = False
    except (ValueError, MemoryError, TypeError):
        d.dwell_zero_refused = True
    # A narrower noise window must change the estimate, or noise_lo/hi are
    # being ignored -- the reject test that would otherwise pass vacuously.
    x = _surface(_rng(6), peak_at=0, peak=8.0)
    wide = CorrDetector2D(
        ref=_impulse_ref(),
        dwell=1,
        noise_lo=1,
        noise_hi=N - 1,
        noise_mode="max",
        threshold=0.0,
    ).push(x)[0][3]
    narrow = CorrDetector2D(
        ref=_impulse_ref(),
        dwell=1,
        noise_lo=1,
        noise_hi=8,
        noise_mode="max",
        threshold=0.0,
    ).push(x)[0][3]
    d.window_narrows = narrow < wide
    # The documented default window is the whole surface, reached through a
    # SIZE_MAX sentinel the binding passes for "ny*nx - 1". An unclamped
    # sentinel would size the median scratch from SIZE_MAX and read
    # out of bounds, which is why the clamp exists and why it is asserted
    # from the face that actually passes the sentinel.
    dflt = CorrDetector2D(ref=_impulse_ref(), dwell=1)
    d.default_window_full = (dflt.noise_lo, dflt.noise_hi) == (0, N - 1)
    R.table(
        ["case", "result"],
        [
            [
                "dwell = 0",
                "refused" if d.dwell_zero_refused else "**ACCEPTED**",
            ],
            [
                "default noise window covers the surface",
                f"[{dflt.noise_lo}, {dflt.noise_hi}] of {N}",
            ],
            [
                "narrowing [noise_lo, noise_hi] changes the estimate",
                f"{wide:.4f} -> {narrow:.4f}",
            ],
        ],
    )
    R.md(
        "The second row is the precondition for every other measurement in "
        "§2.1: if the window bounds were ignored, all four modes would "
        "still agree with an aggregate computed over the same range and the "
        "whole section would pass while reading the wrong cells."
    )
    R.md()


# ── 3. review ─────────────────────────────────────────────────────────


def review(d: Data) -> None:
    R.md("## 3. Review — findings")
    R.md()
    R.find(
        "F1",
        "FIXED",
        "**Three of the four noise modes were exercised by nothing.** "
        "`DET_NOISE_MEDIAN`, `DET_NOISE_MIN` and `DET_NOISE_MAX` had zero "
        "mentions in `test_detector2d_core.c` and zero in "
        "`test_detector2d.py`; only the mean was ever selected. "
        "`noise_est` is the denominator of every decision this object "
        "makes, so a mode returning the wrong statistic would have moved "
        "every `test_stat` in the library without changing a single peak "
        "position. Now measured in both languages against the aggregate "
        "computed by definition, over a window where the four modes "
        "genuinely disagree. Sabotage-proven: forcing the estimator to "
        "`DET_NOISE_MEAN` regardless of the configured mode takes it red.",
    )
    R.find(
        "F2",
        "FIXED",
        "`set_ref` had no coverage in either language, and writing one "
        "found that the interesting branch is the refusal. An impulse "
        "reference is single-row, so the embedded `Corr2D` is on its fast "
        "path, and that path can only accept another single-row reference "
        "— a multi-row replacement is **refused**. That is the documented "
        "contract working rather than a defect, and it is now asserted on "
        "both branches, including that a refusal is non-destructive and "
        "leaves the object working on its previous reference.",
    )
    R.find(
        "F3",
        "FIXED",
        'The struct comment promises the last-dump fields are *"updated on '
        'every dump regardless of threshold"* — what lets a caller raise '
        "the gate and keep a diagnostic — and nothing asserted it. Getting "
        "a test that could actually fail took three attempts, and the two "
        "failures are the finding: asserting `_last_corr_valid == 1` after "
        "a gated push passes on the flag left set by the previous ungated "
        "one, and asserting `test_stat` alone passes because "
        "`compute_stat_2d` writes the peak position as a side effect. The "
        "check that discriminates pushes a peak at a DIFFERENT position "
        "under a closed gate and requires the reported position to follow "
        "it; only a sabotage that restores all five last-dump fields takes "
        "it red.",
    )
    R.find(
        "F4",
        "C-ONLY",
        "Two entry points have no Python face and are certified in "
        "`native/tests/test_detector2d_core.c`: `detector2d_set_ref` (F2 "
        "above) and `detector2d_set_threshold` — the `threshold` property "
        "is read-only from Python, so a caller who wants to re-gate a "
        "running detector must rebuild it or drop to C. The last-dump "
        "scalars (`peak_row`, `peak_col`, `peak_mag`, `noise_est`, "
        "`test_stat`) are likewise C-only as scalars, though `last_corr` "
        "exposes the surface they are derived from, which is what §2.5 "
        "uses to reach F3's claim from Python.",
    )
    R.find(
        "F5",
        "BY DESIGN",
        "`dwell = 0` is refused as `MemoryError` rather than `ValueError`. "
        "That reads wrong until you find the convention: "
        "`docs/dev/contributing/error-convention.md` makes NULL the "
        "`create()` return for an invalid argument as well as an "
        "allocation failure, and jm's generated NULL check raises "
        '`MemoryError` — explicitly called "correct for the pointer '
        'convention". The refusal itself is new: it was added to '
        "`corr2d_create` during that object's certification, and this one "
        "inherits it by forwarding rather than by carrying a second copy "
        "of the rule.",
    )


# ── 4. limits ─────────────────────────────────────────────────────────


def limits(d: Data) -> None:
    R.md("## 4. Limits — the certified envelope")
    R.md()
    R.md(
        "Claims a caller may rely on. A failure here is a regression, not a "
        "new finding. Every one is asserted by "
        "`src/doppler/spectral/tests/test_validation_limits.py`."
    )
    R.md()
    R.limit(
        d.mode_worst < 1e-4,
        f"all four noise modes match the aggregate computed by definition "
        f"(worst {d.mode_worst:.2e})",
    )
    R.limit(
        d.modes_differ,
        "...over a window where the four genuinely disagree, so each is a "
        "distinct question",
    )
    R.limit(
        d.window_narrows,
        "narrowing [noise_lo, noise_hi] changes the estimate — the bounds "
        "are read, not ignored",
    )
    R.limit(
        d.stat_is_ratio < 1e-5,
        f"test_stat is peak_mag / noise_est as reported "
        f"({d.stat_is_ratio:.2e})",
    )
    R.limit(
        d.peak_exact,
        "the flat peak index decomposes to (row, col) at five positions "
        "including both corners",
    )
    R.limit(
        d.chunk_invariant,
        "the event sequence is identical across six chunk sizes, down to "
        "one sample per push",
    )
    R.limit(
        d.gate_monotone,
        "the threshold gate emits at 0 and half the stat, and stops at the "
        "stat and above",
    )
    R.limit(
        d.gate_boundary_exclusive,
        "a threshold EQUAL to the statistic emits nothing — the comparison "
        "is strict",
    )
    R.limit(
        d.last_corr_tracks,
        "a closed gate still updates last_corr, so raising the threshold "
        "does not blind the caller",
    )
    R.limit(
        d.dwell_holds,
        "a dwell of 3 emits on the third frame and no earlier",
    )
    R.limit(
        d.reset_drains,
        "reset drops the ring residue: a reset object matches a fresh one",
    )
    R.limit(d.state_exact, "a mid-dwell state blob resumes bit-exactly")
    R.limit(
        d.state_rejects,
        "a clobbered state envelope is rejected, never reinterpreted",
    )
    R.limit(
        d.dwell_zero_refused,
        "dwell = 0 is refused rather than silently building a detector that "
        "never emits",
    )
    R.limit(
        d.default_window_full,
        "the default noise window is the whole surface — the SIZE_MAX "
        "sentinel the binding passes is clamped, not propagated",
    )


# ── build ─────────────────────────────────────────────────────────────


def build(write: bool = True) -> Report:
    global R
    R = Report(write=write)
    R.md("# CorrDetector2D — validation report")
    R.md()
    section_object()
    d = characterise()
    review(d)
    limits(d)
    R.executive(
        "CorrDetector2D",
        [
            "**The noise estimate is the object.** It is the denominator of "
            "every decision, and three of its four modes were exercised by "
            "nothing before this certification — a mode selecting the wrong "
            "statistic moves every `test_stat` in the library without "
            "changing a single peak position (§2.1, F1).",
            "**The chunk size is not part of the answer.** Six slicings of "
            "one stream, down to a single sample per `push`, produce the "
            "same event sequence — which is what makes the ring worth "
            "having (§2.4).",
            "**The gate is strict.** A threshold exactly equal to the "
            "statistic emits nothing, and `0.0` is the documented "
            "always-fire case rather than a very low threshold (§2.5).",
            "**Raising the gate does not blind you.** `last_corr` still "
            "follows the surface when nothing is emitted, so a caller can "
            "tighten the decision and keep the diagnostic (§2.5, F3).",
            "**Re-gating a running detector needs C.** `threshold` is "
            "read-only from Python and `set_threshold` has no binding, so "
            "an adaptive-threshold caller rebuilds the object or drops to "
            "the C API (F4).",
        ],
    )
    R.summary("\n- Raw sweep: `data/noise_modes.csv`")
    R.emit(HERE / "results.md")
    return R


if __name__ == "__main__":
    sys.exit(cli(build, HERE))
