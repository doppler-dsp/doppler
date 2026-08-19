"""EMA — certification evidence, measured through the shipped binding.

Run directly to regenerate `results.md`, the plots and the CSVs:

    uv run python src/doppler/util/tests/validation/ema/validate.py

`--check` re-renders in memory and diffs against the committed bytes;
`make validate` writes, `make validate-check` checks. Every limit this
records is asserted by
`src/doppler/util/tests/test_validation_limits.py`, which runs this same
`build(write=False)`.

The order is the campaign's, not this file's:
`native/inc/util/util_core.h` is the SSOT, `native/tests/test_util_core.c`
§1-§8 certifies it in C, and this measures the same properties through
`doppler.util` to show the binding delivers them — plus the two
statistical laws (§2.5, §2.6) that no C assertion covers because they are
properties of the recursion over a long run rather than of one step.
"""

from __future__ import annotations

import math
import re
import struct
import sys
from dataclasses import dataclass, field
from decimal import Decimal, getcontext
from pathlib import Path

import numpy as np

from doppler.tests._repo import repo_root
from doppler.tests._validation_common import Report, cli
from doppler.util import ema_alpha_decim, ema_step

HERE = Path(__file__).resolve().parent
DATA = HERE / "data"
ROOT = repo_root(__file__)

R = Report()

# ── Findings F1 and F2 are claims about OTHER files, so they are read ──
# rather than asserted.
#
# Both were written as fixed prose ("NOTHING USES IT", "`agc_steps` forms
# its detector pole by repeated multiplication") and both became false the
# moment the migration landed -- while `make validate-check` went on
# reporting the report "up to date", because that gate re-runs this
# generator and compares, and the generator was the stale part. A finding
# naming `agc_core.c` is a claim ABOUT `agc_core.c`; it rots exactly when
# the work succeeds, which is the worst possible moment for it to read as
# still open.
#
# Deriving it makes the rot impossible: the verdict follows the tree, and
# `validate-check` turns red the moment the tree moves and the committed
# report disagrees. That converts a sentence that needs remembering into a
# gate that does not.
SITES = {
    "agc_core.c": ROOT / "native/src/agc/agc_core.c",
    "async_dsss_receiver_core.c": (
        ROOT / "native/src/async_dsss_receiver/async_dsss_receiver_core.c"
    ),
    "acc_trace_core.c": ROOT / "native/src/acc_trace/acc_trace_core.c",
}

# Comments are stripped before matching, because they discuss the very
# thing being detected: agc_core.c's pole now carries a comment naming the
# `(1 - alpha)` repeated multiply it replaced, and a naive grep reads that
# as the defect still being present.
_COMMENTS = re.compile(r"/\*.*?\*/|//[^\n]*", re.S)
_CALL = re.compile(r"\bema_(?:step|alpha_decim)\s*\(")
_DECIM_CALL = re.compile(r"\bema_alpha_decim\s*\(")


def _code(path: Path) -> str:
    """The file's code with comments removed."""
    return _COMMENTS.sub(" ", path.read_text())


def adopters() -> dict[str, bool]:
    """Which historical call sites now call the shared primitive."""
    return {n: bool(_CALL.search(_code(p))) for n, p in SITES.items()}


def agc_pole_is_compounded() -> bool:
    """True once `agc_steps` forms its detector pole with the primitive."""
    return bool(_DECIM_CALL.search(_code(SITES["agc_core.c"])))


# Coefficients spanning the range the library actually uses: 1e-5 is a
# long detection average, 0.05 the AGC's shipped detector, 0.5 a fast
# tracker.
ALPHAS = (1e-5, 1e-4, 1e-3, 0.01, 0.05, 0.5)

# A spread of magnitudes and signs for the exactness sweeps. The large
# and tiny entries are the ones that expose a re-rounded state.
MAGS = (0.0, 1.0, -1.0, 0.5, -0.25, 7.0, 1e6, -1e6, 1e-9, 123.456, -987.6)


def _ulps(a: float, b: float) -> int:
    """Distance in representable doubles, the unit an exactness claim
    belongs in — a relative error hides how many values lie between."""
    ia = struct.unpack("<q", struct.pack("<d", a))[0]
    ib = struct.unpack("<q", struct.pack("<d", b))[0]
    return abs(ia - ib)


