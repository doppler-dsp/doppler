"""M-PSK constellation — certification evidence, through the binding.

Run directly to regenerate `results.md` and the CSVs:

    uv run python src/doppler/mpsk/tests/validation/mpsk/validate.py

`--check` re-renders in memory and diffs against the committed bytes;
`make validate` writes, `make validate-check` checks. Every limit this
records is asserted by `src/doppler/mpsk/tests/test_validation_limits.py`,
which runs this same `build(write=False)`.

The order is the campaign's, not this file's:
`native/inc/mpsk/mpsk_core.h` is the SSOT,
`native/tests/test_mpsk_core.c` certifies it in C, and
`native/validation/mpsk_diff_penalty.c` measures the differential penalty
by Monte Carlo. This file measures the same properties through
`doppler.mpsk` to show the binding delivers them.

**The binding does not reach everything.** `mpsk_slice`, `mpsk_phi0`,
`mpsk_constellation` and the two Gray helpers are `JM_FORCEINLINE` in the
header with no Python face, and the decision-directed error signal built
from `ahat` is the surface a composing receiver depends on most. Those are
C-only and are certified in `test_mpsk_core.c`; this report says so rather
than quietly reporting a clean bill of health for the surface that matters
least.
"""

from __future__ import annotations

import math
import sys
from dataclasses import dataclass, field
from pathlib import Path

import numpy as np

from doppler.mpsk import (
    mpsk_bits_per_symbol,
    mpsk_demap,
    mpsk_diff_demap,
    mpsk_diff_map,
    mpsk_map,
)
from doppler.source import AWGN
from doppler.tests._repo import repo_root
from doppler.tests._validation_common import Report, cli

HERE = Path(__file__).resolve().parent
DATA = HERE / "data"
ROOT = repo_root(__file__)

R = Report()

M_ALL = (2, 4, 8)

#: Es/N0 grid per M, dB. PER-M deliberately: one shared grid starves the
#: low-order constellations, because at a given Es/N0 BPSK's error rate is
#: orders of magnitude below 8PSK's. A first pass used a shared
#: (4, 8, 12) dB and BPSK at 12 dB collected ZERO errors in 200k symbols —
#: which then read as a 100% deviation from theory, i.e. a defect. Each
#: grid below is chosen so its deepest cell still collects a few hundred
#: errors at NSYM, and MIN_ERRORS asserts that rather than trusting it.
#: The deep cells belong to the C harness, which can afford 4e7 symbols;
#: this file runs on every push and must not.
ESN0_BY_M = {2: (2.0, 4.0, 6.0), 4: (4.0, 8.0, 10.0), 8: (8.0, 12.0, 14.0)}
NSYM = 200_000
MIN_ERRORS = 200
NOISE_SEED = 20260814
DATA_SEED = 7


def _qfunc(x: float) -> float:
    """Gaussian tail probability."""
    return 0.5 * math.erfc(float(x) / math.sqrt(2.0))


def ser_theory(m: int, esn0_db: float) -> float:
    """Closed-form coherent symbol-error rate.

    The EXTERNAL truth. A ratio of two measured paths cannot detect a
    defect the two share, so the coherent path is anchored here before
    the differential penalty is quoted against it.
    """
    esn0 = 10.0 ** (esn0_db / 10.0)
    if m == 2:
        return _qfunc(math.sqrt(2.0 * esn0))
    return 2.0 * _qfunc(math.sqrt(2.0 * esn0) * math.sin(math.pi / m))


@dataclass
class Data:
    """Everything the report measures, so review and limits share one run."""

    amp: dict[int, float] = field(default_factory=dict)
    gray_ok: dict[int, bool] = field(default_factory=dict)
    roundtrip_ok: dict[int, bool] = field(default_factory=dict)
    scale_ok: dict[int, bool] = field(default_factory=dict)
    labels_in_range: bool = True
    qpsk_axis_err: float = 0.0
    bpsk_err: float = 0.0
    ser: dict[tuple[int, float], tuple[float, float, float]] = field(
        default_factory=dict
    )
    n_err: dict[tuple[int, float], int] = field(default_factory=dict)
    diff_rt_ok: dict[int, bool] = field(default_factory=dict)
    diff_rot_ok: dict[int, bool] = field(default_factory=dict)
    diff_arb_ok: dict[int, bool] = field(default_factory=dict)
    diff_first_moves: dict[int, bool] = field(default_factory=dict)


