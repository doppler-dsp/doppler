"""AGC — certification evidence, measured through the shipped binding.

Run directly to regenerate `results.md`, the plots and the CSVs:

    uv run python src/doppler/agc/tests/validation/agc/validate.py

`--check` re-renders in memory and diffs against the committed bytes;
`make validate` writes, `make validate-check` checks. Every limit this
records is asserted by `src/doppler/agc/tests/test_validation_limits.py`,
which runs this same `build(write=False)`.

The order is the campaign's, not this file's: `native/inc/agc/agc_core.h`
is the SSOT, `native/tests/test_agc_core.c` §1-§24 certifies it in C, and
this measures the same properties through `doppler.agc.AGC` to show the
binding delivers them. Claims that cannot be reached from Python are
reported C-ONLY with the C section that covers them, never skipped.
"""

from __future__ import annotations

import math
import sys
from dataclasses import dataclass, field
from pathlib import Path

import numpy as np

from doppler.agc import AGC
from doppler.telemetry import Telemetry
from doppler.tests._validation_common import Report, cli

HERE = Path(__file__).resolve().parent
DATA = HERE / "data"

R = Report()

# A unit-magnitude direction, so scaling by A gives |x| = A while both
# components are exercised.
DIR = complex(0.6, 0.8)


# ── measurement helpers ──────────────────────────────────────────────
def settle_samples(
    loop_bw: float, alpha: float, amp: float, budget: int = 60_000
) -> int:
    """Samples until the gain error falls to 1/e of its initial value.

    The target is analytic — a constant-magnitude input of amplitude `A`
    against `ref_db = 0` converges to `-20*log10(A)` — so this measures
    against an external truth rather than against the loop's own
    eventual resting place, which would beg the question.
    """
    a = AGC(ref_db=0.0, loop_bw=loop_bw, alpha=alpha)
    target = -20.0 * math.log10(amp)
    err0 = abs(target)
    for n in range(budget):
        a.step(DIR * amp)
        if abs(a.gain_db - target) <= err0 / math.e:
            return n + 1
    return -1


def converged_gain(loop_bw: float, amp: float, n: int = 20_000) -> float:
    """The gain a constant-amplitude input settles on."""
    a = AGC(ref_db=0.0, loop_bw=loop_bw, alpha=0.05)
    x = np.full(n, DIR * amp, dtype=np.complex64)
    a.steps(x)
    return a.gain_db


def survives(bad: complex) -> tuple[bool, bool]:
    """Feed one malformed sample; report (state finite, loop still works).

    Returns a pair rather than a bool so the report can say WHICH half
    failed — before the guard existed, the first was already false.
    """
    a = AGC(ref_db=0.0, loop_bw=0.0025, alpha=0.05)
    a.step(bad)
    finite_after = math.isfinite(a.gain_db)
    y = a.step(complex(1.0, 0.0))
    works = (
        math.isfinite(y.real)
        and math.isfinite(y.imag)
        and math.isfinite(a.gain_db)
    )
    return finite_after, works


def silence_then_signal(gap: int, budget: int = 60_000) -> tuple[bool, int]:
    """Run a silent gap, then measure recovery. (state finite, samples)."""
    a = AGC(ref_db=0.0, loop_bw=0.0025, alpha=0.05)
    for _ in range(4000):
        a.step(DIR)
    a.steps(np.zeros(gap, dtype=np.complex64))
    finite = math.isfinite(a.gain_db) and math.isfinite(a.applied_gain_db)
    for n in range(budget):
        a.step(DIR)
        if abs(a.gain_db) < 1.0:
            return finite, n + 1
    return finite, -1


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

    levels_db: np.ndarray = field(default_factory=lambda: np.array([]))
    tau_fast: np.ndarray = field(default_factory=lambda: np.array([]))
    tau_slow: np.ndarray = field(default_factory=lambda: np.array([]))
    gains: np.ndarray = field(default_factory=lambda: np.array([]))
    bw_rows: list[tuple[float, int, float]] = field(default_factory=list)
    seed_rows: list[tuple[float, float]] = field(default_factory=list)
    guard_rows: list[tuple[str, bool, bool]] = field(default_factory=list)
    gap_rows: list[tuple[int, bool, int]] = field(default_factory=list)
    recover_trace: np.ndarray = field(default_factory=lambda: np.array([]))
    tlm_n: np.ndarray = field(default_factory=lambda: np.array([]))
    tlm_level: np.ndarray = field(default_factory=lambda: np.array([]))
    tlm_gain: np.ndarray = field(default_factory=lambda: np.array([]))
    period_rows: list[tuple[int, float]] = field(default_factory=list)
    decim_rows: list[tuple[float, str, float]] = field(default_factory=list)