def _two_product(state: float, x: float, alpha: float) -> float:
    """The other algebraic form, as `acc_trace` writes it.

    Present so the report measures the choice rather than asserting it:
    every accuracy claim below is a comparison against this, not a bare
    number.
    """
    return alpha * x + (1.0 - alpha) * state


def _reference(alpha: float, xs: np.ndarray) -> float:
    """The converged state computed at 40 significant digits.

    The external truth the two double-precision forms are scored
    against. Neither form is used to produce it, so this is not a
    consistency test between two things that could share a defect.
    """
    getcontext().prec = 40
    a = Decimal(alpha)
    s = Decimal(0)
    for x in xs:
        s = s + a * (Decimal(float(x)) - s)
    return s


def _csv(path: Path, cols: list[np.ndarray], header: str) -> None:
    np.savetxt(
        path,
        np.column_stack(cols),
        delimiter=",",
        header=header,
        comments="",
        fmt="%.6g",
    )


@dataclass
class Data:
    """Everything section 2 measured, so 3 and 4 need no re-runs."""

    acc_alpha: np.ndarray = field(default_factory=lambda: np.array([]))
    acc_inc: np.ndarray = field(default_factory=lambda: np.array([]))
    acc_two: np.ndarray = field(default_factory=lambda: np.array([]))
    passthru_bad_naive: int = 0
    passthru_bad_shipped: int = 0
    passthru_total: int = 0
    freeze_bad: int = 0
    fixed_bad: int = 0
    decim_alpha: np.ndarray = field(default_factory=lambda: np.array([]))
    decim_ulps_direct: np.ndarray = field(default_factory=lambda: np.array([]))
    decim_ulps_shipped: np.ndarray = field(
        default_factory=lambda: np.array([])
    )
    compound_rows: list[tuple[float, int, float]] = field(default_factory=list)
    var_rows: list[tuple[float, float, float]] = field(default_factory=list)
    tau_rows: list[tuple[float, int, float]] = field(default_factory=list)
    range_ok: bool = True
    monotone_ok: bool = True
    overshoot_bad: int = 0
    sat_ok: bool = True


# ── 1. the object ────────────────────────────────────────────────────
def section_object() -> None:
    # Title and provenance belong to the executive summary, which
    # Report.executive renders ahead of everything here.
    R.md("## 1. The object")
    R.md()
    R.md(
        "A first-order exponential moving average: `state + alpha * (x - "
        "state)`, the running average every estimator in the library is "
        "built on. It is one function because it was four — a power "
        "detector, a lock statistic, a spectrum accumulator and the "
        "recursion `det_ema_alpha` sizes — written in two different "
        "algebraic forms that are identical on paper and not in floating "
        "point."
    )
    R.md()
    R.md("Design and API, not restated here:")
    R.md()
    R.md(
        "- `native/inc/util/util_core.h` — the SSOT for every claim "
        "below (`ema_step`, `ema_alpha_decim`)"
    )
    R.md("- `native/tests/test_util_core.c` §1-§8 — the C certification")
    R.md(
        "- [EMA design](../../../../../../docs/design/ema.md) — why this "
        "algebraic form, why the boundaries are contract, and why an EMA "
        "is not a loop filter"
    )
    R.md("- `doppler.util.ema_step` / `ema_alpha_decim` — measured below")
    R.md()
    R.md("### Claim coverage — every prose claim in the header")
    R.md()
    R.md(
        "The campaign's order is header first: enumerate what the header "
        "asserts, then ask of each whether it is pinned in C, and only "
        "then measure it through the binding. `NEW` marks a law this "
        "report establishes that no C assertion covers — both are "
        "statistical properties of a long run, not of one step."
    )
    R.md()
    R.table(
        ["#", "claim in `util_core.h`", "C section", "here"],
        [
            [
                "C1",
                "the recursion is `state + alpha*(x - state)`",
                "§4",
                "§2.1",
            ],
            [
                "C2",
                "more accurate than the two-product form, by a margin "
                "that grows as alpha shrinks",
                "—",
                "§2.1 (NEW)",
            ],
            ["C3", "`alpha == 1` is EXACT pass-through", "§1", "§2.2"],
            ["C4", "`alpha == 0` EXACTLY freezes the state", "§2", "§2.2"],
            [
                "C5",
                "the fixed point does not move, at any alpha",
                "§3",
                "§2.2",
            ],
            ["C6", "the step never passes the observation", "§4", "§2.2"],
            ["C7", "`alpha > 1` saturates to pass-through", "§5", "§2.2"],
            [
                "C8",
                "NOT total in `x`: a non-finite observation poisons the state",
                "—",
                "§3 F3, by design",
            ],
            ["C9", "`ema_alpha_decim` returns `1-(1-alpha)^d`", "§7", "§2.4"],
            ["C10", "at `d == 1` it returns alpha EXACTLY", "§6", "§2.3"],
            [
                "C11",
                "`expm1`/`log1p` avoids the cancellation the direct form has",
                "§6",
                "§2.3",
            ],
            [
                "C12",
                "the result stays in [0, 1] and is non-decreasing in d",
                "§8",
                "§2.4",
            ],
            ["C13", "noise reduction is `(2-alpha)/alpha`", "—", "§2.5 (NEW)"],
            [
                "C14",
                "the memory is `-1/ln(1-alpha)` observations",
                "—",
                "§2.6 (NEW)",
            ],
        ],
    )
    R.md()
    R.md(
        "**What Python cannot reach.** Nothing, unusually — both "
        "functions are module-level and fully exposed. The one "
        "C-only property is structural rather than behavioural and is "
        "recorded as §3 F6: every C caller inlines the header definition "
        "while this report exercises the single out-of-line copy the "
        "`extern inline` translation unit emits, and it is the C99 idiom "
        "rather than a test that makes those the same source."
    )
    R.md()


