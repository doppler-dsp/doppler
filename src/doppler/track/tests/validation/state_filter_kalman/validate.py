"""KalmanStateFilter — certification evidence.

Run directly to regenerate `results.md`:

    uv run python \
        src/doppler/track/tests/validation/state_filter_kalman/validate.py

`--check` re-renders in memory and diffs against the committed bytes;
`make validate` writes, `make validate-check` checks. Every limit this
records is asserted by
`src/doppler/track/tests/test_validation_limits.py`, which runs this same
`build(write=False)`.

The order is the campaign's, not this file's:
`native/inc/doppler/state_filter_kalman/state_filter_kalman_core.h` is the
SSOT, `native/tests/state_filter_kalman/test_state_filter_kalman_core.c`
certifies it in C, and `native/validation/state_filter_kalman_loop.c`
measures what a caller meets over the envelope: the gains against the
fixed-gain flavor's, the flavor's own stability edges and bound, the plain
covariance update against a wider reference, the cap on a high-gain start,
and what relaxing it buys. That harness writes `data/*.csv`, which this
file reads (C measures; Python renders and asserts); this file measures
what the binding reaches itself: the same gains and bandwidth, the exact
`stable()` and `bandwidth()` at the gains in force, the response of the
delivered bandwidth to wrong noise figures, and the closed loop.

Regenerate the C half with::

    build/native/validation/validate_state_filter_kalman_loop \\
        --csv src/doppler/track/tests/validation/state_filter_kalman/data
"""

from __future__ import annotations

import csv
import math
import sys
from pathlib import Path
from types import SimpleNamespace

import numpy as np

from doppler.tests._repo import build_dir, exe, repo_root
from doppler.tests._validation_common import Report, cli, harness_blocks
from doppler.track import KalmanStateFilter, StateFilter

HERE = Path(__file__).resolve().parent
DATA = HERE / "data"
ROOT = repo_root(__file__)
HARNESS = exe(
    build_dir(__file__) / "native/validation/validate_state_filter_kalman_loop"
)

R = Report()

# The header's constants (STATE_FILTER_KALMAN_*): the flavor's own bounds.
BNT_MIN, BNT_MAX, START_MAX = 1e-6, 0.12, 5.0
K_MIN, K_MAX = 0.5, 2.0


def read_csv(name: str) -> list[dict[str, float]]:
    """A CSV the C harness wrote, every field a float where it parses."""
    with (DATA / name).open(encoding="utf-8") as fh:
        rows = []
        for r in csv.DictReader(fh):
            row = {}
            for k, v in r.items():
                try:
                    row[k] = float(v)
                except ValueError:
                    row[k] = v  # a label (the binding column of caps.csv)
            rows.append(row)
        return rows


def step_settle(loop, f0: float = 2e-3, n: int = 4000) -> tuple[float, int]:
    """Peak phase error and the updates until it stays under 5% of that
    peak, after a frequency step of ``f0`` cycles per update (an ideal
    detector, noise-free)."""
    phase_in = phase_osc = 0.0
    err = np.empty(n)
    for i in range(n):
        phase_in += f0
        err[i] = phase_in - phase_osc
        phase_osc += loop.step(err[i])
    peak = float(np.abs(err).max())
    return peak, int(np.nonzero(np.abs(err) > 0.05 * peak)[0].max()) + 1


def figures_for(bnt: float) -> tuple[float, float]:
    """(sigma_e, sigma_a) whose steady-state loop has bandwidth ``bnt``,
    found through the binding: sigma_e fixed at 1, sigma_a bisected."""
    lo, hi = 1e-12, 1.0
    for _ in range(100):
        mid = math.sqrt(lo * hi)
        if KalmanStateFilter(sigma_e=1.0, sigma_a=mid).bandwidth() < bnt:
            lo = mid
        else:
            hi = mid
    return 1.0, math.sqrt(lo * hi)


# ── 1. the object ────────────────────────────────────────────────────
def section_object() -> None:
    R.md("## 1. The object — design and expectations")
    R.md()
    R.md(
        "The third-order loop of [`StateFilter`](../state_filter/results.md) "
        "with its gains computed each update from a covariance recursion "
        "instead of held: a Kalman filter on a model whose rate random-walks "
        "(`sigma_a`), measured through a detector with noise `sigma_e`. The "
        "design — the model, the claim that the fixed-gain shape is the "
        "continuous-limit optimum and the discrete difference, the stability "
        "rectangle and the bound on a high-gain start — is "
        "[`docs/design/loop-filter.md`]"
        "(../../../../../../../docs/design/loop-filter.md) Part III; the "
        "contract is `native/inc/doppler/state_filter_kalman/"
        "state_filter_kalman_core.h`. It is its own component, calling "
        "`StateFilter` for the loop's step, the exact stability and the exact "
        "bandwidth. What a caller reads from this report: the gains the "
        "steady state gives, the envelope they are stable over, how much of "
        "a high-gain start is allowed and what it buys, and what a wrong "
        "noise figure costs."
    )
    R.md()


