"""LockDet — certification evidence, measured through the shipped binding.

Run directly to regenerate `results.md` and the CSVs:

    uv run python src/doppler/detection/tests/validation/lockdet/validate.py

`--check` re-renders in memory and diffs against the committed bytes;
`make validate` writes, `make validate-check` checks. Every limit this
records is asserted by
`src/doppler/detection/tests/test_validation_limits.py`, which runs this
same `build(write=False)`.

The order is the campaign's, not this file's:
`native/inc/lockdet/lockdet_core.h` is the SSOT,
`native/tests/test_lockdet_core.c` certifies it in C, and
`native/validation/lockdet_verify.c` measures the probabilistic contract
by Monte Carlo. This file measures the same properties through
`doppler.detection.LockDet` to show the binding delivers them.
"""

from __future__ import annotations

import re
import sys
from dataclasses import dataclass, field
from pathlib import Path

import numpy as np

from doppler.detection import LockDet, det_q_inv, det_verify_delay
from doppler.source import AWGN
from doppler.tests._repo import repo_root
from doppler.tests._validation_common import Report, cli

HERE = Path(__file__).resolve().parent
DATA = HERE / "data"
ROOT = repo_root(__file__)

R = Report()

# One long noise stream, drawn from the SHIPPED generator rather than a
# private one. This file's whole subject is whether a rate comes out right,
# and a local Box-Muller with a slightly wrong variance would move the
# measured rate while every assertion still passed — which is the failure
# `scripts/check_stimulus_sources.py` exists to prevent.
NOISE_SEED = 2024
NOISE_N = 300_000

HEADER = ROOT / "native/inc/lockdet/lockdet_core.h"
CORE_C = ROOT / "native/src/lockdet/lockdet_core.c"


def _text(p: Path) -> str:
    return p.read_text(encoding="utf-8")


def nan_policy_is_shared() -> bool:
    """Does `lockdet_step` route its NaN policy through `saturate()`?

    Derived rather than asserted, so that re-inlining the policy — the
    thing this object already got wrong once — flips the finding back to a
    GAP without anyone remembering to edit this file.
    """
    body = re.sub(r"/\*.*?\*/", "", _text(HEADER), flags=re.S)
    m = re.search(r"lockdet_step\s*\([^)]*\)\s*\{(.*?)\n  \}", body, re.S)
    return bool(m) and "saturate (" in m.group(1)


def _noise() -> np.ndarray:
    """A unit-variance real Gaussian look stream."""
    return AWGN(NOISE_SEED, 1.0).generate(NOISE_N).real.astype(np.float64)


@dataclass
class Data:
    """Everything §2 measured, handed to §3 and §4."""

    rate_rows: list[tuple[float, int, float, float, float, float]] = field(
        default_factory=list
    )
    rate_worst: float = 0.0
    delay_worst: float = 0.0
    band_transitions: int = -1
    single_transitions: int = -1
    latency_exact: bool = False
    nan_never_declares: bool = False
    nan_drop_look: int = -1
    inf_hit: bool = False
    neg_inf_miss: bool = False
    edge_held: bool = False
    split_matches: bool = False
    state_exact: bool = False
    state_carries_config: bool = False
    clamped: bool = False


def _csv(path: Path, header: str, rows: list[list[float]]) -> None:
    if not R.write:
        return
    DATA.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as fh:
        fh.write(header + "\n")
        for r in rows:
            fh.write(",".join(f"{v:.10g}" for v in r) + "\n")


# ── 1. the object ────────────────────────────────────────────────────
def section_object() -> None:
    R.md("## 1. The object — design and expectations")
    R.md()
    R.md(
        "The decision rule every continuous tracking loop in the library "
        "shares: level hysteresis (a declare/drop threshold pair) plus time "
        "hysteresis (`n_up`/`n_down` consecutive looks), over any scalar "
        "lock metric. It computes no statistic of its own — it decides."
    )
    R.md()
    R.md("Four pages already own parts of this, and none is restated here:")
    R.md()
    R.table(
        ["page", "owns"],
        [
            [
                "[`docs/design/lock-detect.md`]"
                "(../../../../../../docs/design/lock-detect.md)",
                "the sizing chain every Gaussian-statistic detector shares",
            ],
            [
                "[`docs/guide/lock-detection.md`]"
                "(../../../../../../docs/guide/lock-detection.md)",
                "which loop carries a detector, and which config entry "
                "point to call",
            ],
            [
                "[`docs/gallery/lockdet.md`]"
                "(../../../../../../docs/gallery/lockdet.md)",
                "the mechanics, runnable: hysteresis, verify counts, and "
                "the non-finite rule",
            ],
            [
                "`native/inc/lockdet/lockdet_core.h`",
                "the contract itself",
            ],
        ],
    )
    R.md(
        "Evidence sits in three places. `native/tests/test_lockdet_core.c` "
        "asks whether each claim still holds per look; "
        "`native/validation/lockdet_verify.c` measures the probabilistic "
        "contract in C; this report measures the same properties through "
        "the binding, so a caller reading `LockDet` gets the object the "
        "header describes."
    )
    R.md()