# ── 2. characterisation ──────────────────────────────────────────────
def characterise() -> Data:
    d = Data()
    R.md("## 2. Characterisation")
    R.md()
    R.md(
        "Measured, no verdicts — those are §3. Each heading names the C "
        "section it tracks; the numbering here is this report's own, "
        "because a section routinely merges several C ones."
    )
    R.md()

    # ── 2.1 accuracy of the two forms ────────────────────────────────
    R.md("### 2.1 The two algebraic forms, scored on accuracy (C §4)")
    R.md()
    R.md(
        "Both forms run over the same 2000-sample random stream and are "
        "scored against a 40-digit reference computed from neither of "
        "them. This is the measurement the design's choice of form rests "
        "on, so it is reported as a comparison, never as a bare number."
    )
    R.md()
    rng = np.random.default_rng(20260812)
    xs = rng.uniform(0.0, 2.0, 2000)
    a_l, inc_l, two_l = [], [], []
    rows = []
    for alpha in ALPHAS:
        ref = _reference(alpha, xs)
        s_inc = 0.0
        s_two = 0.0
        for x in xs:
            s_inc = ema_step(s_inc, float(x), alpha)
            s_two = _two_product(s_two, float(x), alpha)
        e_inc = abs(Decimal(s_inc) - ref)
        e_two = abs(Decimal(s_two) - ref)
        a_l.append(alpha)
        inc_l.append(float(e_inc))
        two_l.append(float(e_two))
        rows.append(
            [
                f"{alpha:g}",
                f"{float(e_inc):.2e}",
                f"{float(e_two):.2e}",
                f"{float(e_two / e_inc):.1f}x" if e_inc > 0 else "—",
            ]
        )
    d.acc_alpha = np.array(a_l)
    d.acc_inc = np.array(inc_l)
    d.acc_two = np.array(two_l)
    R.table(["alpha", "shipped (incremental)", "two-product", "ratio"], rows)
    R.md()
    R.md(
        "The shipped form is the more accurate one across the range, and "
        "the margin widens as the average lengthens — which is the "
        "direction every narrow-band estimator moves. The mechanism is "
        "structural: the incremental form adds a small correction to a "
        "large state, so the large quantity is never re-rounded."
    )
    R.md()
    R.md("![accuracy of the two forms](accuracy.png)")
    R.md()

    # ── 2.2 the boundaries ───────────────────────────────────────────
    R.md("### 2.2 The boundaries are exact, not merely close (C §1-§5)")
    R.md()
    n = 0
    bad_naive = bad_shipped = 0
    freeze_bad = fixed_bad = overshoot_bad = 0
    for s in MAGS:
        for x in MAGS:
            n += 1
            if ema_step(s, x, 1.0) != x:
                bad_shipped += 1
            if s + 1.0 * (x - s) != x:  # the bare recursion, unguarded
                bad_naive += 1
            if ema_step(s, x, 0.0) != s:
                freeze_bad += 1
            for a in (0.05, 0.25, 0.75):
                y = ema_step(s, x, a)
                lo, hi = (s, x) if s <= x else (x, s)
                if not (lo - 1e-12 <= y <= hi + 1e-12):
                    overshoot_bad += 1
        for a in (0.0, 1e-5, 0.01, 0.5, 1.0):
            if ema_step(s, s, a) != s:
                fixed_bad += 1
    d.passthru_total = n
    d.passthru_bad_naive = bad_naive
    d.passthru_bad_shipped = bad_shipped
    d.freeze_bad = freeze_bad
    d.fixed_bad = fixed_bad
    d.overshoot_bad = overshoot_bad
    d.sat_ok = (
        ema_step(0.0, 1.0, 1.5) == 1.0 and ema_step(10.0, -10.0, 4.0) == -10.0
    )
    R.table(
        ["boundary", "shipped", "unguarded recursion"],
        [
            [
                "`alpha = 1` returns x exactly",
                f"{n - bad_shipped}/{n}",
                f"{n - bad_naive}/{n}",
            ],
            [
                "`alpha = 0` returns state exactly",
                f"{n - freeze_bad}/{n}",
                "—",
            ],
            [
                "fixed point does not move",
                f"{len(MAGS) * 5 - fixed_bad}/{len(MAGS) * 5}",
                "—",
            ],
            [
                "step never passes the observation",
                f"{n * 3 - overshoot_bad}/{n * 3}",
                "—",
            ],
            ["`alpha > 1` saturates", "yes" if d.sat_ok else "NO", "—"],
        ],
    )
    R.md()
    R.md(
        f"`alpha = 1` is the case that separates the two forms: the "
        f"unguarded recursion misses it on {bad_naive} of {n} pairs here, "
        "and `det_ema_alpha(0, 0)` returns exactly 1.0, so it is a "
        "coefficient callers really pass."
    )
    R.md()

    # ── 2.3 the compounded coefficient at d == 1 ─────────────────────
    R.md("### 2.3 The compounded coefficient at `d = 1` (C §6)")
    R.md()
    R.md(
        "The property that makes a decimated path comparable to an "
        "undecimated one: the chunk coefficient for a chunk of one "
        "sample must be the per-sample coefficient itself. Scored in "
        "ulps, because a relative error hides how many representable "
        "values lie between."
    )
    R.md()
    fine = np.array([1e-9, 1e-7, 1e-5, 6.25e-5, 1e-3, 0.01, 0.05, 0.5])
    ud, us = [], []
    rows = []
    for a in fine:
        direct = 1.0 - (1.0 - float(a))  # the cancelling form
        shipped = ema_alpha_decim(float(a), 1)
        ud.append(_ulps(direct, float(a)))
        us.append(_ulps(shipped, float(a)))
        rows.append(
            [
                f"{a:g}",
                str(_ulps(direct, float(a))),
                str(_ulps(shipped, float(a))),
            ]
        )
    d.decim_alpha = fine
    d.decim_ulps_direct = np.array(ud, dtype=float)
    d.decim_ulps_shipped = np.array(us, dtype=float)
    R.table(
        ["alpha", "direct `1-(1-alpha)`, ulps off", "shipped, ulps off"], rows
    )
    R.md()
    R.md(
        "The direct form's error grows without bound as the average "
        "lengthens; the shipped form is exact at every coefficient "
        "tried. "
        + (
            "`agc_steps` now forms its detector pole with "
            "`ema_alpha_decim` and therefore sits in the right-hand "
            "column — §3 F2, fixed."
            if agc_pole_is_compounded()
            else "`agc_steps` forms its detector pole by repeated "
            "multiplication and therefore sits in the left-hand column "
            "today — recorded as §3 F2."
        )
    )
    R.md()
    R.md("![ulps off at d = 1](decim_d1.png)")
    R.md()

    # ── 2.4 the compounding identity ─────────────────────────────────
    R.md("### 2.4 Compounding is exact, not approximate (C §7, §8)")
    R.md()
    R.md(
        "`d` steps of `alpha` must equal one step of "
        "`ema_alpha_decim(alpha, d)`. This is what *decimation does not "
        "retune the loop* means."
    )
    R.md()
    rows = []
    for a in (1e-4, 1e-3, 0.01, 0.05):
        for dd in (2, 8, 32, 128):
            s = 0.0
            for _ in range(dd):
                s = ema_step(s, 1.0, a)
            chunk = ema_step(0.0, 1.0, ema_alpha_decim(a, dd))
            resid = abs(s - chunk)
            d.compound_rows.append((a, dd, resid))
            rows.append([f"{a:g}", str(dd), f"{resid:.1e}"])
    R.table(["alpha", "d", "|per-sample − chunked|"], rows)
    R.md()
    rng_ok = True
    mono_ok = True
    for a in (1e-7, 1e-3, 0.05, 0.5, 0.99):
        prev = ema_alpha_decim(a, 1)
        for dd in (2, 4, 8, 16, 64, 256):
            v = ema_alpha_decim(a, dd)
            if not (0.0 <= v <= 1.0):
                rng_ok = False
            if v < prev:
                mono_ok = False
            prev = v
    d.range_ok = rng_ok
    d.monotone_ok = mono_ok
    R.md(
        f"Across the same coefficients the compounded value stays in "
        f"[0, 1] ({'yes' if rng_ok else 'NO'}) and is non-decreasing in "
        f"`d` ({'yes' if mono_ok else 'NO'}) — a longer chunk moves the "
        "average further, never less far, and never past its "
        "observation."
    )
    R.md()

    # ── 2.5 the noise-reduction law ──────────────────────────────────
    R.md("### 2.5 Noise reduction — the law `det_ema_alpha` inverts (NEW)")
    R.md()
    R.md(
        "For a white input of variance `s2`, the converged output "
        "variance is `s2 * alpha/(2 - alpha)`, so the estimator's SNR "
        "improves by `(2 - alpha)/alpha`. `det_ema_alpha` in "
        "`detection_core.h` **inverts exactly this law** to size a "
        "coefficient for a requested estimator SNR — so the law is "
        "already load-bearing, and nothing checked the EMA delivered it. "
        "No C section covers it: it is a property of a long run, not of "
        "one step."
    )
    R.md()
    rows = []
    rng = np.random.default_rng(7)
    for a in (1e-3, 0.01, 0.05, 0.2):
        burn = int(20.0 / a)
        n_meas = max(200_000, burn * 4)
        x = rng.standard_normal(n_meas)
        s = 0.0
        out = np.empty(n_meas)
        for i in range(n_meas):
            s = ema_step(s, float(x[i]), a)
            out[i] = s
        meas = float(np.var(out[burn:]))
        pred = a / (2.0 - a)
        d.var_rows.append((a, meas, pred))
        rows.append(
            [f"{a:g}", f"{meas:.5f}", f"{pred:.5f}", f"{meas / pred:.3f}"]
        )
    R.table(["alpha", "measured var", "predicted `a/(2-a)`", "ratio"], rows)
    R.md()

    # ── 2.6 the time constant ────────────────────────────────────────
    R.md("### 2.6 Memory — samples to the `1/e` point (NEW)")
    R.md()
    R.md(
        "From a cold start against a constant input of 1, the state is "
        "`1-(1-alpha)^n`, so it clears `1 - 1/e` once "
        "`n >= -1/ln(1-alpha)`. The continuous constant is what a caller "
        "budgets a warm-up from; the sample the state *actually* crosses "
        "on is its **ceiling**, and that is an exact discrete claim "
        "rather than a tolerance. Like §2.5 this is a run-length "
        "property with no C section."
    )
    R.md()
    R.md(
        "This section was first written scoring the crossing against the "
        "continuous constant within 2%, and that limit FAILED at "
        "alpha 0.5 — measured 2 against 1.44, a 39% miss. The law was "
        "not wrong; the claim was, because a crossing is an integer and "
        "at alpha 0.5 the quantisation IS the quantity. Recorded rather "
        "than quietly re-toleranced: the fix was a stronger claim, not a "
        "looser one."
    )
    R.md()
    rows = []
    for a in (1e-3, 0.01, 0.05, 0.5):
        target = 1.0 - 1.0 / math.e
        s = 0.0
        n_hit = -1
        for i in range(1, int(50.0 / a) + 10):
            s = ema_step(s, 1.0, a)
            if s >= target:
                n_hit = i
                break
        pred = -1.0 / math.log1p(-a)
        d.tau_rows.append((a, n_hit, pred))
        rows.append(
            [
                f"{a:g}",
                str(n_hit),
                f"{pred:.2f}",
                str(math.ceil(pred)),
                "yes" if n_hit == math.ceil(pred) else "NO",
            ]
        )
    R.table(
        [
            "alpha",
            "measured n",
            "`-1/ln(1-alpha)`",
            "its ceiling",
            "exact?",
        ],
        rows,
    )
    R.md()
    return d