# ── 1. the object ────────────────────────────────────────────────────
def section_object() -> None:
    # Title and provenance belong to the executive summary, which
    # Report.executive renders ahead of everything here.
    R.md("## 1. The object")
    R.md()
    R.md(
        "A log-domain feedback AGC: it gains the stream, measures the "
        "output power with an EMA detector, and integrates the dB error "
        "onto the gain. It is what makes a timing detector's "
        "construct-time slope mean what it says, and it is first in a "
        "receiver's chain, so a level error it leaves behind is not "
        "correctable further along."
    )
    R.md()
    R.md("Design and API, not restated here:")
    R.md()
    R.md("- `native/inc/agc/agc_core.h` — the SSOT for every claim")
    R.md("- `native/tests/test_agc_core.c` §1-§24 — the C certification")
    R.md(
        "- [AGC design](../../../../../../docs/design/agc.md) — why the "
        "filter is in dB and the detector is not, and what the loop "
        "cannot know"
    )
    R.md("- `doppler.agc.AGC` — the Python face measured below")
    R.md()
    R.md(
        "**What Python cannot reach.** The detector state `p_avg` is not "
        "exposed, so the loop's *input* is only observable through the "
        "`level_db` telemetry probe (§2.7). The totality of "
        "`agc_exp10_` / `agc_log10_` and the `saturate` NaN direction are "
        "C-only by construction — they are internal — and are certified "
        "in C §15, §16 and §18."
    )
    R.md()


