"""StateFilter — certification evidence, measured through the shipped binding.

Run directly to regenerate `results.md`, the plots and the CSVs:

    uv run python src/doppler/track/tests/validation/state_filter/validate.py

`--check` re-renders in memory and diffs against the committed bytes;
`make validate` writes, `make validate-check` checks. Every limit this
records is asserted by
`src/doppler/track/tests/test_validation_limits.py`, which runs this same
`build(write=False)`.

The order is the campaign's, not this file's:
`native/inc/doppler/state_filter/state_filter_core.h` is the SSOT,
`native/tests/state_filter/test_state_filter_core.c` certifies it in C, and
`native/validation/state_filter_loop.c` measures what a caller meets over
the envelope (bandwidth by impulse, by the exact function and by noise;
the stable-gain map; transients). This file measures the same properties
through `doppler.track.StateFilter` to show the binding delivers them.
"""

from __future__ import annotations

import sys
from pathlib import Path
from types import SimpleNamespace

import numpy as np

from doppler.tests._validation_common import Report, cli
from doppler.track import LoopFilter, StateFilter

HERE = Path(__file__).resolve().parent
DATA = HERE / "data"

R = Report()

# The rectangle the header advertises (STATE_FILTER_*): the automatic case is
# stable at every loop gain K_MIN..K_MAX and detector delay 0..DELAY_MAX, for
# bn*t up to BNT_MAX.
K_MIN, K_MAX, DELAY_MAX, BNT_MAX = 0.5, 2.0, 2.0, 0.12
BNTS = [0.002, 0.01, 0.02, 0.04, 0.08, 0.12]


def _csv(path: Path, header: str, rows: list[list[float]]) -> None:
    if not R.write:
        return
    DATA.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as fh:
        fh.write(header + "\n")
        for r in rows:
            fh.write(",".join(f"{v:.10g}" for v in r) + "\n")


def parseval(sf: StateFilter, delay: int = 0, gain: float = 1.0, n=60000):
    """Bn = (1/2) sum phi^2 of the loop's impulse response, by running the
    real filter through the binding (integer delays)."""
    r = np.zeros(delay + 1)  # r[0] = the newest error, r[d] = d updates old
    phi = 0.0
    total = 0.0
    for i in range(n):
        r[1:] = r[:-1].copy()
        r[0] = (1.0 if i == 0 else 0.0) - phi
        phi += sf.step(gain * r[delay])
        total += phi * phi
    return 0.5 * total


EPS = float(np.finfo(float).eps)


def rounding_bound(n_terms: int) -> float:
    """Four times the random-walk rounding bound of a sum of ``n_terms``
    terms, ``4 * eps * sqrt(n)``: what a relative check of a Parseval sum can
    be asked for before the claim is about the arithmetic and not the loop."""
    return 4.0 * EPS * float(np.sqrt(n_terms))


def gain_edges(sf: StateFilter, delay: float) -> tuple[float, float]:
    """The stable interval [K_low, K_high] of the loop gain, by bisection on
    the exact test the binding exposes."""
    lo_a, lo_b = 1.0, 1e-3  # stable, then not
    for _ in range(40):
        m = (lo_a * lo_b) ** 0.5
        lo_a, lo_b = (m, lo_b) if sf.stable(delay, m) else (lo_a, m)
    hi_a, hi_b = 1.0, 1e4
    for _ in range(40):
        m = (hi_a * hi_b) ** 0.5
        hi_a, hi_b = (m, hi_b) if sf.stable(delay, m) else (hi_a, m)
    return lo_a, hi_a


def stable_changes(sf: StateFilter, delay: float) -> int:
    """How many times stability changes along a log sweep of K (2 = one
    stable interval)."""
    prev, changes = None, 0
    for K in 0.02 * 5000.0 ** (np.arange(401) / 400.0):
        st = sf.stable(delay, float(K))
        if prev is not None and st != prev:
            changes += 1
        prev = st
    return changes


def phase_taps(loop, n: int = 60000) -> tuple[float, float]:
    """The loop's impulse response read at two taps of the same update: the
    oscillator phase at the detector instant, ``phi[n]`` (the phase the error
    is formed from), and the phase just after the proportional step,
    ``phi[n] + kp*e[n]``. Returns ``(sum phi^2, sum post^2)``; the first is
    ``2*Bn``."""
    kp = loop.kp
    phi = det = post = 0.0
    for i in range(n):
        e = (1.0 if i == 0 else 0.0) - phi
        det += phi * phi
        post += (phi + kp * e) ** 2
        phi += loop.step(e)
    return det, post


def close_ramp(loop, r: float, n: int = 40000) -> float:
    """The phase error after n updates of a frequency ramp r per update per
    update, with an ideal detector."""
    f = w = phi = e = 0.0
    for _ in range(n):
        f += r
        w += f
        e = w - phi
        phi += loop.step(e)
    return e