# ── 2. characterisation ──────────────────────────────────────────────
def characterise() -> Data:
    d = Data()
    R.md("## 2. Characterisation")
    R.md()
    R.md("Measured behaviour. No verdicts — those are §3.")
    R.md()

    noise = _noise()

    # 2.1 the probabilistic contract --------------------------------
    R.md("### 2.1 The verify counts compound as the header says")
    R.md()
    R.md(
        "The claim a caller sizes against: at per-look false-alarm rate `p`, "
        "the false-declare rate is `p^n_up · (1−p) / (1 − p^n_up)`, whose "
        "reciprocal is `det_verify_delay(p, n_up)` — the mean looks to a "
        "declare. Measured by placing the threshold at `det_q_inv(p)` "
        "sigmas of a real Gaussian stream from the shipped `AWGN` "
        "generator, so the per-look hit probability is exactly `p`."
    )
    R.md()
    R.md(
        "**The detector is reset after each declare**, because the formula "
        "describes the mean looks to a declare from a *fresh* run. Without "
        "that reset the detector spends time locked and the measurement is "
        "of a different process entirely — a first draft of this section "
        "did exactly that and read 0.50x against the closed form at "
        "`p = 0.5`."
    )
    R.md()

    rows = []
    for p in (0.5, 0.3, 0.1):
        thr = det_q_inv(p)
        for n_up in (1, 2, 3):
            det = LockDet(up_thresh=thr, down_thresh=thr, n_up=n_up, n_down=1)
            declares = 0
            for x in noise:
                if det.step(x):
                    declares += 1
                    det.reset()
            rate = declares / NOISE_N
            exact = p**n_up * (1.0 - p) / (1.0 - p**n_up)
            delay = 1.0 / rate if rate else float("inf")
            want_delay = det_verify_delay(p, n_up)
            rows.append([p, n_up, rate, exact, delay, want_delay])
            d.rate_rows.append((p, n_up, rate, exact, delay, want_delay))
            d.rate_worst = max(d.rate_worst, abs(rate / exact - 1.0))
            d.delay_worst = max(d.delay_worst, abs(delay / want_delay - 1.0))

    R.table(
        [
            "p",
            "n_up",
            "measured rate",
            "closed form",
            "ratio",
            "measured delay",
            "det_verify_delay",
        ],
        [
            [
                f"{r[0]:g}",
                f"{r[1]:d}",
                f"{r[2]:.6f}",
                f"{r[3]:.6f}",
                f"{r[2] / r[3]:.3f}",
                f"{r[4]:.1f}",
                f"{r[5]:.1f}",
            ]
            for r in rows
        ],
    )
    R.md(
        f"Worst deviation from the closed form across the nine cells: "
        f"**{d.rate_worst * 100:.1f}%**, and it is largest exactly where "
        f"the trial count is smallest — at `p = 0.1, n_up = 3` a "
        f"{NOISE_N:,}-look stream yields only a few hundred declares. That "
        "is Monte-Carlo noise, not a discrepancy in the law."
    )
    R.md()
    _csv(
        DATA / "verify_rate.csv",
        "p,n_up,measured_rate,closed_form,measured_delay,det_verify_delay",
        [list(r) for r in rows],
    )

    # 2.2 level hysteresis -------------------------------------------
    R.md("### 2.2 Level hysteresis is what stops the flag chattering")
    R.md()
    # Declare on a clean hit, then wobble INSIDE the band. The amplitude
    # matters and a first draft got it wrong: a wobble spanning 1.15-1.55
    # crosses both thresholds, so the hysteretic detector chatters exactly
    # as much as the single-threshold one (446 against 446) and the section
    # demonstrates nothing. The band only buys anything for a metric that
    # crosses the midpoint without leaving the band.
    wobble = np.concatenate(
        [
            np.full(5, 2.0),  # a clean declare for both detectors
            1.35 + 0.10 * np.sin(np.arange(2000) * 0.7),  # 1.25 .. 1.45
        ]
    )
    hyst = LockDet(up_thresh=1.5, down_thresh=1.2, n_up=1, n_down=1)
    single = LockDet(up_thresh=1.35, down_thresh=1.35, n_up=1, n_down=1)
    a = hyst.steps(wobble)
    b = single.steps(wobble)
    d.band_transitions = int(np.count_nonzero(np.diff(a)))
    d.single_transitions = int(np.count_nonzero(np.diff(b)))
    R.table(
        ["detector", "flag transitions over 2005 looks"],
        [
            ["hysteresis band [1.2, 1.5]", str(d.band_transitions)],
            ["single threshold at 1.35", str(d.single_transitions)],
        ],
    )
    R.md(
        f"A metric wobbling across a single threshold flips the flag "
        f"**{d.single_transitions}** times; the same metric against a "
        f"declare/drop pair that brackets it flips it "
        f"**{d.band_transitions}**. That is the entire purpose of the "
        "band, and it costs nothing but the two numbers."
    )
    R.md()

    # 2.3 time hysteresis --------------------------------------------
    R.md("### 2.3 Time hysteresis declares on the n_up-th look, exactly")
    R.md()
    det = LockDet(up_thresh=1.0, down_thresh=1.0, n_up=4, n_down=1)
    out = det.steps(np.full(6, 2.0))
    d.latency_exact = out.tolist() == [0, 0, 0, 1, 1, 1]
    R.md(
        f"Six consecutive hits at `n_up = 4` give `{out.tolist()}` — the "
        "declare lands on the fourth look, not the third and not the "
        "fifth. A single contrary look resets the run, which is what makes "
        "the counts compose the way §2.1 measures."
    )
    R.md()

    # 2.4 the non-finite rule -----------------------------------------
    R.md("### 2.4 A non-finite look is a miss in both states")
    R.md()
    nd = LockDet(up_thresh=1.5, down_thresh=1.2, n_up=2, n_down=3)
    d.nan_never_declares = not any(nd.step(float("nan")) for _ in range(50))
    nd.step(2.0)
    nd.step(2.0)  # declared
    drops = [nd.step(float("nan")) for _ in range(4)]
    d.nan_drop_look = drops.index(0) + 1 if 0 in drops else -1

    inf_det = LockDet(up_thresh=1.5, down_thresh=1.2, n_up=1, n_down=1)
    d.inf_hit = bool(inf_det.step(float("inf")) == 1)
    d.neg_inf_miss = bool(inf_det.step(float("-inf")) == 0)

    edge = LockDet(up_thresh=1.5, down_thresh=1.2, n_up=1, n_down=1)
    edge.step(2.0)
    d.edge_held = bool(edge.step(1.2) == 1)

    R.table(
        ["look", "unlocked", "locked"],
        [
            [
                "NaN",
                "never declares (50 looks)",
                f"drops on look {d.nan_drop_look} of n_down=3",
            ],
            ["+inf", "declares (a hit)", "holds (a hit)"],
            ["−inf", "no progress (a miss)", "drops (a miss)"],
            [
                "x == down_thresh",
                "a miss",
                "**not** a miss — the edge is exclusive",
            ],
        ],
    )
    R.md(
        "An unknown lock is not a lock. Only NaN is unordered — the "
        "infinities are ordinary looks, and the exclusive edge at "
        "`down_thresh` is unchanged by the rule. The policy is not "
        "written here: it is `saturate()`'s `nan_to`, whose documentation "
        "names a lock statistic as the caller that wants the floor."
    )
    R.md()

    # 2.5 block path --------------------------------------------------
    R.md("### 2.5 The block path carries across calls")
    R.md()
    seq = np.array([2.0, 2.0, 1.3, 1.0, 1.0, 2.0, 2.0, 1.0, 1.0, 2.0])
    one = LockDet(up_thresh=1.5, down_thresh=1.2, n_up=2, n_down=2)
    spl = LockDet(up_thresh=1.5, down_thresh=1.2, n_up=2, n_down=2)
    full = one.steps(seq)
    part = np.concatenate([spl.steps(seq[:3]), spl.steps(seq[3:])])
    d.split_matches = bool(np.array_equal(full, part)) and spl.cnt == one.cnt
    R.md(
        f"A ten-look sequence split 3+7 across two calls reproduces the "
        f"single-call result exactly, `cnt` included — `{full.tolist()}`. "
        "The header's *frames of any size with no seam*, which a "
        "single-block test cannot see."
    )
    R.md()

    # 2.6 state --------------------------------------------------------
    R.md("### 2.6 Serialized state")
    R.md()
    src = LockDet(up_thresh=1.5, down_thresh=1.2, n_up=3, n_down=2)
    src.step(2.0)  # mid-declare, cnt = 1 of 3
    blob = src.get_state()
    ref = [src.step(2.0) for _ in range(2)]
    dst = LockDet(up_thresh=1.5, down_thresh=1.2, n_up=3, n_down=2)
    dst.set_state(blob)
    d.state_exact = [dst.step(2.0) for _ in range(2)] == ref

    other = LockDet(up_thresh=99.0, down_thresh=98.0, n_up=7, n_down=9)
    other.set_state(blob)
    d.state_carries_config = (
        other.up_thresh == 1.5 and other.n_up == 3 and other.n_down == 2
    )
    R.md(
        "A mid-run split resumes the in-flight verify run, not just the "
        "flag: restoring at `cnt = 1 of 3` declares on schedule two looks "
        "later. The blob is a whole-struct snapshot, so restoring into a "
        "differently-tuned detector carries the source's thresholds and "
        "counts with it (F4)."
    )
    R.md()

    clamped = LockDet(up_thresh=1.5, down_thresh=1.2, n_up=0, n_down=0)
    d.clamped = clamped.n_up == 1 and clamped.n_down == 1
    R.md(
        "Verify counts of zero clamp to 1 at construction — a count of 0 "
        "is never a meaningful config, and 1 means no time hysteresis on "
        "that side."
    )
    R.md()
    return d


