"""How far can each `det_*` model be trusted? — measured against frequency.

`detection` ships five families of sizing helper (see
`docs/design/detection.md` §2), and every Pd and every threshold in them is a
**model**. A model is checkable against exactly one external truth: draw the
statistic the family claims to describe, count how often it crosses, and
compare the frequency to the probability that was priced.

This subject exists because that comparison was being asserted rather than
run. `acq_core.c` sizes its whole (coherent depth, non-coherent look) split
on these helpers and its own comment reads *"Validated against Monte-Carlo to
<1% (det_pd_noncoherent tests)"* — a real claim, spot-checked at three points
in a unit test, with no sweep behind the number.

**Two questions, kept separate, because they fail differently.**

- **H0 — is the threshold priced correctly?** Draw the null statistic, count
  exceedances of the threshold the helper returns, compare to the `pfa` that
  was asked for. A wrong H0 law is a detector that false-alarms at a rate
  nobody chose, and it is invisible in a signal-present test.
- **H1 — is the Pd curve right?** Inject a signal at a known amplitude SNR,
  count detections, compare to the model's Pd. This is the number a caller
  budgets a dwell against.

**What a disagreement here means.** These are asymptotic and series-based
models, so some error is expected and the useful output is *where* it grows —
which family, at which corner of the envelope. A `pfa` resolved by fewer than
a few hundred expected false alarms is measuring the draw, not the model, so
each cell reports its own binomial standard error and cells with too few
counts are printed rather than silently averaged in.

Run:  make characterize      (this subject, plus every other)
      python -m doppler.detection.tests.characterization\
             .models.characterize
"""

from __future__ import annotations

import numpy as np

from doppler.detection import (
    det_pd,
    det_pd_noncoherent,
    det_pd_power,
    det_q_inv,
    det_threshold,
    det_threshold_f,
    det_threshold_noncoherent,
    det_threshold_power,
)

#: Draws per cell. 2e6 resolves a 1e-4 tail to ~5 % relative (200 expected
#: hits) and a 1e-3 tail to ~1.6 %. Deliberately NOT enough for 1e-6 — that
#: cell is reported with its standard error rather than pretended to.
N_DRAWS = 2_000_000

#: The false-alarm budgets swept. 1e-6 is included precisely because it is
#: under-resolved at N_DRAWS: the point is to show where the measurement
#: stops being able to see the model, not to hide it.
PFAS = (1e-2, 1e-3, 1e-4, 1e-6)

#: Non-coherent look counts. 1 must reduce to the coherent family exactly.
LOOKS = (1, 2, 4, 8, 16)

#: Coherent depths for the Pd sweep.
DWELLS = (1, 4, 16, 64)

SEED = 20260824


def _rng(tag: int) -> np.random.Generator:
    """One independent stream per cell, reproducible across runs."""
    return np.random.default_rng(SEED + tag)


def _binom_se(p_hat: float, n: int) -> float:
    """Standard error of a measured frequency — the resolution floor."""
    return float(np.sqrt(max(p_hat, 1.0 / n) * (1.0 - p_hat) / n))


# ── H0: does each family's threshold deliver the pfa it was asked for? ──────


def h0_envelope(pfa: float, tag: int) -> tuple[float, float]:
    """Rayleigh(1): the amplitude-ratio statistic with a known noise floor."""
    r = _rng(tag)
    z = r.standard_normal(N_DRAWS) + 1j * r.standard_normal(N_DRAWS)
    # |z| with unit-variance components IS Rayleigh(1): P(|z| > b) =
    # exp(-b^2/2), which is exactly the law det_threshold inverts. A
    # /sqrt(2) here would be the OTHER convention (unit total power)
    # and prices every threshold 3 dB away from the shipped one.
    stat = np.abs(z)
    hit = float(np.mean(stat > det_threshold(pfa)))
    return hit, _binom_se(hit, N_DRAWS)


def h0_power(pfa: float, tag: int) -> tuple[float, float]:
    """Exponential(1): the power detector's null law."""
    r = _rng(tag)
    stat = r.exponential(1.0, N_DRAWS)
    hit = float(np.mean(stat > det_threshold_power(pfa)))
    return hit, _binom_se(hit, N_DRAWS)


