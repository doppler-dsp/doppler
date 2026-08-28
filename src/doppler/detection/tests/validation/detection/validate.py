"""Certify `doppler.detection` — the sizing helpers every detector shares.

Run:  python -m doppler.detection.tests.validation.detection.validate
      make validate          (regenerates every report)
      make validate-check    (fails if the committed report is stale)

The object is 19 pure functions across five statistical families
(`docs/design/detection.md` §2), all stateless, all bound. So this report has
no streams, no lock, no state round-trip — every measurement is either an
exact identity between two shipped functions, or a frequency measured against
a probability the module priced.

**Where the heavy measurement lives.** The models are checked against
Monte-Carlo, and resolving a 1e-4 tail needs millions of draws, which is not a
per-push cost. That sweep is the characterization subject
`models/`; this validator re-runs a coarse slice of it — enough to
prove each family is wired to the law it claims, not enough to see the tails.
The split is deliberate and the report says which numbers came from which.
"""

from __future__ import annotations

import sys
from dataclasses import dataclass, field
from pathlib import Path

import numpy as np

from doppler.detection import (
    det_dwell,
    det_dwell_gauss,
    det_dwell_power,
    det_ema_alpha,
    det_n_noncoh,
    det_pd,
    det_pd_noncoherent,
    det_pd_power,
    det_q_inv,
    det_snr,
    det_snr_power,
    det_threshold,
    det_threshold_f,
    det_threshold_gauss,
    det_threshold_noncoherent,
    det_threshold_power,
    det_verify_count,
    det_verify_delay,
    marcum_q,
)
from doppler.tests._validation_common import Report, cli

HERE = Path(__file__).resolve().parent
R = Report()

#: Draws for the coarse H0 slice. Resolves pfa=1e-2 to ~3 % relative, which
#: is enough to tell the five families apart (their thresholds differ by more
#: than 10 % at this pfa) and nowhere near enough to see a tail. The tails are
#: the characterization subject's job, and saying so is why this constant has
#: a comment rather than a bigger value.
N_DRAWS = 200_000
SEED = 20260824

PFAS = (1e-2, 1e-3, 1e-4, 1e-6)
LOOKS = (1, 2, 4, 8, 16)
DWELLS = (1, 4, 16, 64)


@dataclass
class Data:
    """Everything §3 and §4 read, measured once in §2."""

    h0_rows: list[list[str]] = field(default_factory=list)
    h0_worst_sigma: float = 0.0
    thr_growth_ok: bool = False
    thr_growth_rows: list[list[str]] = field(default_factory=list)
    f_rows: list[list[str]] = field(default_factory=list)
    f_ratio_16: float = 0.0
    f_ratio_monotone: bool = False
    est_quality_rows: list[list[str]] = field(default_factory=list)
    est_rel_16: float = 0.0
    sens_db_16: float = 0.0
    sens_db_halves: bool = False
    budget_rows: list[list[str]] = field(default_factory=list)
    budget_100_1e3: float = 0.0
    power_exact: bool = False
    power_worst: float = 0.0
    noncoh_reduces: bool = False
    inverse_worst: float = 0.0
    minimal_ok: bool = False
    bigrun_monotone_to: int = 0
    fail_closed_ok: bool = False
    verify_conservative: bool = False
    verify_gap: float = 0.0
    marcum_envelope_worst_sigma: float = 0.0
    thr_roundtrip_worst: float = 0.0
    power_thr_worst: float = 0.0
    q_inv_signed: bool = False
    gauss_scaling_ok: bool = False
    ema_worst: float = 0.0
    pd_is_pfa_worst: float = 0.0
    quadruple_db_worst: float = 0.0
    marcum_special_ok: bool = False


def _rng(tag: int) -> np.random.Generator:
    return np.random.default_rng(SEED + tag)


def _se(p_hat: float, n: int) -> float:
    return float(np.sqrt(max(p_hat, 1.0 / n) * (1.0 - p_hat) / n))


