#!/usr/bin/env python3
"""resamp validation — produces this folder's certification evidence.

Writes ``results.md`` (the authoritative report), the plots it embeds, and
the raw sweeps under ``data/`` so any number in the report can be
re-derived without re-running the measurement.

Three phases, in order: **characterise -> review -> limits**.

This is a C library, so nothing here is where a claim is first tested.
``native/tests/test_resamp_core.c`` §1-§20 is the evidence; this file
characterises the same properties **through the Python binding**, which is
a genuinely different surface — it certifies that the binding delivers what
the C proves, and produces the plots the C cannot. Every section names the
C section it tracks, and claims the binding cannot reach are reported as
**C-ONLY** rather than skipped.

Measurement is dogfooded, not hand-rolled. The artifact floor comes from
``doppler.measure.ToneMeasure`` (``worst_spur_dbc``) and the test tones from
``dp_coherent_freq``, so the spectral machinery is the library's own
instrument rather than a second implementation living in a test.

Run:  make validate
"""

from __future__ import annotations

import sys
from dataclasses import dataclass, field
from pathlib import Path

import numpy as np

from doppler.measure import ToneMeasure, dp_coherent_freq
from doppler.resample import Resampler
from doppler.tests._validation_common import Report, cli

HERE = Path(__file__).parent
DATA = HERE / "data"

#: Inputs per probe. Enough that the filter transient is a small fraction
#: of every record, small enough that the whole report is a few seconds.
NIN = 4096
#: Outputs discarded before projecting: 19 taps over 4096 phases, so 64 is
#: comfortably past the bank's startup.
SKIP = 64
#: Analyser length for the artifact-floor measurements.
NFFT = 8192

R = Report()


# ─────────────────────────────────────────────────────────── stimulus
def tone(n: int, f0: float) -> np.ndarray:
    """A unit-amplitude complex exponential at normalised frequency `f0`."""
    return np.exp(2j * np.pi * f0 * np.arange(n)).astype(np.complex64)


def coherent(f_target: float, n: int) -> float:
    """Snap `f_target` to the nearest leakage-free frequency for `n`.

    An integer number of cycles in the capture means no spectral leakage,
    which is what lets a spur floor be read at -60 dBc and below without
    the carrier's own skirt dominating the answer. `doppler.measure` owns
    this rule; it is not restated here.
    """
    return float(dp_coherent_freq(1.0, f_target, n))


def tone_residual_db(y: np.ndarray, f_out: float, skip: int = SKIP) -> float:
    """Residual of `y` against the pure tone it must be, in dB.

    A resampled pure tone is still a pure tone, so projecting onto that
    tone and measuring the remainder needs **no timing convention** — it
    cannot beg the question a comparison against a reference implementation
    would. Amplitude and phase are solved for, so a constant gain or delay
    is not counted as error.

    This is the C suite's own gate (§3, §4) evaluated through the binding.
    """
    if len(y) <= skip + 64:
        return float("nan")
    m = np.arange(skip, len(y))
    basis = np.exp(2j * np.pi * f_out * m)
    a = np.vdot(basis, y[skip:]) / np.vdot(basis, basis)
    ref = a * basis
    err = float(np.sum(np.abs(y[skip:] - ref) ** 2))
    sig = float(np.sum(np.abs(ref) ** 2))
    return 10.0 * np.log10(err / sig) if sig > 0 else float("nan")