def step_response(loop, n: int) -> tuple[float, int]:
    """Peak phase error to a frequency step, and the updates until the error
    stays within 5% of that peak."""
    phi = 0.0
    err = np.empty(n)
    for i in range(n):
        err[i] = i - phi
        phi += loop.step(err[i])
    peak = float(np.abs(err).max())
    last = int(np.nonzero(np.abs(err) > 0.05 * peak)[0].max())
    return peak, last + 1


# ── 1. the object ────────────────────────────────────────────────────
def section_object() -> None:
    R.md("## 1. The object — design and expectations")
    R.md()
    R.md(
        "A filter over an explicit state vector, the frequency estimate and "
        "its rate: the fixed-gain third-order tracking loop, the sibling of "
        "the second-order `LoopFilter`. The design rationale — the shape of "
        "the loop, the exact discrete-time bandwidth the gains are solved "
        "from, the stability bound and the gain range it tolerates — is "
        "[`docs/design/loop-filter.md`]"
        "(../../../../../../docs/design/loop-filter.md) Part II; the "
        "contract is `native/inc/doppler/state_filter/state_filter_core.h`. "
        "Neither is restated here."
    )
    R.md()
    R.md("Three bodies of evidence stand behind this report:")
    R.md()
    R.table(
        ["evidence", "answers"],
        [
            [
                "`native/tests/state_filter/test_state_filter_core.c`",
                "does each claim in the header still hold, per update, with "
                "every pin proven by sabotage",
            ],
            [
                "`native/validation/state_filter_loop.c`",
                "what the loop delivers over the whole envelope: bandwidth "
                "by impulse, by the exact function and by white noise; the "
                "stable-gain map; transients",
            ],
            [
                "this report",
                "does the binding deliver the same object, and what may a "
                "caller design to",
            ],
        ],
    )
    R.md("The header's claims, and where each is certified:")
    R.md()
    R.table(
        ["claim", "certified by"],
        [
            ["the three-line recurrence, `steps` equal `step`", "C §2, §5"],
            [
                "the gains deliver the bandwidth asked for at zero delay",
                "C §3, §3b; §2.1",
            ],
            ["the domain, and a bound on `bn*t`", "C §1, §7; §2.3"],
            [
                "conditionally stable in the loop gain; the lower edge",
                "C §7; §2.3",
            ],
            ["one exact function, both orders", "C §6; §2.2"],
            ["steady errors by order", "C §8; §2.4"],
            ["the state blob and its flavor key", "C §9; §2.6"],
            [
                "the restore check: the gains must be the ones `bn` and `t` "
                "give, to 1e-9, and the pad word must be 0",
                "C §9 (forged payloads, the tolerance held a decade either "
                "side), §3b (the gains against a long double solve)",
            ],
            [
                "the interpolated fractional delay is a defined model",
                "C §6 (simulation at 0.5 and 1.25)",
            ],
            [
                "the exact test is right off the design manifold (a Kalman "
                "filter's gains while its covariance converges)",
                "C §10",
            ],
            [
                "the decision rule, at loops built at a known radius either "
                "side of 1",
                "C §11",
            ],
        ],
    )