# ── 2. characterisation ──────────────────────────────────────────────
def characterise() -> SimpleNamespace:
    d = SimpleNamespace()
    # the report's harness call is the C regression subset ctest also makes
    # (`--check --emit`): the limits below read this run, so the evidence and
    # the gate cannot disagree. The full sweep's CSVs (`--csv`) are the record.
    B = harness_blocks(
        HARNESS, "KalmanStateFilter", ROOT, args=("--check", "--emit")
    )
    R.md("## 2. Characterisation — what the object delivers")
    R.md()

    # 2.1 gains --------------------------------------------------------
    R.md("### 2.1 The steady-state gains against the fixed-gain flavor's")
    R.md()
    R.md(
        "The Kalman flavor started at its steady state is a constant-gain "
        "loop at the Riccati gains for the noise ratio whose exact "
        "bandwidth is `bn*t`. The fixed-gain flavor's are the Butterworth "
        "gains at the same `bn*t`. They meet as `bn*t` goes to 0 and differ "
        "by a few percent at the bound (C: `state_filter_kalman_loop.c`, "
        "`data/gains.csv`):"
    )
    R.md()
    gains = B["gains"]
    R.table(
        ["bn*t", "kp", "ki", "kr", "kp %", "ki %", "kr %"],
        [
            [
                f"{g['bnt']:.1e}",
                f"{g['kp']:.4e}",
                f"{g['ki']:.4e}",
                f"{g['kr']:.4e}",
                f"{100 * g['kp_rel']:+.2f}",
                f"{100 * g['ki_rel']:+.2f}",
                f"{100 * g['kr_rel']:+.2f}",
            ]
            for g in gains
        ],
    )
    R.md()
    d.gain_rows = gains
    d.bnt_min_refused = False
    try:
        KalmanStateFilter(bn=BNT_MIN * 0.99)
    except ValueError:
        d.bnt_min_refused = True
    d.bnt_min_accepted = KalmanStateFilter(bn=BNT_MIN).bn > 0.0
    # control: the binding's bandwidth of a filter built at twice the asked
    # bn*t is not within the claim's 1e-8 of the asked one
    d.bn_bind_wrong = abs(KalmanStateFilter(bn=0.02).bandwidth() / 0.01 - 1.0)
    d.bw_err = max(abs(g["bn_err"]) for g in gains)
    # through the binding: the delivered bandwidth is the one asked for
    worst = 0.0
    binding_rows = []
    for bnt in (1e-4, 1e-3, 0.01, 0.04, 0.12):
        k = KalmanStateFilter(bn=bnt)
        s = StateFilter(bn=bnt)
        worst = max(worst, abs(k.bandwidth() / bnt - 1.0))
        binding_rows.append(
            [
                f"{bnt:g}",
                f"{k.bandwidth():.8g}",
                f"{k.kr / s.kr - 1.0:+.4f}",
            ]
        )
    d.bn_bind = worst
    R.md("Through the binding, the delivered bandwidth and the `kr` gap:")
    R.md()
    R.table(["bn*t", "bandwidth()", "kr / StateFilter kr − 1"], binding_rows)
    R.md()

    # 2.2 stability ----------------------------------------------------
    R.md("### 2.2 The flavor's own stability edges and bound")
    R.md()
    R.md(
        "Stability at the loop gain `K` and detector delay `d` of the loop "
        "at the Riccati steady-state gains, bisected on the exact "
        "`stable()` (C, `data/edges.csv`). The bound is the largest `bn*t` "
        "stable at the upper corner `K = 2`, delay 2; the flavor has its "
        "own, not the first flavor's 0.1248:"
    )
    R.md()
    edges = B["edges"]
    table = {}
    for e in edges:
        table.setdefault(e["bnt"], {})[int(e["delay"])] = e
    rows = []
    for bnt in sorted(table):
        t = table[bnt]
        rows.append(
            [f"{bnt:g}"]
            + [
                f"{t[dd]['k_low']:.3f} … {t[dd]['k_high']:.2f}"
                for dd in (0, 1, 2)
            ]
        )
    R.table(["bn*t", "d = 0", "d = 1", "d = 2"], rows)
    R.md()
    d.k_low_12 = max(table[0.12][dd]["k_low"] for dd in (0, 1, 2))
    d.k_high_12 = min(table[0.12][dd]["k_high"] for dd in (0, 1, 2))
    d.margin_db = 20.0 * math.log10(K_MIN / table[0.12][2]["k_low"])
    # BNT_MAX: the largest bn*t on the grid whose upper edge at delay 2 keeps
    # 5% headroom over K_MAX
    d.headroom = {
        b: table[b][2]["k_high"] / K_MAX for b in (0.12, 0.13) if b in table
    }
    R.md(
        "`BNT_MAX` is not read off the bound: it is the largest `bn*t` on "
        "this grid at which the upper edge at the worst delay keeps 5% "
        "headroom over `K_MAX`, so that a 5% error in loop gain or detector "
        f"slope stays stable. The headroom is {d.headroom[0.12]:.3f} at 0.12 "
        f"and {d.headroom[0.13]:.3f} at 0.13."
    )
    R.md()
    # through the binding at the bound
    at_bound = KalmanStateFilter(bn=BNT_MAX)
    d.bound_stable = all(
        at_bound.stable(float(dd), float(k))
        for dd in (0, 1, 2)
        for k in (K_MIN, 1.0, K_MAX)
    )
    R.md(
        "The exact test at the bound through the binding: stable at "
        f"`K` in {{0.5, 1, 2}} and delay 0 to 2: {d.bound_stable}. The "
        f"lower edge at the bound, worst over delays, is {d.k_low_12:.3f}, "
        f"{d.margin_db:.1f} dB under `K_MIN` (the first flavor's is 4.1 dB "
        "under at its own bound)."
    )
    R.md()

    # 2.3 wrong figures ------------------------------------------------
    R.md("### 2.3 What a wrong noise figure costs")
    R.md()
    R.md(
        "The delivered bandwidth goes as the sixth root of the noise ratio "
        "`(sigma_a/sigma_e)^2`, so a figure wrong by 10 or 100 in the ratio "
        "is a bandwidth wrong by `10^(1/6) = 1.468` or `100^(1/6) = 2.154`. "
        "Measured through the binding from a base loop at `bn*t = 0.01`:"
    )
    R.md()
    se, sa = figures_for(0.01)
    base = KalmanStateFilter(sigma_e=se, sigma_a=sa).bandwidth()
    rows = []
    d.sixth = []
    for ratio in (0.01, 0.1, 10.0, 100.0):
        # a ratio r in (sigma_a/sigma_e)^2 is sigma_a * sqrt(r)
        bw = KalmanStateFilter(
            sigma_e=se, sigma_a=sa * math.sqrt(ratio)
        ).bandwidth()
        pred = ratio ** (1.0 / 6.0)
        d.sixth.append(abs(bw / base / pred - 1.0))
        rows.append([f"{ratio:g}", f"{bw / base:.3f}", f"{pred:.3f}"])
    R.table(
        ["noise ratio × ", "bandwidth ×  (measured)", "(ratio)^(1/6)"], rows
    )
    R.md()
    # control: the same departure against the wrong exponent (1/4) is far
    # over the bound the claim gives
    d.sixth_wrong = max(
        abs(float(r[1]) / float(r[0]) ** 0.25 - 1.0) for r in rows
    )

    # 2.4 the detector slope -------------------------------------------
    R.md("### 2.4 A detector slope under 1")
    R.md()
    R.md(
        "The loop's effective gain is the Kalman gain times the detector "
        "slope, so a slope below 1 (a detector whose slope compresses at "
        "low signal-to-noise ratio: 0.6 to 0.76) is the loop gain `K` of "
        "the stability map. `bandwidth(gain=slope)` and `stable(gain=slope)` "
        "at the gains in force, `bn*t = 0.04`:"
    )
    R.md()
    k = KalmanStateFilter(bn=0.04)
    rows = []
    # control: a loop gain of 0.05 (a detector slope far under the lower edge)
    # is not stable, so the slope assertion can fail
    d.slope_control = not KalmanStateFilter(bn=0.04).stable(2.0, 0.05)
    d.slope_ok = True
    for slope in (1.0, 0.76, 0.6):
        for dd in (0.0, 1.0, 2.0):
            bw = k.bandwidth(dd, slope)
            ok = k.stable(dd, slope)
            d.slope_ok = d.slope_ok and ok
            rows.append([f"{slope:g}", f"{dd:g}", f"{bw / 0.04:.3f}", str(ok)])
    R.table(["slope", "delay", "delivered bandwidth / asked", "stable"], rows)
    R.md()

    # 2.5 the start ----------------------------------------------------
    R.md("### 2.5 The high-gain start and its cap")
    R.md()
    R.md(
        "An initial frequency uncertainty `sigma_f0` larger than the steady "
        "state's own deviation (`sigma_f`) starts the covariance larger and "
        "the gains rise for a while. The constructor accepts a start only "
        "if the gains along the covariance's whole trajectory stay inside "
        "the stability rectangle (`K` 0.5 to 2, delays 0 to 2); the "
        "ceiling `START_MAX` is 5. The cap is a function of `bn*t`, "
        "measured by the harness's `--check` run: the rule's own cap "
        "(a dense walk, searched to 200) and what the constructor accepts "
        "(its sampled walk). The two agree to the bisection, and the "
        "ceiling binds where the rule's cap is above 5:"
    )
    R.md()
    caps = B["caps"]
    R.table(
        ["bn*t", "the rule's cap", "accepted", "binding limit"],
        [
            [
                f"{c['bnt']:.1e}",
                f"{c['dense_cap']:.2f}",
                f"{c['accepted_cap']:.2f}",
                str(c["binding"]),
            ]
            for c in caps
        ],
    )
    R.md()
    d.caps = caps
    rec_caps = read_csv("caps.csv")
    R.md(
        "The record (the full sweep, `data/caps.csv`, refreshed on purpose "
        "with `--csv`, not part of the gate) adds the slow loops: "
        + ", ".join(
            f"{c['bnt']:.0e}: {c['dense_cap']:.2f}"
            for c in rec_caps
            if c["bnt"] < 1e-3
        )
        + " (the rule's cap), all above the ceiling."
    )
    R.md()
    st = B["settle"]
    d.settle = max(r["settle_times_bnt"] for r in st)
    R.md(
        "The walk covers 8/(bn*t) updates. That horizon is measured: after a "
        "start of 1 to 5 times, at `bn*t` 1e-3 to the bound, the gains are "
        "within 1% of their steady values after at most "
        f"{d.settle:.1f}/(bn*t) updates (`# settle` block of the harness; "
        "the full sweep is `data/settle.csv`)."
    )
    R.md()
    sm = B["sampling"]
    d.samp = {
        (int(r["first"]), int(r["div"])): max(
            x["gap"]
            for x in sm
            if (int(x["first"]), int(x["div"]))
            == (int(r["first"]), int(r["div"]))
        )
        for r in sm
    }
    # the coarser steps are measured by the full sweep (the record), whose
    # slow rows are where a coarse step shows; --check holds three rows
    rec_sm = read_csv("sampling.csv")
    d.samp_rec = {
        (int(r["first"]), int(r["div"])): max(
            x["gap"]
            for x in rec_sm
            if (int(x["first"]), int(x["div"]))
            == (int(r["first"]), int(r["div"]))
        )
        for r in rec_sm
    }
    d.ceiling_floor = math.floor(
        min(c["dense_cap"] for c in caps if c["bnt"] <= 0.04)
    )
    d.first_viol = max(r["viol"] * r["bnt"] for r in sm if r["viol"] > 0)
    R.md(
        "The constants of the walk are measured. The walk tests each next "
        "update 1% of the elapsed updates after the last, and at least one "
        "update after it, so every one of the first 100 updates is tested "
        "(a 1% step is under one update there) and no separate head is "
        "needed. No accept on this grid depends on the step while the "
        "ceiling is 5 (the rule binds only where the walk is under 130 "
        "updates, and elsewhere the ceiling does), so the step is the "
        "coarsest with no cap excess, and it exists to bound the cost of a "
        "create: every update would be 8/(bn*t) walks of four exact tests. "
        "The table is the cap each step gives against the dense "
        "walk (every update), as the largest relative excess over the "
        "full sweep's grid, 1e-4 to 0.12 (a sampled walk can only miss a "
        "violation, so it is never below). The `--check` run holds three "
        "rows of it; the slow rows are where a coarse step shows:"
    )
    R.md()
    R.table(
        ["step (of elapsed updates)", "largest cap excess over the grid"],
        [
            [f"1/{dv}", f"{d.samp_rec[(fr, dv)]:.2e}"]
            for (fr, dv) in ((0, 100), (0, 10), (0, 2))
        ],
    )
    R.md()
    R.md(
        "The shipped 1/100 gives the dense cap to the bisection. The first "
        "violation at the cap comes before "
        f"{d.first_viol:.2f}/(bn*t) updates, inside the walk's 8/(bn*t). "
        "`START_MAX` is the integer part of the smallest cap up to `bn*t` "
        f"0.04: {min(c['dense_cap'] for c in caps if c['bnt'] <= 0.04):.2f}, "
        f"so {d.ceiling_floor}. The sampling table of the full sweep is "
        "`data/sampling.csv`."
    )
    R.md()
    # through the binding: the ceiling and the lower edge
    se, sa = figures_for(0.01)
    sf = KalmanStateFilter(sigma_e=se, sigma_a=sa).sigma_f
    refused = []
    for mult in (0.5, 5.5, 50.0):
        try:
            KalmanStateFilter(sigma_e=se, sigma_a=sa, sigma_f0=mult * sf)
            refused.append(False)
        except ValueError:
            refused.append(True)
    # the minimal pair: the start at the ceiling, with the same figures, is
    # accepted, so the three refusals are for the start and nothing else
    KalmanStateFilter(sigma_e=se, sigma_a=sa, sigma_f0=5.0 * sf)
    d.start_edges = refused
    R.md(
        "Through the binding at `bn*t = 0.01`: `sigma_f0` at 0.5, 5.5 and 50 "
        f"times `sigma_f` is refused ({refused}); 5 times is accepted."
    )
    R.md()

    # 2.6 what the start buys ------------------------------------------
    R.md("### 2.6 What a start buys: the step response")
    R.md()
    R.md(
        "Updates until the phase error stays under 5% of its peak after a "
        "frequency step, noise-free, against the fixed-gain loop of the same "
        "bandwidth, from a start of `sigma_f0 / sigma_f` through a detector "
        "with a slope and a delay. Starts past 5 are built through the "
        "embedder's unguarded `init_qr`, which the validated constructors do "
        "not accept: they are a measurement of what relaxing the cap buys, "
        "not a certified range. The gated rows are the harness's `--check` "
        "run (slope 1, delay 0); the full sweep is the record below."
    )
    R.md()
    relaxed = B["relaxed"]  # the --check rows: slope 1, delay 0
    d.relaxed = relaxed
    for bnt in (0.01, 0.04):
        R.md(f"`bn*t = {bnt:g}`, slope 1, delay 0:")
        R.md()
        rows = []
        for r in relaxed:
            if r["bnt"] == bnt and r["slope"] == 1.0 and r["delay"] == 0:
                rows.append(
                    [
                        f"{r['start']:g}",
                        f"{int(r['settle'])}",
                        f"{r['settle'] / r['fixed_settle']:.2f}",
                        f"{r['peak']:.4f}",
                    ]
                )
        R.table(
            ["start", "settle (updates)", "÷ fixed-gain", "peak error"], rows
        )
        R.md()
    unstable = [r for r in relaxed if not r["ok"]]
    d.check_unstable = len(unstable)
    rec = read_csv("relaxed.csv")
    d.rec_unstable = [r for r in rec if not r["ok"]]
    R.md(
        "**The record** (the full sweep, `data/relaxed.csv`, refreshed on "
        "purpose with `--csv` and not part of the gate): starts of 1 to 100 "
        "through slopes 0.6, 0.76 and 1 and delays 0 to 2, at `bn*t` 0.01 "
        "and 0.04. Settling updates at `bn*t` 0.01, delay 0, by start and "
        "slope (the fixed-gain loop's, at the same slope, beside each):"
    )
    R.md()
    rows = []
    for stt in (1.0, 5.0, 10.0, 30.0, 100.0):
        row = [f"{stt:g}"]
        for slope in (1.0, 0.76, 0.6):
            r = next(
                x
                for x in rec
                if x["bnt"] == 0.01
                and x["start"] == stt
                and x["slope"] == slope
                and x["delay"] == 0
            )
            row.append(f"{int(r['settle'])} ({int(r['fixed_settle'])})")
        rows.append(row)
    R.table(["start", "slope 1", "slope 0.76", "slope 0.6"], rows)
    R.md()
    R.md(
        f"Of the {len(rec)} closed loops in the record, "
        f"{len(d.rec_unstable)} diverged or failed to settle. Frozen at the "
        "gains in force, the loop leaves the rectangle for a few updates "
        "past the cap; the converging loop did not diverge at any slope or "
        "delay measured. That is a measurement of the record, not a "
        "guarantee and not a gated limit."
    )
    R.md()
    # the binding: the example's comparison
    se, sa = figures_for(0.02)
    fixed = StateFilter(
        bn=KalmanStateFilter(sigma_e=se, sigma_a=sa).bandwidth()
    )
    steady = KalmanStateFilter(sigma_e=se, sigma_a=sa)
    fast = KalmanStateFilter(
        sigma_e=se, sigma_a=sa, sigma_f0=4.0 * steady.sigma_f
    )
    _, n_fixed = step_settle(fixed)
    _, n_fast = step_settle(fast)
    d.n_fixed, d.n_fast = n_fixed, n_fast
    R.md(
        "Through the binding at `bn*t = 0.02`: a start of 4 settles the "
        f"step in {n_fast} updates against {n_fixed} for the fixed-gain "
        f"loop ({n_fast / n_fixed:.2f})."
    )
    R.md()

    # 2.7 the plain update ----------------------------------------------
    R.md("### 2.7 The covariance update is the plain one")
    R.md()
    r_bits = int(B["plain_update"][0]["ldbl_bits"])
    d.ldbl_bits = r_bits
    R.md(
        "The form of the covariance update was chosen by measurement: the "
        "plain `P' = F P F^T − S K K^T + Q` in `double`, run beside a copy "
        f"in `long double` ({r_bits} bits of mantissa on the machine that "
        "wrote the data; the same width as `double` on a platform where it "
        "is not, in which case the reference is no wider) for 10/(bn*t) "
        "updates. The gated rows are the harness's `--check` run (`bn*t` "
        "1e-4 and faster):"
    )
    R.md()
    plain = B["plain_update"]
    R.table(
        [
            "bn*t",
            "start",
            "updates",
            "gain departure",
            "worst P entry",
            "PD fail",
        ],
        [
            [
                f"{p['bnt']:.0e}",
                f"{p['start']:g}",
                f"{int(p['updates'])}",
                f"{p['gain_dep']:.1e}",
                f"{p['entry_dep']:.1e}",
                f"{int(p['pd_fail'])}",
            ]
            for p in plain
        ],
    )
    R.md()
    d.plain = plain
    rp = [p for p in read_csv("plain_update.csv") if p["bnt"] < 1e-4]
    R.md(
        "The record (the full sweep, `data/plain_update.csv`, capped at 2e6 "
        "updates, not part of the gate) adds the slow end: "
        + "; ".join(
            f"bn*t {p['bnt']:.0e}, start {p['start']:g}: gain departure "
            f"{p['gain_dep']:.1e}, positive definite at every update "
            f"({int(p['pd_fail'])} failures)"
            for p in rp
        )
        + ". The entry that loses digits is the small `p12`; the gains do "
        "not feel it."
    )
    R.md()
    return d