# ── 2. characterisation ──────────────────────────────────────────────
def characterise() -> Data:
    d = Data()
    R.md("## 2. Characterisation")
    R.md()
    R.md("Measured behaviour. No verdicts here — those are section 3.")
    R.md()

    # 2.1 ─────────────────────────────────────────────────────────────
    R.md("### 2.1 Settling against input level (C §20)")
    R.md()
    R.md(
        "The header's `1/(4*loop_bw)` time constant belongs to the loop "
        "FILTER. The object is slower on a quiet input, because the "
        "detector is inside the loop and measures in power, so a quiet "
        "level's dB reading approaches from the wrong side of a concave "
        "log. 1/e settling, swept across 80 dB of input level:"
    )
    R.md()
    R.md(
        "An input already AT the reference is excluded, and the exclusion "
        'is not a convenience: its initial error is zero, so "time to '
        'fall to 1/e of the initial error" is a question with no answer '
        "— any threshold is already met, or met only on an exact float "
        "equality the loop's dither never satisfies. The on-target case "
        "is measured as a transient bound instead, in §2.3."
    )
    R.md()
    amps = np.array([100.0, 31.6, 10.0, 3.16, 0.316, 0.1, 0.0316, 0.01])
    d.levels_db = 20.0 * np.log10(amps)
    d.tau_fast = np.array(
        [settle_samples(0.005, 0.05, float(a)) for a in amps], dtype=float
    )
    d.tau_slow = np.array(
        [settle_samples(0.005, 0.01, float(a)) for a in amps], dtype=float
    )
    R.table(
        ["input (dB)", "tau, alpha 0.05", "tau, alpha 0.01"],
        [
            [f"{lv:+.0f}", f"{f:.0f}", f"{s:.0f}"]
            for lv, f, s in zip(d.levels_db, d.tau_fast, d.tau_slow)
        ],
    )
    R.md()
    R.md(
        f"Predicted from the filter alone: {1.0 / (4 * 0.005):.0f} samples. "
        f"The loud end sits below it and the quiet end above, and the "
        f"spread widens as the detector is slowed: "
        f"{d.tau_fast.max() / d.tau_fast.min():.2f}x at alpha 0.05 against "
        f"{d.tau_slow.max() / d.tau_slow.min():.2f}x at alpha 0.01."
    )
    R.md()
    R.md("![Settling against input level](settling_vs_level.png)")
    R.md()

    # 2.2 ─────────────────────────────────────────────────────────────
    R.md("### 2.2 Settling against loop bandwidth (C §20)")
    R.md()
    R.md(
        "The same measurement against bandwidth, at one level so the "
        "detector's contribution is common to every row and cancels."
    )
    R.md()
    for bw in (0.01, 0.005, 0.0025, 0.00125):
        t = settle_samples(bw, 0.05, 10.0)
        d.bw_rows.append((bw, t, t * 4.0 * bw))
    R.table(
        ["loop_bw", "predicted 1/(4*bw)", "measured tau", "tau / predicted"],
        [
            [f"{bw:g}", f"{1.0 / (4 * bw):.0f}", f"{t:d}", f"{r:.2f}"]
            for bw, t, r in d.bw_rows
        ],
    )
    R.md()

    # 2.3 ─────────────────────────────────────────────────────────────
    R.md("### 2.3 The seed produces no transient (C §21)")
    R.md()
    R.md(
        "`create()` seeds the detector with the REFERENCE power, so a "
        "stream that arrives already on target moves the loop nowhere. "
        "Measured as the largest excursion over 4000 on-target samples:"
    )
    R.md()
    for ref in (-12.0, -6.0, 0.0, 6.0, 12.0):
        amp = 10.0 ** (ref / 20.0)
        a = AGC(ref_db=ref, loop_bw=0.0025, alpha=0.05)
        worst = 0.0
        for _ in range(4000):
            a.step(DIR * amp)
            worst = max(worst, abs(a.gain_db))
        d.seed_rows.append((ref, worst))
    R.table(
        ["ref_db", "worst |gain_db| over 4000 on-target samples"],
        [[f"{r:+.0f}", f"{w:.4f}"] for r, w in d.seed_rows],
    )
    R.md()

    # 2.4 ─────────────────────────────────────────────────────────────
    R.md("### 2.4 One malformed sample (C §13)")
    R.md()
    R.md(
        "The detector's input is the object's one safety boundary. Before "
        "it was guarded, a single non-finite sample drove the detector "
        "non-finite permanently — a following normal sample did not "
        "recover it, and the object was dead for the rest of the run."
    )
    R.md()
    inf, nan = float("inf"), float("nan")
    for name, bad in (
        ("+Inf real", complex(inf, 0.0)),
        ("+Inf imag", complex(0.0, inf)),
        ("NaN real", complex(nan, 0.0)),
        ("NaN imag", complex(1.0, nan)),
    ):
        fin, works = survives(bad)
        d.guard_rows.append((name, fin, works))
    R.table(
        ["sample", "state finite after", "loop works after"],
        [
            [n, "yes" if f else "NO", "yes" if w else "NO"]
            for n, f, w in d.guard_rows
        ],
    )
    R.md()

    # 2.5 ─────────────────────────────────────────────────────────────
    R.md("### 2.5 A silent gap, and what a returning signal costs (C §14)")
    R.md()
    R.md(
        "Silence drives the detector to its floor and the filter "
        "integrates a constant error. The wind-up is self-limiting: the "
        "gain climbs until the applied gain overflows, the guard reads "
        "the resulting non-finite power as maximally loud, and the loop "
        "is driven back. Recovery therefore does not grow with gap "
        "length."
    )
    R.md()
    for gap in (100, 1000, 3000, 10_000):
        fin, rec = silence_then_signal(gap)
        d.gap_rows.append((gap, fin, rec))
    R.table(
        ["silent gap", "state finite", "samples to recover"],
        [
            [f"{g:,}", "yes" if f else "NO", f"{r:,}" if r > 0 else "never"]
            for g, f, r in d.gap_rows
        ],
    )
    R.md()
    a = AGC(ref_db=0.0, loop_bw=0.0025, alpha=0.05)
    for _ in range(2000):
        a.step(DIR)
    trace = []
    a.steps(np.zeros(2000, dtype=np.complex64))
    for _ in range(6000):
        a.step(DIR)
        trace.append(a.gain_db)
    d.recover_trace = np.asarray(trace)
    R.md("![Recovery after a silent gap](guard_recovery.png)")
    R.md()

    # 2.6 ─────────────────────────────────────────────────────────────
    R.md("### 2.6 The converged gain is the level, inverted (C §2, §4)")
    R.md()
    R.md(
        "Across 80 dB of input the loop settles on exactly the gain that "
        "puts the output at the reference. This is the property the whole "
        "object exists to provide, and it is level-INdependent even "
        "though §2.1's settling time is not."
    )
    R.md()
    d.gains = np.array(
        [converged_gain(0.005, float(a)) for a in amps], dtype=float
    )
    R.table(
        ["input (dB)", "gain (dB)", "input + gain (dB)"],
        [
            [f"{lv:+.0f}", f"{g:+.3f}", f"{lv + g:+.3f}"]
            for lv, g in zip(d.levels_db, d.gains)
        ],
    )
    R.md()

    # 2.7 ─────────────────────────────────────────────────────────────
    R.md("### 2.7 `level_db` is the zero-referenced one (C §12)")
    R.md()
    R.md(
        "The detector's state is not a Python property, so telemetry is "
        "the only way to watch the loop's INPUT. That is also the reason "
        "to prefer it: `level_db` closes on `ref_db` whatever the signal's "
        "true level, while `gain_db` settles to an offset that cannot be "
        "judged without knowing that level."
    )
    R.md()
    tlm = Telemetry(1 << 14)
    a = AGC(ref_db=0.0, loop_bw=0.0025, alpha=0.05)
    a.set_telemetry(tlm, "agc")
    a.steps(np.full(8192, DIR * 0.05, dtype=np.complex64))
    recs = tlm.read()
    lid, gid = tlm.probe_id("agc.level_db"), tlm.probe_id("agc.gain_db")
    lvl, gn = recs[recs["probe"] == lid], recs[recs["probe"] == gid]
    d.tlm_n, d.tlm_level = lvl["n"].astype(float), lvl["value"].astype(float)
    d.tlm_gain = gn["value"].astype(float)
    R.table(
        ["probe", "first", "last", "closes on"],
        [
            [
                "`agc.level_db`",
                f"{d.tlm_level[0]:+.2f} dB",
                f"{d.tlm_level[-1]:+.2f} dB",
                "the reference (0 dB)",
            ],
            [
                "`agc.gain_db`",
                f"{d.tlm_gain[0]:+.2f} dB",
                f"{d.tlm_gain[-1]:+.2f} dB",
                "an input-dependent offset",
            ],
        ],
    )
    R.md()
    R.md("![The zero-referenced probe](telemetry_zero_ref.png)")
    R.md()

    # 2.8 ─────────────────────────────────────────────────────────────
    R.md("### 2.8 `gain_update_period` amortises without moving the answer")
    R.md()
    R.md(
        "P > 1 refreshes the loop-filter command once per P samples with "
        "the step scaled by P, so the integrator advances at the same "
        "per-sample rate (C §19)."
    )
    R.md()
    for p in (1, 8, 32):
        a = AGC(ref_db=0.0, loop_bw=0.0025, alpha=0.05)
        a.gain_update_period = p
        for _ in range(8000):
            a.step(DIR * 10.0)
        d.period_rows.append((p, a.gain_db))
    R.table(
        ["gain_update_period", "converged gain (dB)"],
        [[f"{p}", f"{g:+.4f}"] for p, g in d.period_rows],
    )
    R.md()

    R.md("### 2.9 `decim` against the transient, and the rule that bounds it")
    R.md()
    R.md(
        "`decim` preserves the steady state (C §23) — the open question was "
        "always the TRANSIENT. Both per-chunk coefficients are compounded "
        "(`1-(1-a)^d`, not `d*a`), so what remains is the first-order hold: "
        "a longer chunk ramps the applied gain over a longer span, and the "
        "detector sees a different signal. That is bounded by ONE number, "
        "`4*decim*loop_bw` — how far the loop moves within a chunk."
    )
    R.md()
    R.md(
        "Measured here rather than asserted, so this section cannot outlive "
        "the code: the worst spread between `decim` 8/16/32 at a common "
        "sample index, sampled across the whole transient. Both step "
        "directions, because the loop is not symmetric — the detector is "
        "inside it and measures power, so a RISING gain (weak input) costs "
        "several times a falling one and is what sets the rule."
    )
    R.md()
    for group, bw in ((0.032, 2.5e-4), (0.32, 2.5e-3)):
        for label, amp in (("falling", 10.0), ("rising", 0.1)):
            worst = 0.0
            for n in range(64, 4097, 64):
                gains = []
                for dec in (8, 16, 32):
                    a = AGC(ref_db=0.0, loop_bw=bw, alpha=0.05)
                    a.decim = dec
                    a.steps(np.full(n, DIR * amp, dtype=np.complex64))
                    gains.append(a.gain_db)
                worst = max(worst, max(gains) - min(gains))
            d.decim_rows.append((group, label, worst))
    R.table(
        ["4*decim*loop_bw", "gain direction", "worst spread (dB)"],
        [[f"{g}", lab, f"{w:.3f}"] for g, lab, w in d.decim_rows],
    )
    R.md()
    R.md(
        "**Keep `4*decim*loop_bw <= 0.05` and `decim` costs under 0.3 dB of "
        "transient** — the rule the header states and `test_agc_core.c` §23 "
        "asserts. The 0.32 rows are six times the rule and are what the "
        "2.53 dB anomaly was measured at before the loop gain was "
        "compounded (doppler#699)."
    )
    R.md()
    return d