def h0_gauss(pfa: float, tag: int) -> tuple[float, float]:
    """N(0, 1): a block-averaged lock metric, thresholded in sigmas."""
    r = _rng(tag)
    stat = r.standard_normal(N_DRAWS)
    hit = float(np.mean(stat > det_q_inv(pfa)))
    return hit, _binom_se(hit, N_DRAWS)


def h0_noncoherent(pfa: float, n_noncoh: int, tag: int) -> tuple[float, float]:
    """chi2(2M): magnitude-squared accumulation over M coherent looks.

    The statistic is R = sqrt(sum |z_k|^2 / noise) with unit-variance complex
    looks, so R^2 ~ chi2(2M) and P(R > b) = marcum_q(M, 0, b).
    """
    r = _rng(tag)
    acc = np.zeros(N_DRAWS)
    for _ in range(n_noncoh):
        z = r.standard_normal(N_DRAWS) + 1j * r.standard_normal(N_DRAWS)
        acc += np.abs(z) ** 2
    stat = np.sqrt(acc)
    hit = float(np.mean(stat > det_threshold_noncoherent(pfa, n_noncoh)))
    return hit, _binom_se(hit, N_DRAWS)


def h0_fratio(pfa: float, n: int, tag: int) -> tuple[float, float, float]:
    """F(n, n): BurstDespreader's ratio against an ESTIMATED noise reference.

    Returns the F-gate's realized pfa, its standard error, and what the
    chi-square gate would have realized on the same draws — the second number
    is the 41x of `docs/design/detection.md` §4, measured rather than derived.
    """
    r = _rng(tag)
    re2 = np.sum(r.standard_normal((N_DRAWS, n)) ** 2, axis=1)
    im2 = np.sum(r.standard_normal((N_DRAWS, n)) ** 2, axis=1)
    stat = np.sqrt(n * re2 / im2)
    f_gate = float(np.sqrt(n * det_threshold_f(pfa, n)))
    chi_gate = det_threshold_noncoherent(pfa, max(n // 2, 1))
    hit = float(np.mean(stat > f_gate))
    chi_hit = float(np.mean(stat > chi_gate))
    return hit, _binom_se(hit, N_DRAWS), chi_hit


# ── H1: is the Pd curve right? ─────────────────────────────────────────────


def h1_envelope(
    snr: float, dwell: int, pfa: float, tag: int
) -> tuple[float, float]:
    """Rice(a, 1) with a = sqrt(2*dwell)*snr — the coherent Pd claim."""
    r = _rng(tag)
    a = np.sqrt(2.0 * dwell) * snr
    z = (a + r.standard_normal(N_DRAWS)) + 1j * r.standard_normal(N_DRAWS)
    hit = float(np.mean(np.abs(z) > det_threshold(pfa)))
    return hit, _binom_se(hit, N_DRAWS)


def h1_noncoherent(
    snr: float, n_coh: int, n_noncoh: int, pfa: float, tag: int
) -> tuple[float, float]:
    """Order-M Rice accumulation: a = sqrt(2*n_coh*n_noncoh)*snr, spread
    over n_noncoh looks each carrying a = sqrt(2*n_coh)*snr."""
    r = _rng(tag)
    a = np.sqrt(2.0 * n_coh) * snr
    acc = np.zeros(N_DRAWS)
    for _ in range(n_noncoh):
        z = (a + r.standard_normal(N_DRAWS)) + 1j * r.standard_normal(N_DRAWS)
        acc += np.abs(z) ** 2
    stat = np.sqrt(acc)
    hit = float(np.mean(stat > det_threshold_noncoherent(pfa, n_noncoh)))
    return hit, _binom_se(hit, N_DRAWS)


# ── reporting ──────────────────────────────────────────────────────────────


def _row(name: str, priced: float, measured: float, se: float) -> str:
    """One cell. `sigma` is how many standard errors the model sits away —
    under ~3 the draw cannot distinguish the model from the truth."""
    if se <= 0.0:
        return f"  {name:<28} priced {priced:.3e}   measured {measured:.3e}"
    sig = abs(measured - priced) / se
    ratio = measured / priced if priced > 0 else float("nan")
    flag = "" if sig < 3.0 else "   <-- model and frequency disagree"
    return (
        f"  {name:<28} priced {priced:.3e}   measured {measured:.3e}"
        f" +/- {se:.1e}   x{ratio:5.2f}  {sig:5.1f} sigma{flag}"
    )


def sweep_h0() -> list[str]:
    out: list[str] = []
    tag = 0
    for pfa in PFAS:
        out.append(
            f"\n  pfa = {pfa:.0e}   ({pfa * N_DRAWS:.0f} expected hits)"
        )
        m, se = h0_envelope(pfa, tag := tag + 1)
        out.append(_row("envelope  Rayleigh(1)", pfa, m, se))
        m, se = h0_power(pfa, tag := tag + 1)
        out.append(_row("power     Exp(1)", pfa, m, se))
        m, se = h0_gauss(pfa, tag := tag + 1)
        out.append(_row("gauss     N(0,1)", pfa, m, se))
        for nc in LOOKS:
            m, se = h0_noncoherent(pfa, nc, tag := tag + 1)
            out.append(_row(f"noncoh    chi2({2 * nc})", pfa, m, se))
    return out


def sweep_h1() -> list[str]:
    out: list[str] = []
    tag = 1000
    pfa = 1e-3
    eta = det_threshold(pfa)
    out.append(f"\n  coherent Pd, pfa = {pfa:.0e}, threshold {eta:.4f}")
    for dwell in DWELLS:
        for snr in (0.1, 0.25, 0.5, 1.0):
            model = det_pd(snr, dwell, eta)
            m, se = h1_envelope(snr, dwell, pfa, tag := tag + 1)
            out.append(_row(f"dwell={dwell:<3} snr={snr:<5}", model, m, se))
    out.append("\n  non-coherent Pd, pfa = 1e-3, n_coh = 16")
    for nc in (1, 2, 4, 8):
        eta_nc = det_threshold_noncoherent(pfa, nc)
        for snr in (0.15, 0.3, 0.5):
            model = det_pd_noncoherent(snr, 16, nc, eta_nc)
            m, se = h1_noncoherent(snr, 16, nc, pfa, tag := tag + 1)
            out.append(_row(f"looks={nc:<3} snr={snr:<5}", model, m, se))
    return out


def sweep_fratio() -> list[str]:
    out: list[str] = []
    tag = 2000
    pfa = 1e-3
    out.append(
        f"\n  F(n,n) gate vs the chi-square gate on the SAME draws, "
        f"pfa = {pfa:.0e}"
    )
    for n in (4, 8, 16, 32, 64):
        m, se, chi = h0_fratio(pfa, n, tag := tag + 1)
        out.append(_row(f"n={n:<4} F gate", pfa, m, se))
        out.append(
            f"  {'':<28} chi-square gate on the same draws: "
            f"{chi:.3e}  = {chi / pfa:5.1f}x the priced rate"
        )
    return out


def sweep_equivalence() -> list[str]:
    """The envelope and power families must agree exactly, not merely
    approximately."""
    out = ["\n  envelope vs power — the same detector in different units"]
    worst = 0.0
    for pfa in PFAS:
        eta, p = det_threshold(pfa), det_threshold_power(pfa)
        for dwell in DWELLS:
            for snr in (0.1, 0.5, 1.0, 2.0):
                a = det_pd(snr, dwell, eta)
                b = det_pd_power(snr * snr, dwell, p)
                worst = max(worst, abs(a - b))
    out.append(f"  worst |det_pd - det_pd_power| over the grid: {worst:.3e}")
    return out


def main() -> None:
    print(__doc__.split("\n\n")[0])
    print(f"\n{N_DRAWS:,} draws per cell, seed {SEED}\n")

    print("=" * 78)
    print("H0 — is the threshold priced correctly?")
    print("=" * 78)
    for line in sweep_h0():
        print(line)

    print("\n" + "=" * 78)
    print("H1 — is the Pd curve right?")
    print("=" * 78)
    for line in sweep_h1():
        print(line)

    print("\n" + "=" * 78)
    print("The estimated-noise penalty (docs/design/detection.md §4)")
    print("=" * 78)
    for line in sweep_fratio():
        print(line)

    print("\n" + "=" * 78)
    print("Cross-family identities")
    print("=" * 78)
    for line in sweep_equivalence():
        print(line)

    print(
        "\nRead the sigma column, not the ratio: a cell whose priced rate is "
        "1e-6 gets ~2 expected hits at this draw count, so a ratio of 0.5 or "
        "2.0 there is the draw talking. A model that is genuinely wrong "
        "shows up as many sigma at a WELL-RESOLVED pfa."
    )


if __name__ == "__main__":
    main()