# ── 3. review ────────────────────────────────────────────────────────
def review(d: Data) -> None:
    R.md("## 3. Review — findings, with verdicts")
    R.md()

    R.find(
        "F1",
        "BY DESIGN",
        "**The compounding claim holds through the binding.** Nine "
        f"(p, n_up) cells agree with `p^n_up·(1−p)/(1−p^n_up)` to "
        f"{d.rate_worst * 100:.1f}% worst-case, and the measured latency "
        "tracks `det_verify_delay` (§2.1). This is the contract "
        "`det_verify_count()` sizes against, so a caller picking `n_up` "
        "from a false-alarm budget gets the budget they asked for.",
    )
    R.find(
        "F2",
        "FIXED",
        "**A NaN look used to hold the lock forever.** `NaN < down_thresh` "
        "is false, so while locked a non-finite metric counted as a hit, "
        "reset the drop run every look, and left the flag lit indefinitely "
        "on a dead statistic. It now drops after `n_down` like any other "
        f"miss (§2.4, measured at look {d.nan_drop_look} of 3). An unknown "
        "lock is not a lock.",
    )
    shared = nan_policy_is_shared()
    R.find(
        "F3",
        "BY DESIGN" if shared else "GAP",
        "**The non-finite policy is the shared primitive's, not this "
        "object's.** `lockdet_step` routes its look through "
        "`saturate(x, -inf, +inf, -inf)`, whose documentation names a lock "
        "statistic as the caller that wants NaN at the floor — and which "
        "no lock detector had ever called, so that rationale described a "
        "caller who did not exist. The first fix carried the policy in the "
        "*spelling* of a predicate (`!(x >= t)` versus `x < t`, identical "
        "for every finite x), which is the subtlety that let it be written "
        "the wrong way to begin with. Derived from the source, so "
        "re-inlining it flips this finding.",
    )
    R.find(
        "F4",
        "BY DESIGN",
        "**A restore carries configuration, not just the decision.** The "
        "state blob is a whole-struct POD snapshot, so `set_state` into a "
        "differently-tuned detector silently re-tunes it to the source's "
        "thresholds and verify counts (§2.6). Correct for the documented "
        "use — an identically-built instance — and worth knowing for any "
        "other. The same consequence LoopFilter records as its F8.",
    )
    R.find(
        "F5",
        "BY DESIGN",
        "**An inverted band is not refused.** `down_thresh > up_thresh` is "
        "documented only as advice, and produces the opposite of "
        "hysteresis: a look between the thresholds is a hit while unlocked "
        "AND a miss while locked, so with unit verify counts the flag "
        "chatters every look. Pinned in `test_lockdet_core.c` rather than "
        "rejected, because it is the misconfiguration a caller can reach "
        "and silent chatter is worth being able to recognise.",
    )
    R.find(
        "F6",
        "C-ONLY",
        "**The by-value embedding path has no Python face.** Seven objects "
        "embed a `lockdet_state_t` directly and drive it with "
        "`lockdet_init`/`lockdet_step`; the binding exposes only the heap "
        "instance. That is correct — an embedded detector belongs to its "
        "owner — but it means this report cannot cover the path most of "
        "the library actually takes, and the C test is the only evidence "
        "for it.",
    )