def _inside_rule(d: Data) -> float:
    """Worst decim spread at the settings the rule permits, either way.

    This is the caller-facing PROMISE, and deliberately not the thing F3's
    verdict keys on: the rising direction dominates it, and rising barely
    moves when the loop gain is un-compounded (0.197 -> 0.232 measured),
    because it is set by the first-order hold and the detector's power-law
    asymmetry rather than by the coefficient.
    """
    return max(w for g, _, w in d.decim_rows if g <= 0.05)


def _inside_rule_falling(d: Data) -> float:
    """The same, restricted to the falling-gain direction.

    THIS is what F3's verdict reads, because it is the half that actually
    responds to the fix: 0.059 dB compounded against 0.146 un-compounded,
    a 2.5x separation, verified by reverting the compounding and watching
    this flip. A verdict keyed to a number that cannot move is a verdict
    that cannot be wrong, which is the same as not checking.
    """
    return max(
        w for g, lab, w in d.decim_rows if g <= 0.05 and lab == "falling"
    )


# ── 3. review ────────────────────────────────────────────────────────
def review(d: Data) -> None:
    R.md("## 3. Review")
    R.md()
    R.md("Findings, with verdicts. Limits are section 4.")
    R.md()

    R.find(
        "F1",
        "BY DESIGN",
        "Settling is level-dependent: §2.1 measures "
        f"{d.tau_slow.max() / d.tau_slow.min():.1f}x between a +40 dB and "
        "a -40 dB input at alpha 0.01. This is inherent, not a defect — "
        "the detector measures in POWER on purpose, because an EMA in dB "
        "is a geometric mean and would silently redefine what ref_db "
        "means per waveform. The header claimed the opposite until this "
        "campaign corrected it; a caller sizing a warm-up budget from "
        "1/(4*loop_bw) alone is optimistic by up to 3x on a weak signal.",
    )
    R.find(
        "F2",
        "FIXED",
        "Two input sequences used to destroy the loop permanently: one "
        "non-finite sample, and ~800 samples of silence. Both are closed "
        "by a single saturate() at the detector's input, with the "
        "primitives made total behind it. §2.4 and §2.5 measure the "
        "repaired behaviour through the binding; C §13-§17 certify it.",
    )
    R.find(
        "F3",
        "FIXED" if _inside_rule_falling(d) < 0.1 else "CONFIRMED",
        "decim preserved the steady state but NOT the transient — 2.53 dB "
        "apart at a common sample index, larger decim converging faster, "
        "because the loop-filter gain was scaled LINEARLY by the chunk "
        "length while the detector's pole was compounded. `d*k1` is the "
        "rectangular approximation to `1-(1-k1)^d` and is always the "
        "smaller, so more decimation meant a faster loop. Compounding it "
        f"(doppler#699) cut it to {_inside_rule_falling(d):.3f} dB falling "
        f"and {_inside_rule(d):.3f} dB rising inside the rule, and 3.3x at "
        "the old settings. What remains is the "
        "first-order hold, which is not a coefficient and cannot be "
        "compounded away — so it is BOUNDED instead: "
        "`4*decim*loop_bw <= 0.05` costs under 0.3 dB, §2.9 measures it in "
        "both step directions, and C §23 now asserts it rather than only "
        "recording the divergence. Verdict READ from §2.9's measurement.",
    )
    R.find(
        "F4",
        "GAP",
        "An over-long telemetry prefix is truncated rather than rejected, "
        "so an object's two probes can collapse onto one id while the "
        "attach reports success — both series then interleave under one "
        "name. Not AGC-specific: every set_telemetry builds names the "
        "same way, and composing receivers nest prefixes. Filed as "
        "doppler#676; C §24 tests the table-full reject instead.",
    )
    R.find(
        "F5",
        "C-ONLY",
        "The detector state p_avg has no Python property, so the loop's "
        "input is reachable only through the level_db probe (§2.7). The "
        "totality of agc_exp10_/agc_log10_ and saturate's NaN direction "
        "are internal and certified in C §15, §16 and §18.",
    )
    R.find(
        "F6",
        "GAP",
        "The object has no notion of signal PRESENCE, so left on a noise "
        "floor it amplifies the noise to the reference and the next burst "
        "arrives that many dB hot — measured 59.9 dB on a -60 dB floor. "
        "Level alone cannot separate a weak signal from noise, so this "
        "needs presence information from outside. Open design, stated "
        "with its measurements in docs/design/agc.md section 5, and "
        "tracked as gh-750.",
    )
    R.md()


