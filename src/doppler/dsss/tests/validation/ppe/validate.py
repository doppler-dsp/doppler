"""Certify `PolynomialPhaseEstimator` — the feedforward (freq, chirp) search.

Run:  python -m doppler.dsss.tests.validation.ppe.validate
      make validate          (regenerates every report)
      make validate-check    (fails if the committed report is stale)

A one-shot coherent matched-filter search: for each chirp-rate hypothesis the
segment is dechirped and transformed, and the peak of the resulting
(rate x frequency) surface — refined sub-bin on both axes — is the estimate.
`max_rate = 0` collapses the rate axis to a single FFT, which is the pure
Doppler case.

It is a **stateless by-value analyzer**: no running state, no serialization,
and `reset()` exists only to satisfy the common object protocol. That shapes
this report — there is no resume to verify and no lifecycle to sequence, so
almost all of it is accuracy against a synthesised truth whose parameters are
known exactly.

`BurstDemod` is its caller, and the reason its accuracy matters: a burst gets
one shot at a frequency estimate before demodulation, with no loop to walk the
error out afterwards.
"""

from __future__ import annotations

import sys
from dataclasses import dataclass, field
from pathlib import Path

import numpy as np

from doppler.dsss import PolynomialPhaseEstimator
from doppler.tests._validation_common import Report, cli

HERE = Path(__file__).resolve().parent
R = Report()

L = 512
SEED = 20260824


@dataclass
class Data:
    """Everything §3 and §4 read, measured once in §2."""

    subbin_rows: list[list[str]] = field(default_factory=list)
    subbin_worst: float = 0.0
    rate_rows: list[list[str]] = field(default_factory=list)
    rate_worst_frac: float = 0.0
    doppler_only_exact: bool = False
    nfft_is_4x: bool = False
    snr_rows: list[list[str]] = field(default_factory=list)
    snr_gain_db: float = 0.0
    lowsnr_rows: list[list[str]] = field(default_factory=list)
    lowsnr_worst_bins: float = 0.0
    range_ok: bool = False
    range_worst: float = 0.0
    floor_zeroed: bool = False
    floor_boundary_works: bool = False
    reset_noop: bool = False
    rejects_bad_args: bool = False
    invariance_rows: list[list[str]] = field(default_factory=list)
    invariant: bool = False
    mth_power_rows: list[list[str]] = field(default_factory=list)
    mth_power_bins: float = 0.0
    n_rate_odd: bool = False


def _chirp(n: int, f: float, r: float = 0.0) -> np.ndarray:
    """`exp(j2pi(f m + r m^2 / 2))` — the truth every measurement uses."""
    m = np.arange(n, dtype=np.float64)
    return np.exp(2j * np.pi * (f * m + 0.5 * r * m * m)).astype(np.complex64)


def _noise(n: int, tag: int) -> np.ndarray:
    r = np.random.default_rng(SEED + tag)
    return (
        (r.standard_normal(n) + 1j * r.standard_normal(n)) / np.sqrt(2.0)
    ).astype(np.complex64)