def worst_spur_dbc(y: np.ndarray) -> float:
    """The worst artifact in `y`, relative to the carrier, in dBc.

    Measured by `ToneMeasure`, doppler's own spectral instrument, rather
    than a windowed DFT scan written here — the C test hand-rolls one
    because `test_resamp_core.c` links only `resamp_core`, but Python has
    the whole library available and a second implementation of a spur
    search is exactly the duplication that drifts.
    """
    n = min(NFFT, (len(y) // 2) * 2)
    if n < 1024:
        return float("nan")
    tm = ToneMeasure(n=n, fs=1.0)
    return float(tm.analyze_complex(y[:n].astype(np.complex64)).worst_spur_dbc)


def _csv(path: Path, cols: list[np.ndarray], header: str) -> None:
    """Write one sweep, unless this run is measurement-only."""
    if R.write:
        np.savetxt(
            path,
            np.column_stack(cols),
            delimiter=",",
            header=header,
            comments="",
            fmt="%.10g",
        )


# ─────────────────────────────────────────────────────────── data
@dataclass
class Data:
    """Every measurement the report reads, taken once."""

    rates: list[float] = field(default_factory=list)
    purity: list[float] = field(default_factory=list)
    counts: list[int] = field(default_factory=list)
    count_err: list[float] = field(default_factory=list)
    dc_rates: list[float] = field(default_factory=list)
    dc_gain: list[float] = field(default_factory=list)
    dec_f: list[float] = field(default_factory=list)
    dec_db: list[float] = field(default_factory=list)
    img_rates: list[float] = field(default_factory=list)
    img_lo: list[float] = field(default_factory=list)
    img_hi: list[float] = field(default_factory=list)
    seam_delta: list[float] = field(default_factory=list)
    seam_n: list[int] = field(default_factory=list)
    seam_purity: list[float] = field(default_factory=list)
    struct: list[tuple[float, bool, int, float, float]] = field(
        default_factory=list
    )
    mu_fresh: float = 0.0
    mu_steered: float = 0.0
    mu_settled: float = 0.0
    mu_fixed: float = 0.0
    ctrl_accepts: list[str] = field(default_factory=list)
    ctrl_rejects: list[str] = field(default_factory=list)
    roundtrip: list[tuple[float, bool]] = field(default_factory=list)


# ─────────────────────────────────────────── 1. the object
def section_summary() -> None:
    r = Resampler(rate=2.0)
    # Title and provenance belong to the executive summary, which
    # Report.executive renders ahead of everything here.
    R.md("## 1. The object")
    R.md()
    R.md(
        "A polyphase resampler with a **dual-mode engine**: output-driven "
        "for `rate >= 1` (interpolation) and an input-driven **transposed** "
        "form for `rate < 1` (decimation), where the delay line holds "
        "output samples and inputs accumulate across one output interval. "
        "The control port is the exception — it rides the interpolating "
        "structure at *every* rate, because that is the only one of the "
        "two steerable through unity in both directions, which is what "
        "closing a timing loop requires."
    )
    R.md()
    R.md("Design and API, not restated here:")
    R.md()
    R.md("- `native/inc/resamp/resamp_core.h` — the SSOT for every claim")
    R.md(
        "- `native/tests/test_resamp_core.c` — §1-§20, where every claim "
        "below is actually certified"
    )
    R.md("- [Resampler design](../../../../../../docs/design/RESAMPLER.md)")
    R.md("- `doppler.resample.Resampler` — the Python face measured here")
    R.md()
    R.md(
        f"Built-in bank: **{r.num_phases} phases x {r.num_taps} taps** "
        "(Kaiser, 60 dB, 0.4/0.6 normalised pass/stop)."
    )
    R.md()
    R.md(
        "**This report does not test the algorithm.** doppler is a C "
        "library and its claims are certified in C; every section here "
        "names the C section it tracks and measures the same property "
        "through the binding, which is what proves the binding delivers "
        "it. Where the binding cannot reach a claim at all, §2.11 says so "
        "instead of quietly omitting it."
    )
    R.md()


# ─────────────────────────────────────────── 2. characterise
def characterise() -> Data:
    d = Data()
    R.md("## 2. Characterisation")
    R.md()

    # ── C §3 / §4 ─────────────────────────────────────────────────
    R.md("### 2.1 Tone purity across the rate range (C §3, §4)")
    R.md()
    R.md(
        "The gate that catches the accumulator, and the one that was "
        "silently 55-60 dB down while every unity-rate test in the suite "
        "passed. It needs no timing convention, so it cannot beg the "
        "question a comparison against a reference implementation would."
    )
    R.md()
    for rate in (0.25, 0.3, 0.5, 0.7, 0.923, 1.0, 1.3, 2.0, 2.5, 4.0):
        f0 = 0.05
        y = Resampler(rate=rate).execute(tone(NIN, f0))
        d.rates.append(rate)
        d.purity.append(tone_residual_db(y, f0 / rate))
        d.counts.append(len(y))
        d.count_err.append(len(y) - rate * NIN)
    R.table(
        ["rate", "outputs", "vs `rate x n`", "tone residual (dB)"],
        [
            [f"{r:g}", n, f"{e:+.1f}", f"{p:.1f}"]
            for r, n, e, p in zip(d.rates, d.counts, d.count_err, d.purity)
        ],
    )
    _csv(
        DATA / "tone_purity.csv",
        [np.array(d.rates), np.array(d.purity), np.array(d.counts, float)],
        "rate,residual_db,outputs",
    )
    R.md(
        "Two populations, and the split is structural rather than a "
        "quality gradient: where `1/rate` is a whole number of samples "
        "(0.25, 0.5, 1.0) the fractional part is exactly zero, one arm is "
        "selected forever, and the residual sits at the float32 floor. "
        "Every other rate interpolates between arms and pays the bank's "
        "own error."
    )
    R.md()
    R.md("![tone purity](tone_purity.png)")
    R.md()

    # ── C §5 / §11 ────────────────────────────────────────────────
    R.md("### 2.2 The counting law (C §5, §11)")
    R.md()
    R.md(
        "Output count is the integral of the rate, independent of the "
        "bank and the tone. Every rate above lands within one sample of "
        "`rate x n` — the whole tolerance available, since a fractional "
        "rate leaves a partial output period outstanding at the end of "
        "any finite block. C §11 pins the same law at the level below, "
        "where one wrap of the control accumulator buys one **input** "
        "interval."
    )
    R.md()

    # ── C §13 ─────────────────────────────────────────────────────
    R.md("### 2.3 DC gain (C §13)")
    R.md()
    for rate in (0.25, 0.5, 1.0, 2.0, 4.0):
        y = Resampler(rate=rate).execute(np.ones(NIN, dtype=np.complex64))
        d.dc_rates.append(rate)
        d.dc_gain.append(float(np.mean(y[len(y) // 2 :].real)))
    R.table(
        ["rate", "measured DC gain", "error vs 1.0"],
        [
            [f"{r:g}", f"{g:.6f}", f"{g - 1.0:+.2e}"]
            for r, g in zip(d.dc_rates, d.dc_gain)
        ],
    )
    R.md(
        "`resamp_dc_gain()` **computes** arm 0's tap sum (1.000586, C §13) "
        "and has no Python binding, so this is the realised response "
        "measured through `execute`. They agree at unity, where only arm 0 "
        "is ever selected, and differ below it: a non-unity rate visits "
        "every arm, so the realised gain is the arm average. The spread is "
        "3.4e-4."
    )
    R.md()

    # ── C §17 ─────────────────────────────────────────────────────
    R.md("### 2.4 Decimating: the stopband, normalised to fs_out (C §17)")
    R.md()
    R.md(
        "Decimating, the filter protects the **lower** of the two rates, "
        "which is the output — so the bank's advertised 0.4/0.6 cutoffs "
        "are fractions of `fs_out`, and land at `0.4 x rate` and "
        "`0.6 x rate` on the input grid a caller actually feeds. Swept "
        "here at rate 0.5, so 0.4 of fs_out is input-normalised 0.20 and "
        "0.6 of fs_out is 0.30."
    )
    R.md()
    for f_out in (0.1, 0.2, 0.3, 0.4, 0.44, 0.5, 0.56, 0.6, 0.7, 0.9):
        rate = 0.5
        y = Resampler(rate=rate).execute(tone(NIN, f_out * rate))
        mag = float(np.abs(y[len(y) // 2 :]).mean())
        d.dec_f.append(f_out)
        d.dec_db.append(20.0 * np.log10(mag + 1e-20))
    R.table(
        ["freq (of fs_out)", "of fs_in", "output level (dB)"],
        [
            [f"{f:.2f}", f"{f * 0.5:.3f}", f"{m:.1f}"]
            for f, m in zip(d.dec_f, d.dec_db)
        ],
    )
    _csv(
        DATA / "decim_band.csv",
        [np.array(d.dec_f), np.array(d.dec_db)],
        "freq_of_fs_out,output_db",
    )
    R.md(
        "The -6 dB at 0.5 of fs_out is **the rule applied at its own "
        "edge**, not a stopband failure: that frequency is the output "
        "Nyquist, where the passband and its first image meet and each "
        "contributes half."
    )
    R.md()
    R.md("![decimating band](decim_band.png)")
    R.md()

    # ── C §17b ────────────────────────────────────────────────────
    R.md("### 2.5 Interpolating: the artifact floor (C §17b)")
    R.md()
    R.md(
        "The same bank, its other face. Interpolating, the lower rate is "
        "the **input**, so the 60 dB stopband becomes an anti-imaging "
        "filter — and the structure manufactures its own probe, since "
        "interpolation replicates the input spectrum at every multiple of "
        "`fs_in`. The rule is a floor, not a level at predicted "
        "frequencies: **any artifact above -60 dBc anywhere in the "
        "rejection band fails**, which is what makes fractional rates "
        "measurable at all — there the images do not sit still in the "
        "output grid."
    )
    R.md()
    R.md(
        "Measured with `ToneMeasure.worst_spur_dbc` on a coherent tone, "
        "so the carrier's own leakage cannot masquerade as a spur."
    )
    R.md()
    R.md(
        "**Stricter than C §17b, and deliberately so.** The C scan walks "
        "the rejection band `0.6/R .. Nyquist`; `worst_spur_dbc` is the "
        "worst spur *anywhere* in the spectrum. Where the band happens to "
        "be empty the two part company — at rate 1.5 with the mid-band "
        "tone the C floor reads -154 dBc and this reads -75.5, because "
        "the worst artifact is inside the passband where the C scan does "
        "not look. Everywhere else they agree within ~2 dB. Read a "
        "difference here as the wider search, not as a disagreement."
    )
    R.md()
    for rate in (1.5, 2.0, 2.5, 3.7, 8.0):
        row_lo, row_hi = [], []
        for f_t in (0.05, 0.35):
            f0 = coherent(f_t, NIN)
            y = Resampler(rate=rate).execute(tone(NIN, f0))
            (row_lo if f_t == 0.05 else row_hi).append(
                worst_spur_dbc(y[SKIP:])
            )
        d.img_rates.append(rate)
        d.img_lo.append(row_lo[0])
        d.img_hi.append(row_hi[0])
    R.table(
        ["rate", "worst spur, f0~0.05 (dBc)", "worst spur, f0~0.35 (dBc)"],
        [
            [f"{r:g}", f"{lo:.1f}", f"{hi:.1f}"]
            for r, lo, hi in zip(d.img_rates, d.img_lo, d.img_hi)
        ],
    )
    _csv(
        DATA / "image_floor.csv",
        [np.array(d.img_rates), np.array(d.img_lo), np.array(d.img_hi)],
        "rate,worst_spur_lo_dbc,worst_spur_hi_dbc",
    )
    R.md(
        "The near-edge tone is the whole measurement: it is where the "
        "images crowd the transition. C §17b's sabotage makes the point "
        "numerically — widening the bank's transition takes the 0.35 tone "
        "to -6 dBc while the 0.05 tone barely moves, so a test carrying "
        "only the mid-band tone would pass a bank with no usable stopband."
    )
    R.md()
    R.md("![artifact floor](image_floor.png)")
    R.md()

    # ── C §18 ─────────────────────────────────────────────────────
    R.md("### 2.6 `execute` vs `execute_ctrl`: a delay, not a defect (C §18)")
    R.md()
    R.md(
        "`execute` dispatches on rate and uses the transposed decimator "
        "below unity; `execute_ctrl` rides the interpolator at every rate. "
        "So with a zero control they are bit-identical at and above unity "
        "and **must** differ below it. The difference is large — order "
        "unity against a unit-amplitude signal — which reads alarming "
        "until both are projected: each is a clean tone, and what "
        "separates them is **group delay**, not quality."
    )
    R.md()
    for rate in (0.5, 0.7, 1.0, 2.0):
        f0 = 0.05
        x = tone(NIN, f0)
        a = Resampler(rate=rate).execute(x)
        b = Resampler(rate=rate).execute_ctrl(x, np.zeros(NIN))
        n = min(len(a), len(b))
        same = bool(np.array_equal(a[:n], b[:n]))
        m = np.arange(SKIP, n)
        basis = np.exp(2j * np.pi * (f0 / rate) * m)
        pa = np.vdot(basis, a[SKIP:n]) / np.vdot(basis, basis)
        pb = np.vdot(basis, b[SKIP:n]) / np.vdot(basis, basis)
        # Delay implied by the phase difference of the two fundamentals.
        delay = float(np.angle(pb / pa) / (2 * np.pi * (f0 / rate) + 1e-30))
        maxdiff = float(np.abs(a[:n] - b[:n]).max())
        d.struct.append((rate, same, len(b) - len(a), maxdiff, delay))
    R.table(
        [
            "rate",
            "`execute` structure",
            "identical",
            "count delta",
            "max abs diff",
            "implied delay (out samples)",
        ],
        [
            [
                f"{r:g}",
                "transposed decimator" if r < 1.0 else "interpolator",
                "yes" if s else "**no**",
                f"{dn:+d}",
                f"{md:.3f}",
                f"{dl:+.3f}",
            ]
            for r, s, dn, md, dl in d.struct
        ],
    )
    R.md()

    # ── C §8 ──────────────────────────────────────────────────────
    R.md("### 2.7 The unity window (C §8)")
    R.md()
    R.md(
        "Zero Doppler **is** rate 1.0, so a closing-then-opening geometry "
        "ramps the control straight through it. A private "
        "`(uint32_t)(frac * 2^32 + 0.5)` used to round past 2^32 into the "
        "undefined cast and stall the interpolator for a deviation in "
        "`(0, 1.16e-10]`. This sweep is the regression evidence."
    )
    R.md()
    for delta in (-1e-6, -1e-9, -1e-10, 0.0, 1e-10, 1e-9, 1e-6):
        c = np.full(NIN, delta)
        y = Resampler(rate=1.0).execute_ctrl(tone(NIN, 0.05), c)
        d.seam_delta.append(delta)
        d.seam_n.append(len(y))
        d.seam_purity.append(tone_residual_db(y, 0.05))
    R.table(
        ["ctrl delta", "outputs", "tone residual (dB)"],
        [
            [f"{dl:+.0e}", n, f"{p:.1f}"]
            for dl, n, p in zip(d.seam_delta, d.seam_n, d.seam_purity)
        ],
    )
    _csv(
        DATA / "unity_seam.csv",
        [
            np.array(d.seam_delta),
            np.array(d.seam_n, float),
            np.array(d.seam_purity),
        ],
        "ctrl_delta,outputs,residual_db",
    )
    R.md()
    R.md("![unity seam](unity_seam.png)")
    R.md()

    # ── the widened control port ──────────────────────────────────
    R.md("### 2.8 The control port takes a real `double`")
    R.md()
    x = tone(NIN, 0.05)
    accepted, rejected = [], []
    for label, c in (
        ("float64", np.full(NIN, 0.01)),
        ("float32", np.full(NIN, 0.01, dtype=np.float32)),
        ("Python list", [0.01] * NIN),
        ("complex64", np.full(NIN, 0.01, dtype=np.complex64)),
    ):
        try:
            Resampler(rate=1.0).execute_ctrl(x, c)
            accepted.append(label)
        except TypeError:
            rejected.append(label)
    d.ctrl_accepts = list(accepted)
    d.ctrl_rejects = list(rejected)
    R.table(
        ["ctrl dtype", "accepted"],
        [
            [lbl, "yes" if lbl in accepted else "**TypeError**"]
            for lbl in ("float64", "float32", "Python list", "complex64")
        ],
    )
    R.md(
        "A rate deviation is real and the port now says so — `double`, "
        "matching `execute_ctrl_push`'s scalar and the `double` the base "
        "rate is configured in. It was `complex64`, so half of every "
        "element was read and discarded with no error and no warning "
        "(F2). Anything numpy can safely widen to float64 is accepted; "
        "**`complex64` is now a `TypeError`**, because discarding an "
        "imaginary part is exactly the silent narrowing this removed and "
        "numpy will not make that cast on a caller's behalf."
    )
    R.md()

    # ── the newly bound observable ────────────────────────────────
    R.md("### 2.9 The control accumulator is observable (C §10-§12)")
    R.md()
    fresh = Resampler(rate=1.0)
    d.mu_fresh = float(fresh.ctrl_acc)
    steered = Resampler(rate=1.0)
    steered.execute_ctrl(tone(NIN, 0.05), np.full(NIN, 0.01))
    d.mu_steered = float(steered.ctrl_acc)
    settled = Resampler(rate=0.5)
    settled.execute_ctrl(tone(NIN, 0.05), np.zeros(NIN))
    d.mu_settled = float(settled.ctrl_acc)
    fixed = Resampler(rate=0.5)
    fixed.execute(tone(NIN, 0.05))
    d.mu_fixed = float(fixed.ctrl_acc)
    R.table(
        ["state", "`ctrl_acc`"],
        [
            ["fresh", f"{d.mu_fresh:.6f}"],
            ["steered off-rate (ctrl = 0.01)", f"{d.mu_steered:.6f}"],
            ["settled at an exact rate (ctrl = 0)", f"{d.mu_settled:.6f}"],
            ["driven through `execute()` instead", f"{d.mu_fixed:.6f}"],
        ],
    )
    R.md(
        "`mu` is the fractional delay applied to the stream and "
        "`floor(mu * num_phases)` is the arm the NEXT output reads. A "
        "steady value means the loop has settled on a sampling phase; one "
        "that slews and wraps means a residual RATE error, one input "
        "interval per wrap. The last row is the trap the property's own "
        "docstring warns about: this reports the CONTROL accumulator, so "
        "it stays 0.0 for a caller driving the object through `execute()`, "
        "whose free-running phase is a different accumulator with no "
        "accessor."
    )
    R.md()

    # ── C §6 ──────────────────────────────────────────────────────
    R.md("### 2.10 Serialized state round-trip (C §6)")
    R.md()
    for rate in (0.5, 0.7, 2.0):
        x = tone(NIN, 0.05)
        whole = Resampler(rate=rate).execute(x)
        a = Resampler(rate=rate)
        first = a.execute(x[: NIN // 2])
        b = Resampler(rate=rate)
        b.set_state(a.get_state())
        rest = b.execute(x[NIN // 2 :])
        split = np.concatenate([first, rest])
        n = min(len(whole), len(split))
        ok = len(whole) == len(split) and np.array_equal(whole[:n], split[:n])
        d.roundtrip.append((rate, bool(ok)))
    R.table(
        ["rate", "resumes bit-exactly"],
        [[f"{r:g}", "yes" if ok else "**no**"] for r, ok in d.roundtrip],
    )
    R.md()

    # ── C-ONLY ────────────────────────────────────────────────────
    R.md("### 2.11 Claims the binding cannot reach (C-ONLY)")
    R.md()
    R.md(
        "`Resampler` binds `execute`, `execute_ctrl`, `rate`, `ctrl_acc`, "
        "`num_phases`, `num_taps`, `reset` and the state triplet. Four "
        "public C entry points still have no binding, so their claims are "
        "certified by the C suite alone:"
    )
    R.md()
    R.table(
        ["C entry point", "claim", "C evidence"],
        [
            [
                "`resamp_dc_gain`",
                "arm 0's tap sum answers for every arm",
                "§13",
            ],
            [
                "`resamp_set_rate`",
                "retune preserves the accumulator and the delay line",
                "§15",
            ],
            [
                "`resamp_execute_ctrl_push`",
                "single-input streaming form, `double` control",
                "§8, §10-§12",
            ],
            [
                "`resamp_interp_inputs_needed`",
                "the streaming contract: exactly this many inputs, no over- "
                "or under-production",
                "§7, §20",
            ],
        ],
    )
    R.md()
    return d


# ─────────────────────────────────────────── 3. review
def review(d: Data) -> None:
    R.md("## 3. Review")
    R.md()

    R.find(
        "F1",
        "FIXED",
        "The control port's only observable had no Python binding. "
        "`resamp_get_ctrl_acc` is, in the header's words, 'the only way "
        "to see what a closed timing loop is actually doing to the "
        "sampling instant', and `Resampler` did not expose it — so a "
        "Python caller steering through `execute_ctrl` could not tell a "
        "settled loop from a slewing one, the single diagnostic the C API "
        "offers for the failure that cost a receiver its lock. Now bound "
        "as the read-only `Resampler.ctrl_acc` (2.9). The free-running "
        "phase used by `execute()` is still a separate accumulator with "
        "no accessor, and the property's docstring says so rather than "
        "leaving a caller to discover a permanent 0.0.",
    )
    R.find(
        "F2",
        "FIXED",
        "The block control port was typed `complex64` while its own "
        "streaming twin took a `double`, so half of every ctrl array was "
        "read and discarded silently. Now `double[]` on both faces, "
        "matching `nco` and `lo`, which had the same narrowing fixed. The "
        "one behaviour change worth knowing is on the Python face: "
        "`complex64` is now a TypeError rather than a silent truncation, "
        "because numpy will not safe-cast complex to float. Anything real "
        "still works, float32 and plain lists included (2.8). The "
        "production caller was `doppler_channel`, which computed its "
        "Doppler dilation in double and cast it down to reach this port; "
        "that cast is gone.",
    )
    R.find(
        "F3",
        "FIXED",
        "`resamp_get_ctrl_acc`'s docblock described the wrong structure. "
        "It said `mu` named 'the arm the last output read' and closed by "
        "conceding the opposite reading as a peculiarity of a decimating "
        "terminal stage. That phrasing belongs to the transposed "
        "`rate < 1` form, where the delay line holds output samples and "
        "the arm is selected before the advance — but this accessor "
        "reports `ctrl_phase`, and the control port rides the "
        "interpolating structure at every rate. So the exception's "
        "rate-dependence is spurious and 'the NEXT output's arm' holds "
        "throughout (C §10, at 0.7, 0.923, 1.3 and 2.5). Corrected in the"
        "header, which now states the NEXT-output reading, says it holds at"
        "every rate and why, and drops the spurious `rate <= 1` carve-out.",
    )
    R.find(
        "F4",
        "FIXED",
        "The same docblock stated the slip unit as output periods: 'one "
        "cycle of wrap is one output period of slip'. A wrap buys one "
        "**input** interval (C §11, measured against the counting law at "
        "rates 0.7 and 0.3). The two coincide only at unity, which is "
        "presumably where the sentence was checked. Corrected in the header.",
    )
    R.find(
        "F5",
        "BY DESIGN",
        "`execute` and `execute_ctrl` differ below unity by GROUP DELAY, "
        "not quality (§2.6). The raw difference is order unity against a "
        "unit-amplitude signal, which reads like a defect until both are "
        "projected and each is a clean tone. They are the documented "
        "dual-mode engine working as specified, and are not "
        "interchangeable sample-for-sample below unity — a distinction "
        "invisible from the Python signatures, which differ only by an "
        "extra argument.",
    )
    R.find(
        "F6",
        "BY DESIGN",
        "The -6 dB at the output Nyquist (§2.4) is the folding rule at "
        "its own edge, not a stopband failure: the passband and its first "
        "image meet there and each contributes half.",
    )
    R.find(
        "F7",
        "FIXED",
        "The unity window. A private `(uint32_t)(frac * 2^32 + 0.5)` in "
        "`resamp_execute_ctrl_push` rounded past 2^32 into the undefined "
        "cast, stalling the interpolator for a deviation in "
        "(0, 1.16e-10]. Zero Doppler is rate 1.0, so a ramp through "
        "closest approach crosses it. §2.7 is the regression evidence; "
        "the conversion is now confined and gated by "
        "`make lint-phase-conversion`.",
    )
    R.find(
        "F8",
        "FIXED",
        "`resamp_dc_gain` named arm 0's tap sum without saying so, but the "
        "realised DC gain "
        "at a non-unity rate is the arm average (§2.3): 1.000586 computed "
        "against 1.000249-1.000293 measured. The spread is 3.4e-4 and of "
        "no practical consequence; it is a naming claim, not a numeric "
        "one. Corrected in the header, which now names it as ARM 0's gain "
        "and tabulates the measured arm average beside it.",
    )
    R.find(
        "F9",
        "FIXED",
        "`resamp_interp_inputs_needed`'s docblock UNDERSTATED its own "
        "guarantee. It scopes exactness to an integer interpolation factor "
        "-- 'for an integer interpolation factor ... this is exact, so a "
        "caller can generate precisely this many inputs' -- but the "
        "prediction and the fill run the same recurrence on the same "
        "phase_inc, so they cannot disagree at any rate. Measured exact "
        "across 1800 mid-stream calls at nine rates with randomised "
        "`max_out`, worst deviation zero (C §20). A caller reading the "
        "header is told not to rely on the streaming contract at a "
        "fractional rate, when they can. Understating a guarantee is a "
        "milder defect than overstating one, but it is the same kind: the "
        "header is not what the code does. Corrected in the header, which now"
        "states the guarantee holds at every rate, explains that it is"
        "structural rather than numeric, and keeps the old integer-factor"
        "reading as the different (real) property it actually is.",
    )
    R.md()
    R.table(
        ["tag", "verdict", "finding"],
        [[t, v, x] for t, v, x in R.findings],
    )
    R.md()


# ─────────────────────────────────────────── 4. limits
def limits(d: Data) -> None:
    R.md("## 4. Limits")
    R.md()
    R.md(
        "Claims a caller may rely on. A failure here is a regression, not "
        "a new finding — every one is asserted by "
        "`src/doppler/resample/tests/test_validation_limits.py`, which "
        "runs this same `build()`."
    )
    R.md()

    exact = [
        p
        for r, p in zip(d.rates, d.purity)
        if abs(1.0 / r - round(1.0 / r)) < 1e-12
    ]
    frac = [
        p
        for r, p in zip(d.rates, d.purity)
        if abs(1.0 / r - round(1.0 / r)) >= 1e-12
    ]
    R.limit(
        max(exact) < -120.0,
        f"Where `1/rate` is a whole number the residual is at the float32 "
        f"floor: worst {max(exact):.1f} dB < -120 dB",
    )
    R.limit(
        max(frac) < -60.0,
        f"At every other rate the bank holds the tone above 60 dB clean: "
        f"worst {max(frac):.1f} dB < -60 dB",
    )
    R.limit(
        max(abs(e) for e in d.count_err) <= 1.0,
        f"Output count is the integral of the rate to within one sample: "
        f"worst {max(abs(e) for e in d.count_err):+.1f}",
    )
    R.limit(
        all(abs(g - 1.0) < 1e-3 for g in d.dc_gain),
        f"DC gain is unity to 1e-3 at every rate: worst "
        f"{max(abs(g - 1.0) for g in d.dc_gain):.2e}",
    )
    stop = [db for f, db in zip(d.dec_f, d.dec_db) if f >= 0.6]
    R.limit(
        max(stop) < -60.0,
        f"Decimating, everything from 0.6 of fs_out is rejected by more "
        f"than 60 dB: worst {max(stop):.1f} dB",
    )
    R.limit(
        max(d.img_lo + d.img_hi) < -60.0,
        f"Interpolating, no artifact anywhere in the rejection band rises "
        f"above -60 dBc, at fractional rates included: worst "
        f"{max(d.img_lo + d.img_hi):.1f} dBc",
    )
    tiny = [
        (dl, n, p)
        for dl, n, p in zip(d.seam_delta, d.seam_n, d.seam_purity)
        if abs(dl) <= 1e-9
    ]
    R.limit(
        all(n >= NIN for _, n, _ in tiny),
        "A control deviation inside the old unity window still produces a "
        "full output block (the interpolator does not stall)",
    )
    R.limit(
        all(p < -120.0 for _, _, p in tiny),
        f"...and the stream stays a pure tone through it: worst "
        f"{max(p for _, _, p in tiny):.1f} dB",
    )
    R.limit(
        {"float64", "float32", "Python list"} <= set(d.ctrl_accepts),
        "`execute_ctrl` accepts any real control numpy can widen to "
        "float64 — float32 and a plain list included",
    )
    R.limit(
        d.ctrl_rejects == ["complex64"],
        "...and rejects a complex control outright rather than silently "
        "discarding its imaginary half",
    )
    R.limit(
        d.mu_fresh == 0.0 and 0.0 < d.mu_steered < 1.0,
        f"`ctrl_acc` is observable from Python and lives in [0, 1): 0.0 "
        f"fresh, {d.mu_steered:.4f} under a steer",
    )
    R.limit(
        d.mu_fixed == 0.0,
        "`ctrl_acc` reports the CONTROL accumulator, so it stays 0.0 for "
        "a caller driving the object through `execute()`",
    )
    R.limit(
        all(ok for _, ok in d.roundtrip),
        "Serialized state resumes a split stream bit-exactly at every rate",
    )
    R.limit(
        all(s for r, s, _, _, _ in d.struct if r >= 1.0),
        "At and above unity, a zero control reproduces `execute` bit-for-bit",
    )
    R.md()


# ─────────────────────────────────────────── plots
def plots(d: Data) -> None:
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    fig, ax = plt.subplots(figsize=(7.2, 3.6))
    ax.plot(d.rates, d.purity, "o-", lw=1.4)
    ax.axhline(-60.0, ls="--", lw=1, color="crimson", label="limit (-60 dB)")
    ax.set_xscale("log")
    ax.set_xlabel("rate (output/input)")
    ax.set_ylabel("tone residual (dB)")
    ax.set_title("A resampled tone is still a tone")
    ax.grid(alpha=0.3)
    ax.legend()
    fig.tight_layout()
    fig.savefig(HERE / "tone_purity.png", dpi=110)
    plt.close(fig)

    fig, ax = plt.subplots(figsize=(7.2, 3.6))
    ax.plot(d.dec_f, d.dec_db, "o-", lw=1.4)
    ax.axvline(0.4, ls=":", lw=1, color="grey", label="passband edge (0.4)")
    ax.axvline(0.6, ls=":", lw=1, color="darkgreen", label="stopband (0.6)")
    ax.axhline(-60.0, ls="--", lw=1, color="crimson", label="-60 dB")
    ax.set_xlabel("frequency (fraction of fs_out)")
    ax.set_ylabel("output level (dB)")
    ax.set_title("Decimating at rate 0.5: the band, normalised to fs_out")
    ax.grid(alpha=0.3)
    ax.legend(fontsize=8)
    fig.tight_layout()
    fig.savefig(HERE / "decim_band.png", dpi=110)
    plt.close(fig)

    fig, ax = plt.subplots(figsize=(7.2, 3.6))
    ax.plot(d.img_rates, d.img_lo, "o-", lw=1.4, label="f0 ~ 0.05")
    ax.plot(d.img_rates, d.img_hi, "s-", lw=1.4, label="f0 ~ 0.35")
    ax.axhline(-60.0, ls="--", lw=1, color="crimson", label="-60 dBc")
    ax.set_xlabel("rate (output/input)")
    ax.set_ylabel("worst spur (dBc)")
    ax.set_title("Interpolating: the artifact floor")
    ax.grid(alpha=0.3)
    ax.legend(fontsize=8)
    fig.tight_layout()
    fig.savefig(HERE / "image_floor.png", dpi=110)
    plt.close(fig)

    fig, ax = plt.subplots(figsize=(7.2, 3.6))
    ax.plot(range(len(d.seam_delta)), d.seam_purity, "o-", lw=1.4)
    ax.set_xticks(range(len(d.seam_delta)))
    ax.set_xticklabels([f"{v:+.0e}" for v in d.seam_delta], rotation=30)
    ax.set_xlabel("control deviation from unity")
    ax.set_ylabel("tone residual (dB)")
    ax.set_title("The unity window: the interpolator no longer stalls")
    ax.grid(alpha=0.3)
    fig.tight_layout()
    fig.savefig(HERE / "unity_seam.png", dpi=110)
    plt.close(fig)


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
    section_summary()
    d = characterise()
    review(d)
    limits(d)
    R.executive(
        "resamp",
        [
            "**`mu` is the diagnostic for a closed timing loop**, and it is "
            "now bound: steady `mu` means settled, a slewing or wrapping `mu` "
            "is a residual RATE error, and one wrap is one INPUT interval of "
            "slip (§2.5, F4). Poll it before believing a lock statistic.",
            "**The -6 dB at the output Nyquist is the folding rule at its own "
            "edge, not a stopband failure** (§2.4). Judge the filter inside "
            "the band you asked for, not at the seam.",
            "**`execute` and `execute_ctrl` differ by GROUP DELAY below "
            "unity, not by quality** (§2.6) — so a comparison between them "
            "measures alignment unless you account for it first.",
            "**Rate 1.0 was a trap and is fixed.** A private rounding cast "
            "reached past 2^32 and stalled the interpolator for a window just "
            "above unity — and zero Doppler IS rate 1.0, so any ramp through "
            "closest approach crossed it (F7).",
        ],
    )
    if write:
        plots(d)
    R.summary(
        "\n- Raw sweeps: `data/tone_purity.csv`, `data/decim_band.csv`, "
        "`data/image_floor.csv`, `data/unity_seam.csv`"
    )
    R.emit(HERE / "results.md")
    return R


if __name__ == "__main__":
    sys.exit(cli(build, HERE))