# ── 4. limits ────────────────────────────────────────────────────────
def limits(d: Data) -> None:
    R.md("## 4. Limits")
    R.md()
    R.md(
        "Claims a caller may rely on. A failure here is a regression, not "
        "a new finding — every one is asserted by "
        "`src/doppler/agc/tests/test_validation_limits.py`, which runs "
        "this same `build()`."
    )
    R.md()

    R.limit(
        bool(np.all(np.abs(d.levels_db + d.gains) < 0.5)),
        "The converged gain cancels the input level to within 0.5 dB "
        "across 80 dB of input.",
    )
    R.limit(
        _inside_rule_falling(d) < 0.1,
        "Inside the rule, a FALLING gain holds decim to under 0.1 dB — "
        f"measured {_inside_rule_falling(d):.3f} dB. This is the half that "
        "responds to the loop gain being compounded (0.146 dB if it is "
        "not), so it is the regression detector; the row below is the "
        "promise.",
    )
    R.limit(
        _inside_rule(d) < 0.3,
        "Inside `4*decim*loop_bw <= 0.05`, changing decim costs under "
        f"0.3 dB of transient — measured {_inside_rule(d):.3f} dB in the "
        "worse (rising-gain) direction. The rule a caller picks decim by.",
    )
    R.limit(
        bool(np.all(d.tau_fast > 0) and np.all(d.tau_slow > 0)),
        "Every settling measurement completed within budget, so no "
        "did-not-converge sentinel reaches a ratio or a plot.",
    )
    R.limit(
        all(t > 0 for _, t, _ in d.bw_rows),
        "The loop settles within budget at every bandwidth measured.",
    )
    ratios = [r for _, _, r in d.bw_rows]
    R.limit(
        max(ratios) / min(ratios) < 1.6,
        "Settling tracks 1/(4*loop_bw) across a decade of bandwidth: the "
        "tau/predicted ratio varies by less than 1.6x.",
    )
    R.limit(
        float(d.tau_fast[-1]) > float(d.tau_fast[0]),
        "A quiet input settles no faster than a loud one — the detector's "
        "asymmetry is present and has the documented sign.",
    )
    R.limit(
        max(w for _, w in d.seed_rows) < 0.5,
        "An on-target stream moves the gain by less than 0.5 dB: the seed "
        "is the reference power at every reference.",
    )
    R.limit(
        all(f for _, f, _ in d.guard_rows),
        "One malformed input sample leaves the state finite.",
    )
    R.limit(
        all(w for _, _, w in d.guard_rows),
        "The loop still converges after a malformed sample.",
    )
    R.limit(
        all(f for _, f, _ in d.gap_rows),
        "A silent gap leaves the state finite, at every gap length.",
    )
    R.limit(
        all(r > 0 for _, _, r in d.gap_rows),
        "A returning signal recovers after a silent gap, at every gap length.",
    )
    recs = [r for _, _, r in d.gap_rows if r > 0]
    R.limit(
        bool(recs) and max(recs) < 60_000,
        "Recovery is bounded and does not grow without limit with the gap.",
    )
    gains = [g for _, g in d.period_rows]
    R.limit(
        max(gains) - min(gains) < 0.5,
        "gain_update_period does not move the converged gain by more than "
        "0.5 dB across 1, 8 and 32.",
    )
    R.limit(
        abs(float(d.tlm_level[-1])) < 0.5,
        "The level_db probe closes on the reference within 0.5 dB.",
    )
    R.limit(
        abs(float(d.tlm_level[-1])) < abs(float(d.tlm_level[0])),
        "level_db is zero-referenced: it ends nearer the reference than "
        "it started, which a probe wired to the gain would not.",
    )
    R.limit(
        abs(float(d.tlm_gain[-1])) > 1.0,
        "gain_db meanwhile settles to a non-zero offset, so the two "
        "probes are genuinely different quantities.",
    )
    a = AGC(ref_db=0.0, loop_bw=0.0025, alpha=0.05)
    for _ in range(200):
        a.step(DIR * 4.0)
    blob = a.get_state()
    b = AGC(ref_db=0.0, loop_bw=0.0025, alpha=0.05)
    b.set_state(blob)
    R.limit(
        b.gain_db == a.gain_db and b.get_state() == blob,
        "State round-trips bit-exactly into a fresh instance.",
    )
    R.md()