def _labels(m: int, n: int, seed: int) -> np.ndarray:
    """Uniform Gray labels. A separate stream from the noise, always."""
    rng = np.random.default_rng(seed)
    return rng.integers(0, m, n).astype(np.uint8)


def section_object() -> None:
    R.md("## 1. The object")
    R.md()
    R.md(
        "`doppler.mpsk` is the M-PSK constellation: the Gray-coded map "
        "from a label byte to a unit-amplitude point, the hard decision "
        "back, and the differential pair that carries information on "
        "phase *differences*. It is not a receiver — it is the primitive "
        "a receiver decides with, and `mpsk_rx_loops.h` calls its slicer "
        "once per symbol."
    )
    R.md()
    R.md(
        "The design, including why QPSK alone carries a π/4 offset and "
        "what differential mode costs, is "
        "[docs/design/mpsk.md §9](../../../../../../docs/design/mpsk.md)."
    )
    R.md()
    R.md(
        "**Surfaces this report cannot reach.** `mpsk_slice`, "
        "`mpsk_constellation`, `mpsk_phi0`, `mpsk_gray_encode` and "
        "`mpsk_gray_decode` are inline in the header with no binding, and "
        "the decision `ahat` — whose `Im(y·conj(ahat))` is the "
        "decision-directed carrier error — is a C-only output. They are "
        "certified in `native/tests/test_mpsk_core.c`; nothing below "
        "measures them, and a report that did not say so would be "
        "claiming coverage it does not have."
    )
    R.md()