# ── 3. review ────────────────────────────────────────────────────────
def review(d: Data) -> None:
    R.md("## 3. Review")
    R.md()
    R.md("Findings, with verdicts. Limits are section 4.")
    R.md()
    # F1 and F2 are read off the tree, not asserted — see SITES above.
    who = adopters()
    holdouts = sorted(n for n, ok in who.items() if not ok)
    if holdouts:
        R.find(
            "F1",
            "CONFIRMED",
            "The primitive exists and "
            + (
                "NOTHING USES IT"
                if len(holdouts) == len(who)
                else f"{len(holdouts)} of {len(who)} call sites still "
                "carry their own copy"
            )
            + ": "
            + ", ".join(f"`{n}`" for n in holdouts)
            + ". Until they are migrated this report certifies a "
            "function, not the library's behaviour — the properties "
            "below are true of `ema_step` and say nothing about those "
            "copies. Migration is deliberately separate work so each "
            "site moves against a known contract rather than an "
            "assumption, and two of the three change numerically when "
            "it happens (acc_trace changes form; agc gains an exact "
            "d=1 pole).",
        )
    else:
        R.find(
            "F1",
            "FIXED",
            "Every historical call site now calls the shared primitive: "
            + ", ".join(f"`{n}`" for n in sorted(who))
            + ". So the properties below are statements about the "
            "library's behaviour and not only about `ema_step` — which "
            "is what this finding existed to deny until it was true. "
            "(`det_ema_alpha` sizes the recursion and never runs it, so "
            "there was nothing there to migrate.) Verdict READ from "
            "those files rather than asserted here, so it cannot "
            "outlive the state it describes.",
        )

    worst = int(d.decim_ulps_direct.max())
    if not agc_pole_is_compounded():
        R.find(
            "F2",
            "CONFIRMED",
            f"`agc_steps` forms its detector pole as `1 - a1^d` by "
            f"repeated multiplication, which at d == 1 is "
            f"`1-(1-alpha)` — the cancelling form measured in §2.3 at "
            f"up to {worst} ulps off across the coefficients tried. So "
            "`decim = 1` on the AGC is not bit-for-bit the undecimated "
            "recursion, which is the property that would let its "
            "decimated and per-sample paths be compared at all. "
            "`ema_alpha_decim` is the fix and is not yet adopted "
            "there; see F1.",
        )
    else:
        R.find(
            "F2",
            "FIXED",
            "`agc_steps` forms its detector pole with "
            "`ema_alpha_decim`, so `decim = 1` is now bit-for-bit the "
            "undecimated recursion. It previously used a repeated "
            f"multiply of `(1 - alpha)`, off by up to {worst} ulps at "
            "d == 1 across the coefficients §2.3 sweeps. Note what "
            "this did NOT buy: `agc_steps(decim=1)` and `agc_step` "
            "still differ, because the two apply GAIN differently (a "
            "first-order-hold ramp across the chunk against a "
            "per-period refresh) — the pole was never that gap's "
            "cause. Verdict READ from `agc_core.c`.",
        )
    R.find(
        "F3",
        "BY DESIGN",
        "`ema_step` is not total in `x`: a non-finite observation "
        "poisons the state permanently, and there is no guard. That is "
        "the same decision `saturate` exists to serve — an EMA "
        "remembers, so its input is the boundary where an untrusted "
        "value first becomes persistent state, and one guard there makes "
        "the whole downstream chain total where a clamp at each stage is "
        "several chances to miss one. The caller places it because only "
        "the caller knows which end is safe. The AGC's history is the "
        "argument: one non-finite sample destroyed its loop permanently, "
        "and the fix was a single `saturate` at the detector's input, "
        "not a defensive recursion.",
    )
    R.find(
        "F4",
        "BY DESIGN",
        "A coefficient above 1 saturates to pass-through rather than "
        "applying the bare recursion, which would fly past the "
        "observation and oscillate outward. It is a caller error either "
        "way; saturating makes the worst case 'no averaging', which is "
        "wrong but stable, instead of a diverging estimator.",
    )
    R.find(
        "F5",
        "BY DESIGN",
        "Two expectations about the two-product form did not survive "
        "measurement, and are recorded because the reasoning that "
        "produced them is tempting: it does NOT drift off its fixed "
        "point (both forms return the state exactly when handed their "
        "own value), and it is exact at alpha = 0. So §2.2's freeze and "
        "fixed-point rows pin a floor BOTH forms meet — they are not the "
        "argument for the shipped one. The argument is §2.1's accuracy "
        "margin and the `alpha = 1` row.",
    )
    R.find(
        "F6",
        "C-ONLY",
        "Every C caller inlines the header definition, while this report "
        "exercises the single out-of-line copy emitted by "
        "`native/src/util/ema_step.c`. Those are the same source by "
        "construction — the C99 `extern inline` idiom, where the "
        "translation unit declares rather than redefines — so they "
        "cannot drift. Worth naming because the sibling `square_clip.c` "
        "does NOT follow it: it carries a hand-copied second body, so "
        "the function C inlines and the function Python calls are two "
        "pieces of source kept in agreement by hand. That is the "
        "duplication this primitive exists to end, and it is still "
        "present next door.",
    )
    R.md()