def _csv(path: Path, header: str, rows: list[list[float]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as fh:
        fh.write(header + "\n")
        for r in rows:
            fh.write(",".join(f"{v:.10g}" for v in r) + "\n")


# ── 1. the object ─────────────────────────────────────────────────────


def section_object() -> None:
    R.md("## 1. The object — one shot, no loop behind it")
    R.md()
    R.md(
        "`PolynomialPhaseEstimator` estimates a normalized frequency and "
        "chirp rate from one segment, by a coherent search over "
        "(chirp rate x frequency). It is stateless and by-value: every "
        "estimate depends only on the samples handed to that call."
    )
    R.md()
    R.md(
        "That statelessness is why this report has no resume section and no "
        "lifecycle sequence — there is nothing to serialize, and `reset()` "
        "is documented as a no-op. What is left is accuracy, and accuracy is "
        "the whole of the object's value: `BurstDemod` gets one frequency "
        "estimate per burst with no tracking loop to walk the residual out "
        "afterwards."
    )
    R.md()
    R.table(
        ["page", "owns"],
        [
            [
                "[`docs/design/ppe.md`](../../../../../../docs/design/ppe.md)",
                "the reasoning — why the search is two-dimensional and "
                "coherent, why the transform is 4x its input, and why the "
                "caller strips the modulation",
            ],
            [
                "[`docs/design/dsss-burst-receiver.md`]"
                "(../../../../../../docs/design/dsss-burst-receiver.md)",
                "the burst chain this estimator serves, and the tracked "
                "alternative it is chosen over",
            ],
            [
                "`native/inc/ppe/ppe_core.h`",
                "the contract — the SSOT this report audits",
            ],
        ],
    )
    R.md()
    R.md("### 1.1 The claim inventory")
    R.md()
    R.md(
        "Step 1 of `docs/dev/contributing/validation.md`. The C test is "
        "`test_ppe_core.c`, which was 74 lines. Four claims were untestable "
        "at its tolerances or absent entirely, and one field-level claim "
        "was **wrong**."
    )
    R.md()
    R.table(
        ["header claim", "pinned where", "here"],
        [
            [
                "the peak is refined sub-bin by parabolic interpolation",
                "**tolerance was 2.6 BINS** — would pass on the raw argmax",
                "§2.1",
            ],
            [
                "the rate axis is searched coherently and refined sub-grid",
                "C, at four rates",
                "§2.2",
            ],
            [
                "`max_rate = 0` collapses to a single FFT, rate forced to 0",
                "C",
                "§2.2",
            ],
            [
                "`nfft` is the zero-padded transform length",
                "**the header said next-pow2; it is 4x that**",
                "§2.3, F2",
            ],
            [
                "`snr_db` is the winning-row peak-to-mean",
                "**was nothing** — now C, as a scaling relationship",
                "§2.4",
            ],
            [
                "matched-filter optimal, holds at low SNR",
                "**was nothing** — now measured across input SNR",
                "§2.5",
            ],
            [
                "`freq_norm` lies in `[-0.5, 0.5)`",
                "**was nothing** — now C, at both shoulders",
                "§2.6",
            ],
            [
                "`n_in` in `[4, max_len]`; out of range zeroes every field",
                "**upper bound only** — the floor is now checked too",
                "§2.6",
            ],
            ["`reset()` is a no-op", "**was nothing** — now C", "§2.7"],
            ["`max_len >= 4`, `max_rate >= 0`, else NULL", "C", "§2.7"],
        ],
    )


# ── 2. characterisation ───────────────────────────────────────────────


def characterise() -> Data:
    d = Data()
    R.md("## 2. Characterisation")
    R.md()
    R.md("Measured behaviour. No verdicts — those are §3.")
    R.md()
    _sec_subbin(d)
    _sec_rate(d)
    _sec_nfft(d)
    _sec_snr(d)
    _sec_lowsnr(d)
    _sec_bounds(d)
    _sec_lifecycle(d)
    _sec_invariants(d)
    return d


def _sec_subbin(d: Data) -> None:
    R.md("### 2.1 Sub-bin frequency accuracy")
    R.md()
    R.md(
        f"At `max_len = {L}` the transform is 4x zero-padded, so a *grid* "
        f"step is 1/(4L). The claim worth measuring is against the **FFT "
        f"bin** "
        f"of the segment itself, 1/L = {1.0 / L:.3e}, because that is what a "
        f"caller would get with no refinement at all. Errors below are in "
        f"units of that bin."
    )
    R.md()
    binw = 1.0 / L
    p = PolynomialPhaseEstimator(max_len=L, max_rate=0.0)
    rows, csv = [], []
    worst = 0.0
    for k in range(8):
        frac = k / 8.0
        f = (40.0 + frac) * binw
        e = p.estimate(_chirp(L, f))
        err = abs(e.freq_norm - f) / binw
        worst = max(worst, err)
        rows.append(
            [
                f"{frac:.3f}",
                f"{f:.8f}",
                f"{e.freq_norm:.8f}",
                f"{err:.2e}",
            ]
        )
        csv.append([frac, f, e.freq_norm, err])
    R.table(["offset into the bin", "true", "estimate", "error (bins)"], rows)
    _csv(HERE / "data" / "subbin.csv", "frac,true,est,err_bins", csv)
    d.subbin_rows = rows
    d.subbin_worst = worst
    R.md(
        f"Worst **{worst:.2e} bins** across the bin, noiseless. The C test "
        f"tolerated 2.6 bins, which is four orders of magnitude looser than "
        f"the object achieves and wide enough to pass with the refinement "
        f"deleted — that is F1. Raw sweep: `data/subbin.csv`."
    )
    R.md()


def _sec_rate(d: Data) -> None:
    R.md("### 2.2 The chirp-rate axis, and its collapse at max_rate = 0")
    R.md()
    max_rate = 5e-5
    p = PolynomialPhaseEstimator(max_len=L, max_rate=max_rate)
    step = 2.0 * max_rate / max(p.n_rate - 1, 1)
    rows, csv = [], []
    worst = 0.0
    for r in (0.0, 5e-6, 1e-5, 2e-5, -1.5e-5):
        e = p.estimate(_chirp(L, 0.05, r))
        err = abs(e.rate_norm - r) / step
        worst = max(worst, err)
        rows.append([f"{r:+.2e}", f"{e.rate_norm:+.4e}", f"{err:.3f}"])
        csv.append([r, e.rate_norm, err])
    R.table(["true rate", "estimate", "error (grid steps)"], rows)
    _csv(HERE / "data" / "rate.csv", "true,est,err_steps", csv)
    d.rate_rows = rows
    d.rate_worst_frac = worst
    R.md(
        f"`n_rate = {p.n_rate}` hypotheses over +/-{max_rate:g}, a grid step "
        f"of {step:.2e}. Worst error **{worst:.3f} of a step** — the "
        f"parabolic refinement on the rate axis is what makes a coarse grid "
        f"sufficient, and a zero rate lands on a node because the count is "
        f"forced odd."
    )
    R.md()
    p0 = PolynomialPhaseEstimator(max_len=L, max_rate=0.0)
    e0 = p0.estimate(_chirp(L, 0.12, 0.0))
    d.doppler_only_exact = p0.n_rate == 1 and e0.rate_norm == 0.0
    R.md(
        f"With `max_rate = 0` the rate axis collapses to `n_rate = "
        f"{p0.n_rate}` and the returned rate is **exactly** 0.0, not merely "
        f"small — a caller can test it for equality."
    )
    R.md()


def _sec_nfft(d: Data) -> None:
    R.md("### 2.3 The transform is 4x zero-padded, not next-pow2")
    R.md()
    rows = []
    ok = True
    for n in (100, 512, 800):
        p = PolynomialPhaseEstimator(max_len=n, max_rate=0.0)
        np2 = 1 << (n - 1).bit_length()
        ok &= p.nfft == np2 * 4
        rows.append([str(n), str(np2), str(p.nfft), f"{p.nfft / np2:.0f}x"])
    R.table(["max_len", "next pow2", "nfft", "ratio"], rows)
    d.nfft_is_4x = ok
    R.md(
        'The header documented this field as *"next pow2 of max_len"*. It '
        "is four times that, and the difference is not cosmetic: `nfft` "
        "sizes three buffers, so a caller budgeting memory from the header "
        "was out by 4x — and the same 4x is what makes §2.1 as accurate as "
        "it is. Corrected and pinned (F2)."
    )
    R.md()


def _sec_snr(d: Data) -> None:
    R.md("### 2.4 `snr_db` is a post-integration quantity")
    R.md()
    R.md(
        'The field is documented only as a *"winning-row peak-to-mean '
        '(rough confidence)"*, and nothing measured it. It is a '
        "peak-to-mean taken **after** the coherent transform, so it carries "
        "the processing gain: on the same input it grows with the segment "
        "length. A caller thresholding on it without knowing that is "
        "comparing an integrated number against an input-referred one."
    )
    R.md()
    rows, csv = [], []
    vals = []
    for n in (256, 512, 1024):
        p = PolynomialPhaseEstimator(max_len=n, max_rate=0.0)
        x = (_chirp(n, 0.05) + _noise(n, 1)).astype(np.complex64)
        e = p.estimate(x)
        vals.append(e.snr_db)
        rows.append([str(n), f"{10 * np.log10(n):.2f}", f"{e.snr_db:.2f}"])
        csv.append([n, 10 * np.log10(n), e.snr_db])
    R.table(["segment length", "10log10(L)", "snr_db at 0 dB input"], rows)
    _csv(HERE / "data" / "snr_gain.csv", "L,coh_gain_db,snr_db", csv)
    d.snr_rows = rows
    d.snr_gain_db = float(vals[-1] - vals[0])
    R.md(
        f"Quadrupling the segment adds **{d.snr_gain_db:.2f} dB**, against "
        f"the 6.02 dB of ideal coherent gain — the shortfall is the window's "
        f"noise-equivalent bandwidth and the fact that the surface mean is "
        f"not exactly the noise floor. Asserted as a relationship rather "
        f"than a literal: a literal would pin the noise draw as much as the "
        f"estimator."
    )
    R.md()


def _sec_lowsnr(d: Data) -> None:
    R.md("### 2.5 Where the estimate degrades")
    R.md()
    R.md(
        "The header calls the estimator matched-filter optimal and says it "
        "holds at low SNR, which nothing measured. This is the envelope a "
        "caller sizing a burst preamble actually needs."
    )
    R.md()
    binw = 1.0 / L
    p = PolynomialPhaseEstimator(max_len=L, max_rate=0.0)
    rows, csv = [], []
    worst = 0.0
    for tag, snr_db in enumerate((30, 20, 10, 0, -10)):
        a = 10 ** (snr_db / 20.0)
        errs = []
        for trial in range(8):
            x = (
                a * _chirp(L, 0.05) + _noise(L, 100 + 17 * tag + trial)
            ).astype(np.complex64)
            errs.append(abs(p.estimate(x).freq_norm - 0.05) / binw)
        med = float(np.median(errs))
        mx = float(np.max(errs))
        if snr_db >= 0:
            worst = max(worst, mx)
        rows.append([f"{snr_db:+d}", f"{med:.4f}", f"{mx:.4f}"])
        csv.append([snr_db, med, mx])
    R.table(
        ["input SNR (dB)", "median error (bins)", "worst of 8 (bins)"], rows
    )
    _csv(HERE / "data" / "low_snr.csv", "snr_db,median_bins,worst_bins", csv)
    d.lowsnr_rows = rows
    d.lowsnr_worst_bins = worst
    R.md(
        f"Still inside **{worst:.3f} of a bin** at 0 dB input SNR over eight "
        f"noise draws, degrading gracefully rather than breaking — the "
        f"estimate stays on the right peak and loses precision. Raw sweep: "
        f"`data/low_snr.csv`."
    )
    R.md()


def _sec_bounds(d: Data) -> None:
    R.md("### 2.6 The documented ranges")
    R.md()
    p = PolynomialPhaseEstimator(max_len=L, max_rate=0.0)
    rows = []
    ok = True
    worst = 0.0
    for f in (0.45, 0.499, -0.45, -0.499):
        e = p.estimate(_chirp(L, f))
        inr = -0.5 <= e.freq_norm < 0.5
        same_sign = (e.freq_norm > 0) == (f > 0)
        err = abs(e.freq_norm - f)
        worst = max(worst, err)
        ok &= inr and same_sign
        rows.append(
            [f"{f:+.3f}", f"{e.freq_norm:+.6f}", str(inr), str(same_sign)]
        )
    R.table(["true", "estimate", "in [-0.5, 0.5)", "sign preserved"], rows)
    d.range_ok = ok
    d.range_worst = worst
    R.md(
        f"Accurate to {worst:.1e} at both shoulders with the sign intact. "
        f"The sign check is not redundant: an off-by-one in the bin-to-"
        f"frequency mapping wraps a near-Nyquist tone to the opposite side, "
        f"which is the failure that reads downstream as a receiver locking "
        f"to the negative image."
    )
    R.md()
    zeroed = True
    for n in (0, 1, 3):
        e = p.estimate(_chirp(max(n, 1), 0.05)[:n])
        zeroed &= (e.freq_norm, e.rate_norm, e.snr_db) == (0.0, 0.0, 0.0)
    d.floor_zeroed = zeroed
    e4 = p.estimate(_chirp(4, 0.05))
    d.floor_boundary_works = e4.snr_db != 0.0
    R.md(
        f"Below the documented floor of 4 samples every field is zeroed "
        f"(**{d.floor_zeroed}**), and the floor itself estimates "
        f"(**{d.floor_boundary_works}**) — so the refusal is a boundary and "
        f"not a blanket."
    )
    R.md()


def _sec_lifecycle(d: Data) -> None:
    R.md("### 2.7 reset, and what it refuses to build")
    R.md()
    p = PolynomialPhaseEstimator(max_len=L, max_rate=5e-5)
    x = _chirp(L, 0.077, 8e-6)
    a = p.estimate(x)
    p.reset()
    b = p.estimate(x)
    d.reset_noop = (a.freq_norm, a.rate_norm, a.snr_db) == (
        b.freq_norm,
        b.rate_norm,
        b.snr_db,
    )
    bad = 0
    for kwargs in ({"max_len": 2}, {"max_len": 64, "max_rate": -1.0}):
        try:
            PolynomialPhaseEstimator(**kwargs)
        except (ValueError, MemoryError, TypeError):
            bad += 1
    d.rejects_bad_args = bad == 2
    R.table(
        ["claim", "result"],
        [
            [
                "an estimate is bit-identical either side of reset()",
                str(d.reset_noop),
            ],
            [
                "max_len < 4 and max_rate < 0 are refused",
                str(d.rejects_bad_args),
            ],
        ],
    )
    R.md(
        "The reset check is not ceremony: it rules out a `reset()` that "
        "clears scratch the next estimate depends on, which is the way a "
        "documented no-op stops being one."
    )
    R.md()


def _sec_invariants(d: Data) -> None:
    R.md("### 2.8 Invariances, and the M-th-power contract")
    R.md()
    R.md(
        "The search runs on a magnitude surface, so a constant phase or a "
        "constant amplitude must not move the estimate. These are cheap to "
        "state and worth pinning because a normalisation added later for "
        "numerical reasons is exactly what would break them quietly."
    )
    R.md()
    p = PolynomialPhaseEstimator(max_len=L, max_rate=5e-5)
    base = p.estimate(_chirp(L, 0.077, 8e-6))
    rows = []
    ok = True
    for name, x in (
        ("phase rotated 1.1 rad", _chirp(L, 0.077, 8e-6) * np.exp(1j * 1.1)),
        ("amplitude x 100", _chirp(L, 0.077, 8e-6) * 100.0),
        ("amplitude x 0.01", _chirp(L, 0.077, 8e-6) * 0.01),
    ):
        e = p.estimate(x.astype(np.complex64))
        df = abs(e.freq_norm - base.freq_norm) * L
        dr = abs(e.rate_norm - base.rate_norm)
        ds = abs(e.snr_db - base.snr_db)
        ok &= df < 1e-3 and dr < 1e-8 and ds < 0.01
        rows.append([name, f"{df:.1e}", f"{dr:.1e}", f"{ds:.3f}"])
    R.table(["transform", "d freq (bins)", "d rate", "d snr_db"], rows)
    d.invariance_rows = rows
    d.invariant = ok
    R.md(
        "`snr_db` is invariant too, which follows from it being a "
        "peak-to-MEAN in dB rather than an absolute level — a scale factor "
        "cancels."
    )
    R.md()
    R.md(
        "The header also states a contract the caller must honour: a "
        "non-data-aided caller raises an M-PSK stream to the M-th power to "
        "strip the modulation, **which scales both returned values by M**, "
        "so the caller halves them for BPSK. That is a claim about this "
        "object's output and nothing measured it."
    )
    R.md()
    rng = np.random.default_rng(SEED + 900)
    bits = (rng.integers(0, 2, L) * 2 - 1).astype(np.float64)
    f_true, r_true = 0.03, 4e-6
    x = (bits * _chirp(L, f_true, r_true)).astype(np.complex64)
    e = p.estimate((x * x).astype(np.complex64))
    d.mth_power_bins = abs(e.freq_norm / 2.0 - f_true) * L
    R.table(
        ["quantity", "true", "squared estimate", "halved", "error (bins)"],
        [
            [
                "frequency",
                f"{f_true:.6f}",
                f"{e.freq_norm:.6f}",
                f"{e.freq_norm / 2:.6f}",
                f"{d.mth_power_bins:.3f}",
            ],
            [
                "chirp rate",
                f"{r_true:.3e}",
                f"{e.rate_norm:.3e}",
                f"{e.rate_norm / 2:.3e}",
                "—",
            ],
        ],
    )
    R.md(
        f"Squaring a BPSK stream recovers 2f and 2r; halved, the frequency "
        f"is within **{d.mth_power_bins:.3f} of a bin** of truth. The "
        f"residual is the squaring itself — it doubles the noise and the "
        f"spurs along with the signal — not the estimator."
    )
    R.md()
    odd = True
    rows = []
    for mr in (1e-5, 5e-5, 1e-4, 5e-4):
        q = PolynomialPhaseEstimator(max_len=L, max_rate=mr)
        odd &= q.n_rate % 2 == 1
        rows.append([f"{mr:.0e}", str(q.n_rate), str(q.n_rate % 2 == 1)])
    R.table(["max_rate", "n_rate", "odd"], rows)
    d.n_rate_odd = odd
    R.md(
        "The count is forced odd on purpose: it puts `r = 0` on a grid node, "
        "so a genuinely unchirped signal is found at a hypothesis rather "
        "than between two — which is the common case and the one a "
        "near-static Doppler caller lands on."
    )
    R.md()


# ── 3. review ─────────────────────────────────────────────────────────


def review(d: Data) -> None:
    R.md("## 3. Review — findings")
    R.md()
    R.find(
        "F1",
        "FIXED",
        f"**The sub-bin refinement was pinned at a tolerance 2.6 BINS "
        f"wide.** The header claims the peak is refined sub-bin by "
        f"parabolic interpolation; the C test's `ftol` of 5e-3 against a "
        f"bin of 1/512 = 1.95e-3 is 2.6 bins, so the section would have "
        f"passed with the refinement deleted and the raw argmax returned. "
        f"Measured, the estimator resolves a noiseless tone to "
        f"{d.subbin_worst:.0e} of a bin, and the gate is now 0.01 bins — a "
        f"hundred times tighter than the old tolerance and a hundred times "
        f"looser than the object achieves. Sabotage-proven by forcing the "
        f"raw-argmax fallback branch, which takes it red and leaves every "
        f"pre-existing assertion green.",
    )
    R.find(
        "F2",
        "FIXED",
        "**The `nfft` field comment was wrong by 4x.** It documented the "
        'transform length as *"next pow2 of max_len"*; the implementation '
        "uses `next_pow2 (max_len) << 2`. Not cosmetic: `nfft` sizes `buf`, "
        "`spec` and `mag`, so a caller budgeting memory from the header was "
        "out by a factor of four on three buffers. The 4x is also what "
        "makes the sub-bin accuracy in §2.1 what it is, so the comment was "
        "hiding the mechanism as well as the footprint. Corrected, and "
        "pinned in C so the two cannot drift apart again.",
    )
    R.find(
        "F3",
        "FIXED",
        f"**`snr_db` was measured by nothing**, in either language, and it "
        f"is not an input-referred SNR. It is a peak-to-mean taken after "
        f"the coherent transform, so it carries the processing gain: "
        f"quadrupling the segment adds {d.snr_gain_db:.1f} dB on identical "
        f"input (§2.4). A caller thresholding on it as though it were the "
        f"segment's SNR is comparing an integrated quantity against an "
        f"input-referred one, and the threshold then moves whenever the "
        f"segment length does. Now asserted as that scaling relationship "
        f"rather than as a literal, which would pin the noise draw as much "
        f"as the estimator.",
    )
    R.find(
        "F4",
        "BY DESIGN",
        "The frequency axis's sub-bin refinement is **not in this object**: "
        "`ppe_estimate` delegates to `find_peaks_f32` from `spectral_core` "
        "and only falls back to a raw argmax if that returns nothing. The "
        "header's \"refined sub-bin in both axes by parabolic "
        'interpolation" is therefore accurate about the behaviour and '
        "quiet about the ownership — the rate axis is refined here, the "
        "frequency axis is refined by a shared primitive. Recorded rather "
        "than changed, because the composition is correct and re-inlining "
        "the interpolation would be exactly the duplication the library's "
        "own rule forbids. The consequence worth knowing: a regression in "
        "`find_peaks_f32` shows up as a ppe accuracy failure.",
    )
    R.find(
        "F5",
        "BY DESIGN",
        "There is no state triplet and no `serializable` flag, and that is "
        "correct rather than an omission: the object is a by-value analyzer "
        "that computes each estimate purely from the segment it is handed, "
        "so there is nothing to checkpoint. `reset()` exists only to "
        "satisfy the common object protocol and is now asserted to be the "
        "no-op it claims to be (§2.7) — which is what stops it quietly "
        "acquiring behaviour later.",
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
        d.subbin_worst < 0.01,
        f"a noiseless tone is resolved to {d.subbin_worst:.1e} of an FFT "
        f"bin, across the whole bin",
    )
    R.limit(
        d.rate_worst_frac < 0.05,
        f"the chirp rate is resolved to {d.rate_worst_frac:.3f} of a "
        f"rate-grid step at five rates including zero and both signs",
    )
    R.limit(
        d.doppler_only_exact,
        "max_rate = 0 collapses to one FFT and returns rate EXACTLY 0.0, "
        "so a caller may compare it for equality",
    )
    R.limit(
        d.nfft_is_4x,
        "nfft is 4 * next_pow2(max_len) at three lengths — the documented "
        "footprint, corrected",
    )
    R.limit(
        d.snr_gain_db > 3.0,
        f"snr_db carries the coherent processing gain: quadrupling the "
        f"segment adds {d.snr_gain_db:.1f} dB on identical input",
    )
    R.limit(
        d.lowsnr_worst_bins < 0.1,
        f"the estimate stays within {d.lowsnr_worst_bins:.3f} of a bin down "
        f"to 0 dB input SNR over eight noise draws",
    )
    R.limit(
        d.range_ok,
        "freq_norm stays in [-0.5, 0.5) with the sign intact at both "
        "shoulders — a near-Nyquist tone does not wrap to its image",
    )
    R.limit(
        d.range_worst < 1e-4,
        f"...and is accurate to {d.range_worst:.1e} there",
    )
    R.limit(
        d.floor_zeroed,
        "a segment shorter than the documented floor of 4 zeroes every "
        "field rather than estimating",
    )
    R.limit(
        d.floor_boundary_works,
        "...and the floor itself estimates, so the refusal is a boundary "
        "and not a blanket",
    )
    R.limit(
        d.reset_noop,
        "an estimate is bit-identical either side of reset(), so the "
        "documented no-op is one",
    )
    R.limit(
        d.rejects_bad_args,
        "max_len < 4 and max_rate < 0 are refused at construction",
    )
    R.limit(
        d.invariant,
        "the estimate is invariant to a constant phase and to amplitude "
        "scaling over four decades, snr_db included",
    )
    R.limit(
        d.mth_power_bins < 0.1,
        f"squaring a BPSK stream returns 2f and 2r: halved, the frequency "
        f"is within {d.mth_power_bins:.3f} of a bin of truth",
    )
    R.limit(
        d.n_rate_odd,
        "n_rate is odd at every max_rate, so r = 0 is a grid node rather "
        "than a point between two hypotheses",
    )


# ── build ─────────────────────────────────────────────────────────────


def build(write: bool = True) -> Report:
    global R
    R = Report(write=write)
    R.md("# PolynomialPhaseEstimator — validation report")
    R.md()
    section_object()
    d = characterise()
    review(d)
    limits(d)
    R.executive(
        "PolynomialPhaseEstimator",
        [
            f"**It resolves a tone to {d.subbin_worst:.0e} of an FFT bin**, "
            f"and stays inside {d.lowsnr_worst_bins:.2f} of a bin at 0 dB "
            f"input SNR. That precision is the object's whole value — a "
            f"burst gets one estimate with no loop behind it (§2.1, §2.5).",
            f"**`snr_db` is not an input SNR.** It is a peak-to-mean after "
            f"coherent integration, so it carries the processing gain — "
            f"quadrupling the segment adds {d.snr_gain_db:.1f} dB on "
            f"identical input. A confidence threshold set from it moves "
            f"whenever the segment length does (§2.4, F3).",
            "**`nfft` is 4x next_pow2(max_len), not next_pow2.** The header "
            "said otherwise, and three buffers scale with it — so memory "
            "budgeted from the old comment was out by four (§2.3, F2).",
            "**`max_rate = 0` returns rate exactly 0.0**, not a small "
            "number, so the pure-Doppler case is testable by equality "
            "(§2.2).",
            "**Nothing here is stateful.** No triplet, no resume, and "
            "`reset()` is asserted to be the no-op it claims to be — which "
            "is what stops it quietly acquiring behaviour (§2.7, F5).",
        ],
    )
    R.summary(
        "\n- Raw sweeps: `data/subbin.csv`, `data/rate.csv`, "
        "`data/snr_gain.csv`, `data/low_snr.csv`"
    )
    R.emit(HERE / "results.md")
    return R


if __name__ == "__main__":
    sys.exit(cli(build, HERE))