# ── 3. review ────────────────────────────────────────────────────────
def review(d: SimpleNamespace) -> None:
    R.md("## 3. Review — findings, with verdicts")
    R.md()
    r_big = max(g["kr_rel"] for g in d.gain_rows)
    R.find(
        "riccati_is_not_butterworth",
        "BY DESIGN",
        "**The steady-state gains are the optimal ones for the discrete "
        "model, not the fixed-gain flavor's.** At the bound `kr` is "
        f"{100 * r_big:.1f}% larger and `kp` "
        f"{abs(100 * d.gain_rows[-1]['kp_rel']):.1f}% smaller than the "
        "Butterworth gains of the same `bn*t`; the two meet as `bn*t` goes "
        "to 0 (§2.1). A caller who wants the Butterworth shape uses "
        "`StateFilter`.",
    )
    R.find(
        "thinner_lower_margin",
        "BY DESIGN",
        "**The lower stability margin is thinner than the first flavor's.** "
        f"At the bound the lower edge is {d.k_low_12:.3f}, "
        f"{d.margin_db:.1f} dB under `K_MIN` (the first flavor: 4.1 dB). A "
        "detector slope that compresses to about 0.58 of nominal at low "
        "signal-to-noise ratio is inside it, closer to the edge than "
        "`StateFilter` would put it (§2.2, §2.4).",
    )
    R.find(
        "the_cap_is_a_function_of_bnt",
        "BY DESIGN",
        "**How much high-gain start is allowed depends on `bn*t`, and it is "
        "the ceiling that binds up to 0.04.** The rule's own cap is about "
        "6 for every `bn*t` up to 0.04 and falls past it (3.8 at 0.06, 2.4 "
        "at 0.08, 1.2 at the bound); the constructor accepts at most 5, so "
        "the ceiling binds up to 0.04 and the rule past it (§2.5). A fast "
        "loop (large `bn*t`) gets the least acquisition from a start.",
    )
    best = min(
        (
            r
            for r in d.relaxed
            if r["bnt"] == 0.01 and r["slope"] == 1.0 and r["delay"] == 0
        ),
        key=lambda r: r["settle"],
    )
    base = next(
        r
        for r in d.relaxed
        if r["bnt"] == 0.01
        and r["start"] == 1.0
        and r["slope"] == 1.0
        and r["delay"] == 0
    )
    R.find(
        "relaxing_the_cap_buys_more",
        "BY DESIGN",
        "**A start past the ceiling settles a frequency step faster still, "
        "and did not diverge in any case measured.** At `bn*t = 0.01` the "
        f"best start measured ({best['start']:g}) settles in "
        f"{int(best['settle'])} updates against {int(base['settle'])} for the "
        f"steady state ({best['settle'] / base['settle']:.2f}); the validated "
        "constructors refuse it, and an embedder that passes a larger start "
        "through the unguarded init is outside the certified range (§2.6). "
        "Whether to relax the ceiling is the owner's.",
    )
    R.find(
        "kalman_composition_not_certified",
        "GAP",
        "**The filter is certified as an object, not in a closed loop with a "
        "real discriminator and noise.** Pull-in and cycle slips under noise, "
        "a detector whose slope and output variance compress at low "
        "signal-to-noise ratio, the cost of a start above 1 at loops slower "
        "than `bn*t` 1e-4, the converging loop's stability against the "
        "frozen-gain prediction, a gallery figure and the embedders' adoption "
        "are not covered here. Tracked in "
        "[#3136](https://github.com/doppler-dsp/doppler/issues/3136).",
    )
    R.find(
        "plain_update_c_only",
        "C-ONLY",
        "**The plain covariance update against the wider reference, and the "
        "sampled start rule against the dense one,** are certified in C "
        "(`test_state_filter_kalman_core.c`, "
        "`state_filter_kalman_loop.c`): the binding does not expose `P` "
        "(§2.7).",
    )