def characterise() -> Data:
    R.md("## 2. Characterisation")
    R.md()
    d = Data()

    # ── 2.1 geometry ────────────────────────────────────────────────
    R.md("### 2.1 Constellation geometry")
    R.md()
    rows = []
    for m in M_ALL:
        pts = mpsk_map(np.arange(m, dtype=np.uint8), m)
        amp = float(np.max(np.abs(np.abs(pts) - 1.0)))
        d.amp[m] = amp
        rows.append(
            [
                str(m),
                f"{amp:.2e}",
                f"{np.degrees(np.angle(pts[0])):+.1f}",
                str(mpsk_bits_per_symbol(m)),
            ]
        )
    R.table(["M", "max ||p| − 1|", "label 0 angle (deg)", "bits/symbol"], rows)
    R.md()

    pts4 = mpsk_map(np.arange(4, dtype=np.uint8), 4)
    d.qpsk_axis_err = float(
        np.max(np.abs(np.abs(pts4.real) - np.abs(pts4.imag)))
    )
    pts2 = mpsk_map(np.arange(2, dtype=np.uint8), 2)
    d.bpsk_err = float(
        max(np.max(np.abs(pts2.imag)), np.max(np.abs(np.abs(pts2.real) - 1.0)))
    )
    R.md(
        f"QPSK is axis-separable: `||I| − |Q||` peaks at "
        f"{d.qpsk_axis_err:.2e}, so each of the two bits rides its own "
        f"axis. BPSK sits on the real axis to {d.bpsk_err:.2e}."
    )
    R.md()

    # ── 2.2 Gray labelling ──────────────────────────────────────────
    R.md("### 2.2 Gray labelling")
    R.md()
    for m in M_ALL:
        sym = np.arange(m, dtype=np.uint8)
        pts = mpsk_map(sym, m)
        order = np.argsort(np.angle(pts) % (2 * np.pi))
        lab = sym[order]
        ok = all(
            bin(int(lab[i]) ^ int(lab[(i + 1) % m])).count("1") == 1
            for i in range(m)
        )
        d.gray_ok[m] = ok
    R.md(
        "Sorting the constellation by angle and walking it cyclically, "
        "every neighbouring pair of labels differs in exactly one bit for "
        f"M = {', '.join(str(m) for m in M_ALL if d.gray_ok[m])}. The wrap "
        "from the last point back to label 0 is included, which is where "
        "a near-miss labelling breaks and where a noisy symbol is most "
        "likely to slip."
    )
    R.md()

    # ── 2.3 round-trip and amplitude invariance ─────────────────────
    R.md("### 2.3 Round-trip and amplitude invariance")
    R.md()
    scales = (1e-6, 1e-3, 1.0, 1e3, 1e6)
    for m in M_ALL:
        sym = _labels(m, 4096, DATA_SEED + m)
        pts = mpsk_map(sym, m)
        d.roundtrip_ok[m] = bool(np.array_equal(mpsk_demap(pts, m), sym))
        ok = True
        for s in scales:
            got = mpsk_demap((s * pts).astype(np.complex64), m)
            ok = ok and bool(np.array_equal(got, sym))
            if int(got.max()) >= m:
                d.labels_in_range = False
        d.scale_ok[m] = ok
    R.md(
        "`demap(map(x)) == x` exactly over 4096 random labels at every M, "
        "and the decision is unchanged when the symbol is scaled across "
        f"{scales[0]:.0e} to {scales[-1]:.0e} — the phase is the only "
        "input to it. Every label returned is below M."
    )
    R.md()

    # ── 2.4 error rate against theory ───────────────────────────────
    R.md("### 2.4 Symbol-error rate, and what differential costs")
    R.md()
    rows = []
    for m in M_ALL:
        for e in ESN0_BY_M[m]:
            sym = _labels(m, NSYM, DATA_SEED + m * 31 + int(e))
            esn0 = 10.0 ** (e / 10.0)
            sigma = math.sqrt(1.0 / (2.0 * esn0))
            noise = AWGN(NOISE_SEED + m * 7 + int(e), sigma).generate(NSYM)
            coh = (mpsk_map(sym, m) + noise).astype(np.complex64)
            dif = (mpsk_diff_map(sym, m) + noise).astype(np.complex64)
            n_err = int(np.count_nonzero(mpsk_demap(coh, m) != sym))
            ser_c = n_err / NSYM
            ser_d = float(np.mean(mpsk_diff_demap(dif, m) != sym))
            want = ser_theory(m, e)
            d.ser[(m, e)] = (ser_c, ser_d, want)
            d.n_err[(m, e)] = n_err
            rows.append(
                [
                    str(m),
                    f"{e:.0f}",
                    f"{ser_c:.3e}",
                    f"{want:.3e}",
                    f"{100.0 * (ser_c - want) / want:+.1f}%",
                    f"{ser_d:.3e}",
                    f"{ser_d / ser_c:.2f}x" if ser_c > 0 else "—",
                    f"{n_err:,}",
                ]
            )
    R.table(
        [
            "M",
            "Es/N0 dB",
            "SER coherent",
            "theory",
            "err",
            "SER differential",
            "penalty",
            "errors",
        ],
        rows,
    )
    R.md()
    R.md(
        "Both modes see the **same noise realisation** — the penalty is a "
        "paired measurement, so the seed's luck cancels out of the ratio "
        "— and the coherent column is anchored to closed-form theory so "
        "that a defect shared by both paths cannot divide out to a "
        "plausible 2.0."
    )
    R.md()
    R.md(
        f"The rightmost column is the precondition, not decoration: a cell "
        f"that collected too few errors makes both comparisons "
        f"meaningless and reads exactly like a passing one. Every cell "
        f"here clears {MIN_ERRORS}, and §4 asserts it."
    )
    R.md()

    if R.write:
        DATA.mkdir(exist_ok=True)
        with (DATA / "ser.csv").open("w", encoding="utf-8") as fh:
            fh.write(
                "m,esn0_db,ser_coherent,ser_theory,ser_differential,n_errors\n"
            )
            for (m, e), (c, dd, w) in sorted(d.ser.items()):
                fh.write(
                    f"{m},{e},{c:.6e},{w:.6e},{dd:.6e},{d.n_err[(m, e)]}\n"
                )

    # ── 2.5 differential invariance ─────────────────────────────────
    R.md("### 2.5 Differential mode under an unknown carrier phase")
    R.md()
    for m in M_ALL:
        sym = _labels(m, 2000, DATA_SEED + 100 + m)
        pts = mpsk_diff_map(sym, m)
        d.diff_rt_ok[m] = bool(np.array_equal(mpsk_diff_demap(pts, m), sym))
        rot_ok, arb_ok, first_moves = True, True, False
        for j in range(m):
            rot = (pts * np.exp(2j * np.pi * j / m)).astype(np.complex64)
            got = mpsk_diff_demap(rot, m)
            rot_ok = rot_ok and bool(np.array_equal(got[1:], sym[1:]))
            if j and got[0] != sym[0]:
                first_moves = True
        for phi in (0.137, 0.611, 1.049, 2.718, 4.001):
            rot = (pts * np.exp(1j * phi)).astype(np.complex64)
            got = mpsk_diff_demap(rot, m)
            arb_ok = arb_ok and bool(np.array_equal(got[1:], sym[1:]))
        d.diff_rot_ok[m] = rot_ok
        d.diff_arb_ok[m] = arb_ok
        d.diff_first_moves[m] = first_moves
    R.md(
        "Every symbol after the first survives an arbitrary constant "
        "rotation — not merely the M discrete constellation rotations, "
        "which is the weaker claim usually made for differential mode. A "
        "constant offset shifts every sliced index equally, so it cancels "
        "in the difference regardless of its size. The first symbol "
        "references the implicit zero-phase start and does move; that is "
        "the design, not a defect."
    )
    R.md()
    return d