# ── 4. limits ────────────────────────────────────────────────────────
def limits(d: Data) -> None:
    R.md("## 4. Limits — the certified envelope")
    R.md()
    R.md(
        "Claims a caller may rely on. A failure here is a regression, not "
        "a new finding. Every one is asserted by "
        "`src/doppler/detection/tests/test_validation_limits.py`."
    )
    R.md()

    R.limit(
        d.rate_worst < 0.15,
        f"the false-declare rate matches p^n_up(1-p)/(1-p^n_up) across 9 "
        f"cells (worst {d.rate_worst * 100:.1f}%)",
    )
    R.limit(
        d.delay_worst < 0.15,
        f"the declare latency matches det_verify_delay (worst "
        f"{d.delay_worst * 100:.1f}%)",
    )
    for p, n_up, rate, _exact, _, _ in d.rate_rows:
        R.limit(
            rate > 0.0,
            f"p={p:g}, n_up={n_up}: the detector declares at all "
            f"(rate {rate:.6f})",
        )
    R.limit(
        d.band_transitions < d.single_transitions,
        f"a hysteresis band chatters less than a single threshold "
        f"({d.band_transitions} vs {d.single_transitions} transitions)",
    )
    R.limit(d.latency_exact, "the declare lands on the n_up-th look exactly")
    R.limit(
        d.nan_never_declares,
        "an unlocked detector never declares on NaN, however many looks",
    )
    R.limit(
        d.nan_drop_look == 3,
        f"a locked detector drops on the n_down-th NaN look "
        f"(measured {d.nan_drop_look})",
    )
    R.limit(d.inf_hit, "+inf is an ordinary hit, not a special case")
    R.limit(d.neg_inf_miss, "-inf is an ordinary miss, not a special case")
    R.limit(
        d.edge_held,
        "x == down_thresh is still not a miss; the exclusive edge survives "
        "the non-finite rule",
    )
    R.limit(
        d.split_matches,
        "steps() split across calls matches one call, cnt included",
    )
    R.limit(d.state_exact, "a mid-run state split resumes the verify run")
    R.limit(
        d.state_carries_config,
        "a restore carries the source's configuration, not only its flag",
    )
    R.limit(d.clamped, "verify counts of 0 clamp to 1 at construction")