# ── 2. characterisation ──────────────────────────────────────────────
def characterise() -> SimpleNamespace:
    d = SimpleNamespace()
    R.md("## 2. Characterisation")
    R.md()
    R.md(
        "Measured behaviour. No verdicts — those are §3. Section numbers "
        "of the C test are named where they correspond."
    )
    R.md()

    # 2.1 delivered bandwidth ----------------------------------------
    R.md("### 2.1 The bandwidth asked for is the one delivered (C §3)")
    R.md()
    rows, d.bw0_max_rel = [], 0.0
    d.bw0_n_max = 0
    for bnt in BNTS:
        n_terms = int(1000 / bnt) + 20000
        d.bw0_n_max = max(d.bw0_n_max, n_terms)
        got = parseval(StateFilter(bn=bnt), 0, n=n_terms)
        rel = got / bnt - 1.0
        d.bw0_max_rel = max(d.bw0_max_rel, abs(rel))
        rows.append([bnt, got, rel])
    R.md(
        "Parseval on the impulse response of the real filter through the "
        "binding, at a detector delay of zero. The second-order filter's "
        "delivered bandwidth is wide by a fractional excess that grows with "
        "`bn*t`; here there is none, because the gains are solved from the "
        "exact expression."
    )
    R.md()
    R.table(
        ["`bn*t`", "Bn (Parseval)", "Bn / (bn*t) − 1"],
        [[f"{a:g}", f"{b:.9g}", f"{c:+.1e}"] for a, b, c in rows],
    )
    _csv(DATA / "bandwidth.csv", "bnt,bn_parseval,rel", rows)
    # control: the same check on a detector delay of one update (a different
    # loop, wider than bn*t) is far outside the rounding bound
    d.bw0_control = abs(
        parseval(StateFilter(bn=0.02), 1, n=40000) / 0.02 - 1.0
    )

    # 2.2 delay and gain ---------------------------------------------
    R.md("### 2.2 What a detector delay and a loop gain do (C §6)")
    R.md()
    R.md(
        "The exact function `bandwidth(delay, gain)` against a simulation of "
        "the real filter, at `bn*t = 0.03`. At an integer delay the model is "
        "a real transport delay and the answer is exact."
    )
    R.md()
    d.exact_max_rel = 0.0
    rows = []
    for delay in (0, 1, 2):
        for gain in (1.0, 0.7):
            sf = StateFilter(bn=0.03)
            exact = sf.bandwidth(delay=float(delay), gain=gain)
            meas = parseval(StateFilter(bn=0.03), delay, gain)
            rel = meas / exact - 1.0
            d.exact_max_rel = max(d.exact_max_rel, abs(rel))
            rows.append([delay, gain, exact, meas, rel])
    R.table(
        ["delay", "gain", "exact", "simulated", "difference"],
        [
            [f"{a}", f"{b:g}", f"{c:.7g}", f"{m:.7g}", f"{r:+.1e}"]
            for a, b, c, m, r in rows
        ],
    )
    # control: the exact function at delay 1 against the simulation at delay 0
    d.exact_control = abs(
        parseval(StateFilter(bn=0.03), 0)
        / StateFilter(bn=0.03).bandwidth(delay=1.0)
        - 1.0
    )
    d.delay_widens_control = all(
        StateFilter(bn=b).bandwidth() > StateFilter(bn=b).bandwidth(delay=1.0)
        for b in BNTS
    )
    d.delay_widens = all(
        StateFilter(bn=b).bandwidth(delay=2.0)
        > StateFilter(bn=b).bandwidth(delay=1.0)
        > StateFilter(bn=b).bandwidth()
        for b in BNTS
    )
    d.bw_delay = {
        (b, k): StateFilter(bn=b).bandwidth(delay=float(k)) / b
        for b in BNTS
        for k in (1, 2)
    }
    R.md(
        "The delivered bandwidth over `bn*t` at a delay of one and two "
        "updates (zero delay is 1 exactly):"
    )
    R.md()
    R.table(
        ["`bn*t`", "delay 1", "delay 2"],
        [
            [
                f"{b:g}",
                f"{d.bw_delay[(b, 1)]:.4f}",
                f"{d.bw_delay[(b, 2)]:.4f}",
            ]
            for b in BNTS
        ],
    )

    # 2.3 stability ---------------------------------------------------
    R.md("### 2.3 The stable-gain map and the bound (C §7)")
    R.md()
    R.md(
        "The stable interval of the loop gain `K` (the discriminator slope "
        "times the oscillator gain, as a multiple of nominal) at each "
        "`bn*t` and detector delay, by bisection on the exact test. The "
        "lower edge is set by the shape, near `1/(a*b) = 0.25`; the upper "
        "edge falls with `bn*t` and with delay."
    )
    R.md()
    rows, d.edges, d.changes_max = [], {}, 0
    for bnt in BNTS:
        sf = StateFilter(bn=bnt)
        for delay in (0.0, 1.0, 2.0):
            lo, hi = gain_edges(sf, delay)
            ch = stable_changes(sf, delay)
            d.edges[(bnt, delay)] = (lo, hi)
            d.changes_max = max(d.changes_max, ch)
            rows.append([bnt, delay, lo, hi, ch])
    R.table(
        ["`bn*t`", "delay", "K_low", "K_high", "changes along K"],
        [
            [f"{a:g}", f"{b:g}", f"{c:.4f}", f"{e:.2f}", f"{int(g)}"]
            for a, b, c, e, g in rows
        ],
    )
    _csv(DATA / "stability_map.csv", "bnt,delay,k_low,k_high,changes", rows)

    edge = StateFilter(bn=BNT_MAX)
    d.rect_unstable = sum(
        not edge.stable(float(dl), float(g))
        for dl in np.linspace(0.0, DELAY_MAX, 41)
        for g in np.geomspace(K_MIN, K_MAX, 15)
    )
    d.k_low_bound = max(
        gain_edges(edge, float(dl))[0] for dl in np.linspace(0, DELAY_MAX, 9)
    )
    d.k_low_margin_db = 20 * np.log10(K_MIN / d.k_low_bound)
    # controls: gains 5 to 10 times nominal are NOT inside the rectangle, so
    # the same unstable count is positive there; and the upper edge at a
    # narrower loop (0.06) is not within 5% of K_MAX
    d.rect_control = sum(
        not edge.stable(DELAY_MAX, float(g))
        for g in np.geomspace(5.0, 10.0, 15)
    )
    d.tight_control = gain_edges(StateFilter(bn=0.06), DELAY_MAX)[1]
    # a loop wider than the bound cannot be created, so tightness is read at
    # the bound: the upper edge at the worst delay sits within 5% of K_MAX
    d.k_high_corner = gain_edges(edge, DELAY_MAX)[1]
    d.bound_tight = bool(K_MAX <= d.k_high_corner < 1.05 * K_MAX)
    R.md("![The stable-gain interval](stability_map.png)")
    R.md()
    R.md(
        f"At the bound (`bn*t = {BNT_MAX}`): {d.rect_unstable} unstable "
        f"points on a grid of 41 delays × 15 gains over the advertised "
        f"rectangle ({K_MIN} to {K_MAX} of nominal gain, delay 0 to "
        f"{DELAY_MAX:g}); the worst lower edge over those delays is "
        f"{d.k_low_bound:.3f}, a margin of {d.k_low_margin_db:.1f} dB under "
        f"`K_MIN = {K_MIN}`; the upper edge at delay {DELAY_MAX:g} is "
        f"{d.k_high_corner:.3f}, within 5% of `K_MAX = {K_MAX}`: "
        f"{d.bound_tight}."
    )
    R.md()

    # 2.4 ramp --------------------------------------------------------
    R.md("### 2.4 The frequency ramp the second order cannot null (C §8)")
    R.md()
    r = 2e-7
    bn = 0.02
    second = LoopFilter(bn=bn, zeta=0.707, t=1.0)
    e2 = close_ramp(second, r)
    e3 = close_ramp(StateFilter(bn=bn), r)
    d.ramp = (r, bn, e2, r / second.ki, e3)
    # the ramp's phase accumulator reaches r * n^2 / 2 and carries eps of it:
    # the error being read is a difference of two numbers that large
    n_ramp = 40000
    d.ramp_abs = 4.0 * EPS * (r * n_ramp * n_ramp / 2.0)
    d.ramp_round = d.ramp_abs / (r / second.ki)  # relative to the error read
    d.ramp_control = abs(
        e2 / (r / LoopFilter(bn=0.04, zeta=0.707, t=1.0).ki) - 1.0
    )
    R.md(
        f"A ramp of {r:g} cycles per update per update into both loops at "
        f"`bn = {bn}`, an ideal detector, after 40000 updates:"
    )
    R.md()
    R.md("![A frequency ramp into both loops](ramp.png)")
    R.md()
    R.table(
        ["loop", "steady phase error (cycles)", "predicted"],
        [
            ["second order", f"{e2:+.3e}", f"`r/ki` = {r / second.ki:+.3e}"],
            ["third order", f"{e3:+.3e}", "0"],
        ],
    )

    # 2.5 transients --------------------------------------------------
    R.md("### 2.5 The price: a slower, larger transient to a frequency step")
    R.md()
    R.md(
        "The phase error to a frequency step of one cycle per update, next "
        "to the second-order loop at the same `bn` (zeta 0.707): the peak "
        "error, and the updates until it stays within 5% of that peak."
    )
    R.md()
    rows, d.trans = [], {}
    for bnt in (0.01, 0.02, 0.04, 0.08):
        n = int(60 / bnt) + 2000
        p3, s3 = step_response(StateFilter(bn=bnt), n)
        p2, s2 = step_response(LoopFilter(bn=bnt, zeta=0.707, t=1.0), n)
        d.trans[bnt] = (p3, s3, p2, s2)
        rows.append([bnt, p3, s3, p2, s2, s3 / s2])
    R.table(
        [
            "`bn*t`",
            "peak (3rd)",
            "settle (3rd)",
            "peak (2nd)",
            "settle (2nd)",
            "ratio",
        ],
        [
            [
                f"{a:g}",
                f"{b:.2f}",
                f"{int(c)}",
                f"{e:.2f}",
                f"{int(g)}",
                f"{h:.2f}",
            ]
            for a, b, c, e, g, h in rows
        ],
    )
    _csv(
        DATA / "transients.csv",
        "bnt,peak3,settle3,peak2,settle2,ratio",
        rows,
    )

    # 2.6 domain and state -------------------------------------------
    R.md("### 2.6 The domain and the state (C §1, §9)")
    R.md()
    refused = []
    for bn_, t_ in (
        (-0.01, 1.0),
        (0.01, 0.0),
        (float("nan"), 1.0),
        (0.01, float("inf")),
        (BNT_MAX * 1.01, 1.0),
        (1e-31, 1.0),
    ):
        try:
            StateFilter(bn=bn_, t=t_)
            refused.append(False)
        except ValueError:
            refused.append(True)
    d.refusals = refused
    # the minimal pair: the same call with the offending argument made good
    d.refusal_pair = bool(StateFilter(bn=0.02, t=1.0).bn == 0.02)
    x = np.sin(0.05 * np.arange(300)) * 0.1
    whole = StateFilter(bn=0.03)
    want = whole.steps(x)
    a = StateFilter(bn=0.03)
    a.steps(x[:120])
    b = StateFilter(bn=0.03)
    b.set_state(a.get_state())
    d.split_exact = bool(np.array_equal(b.steps(x[120:]), want[120:]))
    # control: the same resume from a state whose frequency estimate was
    # moved does not equal the uninterrupted run
    moved = StateFilter(bn=0.03)
    moved.set_state(a.get_state())
    moved.integ = moved.integ + 1e-3
    d.split_control = not np.array_equal(moved.steps(x[120:]), want[120:])
    bad = bytearray(a.get_state())
    bad[len(bad) - (8 + 7 * 8)] = 1  # the flavor word
    try:
        b.set_state(bytes(bad))
        d.flavor_rejected = False
    except ValueError:
        d.flavor_rejected = True
    # the minimal pair: the unmodified blob is accepted
    d.flavor_pair = True
    try:
        b.set_state(a.get_state())
    except ValueError:
        d.flavor_pair = False
    one = StateFilter(bn=0.03)
    d.steps_equal = bool(
        np.allclose([one.step(e) for e in x], want, rtol=1e-12, atol=1e-15)
    )
    # control: a one-sample-shifted input is not what steps() computed
    shifted = StateFilter(bn=0.03)
    d.steps_control = not np.allclose(
        [shifted.step(e) for e in np.roll(x, 1)], want, rtol=1e-12, atol=1e-15
    )
    held = StateFilter(bn=0.03)
    held.steps(x[:50])
    integ, rate, kp = held.integ, held.rate, held.kp
    held.configure(0.06, 1.0)
    d.configure_keeps = bool(
        (held.integ, held.rate) == (integ, rate) and held.kp > kp
    )
    d.reset_before = bool((held.integ, held.rate) != (0.0, 0.0))
    d.configure_after_reset = bool((held.integ, held.rate) == (integ, rate))
    held.reset()
    d.reset_zeroes = bool((held.integ, held.rate) == (0.0, 0.0))
    d.configure_control = bool((held.integ, held.rate) == (integ, rate))
    seeded = StateFilter(bn=0.02)
    seeded.integ, seeded.rate = 0.5, -0.01
    fresh = StateFilter(bn=0.02)
    d.seed_moves = bool(seeded.step(0.2) != fresh.step(0.2))
    unseeded = StateFilter(bn=0.02)
    unseeded.integ, unseeded.rate = 0.0, 0.0
    d.seed_control = bool(unseeded.step(0.2) != StateFilter(bn=0.02).step(0.2))
    d.slower = all(s3 > s2 for _, s3, _, s2 in d.trans.values())
    d.slower_control = all(s2 > s3 for _, s3, _, s2 in d.trans.values())
    R.md(
        f"`StateFilter(...)` refuses a negative `bn`, `t = 0`, a NaN, an "
        f"infinite `t`, a `bn*t` past the bound and a `bn*t` below the floor "
        f"(`ValueError`): {sum(refused)} of {len(refused)} refused. A "
        f"mid-stream split resumes bit for bit: {d.split_exact}. A blob "
        f"whose flavor word is not 0 is refused: {d.flavor_rejected}."
    )
    R.md()
    # 2.7 which phase -------------------------------------------------
    R.md("### 2.7 Which phase the bandwidth is of")
    R.md()
    R.md(
        "`bandwidth()` is the bandwidth of the oscillator phase at the "
        "detector instant, `phi[n]`, the phase the error is formed from. An "
        "embedder that applies the proportional term as an immediate phase "
        "step and reads the phase just after it (Costas does) is the same "
        "loop, but that tap's sum of squares is a different, smaller number: "
        "a jitter measured there is held to that sum, not to `2*Bn`. The "
        "ratio `(2*Bn)/(sum post^2)` for both orders, by the impulse "
        "response of the real filters through the binding (raw rows in "
        "`data/phase_taps.csv`):"
    )
    R.md()
    rows7 = []
    for bn_ in (0.005, 0.01, 0.02, 0.04):
        out = [bn_]
        for loop in (LoopFilter(bn=bn_, zeta=0.707), StateFilter(bn=bn_)):
            det, post = phase_taps(loop)
            out += [det, det / post]
        rows7.append(out)
    d.tap = rows7
    R.table(
        ["bn*t", "order 2: 2*Bn", "ratio", "order 3: 2*Bn", "ratio"],
        [
            [f"{b:g}", f"{d2:.6f}", f"{r2:.4f}", f"{d3:.6f}", f"{r3:.4f}"]
            for b, d2, r2, d3, r3 in rows7
        ],
    )
    R.md()
    _csv(
        DATA / "phase_taps.csv",
        "bnt,det2,ratio2,det3,ratio3",
        rows7,
    )
    d.tap3_det = max(abs(r[3] / (2 * r[0]) - 1.0) for r in rows7)
    d.tap_ratios = [r[2] for r in rows7] + [r[4] for r in rows7]

    return d