def review(d: Data) -> None:
    R.md("## 3. Findings, with verdicts")
    R.md()
    lo = min(dd / c for (c, dd, _w) in d.ser.values() if c > 0)
    hi = max(dd / c for (c, dd, _w) in d.ser.values() if c > 0)
    # NB there is no finding for "the coherent path tracks theory". That is
    # not a judgement, it is a limit (§4), and recording it here as well
    # would inflate the finding count with a result that is already gated.
    # A verdict is a judgement about a PROBLEM; the review phase has no
    # vocabulary for "this works", deliberately.
    R.find(
        "F1",
        "FIXED",
        f"The header's `~2x` differential penalty is an ASYMPTOTE, not a "
        f"constant: measured {lo:.2f}x to {hi:.2f}x across this grid, "
        "smallest for the largest M at the lowest Es/N0. A caller sizing "
        "a link at low Es/N0 is charged less than the round number "
        "suggests. The header stated it flatly and is corrected, on both "
        "Python faces, in the design doc and on the gallery page — which "
        "was repeating the flat figure while demonstrating it on 8PSK, "
        "the case furthest from it.",
    )
    R.find(
        "F2",
        "C-ONLY",
        "The decision `ahat` and the inline helpers have no Python face, "
        "so the decision-directed carrier error `Im(y·conj(ahat))` — the "
        "surface `mpsk_rx_loops.h` depends on most — cannot be measured "
        "here. It is pinned in `test_mpsk_core.c` §5e, which asserts the "
        "error is zero on-point, signed with the rotation and monotone "
        "in it.",
    )
    R.find(
        "F3",
        "GAP",
        "`mpsk_core` is folded into no library: `mpsk_map`/`mpsk_demap` "
        "are absent from `libdoppler.a` and `.so`, so a C caller cannot "
        "link them and no C doc snippet can demonstrate them. The Python "
        "face is unaffected, which is why it went unnoticed. Filed as "
        "[#747](https://github.com/doppler-dsp/doppler/issues/747); the "
        "fix wants its own gate and is out of scope here.",
    )
    R.find(
        "F4",
        "BY DESIGN",
        "A rotated differential stream decodes symbol 0 wrongly. It "
        "references the implicit zero-phase start, so there is nothing "
        "for it to difference against — the cost of needing no preamble. "
        "A caller who cannot spend the symbol should send a known first "
        "label.",
    )
    R.find(
        "F5",
        "FIXED",
        "`test_carrier_mpsk_core.c` carried a private O(M) correlation "
        "search instead of calling `mpsk_slice`, so the carrier-loop test "
        "scored against its own slicer and the two could disagree with no "
        "gate noticing. It now delegates; the equivalence it assumed is "
        "proven in `test_mpsk_core.c` §5b.",
    )