# ── build ────────────────────────────────────────────────────────────
def build(write: bool = True) -> Report:
    global R
    R = Report(write=write)
    R.md("# LockDet — validation report")
    R.md()
    section_object()
    d = characterise()
    review(d)
    limits(d)
    R.executive(
        "LockDet",
        [
            "**The verify counts compound as documented, through the "
            "binding.** Nine (p, n_up) cells match "
            "`p^n_up·(1−p)/(1−p^n_up)` and the latency matches "
            "`det_verify_delay`, so an `n_up` picked from a false-alarm "
            "budget delivers that budget (§2.1, F1).",
            "**A NaN look no longer holds the lock forever.** It used to "
            "count as a hit while locked and keep the lamp lit on a dead "
            "statistic; it now drops after `n_down` like any other miss "
            "(§2.4, F2).",
            "**That policy lives in `saturate()`, not here** — the shared "
            "primitive whose documentation named a lock statistic as its "
            "caller and had none. Re-inlining it flips F3 back to a gap.",
            "**Level hysteresis is worth the second number**: across a "
            "wobbling metric a single threshold flips the flag "
            f"{d.single_transitions} times against {d.band_transitions} "
            "for a declare/drop pair (§2.2).",
            "**A restore re-tunes the target**, because the blob is a "
            "whole-struct snapshot — configuration travels with the "
            "decision (§2.6, F4).",
        ],
    )
    R.summary("\n- Raw sweep: `data/verify_rate.csv`")
    R.emit(HERE / "results.md")
    return R


if __name__ == "__main__":
    sys.exit(cli(build, HERE))