# ── 3. review ────────────────────────────────────────────────────────
def review(d: SimpleNamespace) -> None:
    R.md("## 3. Review — findings, with verdicts")
    R.md()
    R.find(
        "bandwidth_is_the_one_asked",
        "BY DESIGN",
        "**The delivered bandwidth is `bn*t` exactly at a detector delay of "
        f"zero**, to {d.bw0_max_rel:.0e} relative across `bn*t` from 0.002 "
        "to 0.12. The second-order filter is wide by an excess that grows "
        "with `bn*t`; the gains here are solved from the exact discrete "
        "expression, so there is nothing to correct for (§2.1).",
    )
    R.find(
        "a_delay_widens_the_loop",
        "BY DESIGN",
        "**A detector delay widens the loop, and the object does not know "
        "it.** At `bn*t = 0.04` one update of delay delivers "
        f"{d.bw_delay[(0.04, 1)]:.3f}× the bandwidth asked for and two "
        f"deliver {d.bw_delay[(0.04, 2)]:.3f}×; at the bound, "
        f"{d.bw_delay[(0.12, 1)]:.2f}× and {d.bw_delay[(0.12, 2)]:.2f}×. "
        "The delay is the embedder's to budget, and `bandwidth(delay, gain)` "
        "is the exact expression to size it from (§2.2).",
    )
    R.find(
        "conditionally_stable",
        "BY DESIGN",
        "**The loop is conditionally stable in the loop gain**: it needs a "
        "discriminator slope above a lower edge near `1/(a*b) = 0.25` of "
        "nominal and has an upper edge that falls with `bn*t` and delay. "
        f"At the bound the worst lower edge over delays 0 to 2 is "
        f"{d.k_low_bound:.3f}, {d.k_low_margin_db:.1f} dB under "
        f"`K_MIN = {K_MIN}`. A second-order loop has no lower edge; an "
        "embedder whose discriminator slope compresses at low "
        "signal-to-noise ratio must check it against this one (§2.3).",
    )
    t = d.trans
    R.find(
        "a_slower_step_response",
        "BY DESIGN",
        "**Nulling the ramp costs the step response.** At the same `bn` the "
        f"third-order loop settles to a frequency step in "
        f"{t[0.01][1] / t[0.01][3]:.1f}× the updates of the second-order "
        f"loop at `bn*t = 0.01` ({t[0.04][1] / t[0.04][3]:.1f}× at 0.04) and "
        "its peak error is larger. That is the price of the rate state; a "
        "caller whose carrier does not ramp should keep the second-order "
        "loop (§2.5).",
    )
    R.find(
        "exact_function_accuracy_c_only",
        "C-ONLY",
        "**The exact function's accuracy against the closed form down to "
        "`bn*t = 1e-6`, and the noise-variance route to the bandwidth** are "
        "certified in C (`test_state_filter_core.c` §3b, "
        "`state_filter_loop.c`): the binding reaches the function at the "
        "cells of §2.2 only.",
    )
    R.find(
        "composition_not_certified",
        "GAP",
        "**The filter is certified as an object, not in a closed loop with a "
        "discriminator.** Pull-in from a frequency offset, cycle slips under "
        "noise and a slope that compresses at low signal-to-noise ratio "
        "belong to the composition that adopts it, and a gallery figure of "
        "the ramp traces is not drawn. Tracked in "
        "[#3123](https://github.com/doppler-dsp/doppler/issues/3123).",
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
        "*Checked by* names the check; *control* is the same assertion "
        "evaluated on a named wrong case, which this report requires to "
        "FAIL (a refusal's control is its minimal pair: the same call with "
        "the offending argument made good must succeed). *Break* is a "
        "one-line break of the source run against the claim: not run for "
        "this report yet, so the column says so. The numeric tolerances are "
        "derived where a derivation exists (rounding of a sum of `n` terms, "
        "`4 * eps * sqrt(n)`, or of the ramp's phase accumulator) and the "
        "rest say they are a pin."
    )
    R.md()
    bw_tol = rounding_bound(d.bw0_n_max)
    sim_tol = rounding_bound(60000)
    _r, _bn, e2, pred, e3 = d.ramp
    ratio_ctl = min(d.tap_ratios) - 1.0
    NR = "not run"
    R.limit(
        d.bw0_max_rel < bw_tol and d.bw0_control > bw_tol,
        f"the delivered bandwidth at zero delay is `bn*t` to "
        f"{d.bw0_max_rel:.1e} across bn*t 0.002 to 0.12, inside the "
        f"rounding bound {bw_tol:.1e} of a sum of up to {d.bw0_n_max} terms",
        by="validator §2.1, Parseval through the binding; C §3",
        control=f"the same check at a detector delay of one update "
        f"({d.bw0_control:.1e}, far over the bound)",
        red=NR,
    )
    R.limit(
        d.exact_max_rel < sim_tol and d.exact_control > sim_tol,
        f"the exact `bandwidth(delay, gain)` agrees with simulation at "
        f"integer delays 0 to 2 and gains 1.0 and 0.7 to "
        f"{d.exact_max_rel:.1e}, inside the rounding bound {sim_tol:.1e} of "
        f"a sum of 60000 terms",
        by="validator §2.2; C §6",
        control=f"the exact function at delay 1 against the simulation at "
        f"delay 0 ({d.exact_control:.1e})",
        red=NR,
    )
    R.limit(
        d.delay_widens and not d.delay_widens_control,
        "a longer detector delay delivers a wider bandwidth, at every bn*t "
        "measured",
        by="validator §2.2",
        control="the same comparison with the delays reversed is false",
        red=NR,
    )
    R.limit(
        d.rect_unstable == 0 and d.rect_control > 0,
        f"at the bound bn*t = {BNT_MAX} the loop is stable over the "
        f"advertised rectangle (gain {K_MIN} to {K_MAX}, delay 0 to "
        f"{DELAY_MAX:g}): {d.rect_unstable} unstable grid points",
        by="validator §2.3 (41 delays by 15 gains); C §7",
        control=f"the same grid at gains 5 to 10 times nominal has "
        f"{d.rect_control} unstable points",
        red=NR,
    )
    R.limit(
        d.changes_max <= 2,
        "the stable gains at every measured (bn*t, delay) form one interval "
        f"(at most {d.changes_max} changes of stability along K)",
        by="validator §2.3, a log sweep of K",
        control="none: the set is an interval at every point measured, so "
        "no wrong case is available in this family",
        red=NR,
    )
    R.limit(
        d.k_low_bound < K_MIN and not d.k_low_bound < 0.25,
        f"the lower gain edge at the bound is {d.k_low_bound:.3f}, under "
        f"K_MIN = {K_MIN} ({d.k_low_margin_db:.1f} dB of margin)",
        by="validator §2.3; C §7 asserts it at every delay to 2",
        control="the same assertion with the floor at 0.25 (the shape's own "
        "lower edge) is false",
        red=NR,
    )
    R.limit(
        d.bound_tight and not (K_MAX <= d.tight_control < 1.05 * K_MAX),
        f"pin: the bound is tight: the upper edge at the worst delay is "
        f"{d.k_high_corner:.3f}, within 5% of K_MAX = {K_MAX} (the exact "
        f"bound is 0.1248 and the constant 0.12; 5% is a pin set above the "
        f"measured 4.2%)",
        by="validator §2.3; C §7 derives the bound by bisection",
        control=f"at bn*t 0.06 the upper edge is {d.tight_control:.2f}, not "
        f"within 5%",
        red=NR,
    )
    R.limit(
        abs(e2 / pred - 1.0) < d.ramp_round and d.ramp_control > d.ramp_round,
        f"the second-order ramp error is r/ki to {abs(e2 / pred - 1):.1e}, "
        f"inside the rounding {d.ramp_round:.1e} of the ramp's phase "
        f"accumulator",
        by="validator §2.4; C §8",
        control=f"r/ki of a loop at twice the bandwidth is "
        f"{d.ramp_control:.1e} off",
        red=NR,
    )
    R.limit(
        abs(e3) < d.ramp_abs and abs(e2) > 100 * abs(e3),
        f"the third-order loop nulls the ramp: {abs(e3):.1e} against "
        f"{abs(e2):.1e} for the second order, inside the accumulator's "
        f"rounding",
        by="validator §2.4; C §8",
        control="the second-order loop's error on the same ramp is not null",
        red=NR,
    )
    R.limit(
        all(d.refusals) and d.refusal_pair,
        "create refuses a negative bn, t = 0, a NaN, an infinite t, a bn*t "
        "past the bound and one below the floor",
        by="validator §2.6; C §1",
        control="the minimal pair: `StateFilter(bn=0.02, t=1.0)` is built",
        red=NR,
    )
    R.limit(
        d.split_exact and d.split_control,
        "a mid-stream split resumes bit for bit",
        by="validator §2.6; C §9",
        control="resuming from a state whose frequency estimate was moved "
        "is not the uninterrupted run",
        red=NR,
    )
    R.limit(
        d.steps_equal and d.steps_control,
        "steps() equals repeated step()",
        by="validator §2.6; C §5",
        control="a one-sample-shifted input is not what steps() computed",
        red=NR,
    )
    R.limit(
        d.configure_keeps and d.configure_control is False,
        "configure retunes the gains and keeps both estimates",
        by="validator §2.6; C §4",
        control="after reset() the estimates no longer equal the held ones",
        red=NR,
    )
    R.limit(
        d.reset_zeroes and d.reset_before,
        "reset zeroes both estimates",
        by="validator §2.6; C §4",
        control="before reset() the estimates are not zero",
        red=NR,
    )
    R.limit(
        d.seed_moves and not d.seed_control,
        "the writable integ and rate seed the loop",
        by="validator §2.6",
        control="seeding zeros does not move the loop",
        red=NR,
    )
    R.limit(
        d.slower and not d.slower_control,
        "the third-order step response is slower than the second-order's "
        "at the same bn, at every bn*t measured (the price of the rate state)",
        by="validator §2.5",
        control="the reverse comparison is false at every bn*t",
        red=NR,
    )
    R.limit(
        d.flavor_rejected and d.flavor_pair,
        "a blob with a flavor word other than 0 is refused",
        by="validator §2.6; C §9",
        control="the minimal pair: the unmodified blob is accepted",
        red=NR,
    )
    R.limit(
        d.tap3_det < sim_tol and ratio_ctl > sim_tol,
        f"the sum of squares of the phase at the detector instant is "
        f"`2*Bn` to {d.tap3_det:.1e} for the third-order loop, inside the "
        f"rounding bound {sim_tol:.1e} of a sum of 60000 terms",
        by="validator §2.7",
        control=f"the phase just after the proportional step is "
        f"{ratio_ctl:.1e} off `2*Bn`",
        red=NR,
    )
    R.limit(
        all(1.0 < r < 1.15 for r in d.tap_ratios),
        "pin: the phase just after the proportional step has a sum of "
        f"squares below `2*Bn` by a factor between {min(d.tap_ratios):.3f} "
        f"and {max(d.tap_ratios):.3f} at bn*t 0.005 to 0.04, both orders "
        "(1.15 allowed: a pin set from the measurement)",
        by="validator §2.7",
        control="the detector-instant ratio (1.000) is not in the open "
        "interval above 1",
        red=NR,
    )