# ── plots ────────────────────────────────────────────────────────────
def plots(d: Data) -> None:
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    fig, ax = plt.subplots(figsize=(7.2, 3.6))
    ax.plot(d.levels_db, d.tau_fast, "o-", lw=1.4, label="alpha 0.05")
    ax.plot(d.levels_db, d.tau_slow, "s-", lw=1.4, label="alpha 0.01")
    ax.axhline(
        1.0 / (4 * 0.005),
        ls="--",
        lw=1,
        color="crimson",
        label="filter alone, 1/(4*loop_bw)",
    )
    ax.set_xlabel("input level (dB)")
    ax.set_ylabel("1/e settling (samples)")
    ax.set_title("The object is slower than its filter, on a quiet input")
    ax.grid(alpha=0.3)
    ax.legend()
    fig.tight_layout()
    fig.savefig(HERE / "settling_vs_level.png", dpi=110)
    plt.close(fig)

    fig, ax = plt.subplots(figsize=(7.2, 3.6))
    ax.plot(d.recover_trace, lw=1.2)
    ax.axhline(0.0, ls="--", lw=1, color="crimson", label="correct gain")
    ax.set_xlabel("samples after the signal returns")
    ax.set_ylabel("gain_db")
    ax.set_title("After a 2000-sample silent gap, the loop comes back")
    ax.grid(alpha=0.3)
    ax.legend()
    fig.tight_layout()
    fig.savefig(HERE / "guard_recovery.png", dpi=110)
    plt.close(fig)

    fig, ax = plt.subplots(figsize=(7.2, 3.6))
    ax.plot(d.tlm_n, d.tlm_level, lw=1.3, label="agc.level_db (loop input)")
    ax.plot(d.tlm_n, d.tlm_gain, lw=1.3, label="agc.gain_db (command)")
    ax.axhline(0.0, ls="--", lw=1, color="crimson", label="ref_db")
    ax.set_xlabel("sample index")
    ax.set_ylabel("dB")
    ax.set_title("level_db closes on the reference; gain_db does not")
    ax.grid(alpha=0.3)
    ax.legend()
    fig.tight_layout()
    fig.savefig(HERE / "telemetry_zero_ref.png", dpi=110)
    plt.close(fig)

    _csv(
        DATA / "settling.csv",
        [d.levels_db, d.tau_fast, d.tau_slow],
        "input_db,tau_alpha_0p05,tau_alpha_0p01",
    )
    _csv(
        DATA / "converged_gain.csv",
        [d.levels_db, d.gains],
        "input_db,gain_db",
    )
    _csv(
        DATA / "telemetry.csv",
        [d.tlm_n, d.tlm_level, d.tlm_gain],
        "sample,level_db,gain_db",
    )