# ── 4. limits ────────────────────────────────────────────────────────
def limits(d: Data) -> None:
    R.md("## 4. Limits")
    R.md()
    R.md(
        "Claims a caller may rely on. A failure here is a regression, "
        "not a new finding."
    )
    R.md()

    R.limit(
        d.passthru_bad_shipped == 0,
        f"alpha = 1 returns the observation bit-exactly "
        f"({d.passthru_total}/{d.passthru_total} pairs)",
    )
    R.limit(
        d.passthru_bad_naive > 0,
        f"the guard is load-bearing: the unguarded recursion misses "
        f"{d.passthru_bad_naive} of those pairs",
    )
    R.limit(
        d.freeze_bad == 0, "alpha = 0 leaves the state bit-exactly unchanged"
    )
    R.limit(d.fixed_bad == 0, "the fixed point does not move, at any alpha")
    R.limit(d.overshoot_bad == 0, "the step never passes the observation")
    R.limit(d.sat_ok, "alpha > 1 saturates to pass-through, never overshoots")
    R.limit(
        bool(np.all(d.decim_ulps_shipped == 0)),
        "ema_alpha_decim(alpha, 1) == alpha bit-exactly, at every "
        "coefficient tried",
    )
    R.limit(
        float(d.decim_ulps_direct.max()) > 0,
        f"the cancellation is real: the direct form is up to "
        f"{int(d.decim_ulps_direct.max())} ulps off at d = 1",
    )
    worst_c = max(r[2] for r in d.compound_rows)
    R.limit(
        worst_c < 1e-15,
        f"d steps of alpha equal one step of the compounded coefficient "
        f"(worst residual {worst_c:.1e})",
    )
    R.limit(d.range_ok, "the compounded coefficient stays inside [0, 1]")
    R.limit(d.monotone_ok, "the compounded coefficient is non-decreasing in d")
    R.limit(
        ema_alpha_decim(0.0, 8) == 0.0 and ema_alpha_decim(1.0, 8) == 1.0,
        "the compounded coefficient answers both degenerate alphas "
        "directly (0 stays frozen, 1 stays pass-through)",
    )
    small = [
        (a, i, t)
        for a, i, t in zip(d.acc_alpha, d.acc_inc, d.acc_two)
        if a <= 1e-3
    ]
    R.limit(
        all(i <= t for _, i, t in small),
        "the shipped form is at least as accurate as the two-product "
        "form at every long-average coefficient (alpha <= 1e-3)",
    )
    R.limit(
        all(abs(meas / pred - 1.0) < 0.05 for _, meas, pred in d.var_rows),
        "noise reduction matches a/(2-a) within 5% over the measured "
        "coefficients",
    )
    R.limit(
        all(n == math.ceil(p) for _, n, p in d.tau_rows),
        "the state crosses 1 - 1/e on exactly sample "
        "ceil(-1/ln(1-alpha)) — the discrete law, not a tolerance",
    )
    R.md()