def plots(d: SimpleNamespace) -> None:
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    # the stable-gain map
    fig, ax = plt.subplots(figsize=(7.2, 4.4))
    for delay, ls in ((0.0, "-"), (1.0, "--"), (2.0, ":")):
        lo = [d.edges[(b, delay)][0] for b in BNTS]
        hi = [d.edges[(b, delay)][1] for b in BNTS]
        ax.loglog(BNTS, lo, ls, c="C0", label=f"K_low, delay {delay:g}")
        ax.loglog(BNTS, hi, ls, c="C3", label=f"K_high, delay {delay:g}")
    ax.fill_between(
        [BNTS[0], BNT_MAX],
        K_MIN,
        K_MAX,
        color="C2",
        alpha=0.18,
        label="the advertised rectangle",
    )
    ax.axvline(BNT_MAX, c="k", lw=1)
    ax.set_xlabel("loop noise bandwidth × update period  bn·t")
    ax.set_ylabel("loop gain K, a multiple of nominal")
    ax.set_title("The stable-gain interval, and the rectangle inside it")
    ax.grid(alpha=0.3, which="both")
    ax.legend(fontsize=7, ncol=2)
    fig.tight_layout()
    fig.savefig(HERE / "stability_map.png", dpi=110)
    plt.close(fig)

    # the ramp traces
    r = 2e-7
    fig, ax = plt.subplots(figsize=(7.2, 4.0))
    for name, loop in (
        ("second order", LoopFilter(bn=0.02, zeta=0.707, t=1.0)),
        ("third order", StateFilter(bn=0.02)),
    ):
        f = w = phi = 0.0
        e = np.empty(6000)
        for i in range(6000):
            f += r
            w += f
            e[i] = w - phi
            phi += loop.step(e[i])
        ax.plot(e, label=name)
    ax.set_xlabel("update")
    ax.set_ylabel("phase error (cycles)")
    ax.set_title("A frequency ramp at bn = 0.02: the rate state nulls it")
    ax.grid(alpha=0.3)
    ax.legend()
    fig.tight_layout()
    fig.savefig(HERE / "ramp.png", dpi=110)
    plt.close(fig)