def build(write: bool = True) -> Report:
    """Measure, review and assert; emit the report only when asked.

    ``write=False`` is the pytest path: every measurement still runs, so
    every limit is genuinely exercised, but nothing is written into the
    repo. See ``doppler/tests/_validation_common.py``.
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
        "AGC",
        [
            "**Settling is level-dependent and always will be.** The detector "
            "sits inside the loop and measures POWER, so a cold start into a "
            "weak signal costs several times what a strong one does (§2.1). "
            "Size a warm-up from the worst initial error you expect, not from "
            "`1/(4*loop_bw)`.",
            "**Keep `4 * decim * loop_bw <= 0.05`.** Inside that, decimating "
            "the gain update costs under 0.3 dB of transient; the residual is "
            "the first-order hold, which is not a coefficient and cannot be "
            "compounded away (§2.3).",
            "**It has no notion of signal PRESENCE.** Left on a noise floor "
            "it amplifies the noise to the reference, so the next burst "
            "arrives far over-driven. Gate it upstream if your signal is "
            "bursty (F6).",
            "**One non-finite sample no longer destroys it**, and neither "
            "does a long silence: both are closed by a single saturate at the "
            "point where an input first becomes persistent state (F2). That "
            "guard is the object’s only defence against its own integrator.",
        ],
    )
    if write:
        plots(d)
    R.summary(
        "\n- Raw sweeps: `data/settling.csv`, `data/converged_gain.csv`, "
        "`data/telemetry.csv`"
    )
    R.emit(HERE / "results.md")
    return R


if __name__ == "__main__":
    sys.exit(cli(build, HERE))