def limits(d: Data) -> None:
    R.md("## 4. Limits")
    R.md()
    R.md(
        "Claims a caller may rely on. A failure here is a regression, "
        "not a new finding."
    )
    R.md()
    for m in M_ALL:
        R.limit(
            d.amp[m] < 1e-6,
            f"M={m}: every constellation point is unit amplitude to 1e-6",
        )
    R.limit(
        d.qpsk_axis_err < 1e-6,
        "QPSK is axis-separable: ||I| − |Q|| < 1e-6 at every point",
    )
    R.limit(d.bpsk_err < 1e-6, "BPSK points are ±1 on the real axis to 1e-6")
    for m in M_ALL:
        R.limit(
            d.gray_ok[m],
            f"M={m}: cyclically adjacent constellation points differ in "
            f"exactly one bit",
        )
    for m in M_ALL:
        R.limit(
            d.roundtrip_ok[m] and d.scale_ok[m],
            f"M={m}: demap(map(x)) == x, and is unchanged over 12 decades "
            f"of amplitude",
        )
    R.limit(
        d.labels_in_range,
        "every demapped label is below M — only the low log2(M) bits are "
        "ever set",
    )
    R.limit(
        [mpsk_bits_per_symbol(m) for m in M_ALL] == [1, 2, 3],
        "mpsk_bits_per_symbol is log2(M) for M in {2, 4, 8}",
    )
    R.limit(
        all(mpsk_bits_per_symbol(m) == 0 for m in (0, 1, 3, 5, 6, 7, 16)),
        "mpsk_bits_per_symbol is 0 for an unsupported M, so a caller can "
        "reject one",
    )
    for m in M_ALL:
        R.limit(
            d.diff_rt_ok[m],
            f"M={m}: differential map/demap round-trips exactly",
        )
    for m in M_ALL:
        R.limit(
            d.diff_rot_ok[m] and d.diff_arb_ok[m],
            f"M={m}: differential decoding is invariant to ANY constant "
            f"carrier phase, past the first symbol",
        )
    worst = max(abs(c - w) / w for (c, _dd, w) in d.ser.values() if w > 0)
    R.limit(
        worst < 0.15,
        f"coherent SER is within 15% of theory at every measured cell "
        f"(worst {100.0 * worst:.1f}%)",
    )
    hi = max(dd / c for (c, dd, _w) in d.ser.values() if c > 0)
    R.limit(
        hi < 2.20,
        f"the differential penalty never exceeds 2.2x (worst {hi:.2f}x) — "
        f"the header's ~2x is an upper bound, not a typical value",
    )
    ser_c, ser_d, _w = d.ser[(4, 10.0)]
    R.limit(
        1.80 < ser_d / ser_c < 2.20,
        f"QPSK at 10 dB Es/N0 pays {ser_d / ser_c:.2f}x for differential "
        f"mode — the asymptote is reached where a receiver operates",
    )
    fewest = min(d.n_err.values())
    R.limit(
        fewest >= MIN_ERRORS,
        f"every error-rate cell collected at least {MIN_ERRORS} errors "
        f"(fewest {fewest:,}) — without this the comparisons above pass "
        f"vacuously on a starved cell",
    )


def build(write: bool = True) -> Report:
    global R
    R = Report(write=write)
    R.md("# M-PSK constellation — validation report")
    R.md()
    section_object()
    d = characterise()
    review(d)
    limits(d)
    lo = min(dd / c for (c, dd, _w) in d.ser.values() if c > 0)
    hi = max(dd / c for (c, dd, _w) in d.ser.values() if c > 0)
    R.executive(
        "M-PSK constellation",
        [
            "**The decision rule is correct against an external truth, "
            "not merely self-consistent.** Coherent SER tracks "
            "closed-form theory across every cell, so the slicer every "
            "M-PSK consumer decides through is anchored rather than "
            "assumed (§2.4, F1).",
            "**`~2x` for differential mode is an asymptote, not a "
            f"constant** — measured {lo:.2f}x to {hi:.2f}x. It is an "
            "upper bound reached where a receiver operates, and the "
            "caller is charged less at low Es/N0 (§2.4, F1).",
            "**Differential mode survives ANY constant carrier phase**, "
            "not just the M constellation rotations — the stronger claim, "
            "and the one that makes it worth its penalty (§2.5).",
            "**The most important surface has no Python face.** The "
            "decision `ahat` and the inline helpers are C-only; this "
            "report says what it cannot reach rather than reporting a "
            "clean bill of health for the rest (F2).",
            "**`mpsk_core` is in no library** — the C face of this module "
            "cannot be linked at all, which every Python gate is blind to "
            "(F3, [#747](https://github.com/doppler-dsp/doppler/issues/747)).",
        ],
    )
    R.summary("\n- Raw sweep: `data/ser.csv`")
    R.emit(HERE / "results.md")
    return R


if __name__ == "__main__":
    sys.exit(cli(build, HERE))