# ── build ────────────────────────────────────────────────────────────
def build(write: bool = True) -> Report:
    global R
    R = Report(write=write)
    R.md("# StateFilter — validation report")
    R.md()
    section_object()
    d = characterise()
    review(d)
    limits(d)
    R.executive(
        "StateFilter",
        [
            "**The bandwidth asked for is the one delivered**, with no "
            "excess to correct for, because the gains are solved from the "
            "exact discrete expression "
            f"({R.ref('bandwidth_is_the_one_asked')}, §2.1).",
            "**A third-order loop nulls a frequency ramp** at the same noise "
            "bandwidth where the second-order loop settles to `r/ki` (§2.4).",
            "**It pays in the step response**: it settles to a frequency "
            "step several times slower than the second-order loop at the "
            f"same `bn` ({R.ref('a_slower_step_response')}, §2.5).",
            "**It is conditionally stable in the loop gain**: a "
            "discriminator slope under about a quarter of nominal breaks "
            "it, and the object cannot see the slope "
            f"({R.ref('conditionally_stable')}, §2.3).",
            "**A detector delay widens the loop, and the embedder budgets "
            "it**: `bandwidth(delay, gain)` is the exact expression "
            f"({R.ref('a_delay_widens_the_loop')}, §2.2).",
        ],
    )
    if write:
        plots(d)
    R.summary(
        "\n- Raw sweeps: `data/bandwidth.csv`, `data/stability_map.csv`, "
        "`data/transients.csv`, `data/phase_taps.csv`"
    )
    R.emit(HERE / "results.md")
    return R


if __name__ == "__main__":
    sys.exit(cli(build, HERE))