def _csv(path: Path, header: str, rows: list[list[float]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as fh:
        fh.write(header + "\n")
        for r in rows:
            fh.write(",".join(f"{v:.10g}" for v in r) + "\n")


# ── 1. the object ─────────────────────────────────────────────────────


def section_object() -> None:
    R.md("## 1. The object — five laws behind one prefix")
    R.md()
    R.md(
        "`doppler.detection`'s `det_*` helpers turn a statistic's null "
        "distribution into a threshold and an integration time. They are "
        "stateless, thread-safe, and shared by every object in the library "
        "that declares anything — `Acquisition`, `LockDet`, `Dll`, "
        "`BurstDespreader`, `BerMeter`. A change here moves all of them at "
        "once, which is the reason this report exists."
    )
    R.md()
    R.md("Neither of these is restated here:")
    R.md()
    R.table(
        ["page", "owns"],
        [
            [
                "[`docs/design/detection.md`](../../../../../../docs/design/detection.md)",
                "why the five families exist, why they must not be crossed, "
                "and what each errs toward",
            ],
            [
                "`native/inc/detection/detection_core.h`",
                "the contract per function — the SSOT this report audits",
            ],
        ],
    )
    R.md("### 1.1 The claim inventory")
    R.md()
    R.md(
        "Step 1 of `docs/dev/contributing/validation.md`: every prose claim "
        "the header makes, and where it is pinned. `test_detection_core.c` "
        "is the C test; `test_detection.py` is the Python one. Three rows "
        "were **absent** in C when this certification began, and all three "
        "are load-bearing for `acq`."
    )
    R.md()
    R.table(
        ["header claim", "pinned where", "here"],
        [
            [
                "`Pfa = exp(-eta^2/2)` inverts exactly",
                "C, round-trip at 3 pfa values",
                "—",
            ],
            [
                "`Pd = Q_1(sqrt(2M)*snr, eta)`; `snr=0` gives Pfa",
                "C + Python",
                "§2.2",
            ],
            [
                "`det_dwell`/`det_snr` invert `det_pd`, minimally",
                "C, incl. the value one below failing",
                "§2.5",
            ],
            [
                "the non-coherent trio reduces to the coherent one at "
                "`n_noncoh = 1`",
                "**was Python only** — now C too",
                "§2.3",
            ],
            [
                "`det_threshold_noncoherent` solves "
                "`marcum_q(n, 0, eta) = pfa`",
                "**was Python only** — now C too",
                "§2.3",
            ],
            [
                "`det_n_noncoh` re-derives the threshold each step",
                "**was Python only** — now C too",
                "§2.3",
            ],
            [
                "the chi-square gate realizes 41x the priced pfa at n=16",
                "**was nowhere** — now C, and Monte-Carlo",
                "§2.4",
            ],
            [
                "the series converges across `a, b <= 15`",
                "**was nowhere** — now C, vs Monte-Carlo",
                "§2.6",
            ],
            [
                "envelope and power are the same detector",
                "C, at 4 grid points",
                "§2.2",
            ],
            [
                "`det_verify_count` is a conservative budget",
                "C, degenerate cases included",
                "§2.5",
            ],
            ["every helper fails closed on nonsense", "C", "§2.7"],
            [
                "the Pd model is unreliable past a few hundred looks "
                "(`acq_core.h`)",
                "nowhere",
                "§2.3 — **contradicted**, F5",
            ],
        ],
    )


# ── 2. characterisation ───────────────────────────────────────────────


def characterise() -> Data:
    d = Data()
    R.md("## 2. Characterisation")
    R.md()
    R.md("Measured behaviour. No verdicts — those are §3.")
    R.md()

    _sec_h0(d)
    _sec_coherent(d)
    _sec_noncoherent(d)
    _sec_fratio(d)
    _sec_inverses(d)
    _sec_marcum(d)
    _sec_identities(d)
    _sec_boundaries(d)
    return d


def _sec_h0(d: Data) -> None:
    R.md("### 2.1 Each family's threshold delivers the rate it was sold at")
    R.md()
    R.md(
        "Draw the null statistic each family claims, count exceedances of "
        "the threshold that family's helper returns, compare to the `pfa` "
        "asked for. `sigma` is the distance in standard errors of the "
        "measured frequency — under 3 the draw cannot tell the model from "
        "the truth. The 1e-6 row is reported and **not** asserted: at "
        f"{N_DRAWS:,} draws it expects 0.2 hits, so it is measuring the draw."
    )
    R.md()
    rows: list[list[str]] = []
    csv: list[list[float]] = []
    worst = 0.0
    tag = 0

    def cell(name: str, pfa: float, stat: np.ndarray, thr: float) -> None:
        nonlocal worst
        hit = float(np.mean(stat > thr))
        se = _se(hit, N_DRAWS)
        sig = abs(hit - pfa) / se
        if pfa >= 1e-4:
            worst = max(worst, sig)
        rows.append(
            [name, f"{pfa:.0e}", f"{thr:.4f}", f"{hit:.3e}", f"{sig:.1f}"]
        )
        csv.append([pfa, thr, hit, se, sig])

    for pfa in PFAS:
        r = _rng(tag := tag + 1)
        z = r.standard_normal(N_DRAWS) + 1j * r.standard_normal(N_DRAWS)
        cell("envelope · Rayleigh(1)", pfa, np.abs(z), det_threshold(pfa))

        r = _rng(tag := tag + 1)
        cell(
            "power · Exp(1)",
            pfa,
            r.exponential(1.0, N_DRAWS),
            det_threshold_power(pfa),
        )

        r = _rng(tag := tag + 1)
        cell(
            "gaussian · N(0,1)",
            pfa,
            r.standard_normal(N_DRAWS),
            det_q_inv(pfa),
        )

        r = _rng(tag := tag + 1)
        acc = np.zeros(N_DRAWS)
        for _ in range(4):
            z = r.standard_normal(N_DRAWS) + 1j * r.standard_normal(N_DRAWS)
            acc += np.abs(z) ** 2
        cell(
            "non-coherent · chi2(8)",
            pfa,
            np.sqrt(acc),
            det_threshold_noncoherent(pfa, 4),
        )

    R.table(["family", "pfa", "threshold", "measured", "sigma"], rows)
    _csv(
        HERE / "data" / "h0_rates.csv",
        "pfa,threshold,measured,stderr,sigma",
        csv,
    )
    d.h0_rows = rows
    d.h0_worst_sigma = worst
    R.md(
        f"Worst deviation over the resolved rows (pfa >= 1e-4): "
        f"**{worst:.1f} sigma**. Raw sweep: `data/h0_rates.csv`."
    )
    R.md()


def _sec_coherent(d: Data) -> None:
    R.md("### 2.2 Coherent depth is free — and power is the same detector")
    R.md()
    R.md(
        "Coherent integration raises the non-centrality to "
        "`a = sqrt(2M)*snr` and leaves the threshold alone: `det_threshold` "
        "depends on `pfa` and nothing else. Doubling `M` is a clean 3 dB, "
        "and it costs the detector nothing at the gate."
    )
    R.md()
    rows = []
    for dwell in DWELLS:
        snr = det_snr(dwell, 0.9, 1e-6)
        rows.append(
            [
                str(dwell),
                f"{snr:.4f}",
                f"{20 * np.log10(snr):+.2f}",
                f"{det_pd(snr, dwell, det_threshold(1e-6)):.4f}",
            ]
        )
    R.table(
        ["dwell", "snr for Pd=0.9", "dB", "Pd achieved"],
        rows,
    )
    R.md(
        "Each quadrupling of dwell buys 6.02 dB, exactly, because the "
        "non-centrality goes as `sqrt(M)` and nothing else moves."
    )
    R.md()

    worst = 0.0
    for pfa in PFAS:
        eta, p = det_threshold(pfa), det_threshold_power(pfa)
        for dwell in DWELLS:
            for snr in (0.1, 0.5, 1.0, 2.0):
                worst = max(
                    worst,
                    abs(
                        det_pd(snr, dwell, eta)
                        - det_pd_power(snr * snr, dwell, p)
                    ),
                )
    d.power_worst = worst
    d.power_exact = worst == 0.0
    R.md(
        f"The envelope and power families agree **exactly** — worst "
        f"`|det_pd - det_pd_power|` over a 64-point grid is `{worst:.1e}`, "
        f"not merely small. A power SNR `s` is an amplitude SNR `sqrt(s)` "
        f"and the Q_1 arguments are identical, so any difference at all "
        f"would mean one of them had grown its own arithmetic."
    )
    R.md()


def _sec_noncoherent(d: Data) -> None:
    R.md("### 2.3 Non-coherent looks are not free — the threshold moves")
    R.md()
    R.md(
        "Magnitude-squared accumulation survives the data-modulation sign "
        "flips a coherent sum cannot, but the H0 law widens from Rayleigh "
        "to chi2(2M) and the threshold grows with the look count. Some of "
        "the SNR bought is handed straight back, and this is why "
        "`det_n_noncoh` re-derives the threshold at every iteration rather "
        "than sizing against a fixed one."
    )
    R.md()
    rows = []
    prev = 0.0
    ok = True
    eta1 = det_threshold_noncoherent(1e-3, 1)
    for nc in LOOKS:
        eta = det_threshold_noncoherent(1e-3, nc)
        ok &= eta > prev
        rows.append(
            [
                str(nc),
                f"{eta:.4f}",
                f"{20 * np.log10(eta / eta1):+.2f}",
                f"{marcum_q(nc, 0.0, eta):.3e}",
            ]
        )
        prev = eta
    R.table(
        ["n_noncoh", "threshold", "dB vs 1 look", "marcum_q(n,0,eta)"],
        rows,
    )
    d.thr_growth_rows = rows
    d.thr_growth_ok = ok
    d.noncoh_reduces = det_threshold_noncoherent(1e-6, 1) == det_threshold(
        1e-6
    ) and det_pd_noncoherent(0.5, 8, 1, eta1) == det_pd(0.5, 8, eta1)
    R.md(
        "The last column is the threshold checked against the law it claims "
        "to solve, not against a literal: `marcum_q` is independently "
        "pinned, and every row returns the `pfa` that was asked for."
    )
    R.md()

    R.md("#### The model does not break where the tree says it does")
    R.md()
    R.md(
        '`acq_core.h` states twice that this model is *"non-monotonic and '
        'unreliable past a few hundred looks"*, and bounds its search at '
        "`ACQ_N_NONCOH_SAFETY_CEILING = 256` on that basis. Swept out to "
        "1024 looks at pfa=1e-3, n_coh=16, snr=0.15, both the threshold and "
        "Pd increase at **every** step:"
    )
    R.md()
    big = []
    prev_eta, prev_pd, first_bad = 0.0, -1.0, 0
    for nc in (1, 16, 128, 256, 512, 1024):
        eta = det_threshold_noncoherent(1e-3, nc)
        pd = det_pd_noncoherent(0.15, 16, nc, eta)
        if (eta <= prev_eta or pd <= prev_pd) and not first_bad:
            first_bad = nc
        big.append([str(nc), f"{eta:.4f}", f"{pd:.6f}"])
        prev_eta, prev_pd = eta, pd
    R.table(["n_noncoh", "threshold", "Pd"], big)
    d.bigrun_monotone_to = 0 if first_bad else 1024
    R.md(
        "First non-monotonic look count: **none**. The characterization "
        "subject takes the accuracy half to Monte-Carlo and finds agreement "
        "within 0.2-0.6 sigma out to 512 looks. Recorded as F5; the ceiling "
        "belongs to `acq`, so moving it is not this report's call."
    )
    R.md()


def _sec_fratio(d: Data) -> None:
    R.md("### 2.4 A threshold on an estimated reference, and how good it is")
    R.md()
    R.md(
        "Every family above prices a statistic normalised by a **known** "
        "noise power. `BurstDespreader` has one burst and estimates the "
        "noise from it — `sum Im^2` against `sum Re^2` — so its exact H0 "
        "law is `R^2 = n*F(n,n)`, whose tail is fatter."
    )
    R.md()
    R.md(
        "None of what follows is a defect. Estimating a reference from `n` "
        "samples means the threshold inherits that estimate's uncertainty; "
        "the only question worth asking is how much, and `det_threshold_f` "
        "exists because doppler already prices it. **How good the estimate "
        "is comes first, because everything else follows from it.** "
        "`sigma_hat^2 = sum Im^2 / n` is unbiased but not sharp: it is "
        "`sigma^2 * chi2(n)/n`, so its relative standard deviation is "
        "`sqrt(2/n)`."
    )
    R.md()
    q_rows = []
    for n in (4, 8, 16, 32, 64, 128):
        rel = float(np.sqrt(2.0 / n))
        q_rows.append(
            [str(n), f"{rel * 100:.1f}%", f"{10 * np.log10(1 + rel):.2f}"]
        )
        if n == 16:
            d.est_rel_16 = rel
    R.table(
        ["n", "1-sigma relative error on the noise estimate", "dB"], q_rows
    )
    d.est_quality_rows = q_rows
    R.md(
        f"At the sixteen prompts a short burst affords, the floor is known "
        f"to about {d.est_rel_16 * 100:.0f}% (1 sigma). Both numbers below "
        f"follow from that one."
    )
    R.md()
    R.md(
        "Because a ratio of two estimates is not a ratio to a constant, "
        "the difference lands in one of two places depending on which gate "
        "is used — and they are different quantities, which is why "
        '"costs 41x" is not a usable sentence. **Using the known-noise '
        "gate, the false-alarm rate moves.** `realized` is what that gate "
        "delivers, found by inverting `det_threshold_f` on its own "
        "quantile."
    )
    R.md()

    def realized(g: float, n: int) -> float:
        lo, hi = 1e-15, 0.9999
        for _ in range(200):
            mid = float(np.sqrt(lo * hi))
            if det_threshold_f(mid, n) > g:
                lo = mid
            else:
                hi = mid
        return float(np.sqrt(lo * hi))

    pfa = 1e-3
    rows, csv, ratios = [], [], []
    for n in (4, 8, 16, 32, 64):
        # Even n only: n // 2 would price chi2(n-1) otherwise. Asserted so
        # a later edit to this tuple cannot reintroduce the approximation.
        assert n % 2 == 0, f"the chi-square comparator needs even n, got {n}"
        eta = det_threshold_noncoherent(pfa, n // 2)
        g = eta * eta / n
        rp = realized(g, n)
        ratios.append(rp / pfa)
        rows.append(
            [
                str(n),
                f"{g:.4f}",
                f"{det_threshold_f(pfa, n):.4f}",
                f"{rp:.2e}",
                f"{rp / pfa:.1f}x",
            ]
        )
        csv.append([n, g, det_threshold_f(pfa, n), rp, rp / pfa])
    R.table(
        [
            "n",
            "chi2 gate (F units)",
            "correct F gate",
            "realized pfa",
            "ratio",
        ],
        rows,
    )
    _csv(
        HERE / "data" / "fratio_penalty.csv",
        "n,chi2_gate,f_gate,realized_pfa,ratio",
        csv,
    )
    d.f_rows = rows
    d.f_ratio_16 = ratios[2]
    d.f_ratio_monotone = all(a > b for a, b in zip(ratios, ratios[1:]))
    R.md(
        f"41 is a multiplier on the false-alarm **rate**, not a cost — "
        f"quoted as a cost it names no unit. It shrinks monotonically as "
        f"the estimate hardens ({ratios[0]:.0f}x to {ratios[-1]:.0f}x)."
    )
    R.md()
    R.md(
        "**Using `det_threshold_f`, the sensitivity moves instead.** Keep "
        "the rate you asked for and the same information shortfall lands "
        "in the threshold, in dB. This is the number a link budget uses."
    )
    R.md()
    srows, sdb = [], []
    for n in (4, 8, 16, 32, 64, 128):
        eta = det_threshold_noncoherent(pfa, n // 2)
        g_known = eta * eta / n
        g_f = det_threshold_f(pfa, n)
        db = float(10 * np.log10(g_f / g_known))
        sdb.append(db)
        srows.append([str(n), f"{g_known:.4f}", f"{g_f:.4f}", f"{db:.2f} dB"])
        if n == 16:
            d.sens_db_16 = db
    R.table(
        ["n", "known-noise gate", "correct F gate", "sensitivity given up"],
        srows,
    )
    # Doubling n tightens sqrt(2/n) by sqrt(2) and roughly halves the dB
    # cost -- the two prices are one quantity seen from two sides.
    d.sens_db_halves = all(0.4 < b / a < 0.75 for a, b in zip(sdb, sdb[1:]))
    R.md(
        "Both columns are `sqrt(2/n)` seen from two sides, which is why "
        "they fall together: doubling the prompts folded into the "
        "reference tightens the estimate by `sqrt(2)` and roughly halves "
        "the dB cost."
    )
    R.md()
    R.md("#### The number to budget")
    R.md()
    R.md(
        "The useful form of all of the above, indexed by the thing a "
        "caller actually knows — how many samples they can spare for the "
        "noise reference. Read off the row and add that much margin to "
        "the link budget."
    )
    R.md()
    brows = []
    for n in (8, 16, 32, 64, 100, 128, 256, 512, 1024):
        ne = n if n % 2 == 0 else n - 1
        cells = []
        for pf in (1e-2, 1e-3, 1e-6):
            eta = det_threshold_noncoherent(pf, ne // 2)
            db = float(
                10 * np.log10(det_threshold_f(pf, ne) / (eta * eta / ne))
            )
            cells.append(f"{db:.2f} dB")
            if n == 100 and pf == 1e-3:
                d.budget_100_1e3 = db
        brows.append([str(n), f"{np.sqrt(2.0 / n) * 100:.1f}%", *cells])
    R.table(
        [
            "samples in the noise estimate",
            "reference known to",
            "budget at pfa 1e-2",
            "1e-3",
            "1e-6",
        ],
        brows,
    )
    d.budget_rows = brows
    R.md(
        f"So: 100 samples of noise reference at `pfa = 1e-3` is "
        f"**{d.budget_100_1e3:.2f} dB** of extra margin, and a burst that "
        f"can only spare 16 pays {d.sens_db_16:.2f} dB. The penalty falls "
        f"faster than `1/sqrt(n)` at small `n` and approaches it from "
        f"above, so a fitted rule of thumb misleads exactly where the cost "
        f"is largest — read the row rather than scaling one."
    )
    R.md()
    R.md(
        "The tighter false-alarm budget is the expensive one: at "
        "`pfa = 1e-6` a 16-sample reference costs 6.15 dB against 2.27 dB "
        "at `pfa = 1e-2`, because the gate sits further into a tail whose "
        "shape is exactly what the estimate is uncertain about."
    )
    R.md()
    R.md(
        "**The comparator is derived, not fitted.** Under H0 a burst's `n` "
        "prompts give `sum Re^2 ~ s^2*chi2(n)` and `sum Im^2 ~ "
        "s^2*chi2(n)`, so `R^2 = n*F(n,n)`. A caller treating `sum Im^2/n` "
        "as exactly `s^2` believes `R^2 ~ chi2(n)` instead and gates at "
        "that quantile; `det_threshold_noncoherent(pfa, M)` solves "
        "`marcum_q(M,0,b) = pfa`, which is `P(chi2(2M) > b^2)`, so `2M = "
        "n` and the comparator is `M = n/2`. Pricing it at `(pfa, n)` "
        "gives 4.8x, which is equally plausible on sight — worth deriving "
        "rather than guessing."
    )
    R.md()
    R.md(
        "**Even `n` only, and the loop asserts it rather than relying on "
        "the sweep.** `n/2` is integer division and doppler cannot price a "
        "known-noise gate at odd degrees of freedom at all, so at odd `n` "
        "this returns the `chi2(n-1)` gate — the same value as `n-1` — and "
        "overstates the penalty by about a fifth (89x against a true 73.7x "
        "at n = 5). `det_threshold_f` has no such restriction, deliberately: "
        "a burst's prompt count is whatever the burst contained. Raw sweep: "
        "`data/fratio_penalty.csv`."
    )
    R.md()


def _sec_inverses(d: Data) -> None:
    R.md("### 2.5 What inverts exactly, and what is a deliberate budget")
    R.md()
    worst = 0.0
    rows = []
    for dwell in (1, 4, 16, 64):
        for pd_min in (0.5, 0.9, 0.99):
            snr = det_snr(dwell, pd_min, 1e-6)
            back = det_pd(snr, dwell, det_threshold(1e-6))
            worst = max(worst, abs(back - pd_min))
            sp = det_snr_power(dwell, pd_min, 1e-6)
            rows.append(
                [
                    str(dwell),
                    f"{pd_min:g}",
                    f"{snr:.4f}",
                    f"{back - pd_min:+.1e}",
                    f"{abs(sp - snr * snr):.1e}",
                ]
            )
    R.table(
        [
            "dwell",
            "Pd asked",
            "snr returned",
            "Pd(snr) - asked",
            "power/amplitude gap",
        ],
        rows,
    )
    d.inverse_worst = worst
    R.md(
        f"`det_snr` inverts `det_pd` to **{worst:.1e}** across the grid, and "
        f"`det_snr_power` is its square to the same precision — a bisection "
        f"to 64 iterations, not a fit."
    )
    R.md()

    minimal = True
    for snr, pfa in ((0.5, 1e-6), (0.25, 1e-3)):
        m = det_dwell(snr, 0.9, pfa, 512)
        minimal &= m > 1 and det_pd(snr, m, det_threshold(pfa)) >= 0.9
        minimal &= det_pd(snr, m - 1, det_threshold(pfa)) < 0.9
        k = det_n_noncoh(snr, 16, 0.9, pfa, 256)
        if k > 1:
            minimal &= (
                det_pd_noncoherent(
                    snr, 16, k, det_threshold_noncoherent(pfa, k)
                )
                >= 0.9
            )
            minimal &= (
                det_pd_noncoherent(
                    snr, 16, k - 1, det_threshold_noncoherent(pfa, k - 1)
                )
                < 0.9
            )
        minimal &= det_dwell_power(snr * snr, 0.9, pfa, 512) == det_dwell(
            snr, 0.9, pfa, 512
        )
    d.minimal_ok = minimal
    R.md(
        "Every search helper returns the **minimum** value meeting the "
        "requirement: `det_dwell`, `det_n_noncoh` and `det_dwell_power` "
        "were each checked with the value one below, at its own threshold, "
        "failing. For `det_n_noncoh` that re-derivation is the whole claim "
        "— a fixed threshold would under-size every answer."
    )
    R.md()

    rows = []
    worst_gap = 0.0
    for p in (0.5, 0.9, 0.99):
        for target in (1e-3, 1e-6):
            n = det_verify_count(1 - p, target)
            budget = (1 - p) ** n
            exact = budget * p / (1 - budget) if budget < 1 else budget
            worst_gap = max(worst_gap, budget - exact)
            rows.append(
                [
                    f"{1 - p:g}",
                    f"{target:.0e}",
                    str(n),
                    f"{budget:.3e}",
                    f"{exact:.3e}",
                    f"{det_verify_delay(p, n):.1f}",
                ]
            )
    R.table(
        [
            "p_look",
            "target",
            "n",
            "budget p^n",
            "exact run rate",
            "E[looks]",
        ],
        rows,
    )
    d.verify_gap = worst_gap
    d.verify_conservative = worst_gap >= 0.0
    R.md(
        "`det_verify_count` sizes on `p^n`, which is deliberately above the "
        "exact consecutive-run rate `p^n(1-p)/(1-p^n)` — so it "
        "over-provisions the count rather than under-provisioning it. The "
        f"gap is at most {worst_gap:.1e} here, and `det_verify_delay` is "
        f"exact, which is the pair a caller uses: size with one, predict "
        f"the observed latency with the other."
    )
    R.md()


def _sec_marcum(d: Data) -> None:
    R.md("### 2.6 `marcum_q` across the envelope the header promises")
    R.md()
    R.md(
        "Every closed-form value pinned in the C test sits at `a <= 3`. The "
        "series is a Poisson-weighted sum whose window is centred on "
        "`k ~ a^2/2`, so the interesting question is at the far end. "
        "Measured against Monte-Carlo — external to the series in a way "
        "another closed form would not be: draw `Rice(a, 1)` and count."
    )
    R.md()
    rows = []
    worst = 0.0
    for i, (a, b) in enumerate(((8.0, 8.0), (12.0, 14.0), (15.0, 15.0))):
        r = _rng(500 + i)
        z = (a + r.standard_normal(N_DRAWS)) + 1j * r.standard_normal(N_DRAWS)
        mc = float(np.mean(np.abs(z) > b))
        q = marcum_q(1, a, b)
        se = _se(q, N_DRAWS)
        worst = max(worst, abs(mc - q) / se)
        rows.append(
            [
                f"{a:g}",
                f"{b:g}",
                f"{q:.6f}",
                f"{mc:.6f}",
                f"{abs(mc - q) / se:.1f}",
            ]
        )
    R.table(["a", "b", "marcum_q", "Monte-Carlo", "sigma"], rows)
    d.marcum_envelope_worst_sigma = worst
    R.md(
        f"Worst disagreement **{worst:.1f} sigma**. The term count is not "
        f"the fixed ~60 the header used to claim: the window half-width is "
        f"`12*sqrt(a^2/2 + 1) + 60`, so it is ~60 terms at `a = 0` and ~187 "
        f"at `a = 15` — corrected in the header as part of this "
        f"certification (F4)."
    )
    R.md()


def _sec_identities(d: Data) -> None:
    R.md("### 2.8 The exact identities each family rests on")
    R.md()
    R.md(
        "Everything above is a frequency compared to a probability. These "
        "are the closed forms underneath, checked as identities rather than "
        "measured — a family whose inversion has drifted will still look "
        "self-consistent in a Monte-Carlo run against its own threshold."
    )
    R.md()

    rows = []
    worst_rt = worst_pw = 0.0
    for pfa in PFAS:
        eta = det_threshold(pfa)
        pwr = det_threshold_power(pfa)
        rt = abs(np.exp(-0.5 * eta * eta) - pfa) / pfa
        pw = max(abs(pwr + np.log(pfa)), abs(pwr - 0.5 * eta * eta))
        worst_rt = max(worst_rt, rt)
        worst_pw = max(worst_pw, pw)
        rows.append(
            [
                f"{pfa:.0e}",
                f"{eta:.6f}",
                f"{pwr:.6f}",
                f"{rt:.1e}",
                f"{pw:.1e}",
            ]
        )
    R.table(
        [
            "pfa",
            "det_threshold",
            "det_threshold_power",
            "rel. err of exp(-eta^2/2)",
            "|p + ln pfa|, |p - eta^2/2|",
        ],
        rows,
    )
    d.thr_roundtrip_worst = worst_rt
    d.power_thr_worst = worst_pw
    R.md(
        "The power threshold is the envelope threshold in power units — "
        "`p = -ln(pfa) = eta^2/2` — which is why the two families cannot "
        "disagree about Pd and why §2.2 finds them bit-identical."
    )
    R.md()

    d.q_inv_signed = (
        det_q_inv(0.5) == 0.0
        and det_q_inv(0.99) < 0.0
        and det_q_inv(1e-6) > det_q_inv(1e-3) > 0.0
    )
    R.md(
        f"`det_q_inv` is **signed**: `{det_q_inv(0.5):.1f}` at the median, "
        f"`{det_q_inv(0.99):+.4f}` above it, `{det_q_inv(5e-6):+.4f}` in the "
        f"far tail. That sign is load-bearing — `det_dwell_gauss` computes "
        f"`Q_inv(pfa) - Q_inv(pd)` and every caller's `pd` is above 0.5, so "
        f"the difference is a SUM of two tails. Clamping the negative "
        f"branch reads as defensive and halves every dwell it sizes."
    )
    R.md()

    mean, var, pd_req, pfa_req = 0.4, 0.5, 0.99, 1e-5
    base_n = det_dwell_gauss(mean, var, pd_req, pfa_req)
    base_t = det_threshold_gauss(mean, pd_req, pfa_req)
    d.gauss_scaling_ok = (
        abs(det_threshold_gauss(2 * mean, pd_req, pfa_req) - 2 * base_t)
        < 1e-12
        and det_dwell_gauss(2 * mean, var, pd_req, pfa_req)
        == int(np.ceil(base_n / 4))
        and det_dwell_gauss(mean, 2 * var, pd_req, pfa_req) == 2 * base_n
    )
    R.table(
        ["quantity", "at (mean, var)", "at 2x mean", "at 2x var"],
        [
            [
                "det_dwell_gauss",
                str(base_n),
                str(det_dwell_gauss(2 * mean, var, pd_req, pfa_req)),
                str(det_dwell_gauss(mean, 2 * var, pd_req, pfa_req)),
            ],
            [
                "det_threshold_gauss",
                f"{base_t:.4f}",
                f"{det_threshold_gauss(2 * mean, pd_req, pfa_req):.4f}",
                f"{base_t:.4f} (independent)",
            ],
        ],
    )
    R.md(
        "`n = var * ((Q_inv(pfa) - Q_inv(pd)) / mean)^2`, so the look count "
        "goes linearly in variance and inverse-square in mean; the "
        "threshold is where the two tails cross and does not depend on "
        "either the variance or the look count."
    )
    R.md()

    worst_ema = 0.0
    for snr_in, gain_db in ((0.0, 20.0), (10.0, 20.0), (3.0, 6.0)):
        a = det_ema_alpha(snr_in, snr_in + gain_db)
        got = 10 * np.log10((2.0 - a) / a)
        worst_ema = max(worst_ema, abs(got - gain_db))
    d.ema_worst = worst_ema
    R.md(
        f"`det_ema_alpha` delivers the variance reduction it was asked for "
        f"— `(2-alpha)/alpha` matches the requested gain to "
        f"{worst_ema:.1e} dB, and depends only on the gain, not on where "
        f"the input SNR sits."
    )
    R.md()

    worst_pdpfa = 0.0
    for pfa in PFAS:
        eta = det_threshold(pfa)
        worst_pdpfa = max(worst_pdpfa, abs(det_pd(0.0, 8, eta) - pfa) / pfa)
        for nc in (2, 4, 8):
            e = det_threshold_noncoherent(pfa, nc)
            worst_pdpfa = max(
                worst_pdpfa,
                abs(det_pd_noncoherent(0.0, 16, nc, e) - pfa) / pfa,
            )
    d.pd_is_pfa_worst = worst_pdpfa

    worst_q = 0.0
    for pfa in (1e-3, 1e-6):
        for dwell in (1, 4, 16):
            a = det_snr(dwell, 0.9, pfa)
            b = det_snr(4 * dwell, 0.9, pfa)
            worst_q = max(worst_q, abs(20 * np.log10(a / b) - 6.0206))
    d.quadruple_db_worst = worst_q
    d.marcum_special_ok = (
        marcum_q(1, 0.0, 0.0) == 1.0
        and marcum_q(3, 2.0, -1.0) == 1.0
        and abs(marcum_q(1, 0.0, 1.0) - np.exp(-0.5)) < 1e-12
    )
    R.md(
        f"Two more that hold across every family: a zero-SNR input detects "
        f"at exactly the false-alarm rate (worst relative error "
        f"{worst_pdpfa:.1e}), and a **quadrupling of coherent dwell is "
        f"{6.0206 - worst_q:.4f}-{6.0206 + worst_q:.4f} dB** of sensitivity "
        f"— the `sqrt(M)` non-centrality, with nothing else moving."
    )
    R.md()


def _sec_boundaries(d: Data) -> None:
    R.md("### 2.7 Every helper fails closed")
    R.md()
    R.md(
        "These size a detector once, at startup, from numbers a config file "
        "supplied — so nonsense must not become a NaN threshold compared "
        "against every sample for the life of the process."
    )
    R.md()
    checks = [
        ("det_q_inv(0.0)", det_q_inv(0.0) == 0.0),
        ("det_q_inv(1.0)", det_q_inv(1.0) == 0.0),
        ("det_threshold_f(1e-3, 0)", det_threshold_f(1e-3, 0) == 0.0),
        ("det_threshold_f(0.0, 16)", det_threshold_f(0.0, 16) == 0.0),
        (
            "det_threshold_gauss(0.0, .99, 1e-5)",
            det_threshold_gauss(0.0, 0.99, 1e-5) == 0.0,
        ),
        (
            "det_dwell_gauss(mean=0)",
            det_dwell_gauss(0.0, 0.5, 0.99, 1e-5) == -1,
        ),
        (
            "det_dwell_gauss(var=0)",
            det_dwell_gauss(0.4, 0.0, 0.99, 1e-5) == -1,
        ),
        (
            "det_dwell_gauss(pd<=pfa)",
            det_dwell_gauss(0.4, 0.5, 1e-5, 0.99) == -1,
        ),
        ("det_dwell(unreachable)", det_dwell(0.001, 0.9, 1e-6, 10) == -1),
        (
            "det_n_noncoh(unreachable)",
            det_n_noncoh(1e-4, 1, 0.99, 1e-9, 4) == -1,
        ),
        ("det_verify_count(p_look=1)", det_verify_count(1.0, 0.5) > 2**30),
        ("det_verify_delay(p=0)", np.isinf(det_verify_delay(0.0, 3))),
        ("det_ema_alpha(no gain asked)", det_ema_alpha(10.0, 5.0) == 1.0),
    ]
    R.table(
        ["expression", "fails closed"],
        [[c, "yes" if ok else "**NO**"] for c, ok in checks],
    )
    d.fail_closed_ok = all(ok for _, ok in checks)
    R.md(
        "`-1` and `INT_MAX` both mean not-achievable and differ on purpose: "
        "one is a dwell a caller must not clamp, the other a count a caller "
        "may."
    )
    R.md()


# ── 3. review ─────────────────────────────────────────────────────────


def review(d: Data) -> None:
    R.md("## 3. Review — findings")
    R.md()
    R.find(
        "F1",
        "FIXED",
        "The non-coherent trio — `det_threshold_noncoherent`, "
        "`det_pd_noncoherent`, `det_n_noncoh` — had **zero** mentions in "
        "`native/tests/test_detection_core.c`. The coverage existed, in "
        "`test_detection.py`, which is real evidence in the wrong language "
        "for an object whose header is the SSOT — and these three are what "
        "`acq_core.c` sizes its entire (coherent depth, look count) split "
        "with (`acq_core.c:237`, `:275`, `:308`). Added in C: the exact "
        "reduction at one look, the threshold checked against `marcum_q` "
        "rather than a literal, growth with the look count, Pd = Pfa at "
        "zero SNR, and `det_n_noncoh`'s minimality at its own re-derived "
        "threshold. Sabotage-proven: forcing the coherent answer regardless "
        "of look count takes it red.",
    )
    R.find(
        "F2",
        "FIXED",
        f"`detection_core.h` explains why `det_threshold_f` exists with "
        f"one number — the chi-square gate realizing 41x the priced pfa at "
        f"n = 16 — and nothing re-derived it. The behaviour behind that is "
        f"not a finding: a threshold built on a reference estimated from "
        f"`n` samples inherits the estimate's uncertainty, and the library "
        f"already prices it correctly. What was missing was the arithmetic "
        f"executing anywhere, so the figure could drift from the code with "
        f"no gate noticing. Now re-derived in C ({d.f_ratio_16:.1f}x) and "
        f"reproduced by Monte-Carlo in the characterization subject "
        f"(41.1x). §2.4 also supplies what the ratio was standing in for "
        f"and what a caller actually needs: a budget table in dB indexed "
        f"by how many samples are available for the reference — "
        f"{d.sens_db_16:.2f} dB at 16, {d.budget_100_1e3:.2f} dB at 100. A "
        f"rate multiplier cannot be added to a link budget; that number "
        f"can. Sabotage-proven with a defect no existing literal could "
        f"see: perturbing `det_threshold_noncoherent` at `n_noncoh == 32` "
        f"only, which exactly two assertions catch, both new.",
    )
    R.find(
        "F3",
        "FIXED",
        "`det_dwell_power` was declared **twice** in the header — once with "
        "its full doc comment at the end of the power section, once bare on "
        'the last line before the `extern "C"` close. Harmless to a '
        "compiler and not harmless to a reader or to `jm`'s declaration "
        "injection, which reads the header to decide what needs patching. "
        "The bare duplicate is removed.",
    )
    R.find(
        "F4",
        "FIXED",
        'The `marcum_q` doc claimed the series *"converges in ~60 terms '
        "for practical a, b <= 15\"*. The implementation's window "
        "half-width is `12*sqrt(u+1) + 60` with `u = a^2/2` — about 60 "
        "terms at `a = 0` and about **187** at `a = 15`, and the scaling is "
        "load-bearing rather than incidental: a fixed 60-wide window "
        "anchored at `k = 0` misses the Poisson mass entirely once `a` is "
        "large, which `marcum_q.c` already carries a comment about. The "
        "prose said the constant and called it the total. Corrected, and "
        "the envelope is now measured (§2.6) instead of asserted.",
    )
    R.find(
        "F5",
        "CONFIRMED",
        "`acq_core.h` states twice (lines 75 and 307) that this module's "
        'non-coherent Pd model is *"non-monotonic and unreliable past a '
        'few hundred looks"*, and bounds its search at '
        "`ACQ_N_NONCOH_SAFETY_CEILING = 256` on that basis. Neither half "
        "reproduces: the threshold and Pd are monotone out to **1024** "
        "looks (§2.3), and Monte-Carlo agrees with the model to **0.2-0.6 "
        "sigma at 512** looks with H0 priced correctly there too. The "
        "ceiling may still be right for reasons of runtime or latency, but "
        "those are not the reasons the header gives, and non-coherent looks "
        "are where the wideband mode buys all of its margin. Left open "
        "because the ceiling is `acq`'s to move, not `detection`'s: "
        "[#997](https://github.com/doppler-dsp/doppler/issues/997).",
    )
    R.find(
        "F6",
        "BY DESIGN",
        "`det_threshold` and `det_q_inv` both take a probability and return "
        "a small number near 5, and at `pfa = 5e-6` they return 4.9409 and "
        "4.4172 — different laws, no type-level distinction, and an easy "
        "silent swap. Kept as two functions rather than one dispatching on "
        "a mode flag: the caller genuinely knows which law its statistic "
        "obeys, and a mode flag would move the same mistake one level out "
        "while making it harder to see at the call site. The defence is "
        "`docs/design/detection.md` §2, the header warning, and the fact "
        "that `det_q_inv` is signed — so a caller who clamps its negative "
        "branch, which reads as defensive, halves every dwell it sizes.",
    )


# ── 4. limits ─────────────────────────────────────────────────────────


def limits(d: Data) -> None:
    R.md("## 4. Limits — the certified envelope")
    R.md()
    R.md(
        "Claims a caller may rely on. A failure here is a regression, not a "
        "new finding. Every one is asserted by "
        "`src/doppler/detection/tests/test_validation_limits.py`."
    )
    R.md()
    R.limit(
        d.h0_worst_sigma < 3.0,
        f"every family's threshold delivers its priced false-alarm rate "
        f"across pfa 1e-2..1e-4 (worst {d.h0_worst_sigma:.1f} sigma)",
    )
    R.limit(
        d.power_exact,
        f"the envelope and power families are the same detector exactly "
        f"(worst difference {d.power_worst:.1e} over 64 grid points)",
    )
    R.limit(
        d.noncoh_reduces,
        "the non-coherent helpers reduce to the coherent ones exactly at "
        "n_noncoh = 1",
    )
    R.limit(
        d.thr_growth_ok,
        "the non-coherent threshold grows strictly with the look count",
    )
    R.limit(
        d.bigrun_monotone_to == 1024,
        f"the non-coherent model stays monotone in threshold and Pd out to "
        f"{d.bigrun_monotone_to} looks — 4x the ceiling acq sizes against",
    )
    R.limit(
        abs(d.f_ratio_16 - 41.0) < 1.0,
        f"the chi-square gate realizes {d.f_ratio_16:.1f}x the priced pfa "
        f"at n=16 — the header's 41x, re-derived",
    )
    R.limit(
        d.f_ratio_monotone,
        "the estimated-noise penalty shrinks monotonically as the noise "
        "estimate hardens",
    )
    R.limit(
        abs(d.est_rel_16 - 0.3536) < 1e-3,
        f"the noise reference at n=16 is known to "
        f"{d.est_rel_16 * 100:.1f}% (1 sigma) — sqrt(2/n), the cause of "
        f"both prices",
    )
    R.limit(
        abs(d.sens_db_16 - 3.27) < 0.05,
        f"pricing an estimated reference correctly costs "
        f"{d.sens_db_16:.2f} dB of sensitivity at n=16",
    )
    R.limit(
        d.sens_db_halves,
        "doubling the prompts in the noise estimate roughly halves that dB "
        "cost, as sqrt(2/n) requires",
    )
    R.limit(
        abs(d.budget_100_1e3 - 0.97) < 0.05,
        f"a 100-sample noise reference at pfa=1e-3 costs "
        f"{d.budget_100_1e3:.2f} dB — the budget table a caller reads off",
    )
    R.limit(
        d.inverse_worst < 1e-9,
        f"det_snr inverts det_pd to {d.inverse_worst:.1e} across 12 cells",
    )
    R.limit(
        d.minimal_ok,
        "det_dwell, det_n_noncoh and det_dwell_power each return the "
        "MINIMUM value meeting the requirement, at its own threshold",
    )
    R.limit(
        d.verify_conservative,
        f"det_verify_count is never below the exact consecutive-run rate "
        f"(worst margin {d.verify_gap:.1e})",
    )
    R.limit(
        d.marcum_envelope_worst_sigma < 3.0,
        f"marcum_q matches a Rice frequency across its stated a,b <= 15 "
        f"envelope (worst {d.marcum_envelope_worst_sigma:.1f} sigma)",
    )
    R.limit(
        d.fail_closed_ok,
        "every helper fails closed on out-of-range or unachievable input",
    )
    R.limit(
        d.thr_roundtrip_worst < 1e-12,
        f"det_threshold inverts Pfa = exp(-eta^2/2) exactly (worst relative "
        f"error {d.thr_roundtrip_worst:.1e})",
    )
    R.limit(
        d.power_thr_worst < 1e-12,
        f"det_threshold_power is -ln(pfa) and equals eta^2/2 (worst "
        f"{d.power_thr_worst:.1e})",
    )
    R.limit(
        d.q_inv_signed,
        "det_q_inv is signed: 0 at the median, negative above it, and "
        "monotone in the tail",
    )
    R.limit(
        d.gauss_scaling_ok,
        "det_dwell_gauss scales linearly in variance and inverse-square in "
        "mean; det_threshold_gauss scales in mean and ignores the variance",
    )
    R.limit(
        d.ema_worst < 1e-9,
        f"det_ema_alpha delivers the requested variance reduction (worst "
        f"{d.ema_worst:.1e} dB)",
    )
    R.limit(
        d.pd_is_pfa_worst < 1e-6,
        f"a zero-SNR input detects at exactly the false-alarm rate, in both "
        f"families (worst relative error {d.pd_is_pfa_worst:.1e})",
    )
    R.limit(
        d.quadruple_db_worst < 1e-3,
        f"a quadrupling of coherent dwell is 6.021 dB of sensitivity (worst "
        f"deviation {d.quadruple_db_worst:.1e} dB)",
    )
    R.limit(
        d.marcum_special_ok,
        "marcum_q's special cases hold: b <= 0 returns 1, and Q_1(0,b) is "
        "exp(-b^2/2)",
    )


# ── build ─────────────────────────────────────────────────────────────


def build(write: bool = True) -> Report:
    global R
    R = Report(write=write)
    R.md("# detection — validation report")
    R.md()
    section_object()
    d = characterise()
    review(d)
    limits(d)
    R.executive(
        "detection",
        [
            "**Pick the family by the H0 law of the statistic you are "
            "thresholding, never by the nearest function.** At pfa = 5e-6 "
            "`det_threshold` returns 4.9409 and `det_q_inv` returns 4.4172 "
            "— both plausible, only one a sigma count, and nothing catches "
            "the swap (§2.1, F6).",
            "**Coherent depth is free at the gate; non-coherent looks are "
            "not.** The non-coherent threshold grows with the look count, "
            "so `det_n_noncoh` re-derives it every iteration — sizing "
            "against a fixed threshold under-provisions every answer "
            "(§2.3).",
            f"**Estimating your own noise reference costs margin — budget "
            f"it.** {d.sens_db_16:.2f} dB at 16 samples, "
            f"{d.budget_100_1e3:.2f} dB at 100, at `pfa = 1e-3`, and more "
            f"at a tighter false-alarm budget. §2.4 has the table, indexed "
            f"by how many samples you can spare for the estimate. "
            f"`det_threshold_f` is what turns that uncertainty into a "
            f"threshold rather than a false-alarm surprise.",
            "**The non-coherent model does not break where the tree says "
            "it does.** It is monotone to 1024 looks and within 0.6 sigma "
            "of Monte-Carlo at 512, while `acq` bounds its search at 256 "
            "citing unreliability. Sensitivity may be sitting on the table "
            "(§2.3, F5 / #997).",
            "**These are design-time helpers and they fail closed** — `-1` "
            "for unachievable, `0.0` for out-of-range, `INT_MAX` for a "
            "look that can never compound. A caller that ignores the sign "
            "gets an obviously broken configuration rather than a NaN "
            "threshold that survives to production (§2.7).",
        ],
    )
    R.summary(
        "\n- Raw sweeps: `data/h0_rates.csv`, `data/fratio_penalty.csv`"
        "\n- The full Monte-Carlo envelope, at 10x these draw counts, is "
        "`make characterize` (subject `models`)"
    )
    R.emit(HERE / "results.md")
    return R


if __name__ == "__main__":
    sys.exit(cli(build, HERE))