# ── plots ────────────────────────────────────────────────────────────
def plots(d: Data) -> None:
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    fig, ax = plt.subplots(figsize=(7, 4))
    ax.loglog(d.acc_alpha, d.acc_inc, "o-", label="shipped (incremental)")
    ax.loglog(d.acc_alpha, d.acc_two, "s--", label="two-product")
    ax.set_xlabel("alpha")
    ax.set_ylabel("error vs a 40-digit reference")
    ax.set_title("EMA accuracy: the margin grows as the average lengthens")
    ax.grid(True, which="both", alpha=0.3)
    ax.legend()
    fig.tight_layout()
    fig.savefig(HERE / "accuracy.png", dpi=110)
    plt.close(fig)

    fig, ax = plt.subplots(figsize=(7, 4))
    ax.loglog(
        d.decim_alpha,
        np.maximum(d.decim_ulps_direct, 0.5),
        "o-",
        label="direct 1-(1-alpha)",
    )
    ax.loglog(
        d.decim_alpha,
        np.maximum(d.decim_ulps_shipped, 0.5),
        "s--",
        label="shipped (expm1/log1p)",
    )
    ax.set_xlabel("alpha")
    ax.set_ylabel("ulps off alpha at d = 1  (0 plotted at 0.5)")
    ax.set_title("Compounded coefficient at d = 1: exact, or not")
    ax.grid(True, which="both", alpha=0.3)
    ax.legend()
    fig.tight_layout()
    fig.savefig(HERE / "decim_d1.png", dpi=110)
    plt.close(fig)

    _csv(
        DATA / "accuracy.csv",
        [d.acc_alpha, d.acc_inc, d.acc_two],
        "alpha,err_incremental,err_two_product",
    )
    _csv(
        DATA / "decim_d1.csv",
        [d.decim_alpha, d.decim_ulps_direct, d.decim_ulps_shipped],
        "alpha,ulps_direct,ulps_shipped",
    )
    _csv(
        DATA / "noise_reduction.csv",
        [
            np.array([r[0] for r in d.var_rows]),
            np.array([r[1] for r in d.var_rows]),
            np.array([r[2] for r in d.var_rows]),
        ],
        "alpha,var_measured,var_predicted",
    )


