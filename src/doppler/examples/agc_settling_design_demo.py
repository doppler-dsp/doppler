#!/usr/bin/env python3
"""How long must you wait for an AGC to settle? A design chart.

`agc_core.h` gives the loop FILTER a time constant of ``1/(4*loop_bw)``
samples. The object is not the filter: the power detector sits inside the
loop and measures in *power*, so a quiet input's dB reading crawls up a
concave log and the object settles more slowly than its filter predicts —
by up to 5x at the shipped ``MPSK_RX_AGC_ALPHA`` of 0.01.

So ``1/(4*loop_bw)`` is a **floor**, not an estimate, and anything derived
from it alone (a receiver's warm-up budget, say) is optimistic. This script
measures the multiplier that turns the floor into an answer:

    settling ~ M / (4 * loop_bw)   samples

and shows that **M is not a free function of both parameters**. It depends
on the initial gain error and on one dimensionless group — the detector's
speed relative to the filter's:

    ratio = alpha / (4 * loop_bw)

Which is what makes the chart reusable: measure M once against `ratio`, and
it applies at any loop bandwidth. The right panel is that claim under test —
three (alpha, loop_bw) pairs spanning 20x in each land on one curve.

Reading it, for a design:

1. Pick `loop_bw` from the disturbance you must track, and `alpha` from how
   hard the envelope needs smoothing. Form `ratio = alpha / (4*loop_bw)`.
2. Take the largest gain error you expect to start from — a cold receiver's
   is the whole dynamic range it must cover.
3. Read M off the left panel and multiply by `1/(4*loop_bw)`.

The floor line at M = 1 is the filter alone. A loud input beats it slightly
(the detector's EMA climbs quickly in dB when the power is rising), which is
why the -40 dB curve sits below 1 — settling faster than the filter predicts
is possible, but only in the direction nobody budgets for.

Generates ``docs/assets/agc_settling_design.png``.

Run:  uv run python src/doppler/examples/agc_settling_design_demo.py
"""

from __future__ import annotations

# The include region starts above the imports on purpose: the block this
# page publishes has to run as shown, and `math` is used inside
# settle_multiplier. A region that starts below its own imports produces a
# snippet that reads fine and raises NameError for anyone who copies it.
# --8<-- [start:chart]
import math

import numpy as np

from doppler.agc import AGC

DIR = complex(0.6, 0.8)  # |DIR| == 1, so scaling exercises both components


def settle_multiplier(
    loop_bw: float, alpha: float, gain_err_db: float
) -> float:
    """1/e settling, in units of the filter's own time constant.

    The target is analytic — a constant input needing `gain_err_db` of gain
    converges there — so this measures against an external truth rather than
    against wherever the loop happens to stop, which would beg the question.
    """
    amp = 10.0 ** (-gain_err_db / 20.0)
    agc = AGC(ref_db=0.0, loop_bw=loop_bw, alpha=alpha)
    err0 = abs(gain_err_db)
    budget = int(40.0 / loop_bw)
    for n in range(budget):
        agc.step(DIR * amp)
        if abs(agc.gain_db - gain_err_db) <= err0 / math.e:
            return (n + 1) * 4.0 * loop_bw
    return float("nan")


# The design axes: how far the loop starts from home, and how fast the
# detector is relative to the filter.
ERRORS_DB = np.array([-40.0, -20.0, -10.0, 10.0, 20.0, 30.0, 40.0])
RATIOS = np.array([0.5, 1.0, 2.5, 5.0, 10.0])
ALPHA = 0.05

chart = np.array(
    [
        [
            settle_multiplier(ALPHA / (4.0 * r), ALPHA, float(e))
            for e in ERRORS_DB
        ]
        for r in RATIOS
    ]
)

# The reusability claim: same ratio, different (alpha, loop_bw), same M.
CHECK_ALPHAS = [0.2, 0.05, 0.01]
collapse = np.array(
    [
        [settle_multiplier(a / (4.0 * r), a, 40.0) for r in RATIOS]
        for a in CHECK_ALPHAS
    ]
)
# --8<-- [end:chart]

spread = np.max(
    np.abs(collapse - collapse.mean(axis=0)) / collapse.mean(axis=0)
)

# ── physical checks: the chart must mean what the docstring says ──────
assert np.all(np.isfinite(chart)), "a configuration never settled"