# ── 4. limits ────────────────────────────────────────────────────────
def limits(d: SimpleNamespace) -> None:
    R.md("## 4. Limits — the certified envelope")
    R.md()
    R.md(
        "Claims a caller may rely on. A failure here is a regression, not a "
        "new finding. Every one is asserted by "
        "`src/doppler/track/tests/test_validation_limits.py`."
    )
    R.md()
    R.md(
        "The last two columns are the evidence that a claim can fail. "
        "*Checked by* names the check that pins it: a block of the harness's "
        "`--check` run (`native/validation/state_filter_kalman_loop.c`, the "
        "run `ctest` also makes), a check of the C test, or the binding. "
        "*Sabotage* is a one-line break of "
        "`native/src/state_filter_kalman/state_filter_kalman_core.c`, built "
        "and run on this head, and what went red; every row that names one "
        "was observed red, and each break was reverted. They are recorded "
        "by hand: whether a record like this is generated is "
        "[#847](https://github.com/doppler-dsp/doppler/issues/847)."
    )
    R.md()
    R.md(
        "The limit marked *pin* is a number set from a measurement, not a "
        "claim the page makes, so a change that moves it is for a person to "
        "judge, not necessarily a defect."
    )
    R.md()
    R.limit(
        d.bw_err < 1e-8,
        f"the delivered bandwidth is `bn*t` to {d.bw_err:.1e} from bn*t 1e-6 "
        "to 0.12 (the harness's `--check` run)",
        by="harness `gains` block, exact bandwidth at the design gains",
        control="the same bandwidth check on gains with `kr` 0.1% off",
        red="bisection target ×1.02 → this row (2.0e-2)",
    )
    R.limit(
        d.bn_bind < 1e-8 and d.bn_bind_wrong >= 1e-8,
        f"through the binding `bandwidth()` is `bn*t` to {d.bn_bind:.1e} at "
        "1e-4 to 0.12",
        by="the binding, on built filters",
        control="the same check on a filter built at twice the asked `bn*t`",
        red="bisection target ×1.02 → this row (2.0e-2)",
    )
    R.limit(
        all(
            abs(g["kr_rel"]) < 0.10 and abs(g["kp_rel"]) < 0.06
            for g in d.gain_rows
        ),
        "the steady-state gains are within 6% (`kp`) and 10% (`kr`) of the "
        "fixed-gain flavor's at every `bn*t` to the bound",
        control="the same 10% check against the fixed-gain gains of a loop "
        "ten times as wide",
        red="`kp` ×1.3 → this row; `kr` ×0.3 → this row and the harness check",
    )
    R.limit(
        all(abs(g["kr_rel"]) < 0.01 for g in d.gain_rows if g["bnt"] <= 0.01),
        "they are within 1% (`kr`) of it for `bn*t` up to 0.01",
        control="the same 1% check at the bound, where the flavors differ by "
        "9%",
        red="`kr` ×1.01 → this row and the harness check",
    )
    R.limit(
        d.bound_stable,
        "at the bound the loop is stable at every loop gain 0.5 to 2 and "
        "detector delay 0 to 2",
        by="harness `edges` block; check `in_rectangle (g)` at 0.12",
        control="the same assertion on gains with `kp` 3 times too large",
        red="`kp` ×1.3 → this row; `kr` ×0.3 → this row and the harness check",
    )
    # A lower bound is the claim a caller relies on: at least 2 dB of room
    # under K_MIN at the worst delay. The measured value (2.72 dB) is shown
    # beside it; the first flavor's 4.1 dB is a comparison in prose, and a
    # band around today's number would be a fitted gate.
    R.limit(
        d.margin_db > 2.0,
        f"the lower margin at the bound is {d.margin_db:.2f} dB under "
        "`K_MIN`, at least 2 dB (the first flavor's is 4.1 dB)",
        control="the same margin assertion on gains with `kp` 0.85 times "
        "(0.80 dB)",
        red="`kp` ×0.85 → 0.80 dB; `kr` ×1.3 → 0.77 dB (this row and the "
        "harness check)",
    )
    # The horizon the start rule walks (8/(bn*t)) is a derived claim: the
    # measured settling is 4.5/(bn*t) at most, so 8 has a margin of 1.8.
    R.limit(
        d.settle < 5.0,
        f"the gains settle to 1% of steady within {d.settle:.1f}/(bn*t) "
        "updates (the start rule walks 8)",
        control="the same settle assertion against the steady state of a loop "
        "ten times as wide",
        red="the steady solve's initial noise term √q → 30.0/(bn*t)",
    )
    R.limit(
        d.k_high_12 > K_MAX,
        f"the upper edge at the bound exceeds `K_MAX` ({d.k_high_12:.2f})",
        control="the same assertion at `bn*t` 0.14, past the bound (the edge "
        "is 1.85)",
        red="`kp` ×1.3 → the edge is 1.91; `kr` ×0.3 → 1.95",
    )
    # PIN. The law is the continuous limit; the loop is discrete, so the
    # departure is the discretisation, measured 1.1% at the worst ratio
    # (100, which delivers bn*t of 0.0215) from bn*t 0.01. 2% is about twice
    # that. It is a regression pin on the delivered-bandwidth law, not a
    # bound derived from the discretisation.
    R.limit(
        max(d.sixth) < 0.02 <= d.sixth_wrong,
        f"pin: the delivered bandwidth goes as the sixth root of the noise "
        f"ratio to {100 * max(d.sixth):.1f}% for ratios 0.01 to 100 from "
        "bn*t 0.01 (2% allowed: the discrete loop's departure from the "
        "continuous law)",
        control="the same departure against the exponent 1/4 (far over the "
        "bound)",
        red="the steady solve's initial noise term √q → this row",
    )
    R.limit(
        d.slope_ok and d.slope_control,
        "the loop at bn*t 0.04 is stable at detector slopes 0.6, 0.76 and 1 "
        "and delays 0 to 2",
        control="the same stability assertion at a loop gain of 0.05, under "
        "the lower edge",
        red="none found: the edges widen as bn*t falls, so no break of the "
        "gains turns this row red before the bound row above does (`kr` ×0.3 "
        "turned that red and not this)",
    )
    R.limit(
        d.start_edges == [True, True, True],
        "`sigma_f0` below the steady state's deviation, above the ceiling "
        "and far above it are refused",
        control="the minimal pair: a start at the ceiling, same figures, is "
        "accepted",
        red="ceiling check removed → this row; floor check removed → this row",
    )
    R.limit(
        all(
            c["accepted_cap"] <= c["dense_cap"] * 1.0001
            and c["accepted_cap"] <= START_MAX * 1.0001
            for c in d.caps
        ),
        "the constructor never accepts a start past the rule's cap or the "
        "ceiling, at any `bn*t` measured",
        by="harness `caps` block, check `alo <= dens`; C test, `dense == "
        "smpl`",
        control="the minimal pair at the cap: 0.99 of it accepted, 1.01 of it "
        "refused",
        red="rule removed, delay-0-only and gains not refreshed in the walk "
        "→ this row, the harness check and the C test; a walk of 0.5/(bn*t) "
        "instead of 8 → this row and the harness check, and the C test stays "
        "green",
    )
    R.limit(
        all(c["accepted_cap"] > 3.0 for c in d.caps if c["bnt"] <= 0.06),
        "a start of at least 3 times the steady state is accepted for "
        "`bn*t` up to 0.06",
        control="the same assertion at the bound, where the accepted cap is "
        "1.19",
        red="the rule's worst delay moved from 2 to 4 → this row",
    )
    # A claim a caller relies on: at least 1.5 times as fast as the
    # fixed-gain loop (2/3 of its updates). The measured 0.58 is shown beside
    # it, and the guide says about 1.7 times.
    R.limit(
        d.n_fast < 2.0 / 3.0 * d.n_fixed,
        f"a start of 4 settles a frequency step in {d.n_fast / d.n_fixed:.2f} "
        "of the fixed-gain loop's updates at bn*t 0.02, at least 1.5 times "
        "as fast",
        by="the binding, a closed loop, §2.6; harness check `a.settle < "
        "(2.0 / 3.0) * fx.settle`",
        control="the same speed-up assertion at a start of 1, which settles "
        "as the fixed-gain loop does",
        red="the update does not refresh the gains → 1.00; `kp` ×1.3 → 0.87",
    )
    R.limit(
        d.check_unstable == 0,
        "every closed loop in the harness's `--check` run (starts 1 to 100 "
        "at slope 1, delay 0, bn*t 0.01 and 0.04) settles to the step",
        control="the same assertion on a loop with a detector slope of 500, "
        "which diverges",
        red="a covariance term broken so P loses positive definiteness → "
        "the harness check; the steady solve's √q → this row",
    )
    R.limit(
        all(p["pd_fail"] == 0 for p in d.plain),
        "the covariance stays positive definite at every update of every "
        "plain-update run of the `--check` subset",
        by="harness `plain_update` block; check `r.pd_fail == 0`",
        control="the same positive-definite test on two matrices that are not",
        red="the `S K K^T` term of the last diagonal entry ×3 → the "
        "harness check",
    )
    # Derived, not fitted: the plain update's rounding can accumulate
    # linearly in the number of updates N, so the gains can drift by about
    # N * eps(double). The longest run of the subset is N = 1e5 (bn*t 1e-4,
    # 10/(bn*t) updates), so N * eps = 1.1e-11; the measured worst is 4.2e-13,
    # 26 times under it. The sweep to 1e-6 is the record and is not gated.
    # Where long double is no wider than double the reference is the same
    # arithmetic written twice, so this row checks the transcription and not
    # the rounding; the report says so.
    if d.ldbl_bits > 53:
        gd_claim = (
            "the plain update's gains are within 1e-11 of the wider "
            f"reference ({d.ldbl_bits}-bit mantissa) at every run of the "
            "`--check` subset (bn*t 1e-4 and faster); 1e-11 is N·ε for the "
            "longest run, N = 1e5"
        )
    else:
        gd_claim = (
            "the plain update's gains equal the reference's: `long double` "
            f"is {d.ldbl_bits} bits here, no wider than `double`, so this "
            "checks the recursion's transcription and not its rounding (the "
            "1e-11 bound is meaningful only with a wider reference, and is "
            "not asserted for rounding)"
        )
    R.limit(
        all(p["gain_dep"] < 1e-11 for p in d.plain),
        gd_claim,
        control="the same departure against a reference whose process noise "
        "is 1% off",
        red="process noise ×1.01 in the covariance recursion → this row; "
        "and the harness check `r.gain_dep < 1e-7`",
    )
    R.limit(
        d.samp[(0, 100)] < 1e-3,
        f"the start rule's sampled walk (1% steps) gives the dense cap to "
        f"{d.samp[(0, 100)]:.0e} at every bn*t of the `--check` rows; a "
        f"10% step would be {100 * d.samp_rec[(0, 10)]:.2f}% high on the "
        "full sweep's grid",
        by="harness `sampling` block; check `gap < 1e-3`",
        control=(
            "a 1/10 step accepts a start of 6.07 at 1e-3 that the dense walk "
            "refuses and the 1/100 step refuses"
        ),
        red="the instrument's step 1/100 → 1/10 → the harness check; "
        "changing the shipped step turns nothing red, because with the "
        "ceiling at 5 no accept depends on it (see the text above)",
    )
    R.limit(
        d.first_viol < 1.0,
        f"the first violation at the cap comes before {d.first_viol:.2f}"
        "/(bn*t) updates, inside the walk's 8/(bn*t)",
        control="the same assertion on a start 10% under the cap, which never "
        "leaves the rectangle",
        red="none found: a measurement of when the excursion occurs, which "
        "no one-line break moved without also failing a gain row",
    )
    R.limit(
        d.ceiling_floor == START_MAX,
        "`START_MAX` is the integer part of the smallest rule cap up to "
        f"`bn*t` 0.04 ({d.ceiling_floor:g})",
        control="the same equality over every `bn*t`, where the smallest cap "
        "has another integer part",
        red="`START_MAX` 5 → 6 → the harness check, and the start-edge "
        "and ceiling rows",
    )
    R.limit(
        d.headroom[0.12] >= 1.05 > d.headroom[0.13],
        "`BNT_MAX` is the largest `bn*t` on the grid with 5% headroom over "
        f"`K_MAX` ({d.headroom[0.12]:.3f} at 0.12, {d.headroom[0.13]:.3f} "
        "at 0.13)",
        control="the same selection with no headroom asked, which lands on "
        "0.13",
        red="`BNT_MAX` 0.12 → 0.13 → the harness check",
    )
    R.limit(
        all(
            c["accepted_cap"] >= 0.999 * min(c["dense_cap"], START_MAX)
            for c in d.caps
        ),
        "the constructor accepts every start up to the rule's cap or the "
        "ceiling, whichever is smaller, at every `--check` row (the bound "
        "included)",
        by="harness `caps` block; check `alo >= 0.999 * want`",
        control=(
            "the same lower bound on a constructor that refuses every "
            "start (its cap reads 1)"
        ),
        red="the check refusing every start above 1 → this row and control 12",
    )
    R.limit(
        d.bnt_min_refused and d.bnt_min_accepted,
        "the constructor accepts `bn*t` = 1e-6, the smallest measured, and "
        "refuses 1% below it",
        by="the binding; harness check `BNT_MIN >= 1e-6`",
        control="the minimal pair: `bn*t` 1e-6 accepted, 1% below it refused",
        red="`BNT_MIN` 1e-6 → 1e-7 → the harness check and this row",
    )