def build(write: bool = True) -> Report:
    """Measure everything and render the report.

    `write=False` is the pytest path: every measurement still runs, so
    every limit is genuinely exercised, but nothing is written into the
    repo.
    """
    global R
    R = Report(write=write)
    if write:
        DATA.mkdir(parents=True, exist_ok=True)
    section_object()
    d = characterise()
    review(d)
    limits(d)
    R.executive(
        "EMA",
        [
            "**`ema_step` is not total in `x`.** One non-finite observation "
            "poisons the state permanently and there is no guard (F3) — guard "
            "at the boundary where an input first becomes persistent state, "
            "which is what the AGC now does.",
            "**Use `ema_alpha_decim` whenever you decimate the update.** A "
            "pole scaled linearly with the chunk is not the same filter as "
            "one compounded over it; `decim = 1` is bit-for-bit the "
            "undecimated recursion (§2.3, F2).",
            "**A coefficient above 1 saturates to pass-through** rather than "
            "applying the bare recursion, which would fly past the "
            "observation (F4). That is a deliberate clamp, not an accident of "
            "the arithmetic.",
            "**Every C caller inlines the header definition** while this "
            "report exercises the single out-of-line copy (F6), so the "
            "evidence covers the definition rather than each inlined "
            "instance.",
        ],
    )
    if write:
        plots(d)
    R.summary(
        "\n- Raw sweeps: `data/accuracy.csv`, `data/decim_d1.csv`, "
        "`data/noise_reduction.csv`"
    )
    R.emit(HERE / "results.md")
    return R


if __name__ == "__main__":
    sys.exit(cli(build, HERE))