# The floor is real: no configuration is dramatically faster than the
# filter, and the quiet end is always slower than the loud end.
assert chart.min() > 0.5, f"M fell to {chart.min():.2f} — below any floor"
for i, r in enumerate(RATIOS):
    row = chart[i]
    assert row[0] < row[-1], (
        f"ratio {r}: a -40 dB start settled slower than a +40 dB one "
        f"({row[0]:.2f} vs {row[-1]:.2f}) — the detector asymmetry has "
        f"reversed sign"
    )

# M falls monotonically as the detector outruns the filter, and approaches
# the floor from above: that is what makes "ratio" the right axis.
worst = chart[:, -1]
assert np.all(np.diff(worst) < 0), (
    f"M(+40 dB) is not monotone in the ratio: {np.round(worst, 2)}"
)
assert worst[-1] < 1.6, (
    f"at ratio {RATIOS[-1]} the multiplier is still {worst[-1]:.2f}; the "
    f"chart never reaches its floor"
)

# The reusability claim, which is what makes this a chart and not a table.
assert spread < 0.10, (
    f"M varies by {spread:.1%} between (alpha, loop_bw) pairs at the same "
    f"ratio — it is not a function of the ratio alone, so the chart cannot "
    f"be read at an arbitrary bandwidth"
)

# And the recipe predicts a configuration that built none of the curves.
held_out_bw, held_out_alpha, held_out_err = 0.004, 0.02, 25.0
predicted_m = float(
    np.interp(
        held_out_err,
        ERRORS_DB,
        chart[
            int(np.argmin(np.abs(RATIOS - held_out_alpha / (4 * held_out_bw))))
        ],
    )
)
measured_m = settle_multiplier(held_out_bw, held_out_alpha, held_out_err)
rel = abs(measured_m - predicted_m) / predicted_m
assert rel < 0.15, (
    f"the chart predicted M={predicted_m:.2f} for a held-out design and it "
    f"measured {measured_m:.2f} ({rel:.0%} out) — the guide does not "
    f"generalise"
)


def main() -> int:
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    fig, (ax, bx) = plt.subplots(1, 2, figsize=(11.0, 4.2))

    for i, r in enumerate(RATIOS):
        ax.plot(
            ERRORS_DB, chart[i], "o-", lw=1.5, ms=4, label=f"ratio = {r:g}"
        )
    ax.axhline(1.0, ls="--", lw=1.2, color="crimson", label="the filter alone")
    ax.set_xlabel("initial gain error (dB)   —   quiet input to the right")
    ax.set_ylabel("M  =  settling  ×  4·loop_bw")
    ax.set_title("Settling, in units of the filter's time constant")
    ax.grid(alpha=0.3)
    ax.legend(fontsize=8, title="alpha / (4·loop_bw)", title_fontsize=8)

    for j, a in enumerate(CHECK_ALPHAS):
        bx.plot(
            RATIOS, collapse[j], "o-", lw=1.4, ms=5, label=f"alpha = {a:g}"
        )
    bx.axhline(1.0, ls="--", lw=1.2, color="crimson")
    bx.set_xscale("log")
    bx.set_xlabel("alpha / (4·loop_bw)")
    bx.set_ylabel("M at a +40 dB start")
    bx.set_title(f"One curve, not three: {spread:.1%} spread over 20× in both")
    bx.grid(alpha=0.3)
    bx.legend(fontsize=8)

    fig.suptitle(
        "AGC settling: 1/(4·loop_bw) is a floor, and this is the multiplier",
        fontsize=11,
    )
    fig.tight_layout()
    fig.savefig("agc_settling_design.png", dpi=110)
    plt.close(fig)

    print("M = settling / (1/(4*loop_bw)), measured:")
    header = "  ratio |" + "".join(f"{e:+7.0f}" for e in ERRORS_DB)
    print(header)
    print("  " + "-" * (len(header) - 2))
    for i, r in enumerate(RATIOS):
        print(f"  {r:5g} |" + "".join(f"{m:7.2f}" for m in chart[i]))
    print()
    print(f"reusable to {spread:.1%} across 20x in alpha and loop_bw")
    print(
        f"held-out design (loop_bw {held_out_bw:g}, alpha {held_out_alpha:g}, "
        f"{held_out_err:+.0f} dB): predicted M={predicted_m:.2f}, "
        f"measured {measured_m:.2f} ({rel:.0%})"
    )
    print("-> agc_settling_design.png")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