def build(write: bool = True) -> Report:
    global R
    R = Report(write=write)
    R.md("# KalmanStateFilter — validation report")
    R.md()
    section_object()
    d = characterise()
    review(d)
    limits(d)
    R.executive(
        "KalmanStateFilter",
        source=(
            "Generated by `validate.py` in this folder. The gated numbers "
            "come from two places, named in each section: the C harness "
            "`native/validation/state_filter_kalman_loop.c`, whose "
            "`--check --emit` run (the regression subset `ctest` also makes) "
            "this file runs and renders, and the Python binding itself. The "
            "tables marked as the record come from the harness's full sweep "
            "(`--csv`), refreshed on purpose and not part of the gate. "
            "Nothing is modelled. Re-run to regenerate."
        ),
        takeaways=[
            "**Started at its steady state it is a constant-gain loop at the "
            "optimal gains for the noise figures**, not the fixed-gain "
            "flavor's Butterworth gains: the two differ by under 1% up to "
            "`bn*t` 0.01 and by "
            f"{100 * max(g['kr_rel'] for g in d.gain_rows):.0f}% of `kr` at "
            f"the bound ({R.ref('riccati_is_not_butterworth')}, §2.1).",
            "**Its stability bound is its own** (about 0.13) **and its "
            f"lower margin is thinner**: {d.margin_db:.1f} dB under `K_MIN` "
            f"at the bound ({R.ref('thinner_lower_margin')}, §2.2).",
            "**A wrong noise ratio moves the bandwidth as its sixth root**: "
            "a ratio wrong by 10 is a bandwidth wrong by 1.47 (§2.3).",
            "**A high-gain start settles a frequency step about 1.7 times as "
            "fast as the fixed-gain loop, within a cap that is 5 up to "
            f"`bn*t` 0.04 and falls past it** "
            f"({R.ref('the_cap_is_a_function_of_bnt')}, §2.5).",
            "**Relaxing the ceiling buys more and diverged nowhere "
            "measured**, "
            "but a start past it is outside the certified range "
            f"({R.ref('relaxing_the_cap_buys_more')}, §2.6).",
        ],
    )
    R.summary(
        "\n- Raw sweeps (written by `native/validation/"
        "state_filter_kalman_loop.c`): `data/gains.csv`, `data/edges.csv`, "
        "`data/caps.csv`, `data/starts.csv`, `data/relaxed.csv`, "
        "`data/plain_update.csv`"
    )
    R.emit(HERE / "results.md")
    return R


if __name__ == "__main__":
    sys.exit(cli(build, HERE))
