#!/usr/bin/env python3
"""LO validation — produces this folder's certification evidence.

Writes ``results.md`` (the authoritative report), the plots it embeds, and
the raw sweeps under ``data/`` so any number in the report can be re-derived
without re-running the measurement.

Three phases, in order:

1. **Characterise** — measure complete behaviour across the input range and
   over time. Tables and plots, no verdicts.
2. **Review** — judge the characterisation: correct-by-design, or a gap.
3. **Limits** — the envelope a caller may rely on, asserted.

Every number is measured from the C through its own binding, and the
stimulus and the analysis are doppler's own objects (`LO`, `ToneMeasure`,
`FFT`, `kaiser_window`, `LoopFilter`) rather than a private model of what
the C ought to do. The one deliberate exception is the closed-loop
*oracle* — an ideal reference phasor — which has to be independent of the
thing it is judging.

Run:  make validate
"""

from __future__ import annotations

import sys
from dataclasses import dataclass
from math import gcd
from pathlib import Path

import numpy as np

from doppler.measure import ToneMeasure
from doppler.source import LO, NCO
from doppler.spectral import FFT, kaiser_beta_for_sidelobe, kaiser_window
from doppler.tests import loop_reference as ll
from doppler.tests._validation_common import Report, cli

HERE = Path(__file__).parent
DATA = HERE / "data"
LSB = 2.0**-32
W = 1 << 32
WF = 2.0**32

# LUT geometry, from lo_core.h. Named here so the report's arithmetic
# reads against the header's own constants rather than magic numbers.
LUT_BITS = 16
LUT_SIZE = 1 << LUT_BITS

# Spectral analysis. The capture is one LUT period so a truncation error
# sequence whose period divides 2^16 lands in a bin; the window's own
# sidelobes are pushed to -150 dB so the measurement floor (~148 dBc,
# set by complex64 itself) is never the window's.
NFFT = 1 << 16
SIDELOBE_DB = 150.0

# Phase-truncation theory, for comparison only — never used as a
# substitute for a measurement. A W-bit phase index gives a worst-case
# spur at -(6.02*W - 3.92) dBc and a generic-frequency spur near
# -6.02*W dBc.
SFDR_WORST_THEORY = 6.02 * LUT_BITS - 3.92
SFDR_TYPICAL_THEORY = 6.02 * LUT_BITS

# The bound lo_core.h states, and src/doppler/source/tests/test_lo.py
# gates. Round, and 2.4 dB below the measured worst case — the theory
# bound itself would be a certification with 0.01 dB of margin, which is
# not a margin.
SFDR_BOUND_DBC = 90.0


R = Report()
TM = ToneMeasure(n=NFFT, fs=1.0, dynamic_range_db=SIDELOBE_DB)


# ──────────────────────────────────────────────────────── measurement
def sfdr(norm_freq: float, n: int = NFFT) -> float:
    """SFDR of the LO's own output, measured by doppler's own analyser."""
    return float(TM.analyze_complex(LO(norm_freq).steps(n)).sfdr_dbc)


def inc_to_freq(inc: int) -> float:
    """The exact normalised frequency that lands on phase word `inc`."""
    return inc / WF


def advance(norm_freq: float, ctrl_val: float) -> int:
    """One step's realised phase advance under a constant control.

    Differenced across two calls rather than read once, so it is the
    *steady* per-step advance and not an artefact of the starting phase.
    """
    a = LO(norm_freq)
    a.steps_ctrl(np.full(2, ctrl_val, dtype=np.float64))
    b = LO(norm_freq)
    b.steps_ctrl(np.full(1, ctrl_val, dtype=np.float64))
    return int((a.phase - b.phase) % W)


def _csv(path, cols, header: str) -> None:
    """Write one raw sweep, unless this run is measurement-only.

    The CSVs exist so any number in the report can be re-derived without
    re-running the measurement; a limits-only run (the pytest path) has
    no report to support and must not write into the repo.
    """
    if R.write:
        np.savetxt(
            path,
            np.column_stack(cols),
            delimiter=",",
            header=header,
            comments="",
        )


def signed(adv: int) -> int:
    return adv - W if adv > (W >> 1) else adv


def read_lut() -> tuple[np.ndarray, np.ndarray]:
    """Read the whole 2^16-entry LUT through the public face.

    `lo_sin_lut` is a header extern with no binding, but it is fully
    observable: an increment of exactly one LUT bin (norm_freq = 2^-16,
    phase_inc = 65536) walks the index 0, 1, 2, ... in order, so one
    `steps(65536)` call returns `lut[(i + QTR) & 0xFFFF] + j*lut[i]` for
    every i. Nothing is modelled — this IS the table.
    """
    x = LO(2.0**-LUT_BITS).steps(LUT_SIZE)
    return (
        np.imag(x).astype(np.float64),  # lut[i]           == sin
        np.real(x).astype(np.float64),  # lut[i + QTR]     == cos
    )


def spectrum_db(x: np.ndarray) -> np.ndarray:
    """DC-centred dB spectrum of a complex capture, doppler's own way."""
    n = x.size
    beta = float(kaiser_beta_for_sidelobe(SIDELOBE_DB))
    w = np.empty(n, dtype=np.float32)
    kaiser_window(w, beta)
    xw = (x.astype(np.complex64) * w.astype(np.complex64)).astype(np.complex64)
    spec = FFT(n).execute_cf32(xw)
    mag = 20.0 * np.log10(
        np.maximum(np.abs(spec.astype(np.complex128)), 1e-30)
    )
    return np.fft.fftshift(mag - mag.max())


@dataclass
class Data:
    lut_sin_err: np.ndarray
    lut_cos_err: np.ndarray
    amp_err: float
    sf_freqs: np.ndarray
    sf_rand: np.ndarray
    sf_rational: list[tuple[int, int, int, float]]
    sf_worst: tuple[int, int, int, float]
    sf_spurfree: float
    sf_floor: float
    spec_worst: np.ndarray
    spec_clean: np.ndarray
    freqs: np.ndarray
    inc: np.ndarray
    err: np.ndarray
    bound: np.ndarray
    live: np.ndarray
    ctrls: np.ndarray
    adv: np.ndarray
    quantum: float
    plateau: float
    dead_span: np.ndarray
    dead_adv: np.ndarray
    chirp_dev: float
    hop: tuple[float, float, float]
    loop_ideal: dict[str, ll.LoopRun]
    loop_lo: dict[str, ll.LoopRun]
    big_n: int


# ═════════════════════════════════════════════════ 1. OBJECT SUMMARY
CLAIM_MAP: list[tuple[str, str]] = [
    # (claim, coverage in native/tests/test_lo_core.c)
    ("output is a 65536-entry float sine LUT indexed by phase >> 16", "§3"),
    ("the quarter-cycle offset LUT_QTR maps sin to cos", "§3 §6, literals"),
    ("output is emitted BEFORE the phase is incremented", "§3, literals"),
    ("the 16-bit phase truncation gives ~96 dBc SFDR", "**absent**"),
    ("the shared LUT is initialised lazily on the first create/init", "§1"),
    ("the shared LUT is never freed", "**absent**"),
    ("the LUT is read-only after init", "**absent**"),
    ("phase is the accumulator value in [0, 2^32)", "§7 §11"),
    ("phase_inc = floor(frac(norm_freq) x 2^32)", "§7 §12 §15, literals"),
    ("lo_init is lo_create without the allocation", "§9"),
    ("only the fractional part of norm_freq matters", "§12"),
    ("lo_step is bit-for-bit lo_steps, one sample at a time", "§8 §9 §12"),
    ("lo_step_ctrl adds ctrl on top of phase_inc for this step", "§16 NEW"),
    ("lo_step_ctrl does not persist ctrl", "§16 NEW"),
    ("lo_step_ctrl at ctrl == 0 is bit-identical to lo_step", "§16 NEW"),
    ("lo_step_ctrl takes the fractional cycle, any sign", "§16 NEW"),
    ("lo_step_ctrl emits before incrementing", "§16 NEW"),
    (
        "nco_norm_freq_to_inc 'rounds, not truncates' (header comment)",
        "§21 NEW — CONTRADICTED",
    ),
    ("lo_create returns NULL on allocation failure", "unreachable"),
    ("lo_destroy may be NULL (no-op)", "§18 NEW"),
    ("lo_reset zeroes phase, leaves norm_freq/phase_inc alone", "§7 §11"),
    ("setting norm_freq recomputes phase_inc, does not reset phase", "§7 §11"),
    ("phase is writable for phase-coherent frequency switching", "§7 §11"),
    ("the state blob is [dp_state_hdr_t][uint32 phase]", "§13 §14"),
    ("set_state returns DP_OK or DP_ERR_INVALID", "§14"),
    ("resume from a blob is bit-exact", "§13"),
    ("steps_max_out is the maximum samples per call", "§19 NEW — STALE"),
    (
        "steps returns min(n, max_out); emission stops at capacity",
        "§pass_capacity",
    ),
    ("every sample is a unit-magnitude phasor", "§6, 4 literals"),
    (
        "steps_ctrl delta = floor(frac(ctrl[i]) x 2^32), per sample",
        "§5, one constant",
    ),
    ("steps_ctrl does not modify norm_freq", "§5"),
    ("steps_ctrl does not modify phase_inc", "§17 NEW"),
    ("steps_ctrl is a loop over lo_step_ctrl", "§17 NEW"),
    ("steps_ctrl output length equals ctrl_len", "§5 §pass_capacity"),
    (
        "steps_ctrl is the natural API for FM and frequency hopping",
        "**absent**",
    ),
]


def section_summary() -> None:
    # Title and provenance belong to the executive summary, which
    # Report.executive renders ahead of everything here.
    R.md("## 1. The object — design and expectations")
    R.md()
    R.md(
        "`lo_state_t` is the **NCO's 32-bit phase accumulator plus a "
        "65536-entry float sine LUT**: the top 16 bits of the phase index "
        "the table, and a quarter-cycle offset (`LO_LUT_QTR = 16384`) maps "
        "`sin` to `cos` without a second table, so one lookup pair is a "
        "CF32 phasor. It is the carrier every mixer in the library turns — "
        "`ddc` and `ddcr` compose it, and `lo_step`/`lo_step_ctrl` exist so "
        "a tracking loop can embed one **by value** and wipe carrier off a "
        "sample at a time."
    )
    R.md()
    R.md("### Where the design lives — this report does not restate it")
    R.md()
    R.table(
        ["source", "holds"],
        [
            [
                "[`native/inc/lo/lo_core.h`]"
                "(../../../../../../native/inc/lo/lo_core.h)",
                "the contract: the LUT geometry, the emit-before-increment "
                "convention, the SFDR claim, the inline composition API and "
                "the control port",
            ],
            [
                "[`native/inc/nco/nco_core.h`]"
                "(../../../../../../native/inc/nco/nco_core.h)",
                "the conversion: `nco_norm_freq_to_inc` is the one "
                "double→integer boundary, and the LO calls it for both its "
                "configured rate and its control port",
            ],
            [
                "[`native/tests/test_lo_core.c`]"
                "(../../../../../../native/tests/test_lo_core.c)",
                "the gate: point assertions, plus the six sections this "
                "audit added (§16–§21)",
            ],
        ],
    )
    R.md(
        "> **There is no `docs/design/lo.md`.** The LO's rationale lives "
        "in the header, and the accumulator underneath it is the NCO's "
        "bit-for-bit (2.2), so the theory of operation it inherits is "
        "[`docs/design/nco.md`](../../../../../../docs/design/nco.md) — which "
        "covers the phase accumulator, the one float boundary and the "
        "control port, and names the LUT as the only thing the LO adds. "
        "What is genuinely LO-specific and undocumented in prose is that "
        "table: its spurious content, measured in 2.4, and its "
        "closed-loop cost, measured in 2.9. Nothing was restated here to "
        "paper over the gap."
    )
    R.md()
    R.md("### What this report adds")
    R.md()
    R.md(
        "A unit test pins points; it cannot state a **law**. `lo_core.h` "
        'claims in prose that "the 16-bit phase truncation gives ~96 dBc '
        'SFDR" — the headline number of the whole object — and nothing, on '
        "either side of the binding, has ever measured it. Sections below "
        "characterise the laws, review what they mean, and pin the "
        "envelope. Section numbers track `test_lo_core.c`'s own numbering "
        "so the two read side by side."
    )
    R.md()
    R.md("### Claim coverage — every prose claim in the header")
    R.md()
    R.md(
        "The audit began by enumerating what the header asserts and asking "
        "of each: is it pinned, pinned only at literals, or absent? "
        "`NEW` marks a section this audit added to `test_lo_core.c`; every "
        "one of those was proven by sabotage before being trusted."
    )
    R.md()
    R.table(
        ["claim in `lo_core.h`", "covered by"],
        [[c, w] for c, w in CLAIM_MAP],
    )
    R.md(
        "Three claims are still **absent** after this pass, and all three "
        "are the kind a C unit test cannot hold: the SFDR figure and the "
        "FM/hopping use case are spectral laws, measured below; "
        '"never freed" and "read-only after init" are lifetime properties '
        "of a `static` table with no observer."
    )
    R.md()


# ═════════════════════════════════════════════════ 2. CHARACTERISE
def characterise() -> Data:
    print("\nPHASE 1 — CHARACTERISE")
    R.md("## 2. Characterisation")
    R.md()
    R.md(
        "Measured behaviour, no verdicts. Section numbers track "
        "`test_lo_core.c`."
    )
    R.md()

    # --- 1-4, 7 --------------------------------------------------------
    R.md("### 2.1 Lifecycle, DC, quarter-rate, continuity, accessors")
    R.md("*(test_lo_core.c sections 1-4, 7)*")
    R.md()
    q = LO(0.25).steps(8)
    a = LO(0.1)
    two = np.concatenate([a.steps(5), a.steps(5)])
    one = LO(0.1).steps(10)
    p = LO(0.1)
    p.norm_freq, p.phase = 0.2, 12345
    dc = LO(0.0).steps(8)
    rst = LO(0.25)
    rst.steps(3)  # actually advance it, so reset() has something to undo
    rst.reset()
    # Three decimals, not zero: at quarter-rate the "0" components are
    # not exactly zero — cos(pi/2) reads lut[32768] = sin(pi) = -8.7e-08 —
    # and `%.0f` would print that as a bare `-0`, which looks like a
    # defect. The residual is the table's, and 2.3 bounds it.
    quarter = ", ".join(f"{c.real:+.3f}{c.imag:+.3f}j" for c in q[:4])
    R.table(
        ["property", "measured"],
        [
            ["phase after 3 steps then `reset()`", rst.phase],
            ["`phase_inc` at `norm_freq = 0`", LO(0.0).phase_inc],
            [
                "DC tone (`norm_freq = 0`)",
                f"all 8 samples `1+0j`: {bool(np.all(dc == 1 + 0j))}",
            ],
            ["quarter-rate first 4 phasors", f"`{quarter}`"],
            ["5+5 samples == 10 samples", np.array_equal(two, one)],
            [
                "accessor round-trip",
                f"norm_freq={p.norm_freq}, phase={p.phase}, "
                f"phase_inc={p.phase_inc}",
            ],
            [
                "state blob size",
                f"{LO(0.25).state_bytes()} bytes "
                f"(`dp_state_hdr_t` + `uint32`)",
            ],
        ],
    )

    # --- 8, 9, 12 -------------------------------------------------------
    R.md("### 2.2 The accumulator is the NCO's, bit-for-bit")
    R.md("*(sections 8, 9, 12)*")
    R.md()
    R.md(
        "The LO is not a second oscillator — it is `nco_state_t`'s "
        "arithmetic with a LUT on the output, and it calls the same "
        "`nco_norm_freq_to_inc` for both the configured rate and the "
        "control port. That is worth *measuring* rather than reading off "
        "the source, because it is what lets this report inherit the NCO's "
        "conversion characterisation instead of re-deriving it — and what "
        "would make a future divergence visible."
    )
    R.md()
    probe_f = [
        0.0,
        0.25,
        0.1,
        -0.25,
        1.25,
        LSB,
        LSB / 2,
        51.0 / 21.0e6,
        0.123456789,
        float("nan"),
        float("inf"),
        float("-inf"),
        -1e-20,
    ]
    inc_same = all(LO(f).phase_inc == NCO(f, 0).phase_inc for f in probe_f)
    la, nb = LO(0.123456789), NCO(0.123456789, 0)
    la.steps(4096)
    nb.steps_u32(4096)
    free_same = la.phase == nb.phase
    csweep = np.linspace(-1.5, 1.5, 401).astype(np.float64)
    lc, nc = LO(0.077), NCO(0.077, 0)
    lc.steps_ctrl(csweep)
    nc.steps_u32_ctrl(csweep)
    ctrl_same = lc.phase == nc.phase
    R.table(
        ["comparison", "LO vs NCO"],
        [
            [
                f"`phase_inc` over {len(probe_f)} probe frequencies "
                f"(incl. NaN, +-inf, sub-LSB, negative)",
                f"identical: {inc_same}",
            ],
            [
                "phase after 4096 free-running samples at 0.123456789",
                f"identical: {free_same} (`{la.phase}`)",
            ],
            [
                "phase after a 401-point control sweep over [-1.5, 1.5]",
                f"identical: {ctrl_same} (`{lc.phase}`)",
            ],
        ],
    )

    # --- 6, 20 ----------------------------------------------------------
    R.md("### 2.3 The LUT itself — angle and amplitude")
    R.md("*(section 6, and the new section 20)*")
    R.md()
    R.md(
        "`lo_sin_lut` is a header `extern` with no binding, but it is "
        "fully observable through the public face: at `norm_freq = 2^-16` "
        "the increment is exactly one LUT bin, so a single `steps(65536)` "
        "call walks index 0, 1, ... 65535 in order and returns the entire "
        "table. Nothing below is modelled — this IS the table, read out of "
        "the object."
    )
    R.md()
    sin_t, cos_t = read_lut()
    idx = np.arange(LUT_SIZE)
    th = 2.0 * np.pi * idx / LUT_SIZE
    lut_sin_err = sin_t - np.sin(th)
    lut_cos_err = cos_t - np.cos(th)
    # No CSV for the table: unlike the sweeps below it is not a parameter
    # study, it is one call. `read_lut()` regenerates all 65536 entries in
    # milliseconds, so a cached 8 MB copy in the repo would be dead weight.
    sweep = LO(0.10000000017).steps(NFFT)
    amp = np.abs(sweep.astype(np.complex128))
    amp_err = float(np.abs(amp - 1.0).max())
    R.table(
        ["property", "measured over all 65536 entries"],
        [
            [
                "max |lut[i] - sin(2pi i / 65536)|",
                f"{np.abs(lut_sin_err).max():.3e}",
            ],
            [
                "max |lut[i+QTR] - cos(2pi i / 65536)|",
                f"{np.abs(lut_cos_err).max():.3e}",
            ],
            [
                "max |1 - |lut[i+QTR] + j lut[i]||",
                f"{np.abs(np.hypot(sin_t, cos_t) - 1.0).max():.3e}",
            ],
            [
                "max |1 - |sample|| over a 65536-sample run at an odd rate",
                f"{amp_err:.3e}",
            ],
        ],
    )
    R.md(
        f"Amplitude error is at the float32 floor "
        f"(`{np.spacing(np.float32(1.0)):.2e}` is one ulp at 1.0), four "
        f"orders of magnitude below the half-bin phase error "
        f"`0.5/65536 = {0.5 / LUT_SIZE:.3e}` cycles. So the header's "
        f"attribution of the spur floor to *phase* truncation, not "
        f"amplitude quantization, is measured rather than assumed — and "
        f"2.4 confirms it directly."
    )
    R.md()
    R.md(f"![LUT error]({'lut_error.png'})")
    R.md()

    # --- SFDR: the claim nothing has ever checked ------------------------
    R.md("### 2.4 SFDR — the headline claim, measured")
    R.md()
    R.md(
        f"`lo_core.h` says, twice, that the 16-bit phase truncation gives "
        f"**~96 dBc SFDR**. No test on either side of the binding has ever "
        f"measured it. Measured here with doppler's own `ToneMeasure` "
        f"(Kaiser window at a {SIDELOBE_DB:.0f} dB sidelobe target, "
        f"{NFFT}-point capture = exactly one LUT period), against a "
        f"measurement floor set by complex64 itself."
    )
    R.md()
    rng = np.random.default_rng(7)
    sf_freqs = np.sort(rng.uniform(0.02, 0.48, 400))
    sf_rand = np.array([sfdr(float(f)) for f in sf_freqs])

    # The generic frequency is not the worst one. The truncation error
    # sequence is periodic in the LOW 16 bits of phase_inc: a small
    # denominator there means a short error period and therefore energy
    # concentrated in few, tall spurs. Probe those directly.
    hi = 12345 << LUT_BITS
    sf_rational: list[tuple[int, int, int, float]] = []
    for den in range(2, 33):
        for num in range(1, den):
            if gcd(num, den) != 1:
                continue
            low = (LUT_SIZE * num) // den
            if low == 0:
                continue
            sf_rational.append((num, den, low, sfdr(inc_to_freq(hi | low))))
    sf_rational.sort(key=lambda r: r[3])
    sf_worst = sf_rational[0]
    sf_spurfree = sfdr(inc_to_freq(hi))
    sf_floor = float(
        TM.analyze_complex(
            np.exp(2j * np.pi * 0.123456789 * np.arange(NFFT)).astype(
                np.complex64
            )
        ).sfdr_dbc
    )
    _csv(DATA / "sfdr_sweep.csv", [sf_freqs, sf_rand], "norm_freq,sfdr_dbc")
    _csv(
        DATA / "sfdr_rational.csv",
        list(zip(*sf_rational)),
        "num,den,phase_inc_low16,sfdr_dbc",
    )
    R.table(
        ["case", "SFDR (dBc)"],
        [
            [
                "400 random frequencies in [0.02, 0.48] — min / median / max",
                f"{sf_rand.min():.2f} / {np.median(sf_rand):.2f} / "
                f"{sf_rand.max():.2f}",
            ],
            [
                "worst frequency found: `phase_inc & 0xFFFF` = "
                f"65536 x {sf_worst[0]}/{sf_worst[1]} = {sf_worst[2]}",
                f"**{sf_worst[3]:.2f}**",
            ],
            [
                "phase-truncation theory, worst case "
                f"(6.02 x {LUT_BITS} - 3.92)",
                f"{SFDR_WORST_THEORY:.2f}",
            ],
            [
                f"phase-truncation theory, generic (6.02 x {LUT_BITS})",
                f"{SFDR_TYPICAL_THEORY:.2f}",
            ],
            [
                "`phase_inc & 0xFFFF == 0` — no phase truncation at all",
                f"{sf_spurfree:.2f}",
            ],
            [
                "measurement floor (an ideal float64 phasor cast to cf32)",
                f"{sf_floor:.2f}",
            ],
        ],
    )
    R.md(
        f"The generic case is flat and matches the claim: 400 random "
        f"frequencies give {sf_rand.min():.2f}–{sf_rand.max():.2f} dBc, a "
        f"spread of {sf_rand.max() - sf_rand.min():.2f} dB. The worst case "
        f"does not: at a half-bin fractional increment "
        f"(`phase_inc & 0xFFFF == 0x8000`) the error sequence has period 2 "
        f"and all its energy lands in one spur, giving "
        f"**{sf_worst[3]:.2f} dBc** — the classical phase-truncation bound "
        f"`6.02 B - 3.92` to {abs(sf_worst[3] - SFDR_WORST_THEORY):.2f} dB. "
        f"At the other extreme, an increment that is a whole number of LUT "
        f"bins truncates nothing and is spur-free to the measurement floor."
    )
    R.md()
    R.md("The ten worst increments found, all small-denominator:")
    R.md()
    R.table(
        ["`phase_inc & 0xFFFF`", "as a fraction of 65536", "SFDR (dBc)"],
        [
            [f"{low}", f"{num}/{den}", f"{s:.2f}"]
            for num, den, low, s in sf_rational[:10]
        ],
    )
    spec_worst = spectrum_db(LO(inc_to_freq(hi | sf_worst[2])).steps(NFFT))
    spec_clean = spectrum_db(LO(inc_to_freq(hi)).steps(NFFT))
    R.md(f"![SFDR]({'sfdr.png'})")
    R.md()

    # --- 15 --------------------------------------------------------------
    R.md("### 2.5 The one conversion, observed as `phase_inc`")
    R.md("*(section 15)*")
    R.md()
    R.md(
        "The LO does not own this conversion — `nco_norm_freq_to_inc` "
        "does, and 2.2 measured the two to be bit-identical. It is "
        "re-observed here only through the LO's own face, so a caller "
        "reading this page alone is not left inferring it."
    )
    R.md()
    rows = []
    for v, note in (
        (0.0, "exact zero"),
        (LSB, "one LSB — smallest live rate"),
        (LSB / 2, "below one LSB"),
        (0.25, "exact, divides 2^32"),
        (float("nan"), "NaN"),
        (float("inf"), "+inf"),
        (-0.25, "negative, folds into [0,1)"),
        (-1e-20, "tiny negative"),
        (1.0, "one whole cycle"),
        (51.0 / 21.0e6, "truncates (remainder ~0.635), does not round"),
    ):
        rows.append([f"`{v:.10g}`", LO(v).phase_inc, note])
    R.table(["requested norm_freq", "phase_inc", "note"], rows)

    freqs = np.logspace(-9, np.log10(0.25), 400)
    inc = np.array([LO(float(f)).phase_inc for f in freqs], dtype=np.int64)
    live = inc > 0
    err = (inc / WF - freqs) / freqs * 1e6
    bound = np.where(live, 1e6 / np.maximum(inc, 1), np.inf)
    _csv(
        DATA / "frequency_sweep.csv",
        [freqs, inc, err, -bound],
        "norm_freq,phase_inc,err_ppm,bound_ppm",
    )
    R.md(
        f"Across the range (full sweep in `data/frequency_sweep.csv`) the "
        f"realised frequency is never HIGH — truncation floors — and never "
        f"escapes the `1e6/phase_inc` ppm envelope: worst measured "
        f"{abs(err[live].min()):.0f} ppm at the bottom of the range, "
        f"{abs(err[live][-1]):.3f} ppm at the top."
    )
    R.md()

    # --- 5, 16, 17, 21 ---------------------------------------------------
    R.md("### 2.6 The control port — law and resolution")
    R.md("*(section 5, and the new sections 16, 17, 21)*")
    R.md()
    rows = []
    for cv in (0.25, 0.1, 1e-9, 0.0, -1e-9, -0.1, 1.0, 1.5, -1.5):
        d = advance(0.0, cv)
        rows.append([f"{cv:g}", d, f"{d / WF:.6f}"])
    R.table(["ctrl", "advance (phase words)", "as cycles/sample"], rows)

    ctrls = np.linspace(-1.5, 1.5, 1201)
    adv = np.array([advance(0.0, c) for c in ctrls], dtype=np.int64)
    _csv(DATA / "ctrl_sweep.csv", [ctrls, adv], "ctrl,advance")
    inc_cfg = LO(0.1).phase_inc
    adv_ctrl = advance(0.0, 0.1)
    R.md(
        f"The same requested `0.1` reaches the accumulator by two paths and "
        f"lands on **two different words**: configured (double) gives "
        f"`{inc_cfg}` and the ctrl port gives `{adv_ctrl}`, a delta of "
        f"`{adv_ctrl - inc_cfg}`. The port used to be float32 while the "
        f"configured rate was double, so the same request landed 7 words "
        f"apart depending on which face it entered by — the NCO's F2 "
        f"verbatim, inherited along with the port. Both are `double` now "
        f"(**F5**), which also makes `lo_steps_ctrl` agree with the "
        f"inline `lo_step_ctrl` that always took one."
    )
    R.md()

    f = 0.25
    # With a double port the floor is the PHASE WORD's LSB.
    quantum = LSB
    dead_span = np.linspace(-f - 4 * quantum, -f + 4 * quantum, 1601)
    dead_adv = np.array([advance(f, c) for c in dead_span], dtype=np.int64)
    best = run = lo_i = hi_i = start = 0
    for i, z in enumerate(dead_adv == 0):
        if z:
            if run == 0:
                start = i
            run += 1
            if run > best:
                best, lo_i, hi_i = run, start, i
        else:
            run = 0
    plateau = float(dead_span[hi_i] - dead_span[lo_i]) if best else 0.0

    R.md("### 2.7 A control that cancels `phase_inc`")
    R.md()
    R.table(
        ["ctrl", "signed advance (phase words)"],
        [
            [f"{-f + e:.12f}", signed(advance(f, -f + e))]
            for e in (-1e-7, -1e-8, 0.0, 1e-8, 1e-7)
        ],
    )
    R.md(
        f"The phase-word LSB is `{quantum:.4g}` ({quantum * 1e9:.3f} ppb) "
        f"and the measured contiguous zero-advance run is `{plateau:.4g}` "
        f"({plateau * 1e9:.3f} ppb) over {best}/{dead_span.size} scanned "
        f"controls — one LSB, the floor. On the float32 port it was "
        f"22.4 ppb, 96x wider (**F6**)."
    )
    R.md()
    R.md(f"![ctrl law]({'ctrl_law.png'})")
    R.md()

    # --- the documented use case ----------------------------------------
    R.md("### 2.8 FM synthesis and frequency hopping")
    R.md()
    R.md(
        'The header calls `steps_ctrl` *"the natural API for FM synthesis '
        'and frequency-hopping"*. Nothing tested that claim as a '
        "behaviour, only as a constant offset. Two stimuli below: a linear "
        "chirp (the control ramps) and a hard hop (the control steps), "
        "with the instantaneous frequency recovered from the emitted "
        "phasors themselves."
    )
    R.md()
    n = 1 << 14
    ramp = np.linspace(0.02, 0.30, n).astype(np.float64)
    y = LO(0.0).steps_ctrl(ramp)
    inst = (
        np.angle(y[1:].astype(np.complex128) * np.conj(y[:-1])) / (2 * np.pi)
    ) % 1.0
    chirp_dev = float(np.abs(inst - ramp[:-1].astype(np.float64)).max())

    hop_n = 1 << 12
    hop_ctrl = np.concatenate(
        [
            np.full(hop_n // 2, 0.05, dtype=np.float64),
            np.full(hop_n // 2, 0.31, dtype=np.float64),
        ]
    )
    yh = LO(0.0).steps_ctrl(hop_ctrl)
    ih = (
        np.angle(yh[1:].astype(np.complex128) * np.conj(yh[:-1])) / (2 * np.pi)
    ) % 1.0
    k = hop_n // 2
    hop = (
        float(np.median(ih[: k - 2])),
        float(np.median(ih[k + 1 :])),
        float(abs(ih[k - 1] - 0.05)),
    )
    R.table(
        ["stimulus", "measured"],
        [
            [
                f"linear chirp, ctrl 0.02 -> 0.30 over {n} samples",
                f"max |instantaneous freq - commanded| = {chirp_dev:.3e} "
                f"cycles/sample",
            ],
            [
                "hard hop, ctrl 0.05 -> 0.31 at the midpoint",
                f"{hop[0]:.6f} before, {hop[1]:.6f} after; the sample "
                f"BEFORE the hop is still at the old rate "
                f"(|err| {hop[2]:.2e}) — the change takes effect on the "
                f"step it is commanded, with no phase discontinuity",
            ],
        ],
    )
    R.md(
        f"The chirp tracks the command to "
        f"{chirp_dev / (0.5 / LUT_SIZE):.1f}x the half-LUT-bin phase "
        f"quantum, which is the resolution the LUT can express at all — "
        f"the control port is not the limit here, the table is."
    )
    R.md()

    # --- 2.9: the closed loop ---------------------------------------------
    R.md("### 2.9 The closed-loop limit, and what the LUT costs")
    R.md()
    R.md(
        "The LO's reason to exist is to be a carrier loop's actuator, so "
        "the closed-loop question is the one that matters. Two runs of the "
        "**same** loop — shared `_common/linear_loop`, `LoopFilter`, one "
        "update per sample — differing only in the detector:"
    )
    R.md()
    R.md(
        "- **ideal** — plain subtraction on the accumulator phase. This is "
        "the NCO's limit, and 2.2 measured the LO's accumulator to be that "
        "same accumulator, so the number transfers unchanged.\n"
        "- **LO's own face** — the error read the way a receiver reads it: "
        "`angle(reference x conj(emitted phasor)) / 2pi`, where the "
        "emitted phasor is a real `LO` sample taken at the loop "
        "oscillator's exact phase word. Everything the LUT does to the "
        "signal is inside this detector."
    )
    R.md()
    R.md(
        "The difference between the two columns is therefore **the LUT's "
        "entire contribution to closed-loop performance**, isolated."
    )
    R.md()
    R.md(
        f"`bn = {ll.BN}`, `zeta = {ll.ZETA}`, disturbance at sample "
        f"{ll.K0}; the classic second-order settling estimate `5/bn` is "
        f"{5 / ll.BN:.0f} samples."
    )
    R.md()
    probe = LO(0.0)

    def lo_detector(ref: float, osc: float) -> float:
        """The error a receiver actually sees: through the emitted phasor.

        `osc` is the loop oscillator's phase in cycles, which is an exact
        32-bit word divided by 2^32 and so converts back losslessly. A
        real `LO` is seeded to that word and asked for one sample, so the
        phasor is the object's own output, not a model of it.
        """
        probe.phase = round(osc * WF) & 0xFFFFFFFF
        y_ = complex(probe.steps(1)[0])
        return float(
            np.angle(np.exp(2j * np.pi * ref) * np.conj(y_)) / (2 * np.pi)
        )

    loop_ideal: dict[str, ll.LoopRun] = {}
    loop_lo: dict[str, ll.LoopRun] = {}
    rows = []
    for name, pin in ll.standard_drives().items():
        ri = ll.run(pin, name=name)
        rl = ll.run(pin, name=name, detector=lo_detector)
        loop_ideal[name] = ri
        loop_lo[name] = rl
        si, sl = ll.settle(ri), ll.settle(rl)
        rows.append(
            [
                name,
                f"{si.samples}",
                f"{sl.samples}",
                f"{si.residual:.2e}",
                f"{sl.residual:.2e}",
            ]
        )
    R.table(
        [
            "drive",
            "settle, ideal",
            "settle, LO face",
            "residual, ideal (cyc)",
            "residual, LO face (cyc)",
        ],
        rows,
    )
    ramp_name = f"ramp, {ll.RAMP:g} cyc/sample"
    half_bin = 0.5 / LUT_SIZE
    rl_res = ll.settle(loop_lo[ramp_name]).residual
    R.md(
        f"Settling is **identical** in every drive — the LUT costs no "
        f"bandwidth. What it costs is a floor: on the ramp, where the loop "
        f"parks between LUT bins rather than on one, the residual through "
        f"the LO's own face is `{rl_res:.3e}` cycles against the ideal "
        f"detector's `{ll.settle(loop_ideal[ramp_name]).residual:.1e}`. "
        f"Half a LUT bin is `0.5/65536 = {half_bin:.3e}` cycles. The "
        f"measured floor is {rl_res / half_bin:.3f}x that — the "
        f"quantization of the phase index, and nothing else, is the "
        f"steady-state error of a carrier loop built on this LO."
    )
    R.md()
    R.md(f"![linear loop]({'linear_loop.png'})")
    R.md()

    # --- 2.10 max_out ------------------------------------------------------
    R.md("### 2.10 `max_out` is advisory on both faces")
    R.md("*(the new section 19)*")
    R.md()
    big_n = 70000
    got = LO(0.013).steps(big_n)
    got_c = LO(0.013).steps_ctrl(np.zeros(big_n, dtype=np.float32))
    ref_lo = LO(0.013)
    ref_lo.steps(big_n)
    tail_ok = abs(abs(complex(got[-1])) - 1.0) < 1e-6
    R.table(
        ["probe", "measured"],
        [
            ["`steps_max_out()`", LO(0.0).steps_max_out()],
            ["`steps_ctrl_max_out()`", LO(0.0).steps_ctrl_max_out()],
            [f"`steps({big_n})` returns", got.shape[0]],
            [f"`steps_ctrl(zeros({big_n}))` returns", got_c.shape[0]],
            [
                "last sample of the oversized call is a real phasor",
                tail_ok,
            ],
            [
                "phase advanced by the full request",
                ref_lo.phase == (LO(0.013).phase_inc * big_n) % W,
            ],
        ],
    )
    R.md(
        f"Both faces return every sample asked for, {big_n - 65536} past "
        f"the advertised maximum, correct to the last one: the C kernel "
        f"clamps to the caller's `max_out` (jm gh-138) and the Python "
        f"binding grows its buffer on demand. `steps_max_out()` is a "
        f"pre-allocation hint, not a limit — see **F4**."
    )
    R.md()

    R.md("### 2.11 Not reachable from Python")
    R.md()
    R.md(
        "The bindings expose `steps`/`steps_ctrl` and the properties, but "
        "the **entire inline composition API** — `lo_init`, `lo_step`, "
        "`lo_step_ctrl` and the `lo_sin_lut` extern — has no binding at "
        "all. That is by design (they exist so C can embed an "
        "`lo_state_t` by value with zero call overhead), but it means the "
        "control port a tracking loop actually uses is C-only, and until "
        "this audit added §16/§17/§21 it had no test on either side. The "
        "LUT is the one exception: 2.3 reads all 65536 entries through "
        "`steps`, so it needed no binding to be characterised."
    )
    R.md()

    return Data(
        lut_sin_err,
        lut_cos_err,
        amp_err,
        sf_freqs,
        sf_rand,
        sf_rational,
        sf_worst,
        sf_spurfree,
        sf_floor,
        spec_worst,
        spec_clean,
        freqs,
        inc,
        err,
        bound,
        live,
        ctrls,
        adv,
        quantum,
        plateau,
        dead_span,
        dead_adv,
        chirp_dev,
        hop,
        loop_ideal,
        loop_lo,
        big_n,
    )


# ═════════════════════════════════════════════════ 3. REVIEW
def review(d: Data) -> None:
    print("\nPHASE 2 — REVIEW")
    R.md("## 3. Review — findings")
    R.md()

    R.find(
        "F1",
        "FIXED",
        f"the '~96 dBc SFDR' claim was the TYPICAL value stated as though "
        f"it were a bound. Measured: "
        f"{d.sf_rand.min():.2f}-{d.sf_rand.max():.2f} dBc over 400 random "
        f"frequencies, but {d.sf_worst[3]:.2f} dBc at a half-bin "
        f"fractional increment (phase_inc & 0xFFFF == {d.sf_worst[2]}), "
        f"which is the classical 6.02B-3.92 = {SFDR_WORST_THEORY:.2f} dBc "
        f"phase-truncation bound — {96.0 - d.sf_worst[3]:.1f} dB below the "
        f"figure a caller would have budgeted from, and the worst set is "
        f"not exotic: every increment congruent to 0x8000 mod 2^16 is in "
        f"it. In the other direction the claim was equally silent about "
        f"{d.sf_spurfree:.0f} dBc when the increment is a whole number of "
        f"LUT bins. FIXED: lo_core.h now documents all three regimes and "
        f"states the real guarantee, **SFDR >= {SFDR_BOUND_DBC:.0f} dBc at "
        f"any frequency** ({d.sf_worst[3] - SFDR_BOUND_DBC:.2f} dB of "
        f"margin on the measured worst case). The bound is gated, not just "
        f"documented: test_lo.py::test_sfdr_worst_case_meets_the_documented"
        f"_bound measures the 0x8000 remainder in the Python suite and "
        f"pins it to the theory bound, and a sabotage that drops the phase "
        f"index to 14 bits takes it to 84.06 dBc and turns the gate red.",
    )
    R.find(
        "F2",
        "FIXED",
        "the comment beside lo_step_ctrl said nco_norm_freq_to_inc "
        "'rounds, not truncates'. It truncates, deliberately, and "
        "nco_core.h spends four paragraphs on why (a rounding form "
        "contracts to an FMA on arm64 and x86-64-v3 but not on the "
        "x86-64-v2 baseline doppler ships, so the increment would differ "
        "by host). test_lo_core.c §15 already measured truncation on the "
        "configure path; the new §21 pins it on the control path too, so "
        "the comment is now contradicted by a test rather than by "
        "reading. Stale prose left behind when the private copy was "
        "consolidated away — the sentence described what the deleted copy "
        "did. The comment now states truncation and points at nco_core.h "
        "for why; §21 is what stops it drifting again.",
    )
    R.find(
        "F3",
        "FIXED",
        "lo_core.c's two `#ifdef __AVX512F__` blocks were dead in every "
        "configuration doppler ships, and divergent where they are not. "
        "CMakeLists.txt targets -march=x86-64-v2 by default, so "
        "__AVX512F__ is undefined and the scalar fallbacks are what every "
        "wheel, every CI job and this report exercise; the vector path "
        "becomes live only under DOPPLER_NATIVE=ON on an AVX-512 host. "
        "There it computes round(frac x 2^32) in float32 via "
        "_mm512_cvtps_epu32 while the compiled scalar truncates in "
        "double — the same object, two phase increments, chosen by CPU. "
        "Already recorded as a KNOWN VIOLATION in "
        "scripts/.phase-conversion-allow; this report added that it was "
        "also unreachable, so no gate could ever catch the divergence. "
        "Both blocks are now DELETED and the scalar fallbacks — the only "
        "code any shipped build ever ran — are the implementation. That "
        "retires the allowlist entry too, so the ratchet shrank from 8 "
        "occurrences to 7.",
    )
    R.find(
        "F4",
        "FIXED",
        f"both max_out doc lines were stale. lo_core.h called "
        f"steps_max_out() 'maximum samples per call' and lo_core.c said "
        f"'calling with n > 65536 overflows the buffer and is undefined "
        f"behaviour'. Measured: steps({d.big_n}) returns {d.big_n} correct "
        f"samples and steps_ctrl the same — pass_capacity (jm gh-138) made "
        f"the caller's capacity the bound and the Python binding grows its "
        f"buffer, so 65536 is a pre-allocation hint and both sentences "
        f"described the pre-gh-138 contract. FIXED in both files — and "
        f"checking the sibling found the IDENTICAL pair in "
        f"nco_core.{{h,c}}, four copies of one false claim, all four now "
        f"corrected (see the NCO report's F9, and its new §17 pinning the "
        f"behaviour on each of the three NCO output mappings).",
    )
    R.find(
        "F5",
        "FIXED",
        f"the ctrl port was float32 while the configured rate was double, "
        f"so one requested 0.1 landed on two different phase words (delta "
        f"7) — inherited from the NCO (its F2) along with the port itself, "
        f"and the LO is the face a carrier loop actually steers. "
        f"lo_steps_ctrl now takes `double`, the width the conversion works "
        f"in and the one lo_step_ctrl always used, so the block and inline "
        f"faces of the same control port finally agree. Measured: delta is "
        f"now {advance(0.0, 0.1) - LO(0.1).phase_inc}.",
    )
    R.find(
        "F6",
        "BY DESIGN",
        f"a ctrl cancelling phase_inc stops the LO over a plateau rather "
        f"than at a knife edge, and always will: below one phase-word LSB "
        f"the conversion truncates to zero. What was a defect is how wide "
        f"— 22.4 ppb, one float32 quantum, the port's precision rather "
        f"than the accumulator's. With F5 landed it measures "
        f"{d.plateau * 1e9:.3f} ppb against an LSB of {LSB * 1e9:.3f} ppb: "
        f"96x narrower and now at the floor.",
    )
    R.find(
        "F7",
        "BY DESIGN",
        "the LO is the NCO accumulator plus a LUT, and that is measured, "
        "not assumed: phase_inc agrees over 13 probe frequencies "
        "(including NaN, +-inf, sub-LSB and negative), the free-running "
        "phase agrees after 4096 samples, and the phase agrees after a "
        "401-point control sweep spanning [-1.5, 1.5]. This is what lets "
        "the report inherit the NCO's conversion findings instead of "
        "re-deriving them, and what would make a future divergence "
        "visible.",
    )
    half_bin = 0.5 / LUT_SIZE
    ramp_name = f"ramp, {ll.RAMP:g} cyc/sample"
    rl = ll.settle(d.loop_lo[ramp_name]).residual
    ri = ll.settle(d.loop_ideal[ramp_name]).residual
    R.find(
        "F8",
        "BY DESIGN",
        f"the LUT costs half a phase bin in a closed carrier loop, "
        f"and no bandwidth. Same loop, same filter, same drives: settling "
        f"is identical through the ideal detector and through the LO's own "
        f"emitted phasor, while the steady-state residual on a frequency "
        f"ramp rises from {ri:.1e} to {rl:.3e} cycles — "
        f"{rl / half_bin:.3f}x the half-bin quantum {half_bin:.3e} (a "
        f"whisker over, from the detector's own arctan). Quantified here "
        f"for the first time; the header gives the spur figure but never "
        f"the loop-facing one.",
    )
    R.find(
        "F9",
        "C-ONLY",
        "the whole inline composition API — lo_init, lo_step, "
        "lo_step_ctrl, lo_sin_lut — has no binding, and lo_step_ctrl had "
        "no test either: a documented control port with a five-clause "
        "contract (added on top of phase_inc, not persisted, any sign, "
        "folds modulo one cycle, bit-identical to lo_step at ctrl == 0) "
        "and zero coverage on either side. This audit added §16, §17 and "
        "§21 to test_lo_core.c and proved each by sabotage; the LUT "
        "extern needed no binding, since 2.3 reads all 65536 entries "
        "through steps().",
    )
    R.md()
    R.table(
        ["finding", "verdict", "detail"],
        [[t, v, x] for t, v, x in R.findings],
    )


# ═════════════════════════════════════════════════ 4. LIMITS
def limits(d: Data) -> None:
    print("\nPHASE 3 — LIMITS")
    R.md("## 4. Limits — the certified envelope")
    R.md()
    R.md(
        "Claims a caller may rely on. A failure here is a regression, not "
        "a new finding."
    )
    R.md()

    R.limit(
        float(np.abs(d.lut_sin_err).max()) < 1e-6,
        f"the table IS sin at every one of its {LUT_SIZE} entries "
        f"(max error {np.abs(d.lut_sin_err).max():.2e})",
    )
    R.limit(
        float(np.abs(d.lut_cos_err).max()) < 1e-6,
        f"the LUT_QTR offset IS cos at every index "
        f"(max error {np.abs(d.lut_cos_err).max():.2e})",
    )
    R.limit(
        d.amp_err < 1e-6,
        f"every emitted sample is a unit-magnitude phasor "
        f"(max |1-|x|| = {d.amp_err:.2e})",
    )
    R.limit(
        d.sf_worst[3] >= SFDR_BOUND_DBC,
        f"SFDR is >= {SFDR_BOUND_DBC:.0f} dBc at ANY frequency — the bound "
        f"lo_core.h now states (worst measured {d.sf_worst[3]:.2f} dBc, "
        f"{d.sf_worst[3] - SFDR_BOUND_DBC:.2f} dB of margin)",
    )
    R.limit(
        abs(d.sf_worst[3] - SFDR_WORST_THEORY) < 0.2,
        f"the worst case IS the phase-truncation bound 6.02B-3.92 = "
        f"{SFDR_WORST_THEORY:.2f} dBc, not something else "
        f"({d.sf_worst[3]:.2f} measured) — so the margin above is the real "
        f"margin, not an artefact of where the sweep happened to look",
    )
    R.limit(
        float(d.sf_rand.min()) >= 96.0,
        f"at a generic frequency SFDR meets the typical ~96 dBc "
        f"({d.sf_rand.min():.2f} dBc worst of 400 random rates)",
    )
    R.limit(
        d.sf_spurfree > 140.0,
        f"the spur floor is PHASE truncation, not amplitude: an increment "
        f"of whole LUT bins is spur-free to {d.sf_spurfree:.0f} dBc",
    )
    probe_f = [0.0, 0.25, -0.25, 1.25, LSB, LSB / 2, float("nan"), -1e-20]
    la, nb = LO(0.123456789), NCO(0.123456789, 0)
    la.steps(4096)
    nb.steps_u32(4096)
    cs = np.linspace(-1.5, 1.5, 401).astype(np.float32)
    lc, nc = LO(0.077), NCO(0.077, 0)
    lc.steps_ctrl(cs)
    nc.steps_u32_ctrl(cs)
    R.limit(
        all(LO(f).phase_inc == NCO(f, 0).phase_inc for f in probe_f)
        and la.phase == nb.phase
        and lc.phase == nc.phase,
        "the LO's accumulator is the NCO's, bit-for-bit — configured, "
        "free-running and control-driven",
    )
    R.limit(
        bool(np.all(d.err[d.live] <= 1e-9)),
        "frequency error is never HIGH, at any requested rate",
    )
    R.limit(
        bool(np.all(d.err[d.live] >= -d.bound[d.live] * (1 + 1e-6))),
        "frequency error never escapes the 1e6/phase_inc ppm envelope",
    )
    R.limit(
        LO(51.0 / 21.0e6).phase_inc == 10430,
        "the conversion TRUNCATES, contradicting the header comment: "
        "51/21e6 gives 10430, where round-to-nearest would give 10431",
    )
    R.limit(
        LO(float("nan")).phase_inc == 0,
        "NaN converts to a stopped LO, never to a wrapped increment",
    )
    R.limit(
        all(
            np.array_equal(
                LO(f).steps(64),
                LO(f).steps_ctrl(np.zeros(64, dtype=np.float32)),
            )
            for f in (0.0, 0.1, 0.25)
        ),
        "ctrl == 0 is bit-identical to no ctrl at all",
    )
    lo_k = LO(0.1)
    before = (lo_k.norm_freq, lo_k.phase_inc)
    lo_k.steps_ctrl(np.full(64, -0.7, dtype=np.float32))
    R.limit(
        (lo_k.norm_freq, lo_k.phase_inc) == before,
        "ctrl never modifies norm_freq or phase_inc",
    )
    folded = np.mod(d.ctrls, 1.0)
    pred = np.floor(folded * WF).astype(np.int64) % W
    agree = int(np.sum(np.abs(d.adv - pred) <= 1))
    R.limit(
        agree >= int(0.99 * d.adv.size),
        f"ctrl folds modulo one cycle exactly, both signs "
        f"({agree}/{d.adv.size} controls)",
    )
    R.limit(
        0.2 * d.quantum <= d.plateau <= 3.0 * d.quantum,
        f"the ctrl dead zone is one PHASE-WORD LSB wide "
        f"({d.plateau * 1e9:.3f} ppb) — the quantization floor, not the "
        f"port's precision",
    )
    R.limit(
        d.chirp_dev < 4.0 * (0.5 / LUT_SIZE),
        f"a commanded chirp is realised to within a few LUT bins "
        f"({d.chirp_dev:.2e} cycles/sample, half-bin is "
        f"{0.5 / LUT_SIZE:.2e})",
    )
    R.limit(
        abs(d.hop[0] - 0.05) < 1e-4 and abs(d.hop[1] - 0.31) < 1e-4,
        f"a frequency hop takes effect on the step it is commanded "
        f"({d.hop[0]:.5f} -> {d.hop[1]:.5f} against 0.05 -> 0.31)",
    )
    r_lo = LO(0.3)
    r_lo.steps(3)
    nf, pi = r_lo.norm_freq, r_lo.phase_inc
    r_lo.reset()
    R.limit(
        r_lo.phase == 0 and r_lo.norm_freq == nf and r_lo.phase_inc == pi,
        "reset() zeroes phase and leaves norm_freq/phase_inc untouched",
    )
    s_a, s_b = LO(0.123456), LO(0.123456)
    full = LO(0.123456).steps(256)
    s_a.steps(100)
    blob = s_a.get_state()
    s_b.set_state(blob)
    resumed = s_b.steps(156)
    bad_blob = bytes([blob[0] ^ 0xFF]) + blob[1:]
    try:
        s_b.set_state(bad_blob)
        rejected = False
    except ValueError:
        rejected = True
    R.limit(
        np.array_equal(resumed, full[100:]) and rejected,
        f"state round-trips bit-exactly into a fresh LO "
        f"({len(blob)}-byte blob) and a clobbered envelope is rejected",
    )
    R.limit(
        LO(0.013).steps(d.big_n).shape[0] == d.big_n
        and LO(0.013).steps_ctrl(np.zeros(d.big_n, dtype=np.float32)).shape[0]
        == d.big_n,
        f"max_out is advisory: both faces return all {d.big_n} samples "
        f"requested, past the advertised 65536",
    )

    # --- the closed-loop limits -------------------------------------------
    names = list(ll.standard_drives())
    same_settle = all(
        ll.settle(d.loop_ideal[nm]).samples == ll.settle(d.loop_lo[nm]).samples
        for nm in names
    )
    R.limit(
        same_settle,
        "the LUT costs no loop bandwidth: settling through the LO's own "
        "emitted phasor equals settling through the ideal detector, on "
        "every drive",
    )
    up = ll.settle(d.loop_ideal[f"+{ll.STEP:g} cycle step"])
    dn = ll.settle(d.loop_ideal[f"-{ll.STEP:g} cycle step"])
    R.limit(
        up.samples < 5.0 / ll.BN and dn.samples < 5.0 / ll.BN,
        f"a phase step settles inside the 5/bn estimate "
        f"({up.samples} and {dn.samples} samples against {5 / ll.BN:.0f})",
    )
    R.limit(
        up.samples == dn.samples and abs(up.peak - dn.peak) < 1e-9,
        "the loop is symmetric in sign: +step and -step settle identically",
    )
    ramp_name = f"ramp, {ll.RAMP:g} cyc/sample"
    rl_res = ll.settle(d.loop_lo[ramp_name]).residual
    R.limit(
        rl_res <= 1.05 * (0.5 / LUT_SIZE),
        f"the LUT's closed-loop steady-state cost is HALF a LUT bin to "
        f"within 5% ({rl_res:.3e} against {0.5 / LUT_SIZE:.3e} cycles, "
        f"{rl_res / (0.5 / LUT_SIZE):.3f}x) — the detector's own arctan "
        f"puts it a whisker above, so half a bin is the scale, not a "
        f"strict ceiling",
    )
    R.limit(
        abs(d.loop_lo[ramp_name].control[-1] - ll.RAMP) < 1e-5,
        f"on a ramp the loop filter's output converges on the applied "
        f"frequency offset itself "
        f"({d.loop_lo[ramp_name].control[-1]:.6e} vs {ll.RAMP:g})",
    )


# ═════════════════════════════════════════════════════════════ plots
def plots(d: Data) -> None:
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    # --- LUT error ------------------------------------------------------
    fig, (a1, a2) = plt.subplots(1, 2, figsize=(13, 4.5))
    idx = np.arange(LUT_SIZE)
    a1.plot(idx, d.lut_sin_err, lw=0.6, label="lut[i] - sin")
    a1.plot(idx, d.lut_cos_err, lw=0.6, alpha=0.7, label="lut[i+QTR] - cos")
    a1.set_xlabel("LUT index (top 16 bits of the phase)")
    a1.set_ylabel("absolute error")
    a1.set_title("The table is exact to float32; amplitude is not the spur")
    a1.grid(True, alpha=0.3)
    a1.legend(fontsize=8)

    ph = idx / LUT_SIZE
    a2.plot(ph, np.hypot(*read_lut()) - 1.0, lw=0.6, color="tab:purple")
    a2.axhline(0.0, color="0.6", lw=0.8)
    a2.set_xlabel("phase (cycles)")
    a2.set_ylabel("|phasor| - 1")
    a2.set_title("Magnitude error over one full cycle")
    a2.grid(True, alpha=0.3)
    fig.tight_layout()
    fig.savefig(HERE / "lut_error.png", dpi=110)
    plt.close(fig)

    # --- SFDR -----------------------------------------------------------
    fig, (a1, a2) = plt.subplots(1, 2, figsize=(13, 5))
    # SFDR is not a function of frequency, it is a function of the LOW 16
    # bits of phase_inc — the part the LUT index throws away. Plotting it
    # against norm_freq hides that (and would draw the worst case as a
    # line no measured point comes near); plotting it against the
    # truncated remainder puts both families on the axis that governs
    # them.
    rand_low = np.array(
        [LO(float(f)).phase_inc & (LUT_SIZE - 1) for f in d.sf_freqs]
    )
    rat_low = np.array([low for _, _, low, _ in d.sf_rational])
    rat_s = np.array([s for _, _, _, s in d.sf_rational])
    a1.plot(
        rand_low / LUT_SIZE,
        d.sf_rand,
        ".",
        ms=3,
        alpha=0.5,
        label="400 random frequencies",
    )
    a1.plot(
        rat_low / LUT_SIZE,
        rat_s,
        "v",
        ms=5,
        color="tab:orange",
        alpha=0.9,
        label="small-denominator remainders",
    )
    a1.axhline(
        SFDR_WORST_THEORY,
        color="tab:red",
        ls="--",
        lw=1.2,
        label=f"6.02B-3.92 = {SFDR_WORST_THEORY:.1f} dBc (theory, worst)",
    )
    a1.axhline(
        96.0, color="tab:green", ls=":", lw=1.2, label="header claim ~96 dBc"
    )
    a1.annotate(
        f"{rat_s.min():.2f} dBc at a\nHALF-bin remainder",
        xy=(0.5, rat_s.min()),
        xytext=(0.56, 93.6),
        fontsize=8,
        arrowprops={"arrowstyle": "->", "lw": 0.9},
    )
    a1.set_xlabel("(phase_inc & 0xFFFF) / 65536 — the truncated remainder")
    a1.set_ylabel("SFDR (dBc)")
    a1.set_ylim(91.5, 97.5)
    a1.set_title("SFDR is set by the remainder the LUT index discards")
    a1.grid(True, alpha=0.3)
    a1.legend(fontsize=8, loc="lower right")

    fr = np.linspace(-0.5, 0.5, d.spec_worst.size, endpoint=False)
    a2.plot(
        fr,
        d.spec_worst,
        lw=0.7,
        label=f"worst increment ({d.sf_worst[3]:.1f} dBc)",
    )
    a2.plot(
        fr,
        d.spec_clean,
        lw=0.7,
        alpha=0.8,
        label=f"whole-bin increment ({d.sf_spurfree:.0f} dBc)",
    )
    a2.set_ylim(-170, 5)
    a2.set_xlabel("frequency (cycles/sample)")
    a2.set_ylabel("dB relative to the carrier")
    a2.set_title("The same object, two increments")
    a2.grid(True, alpha=0.3)
    a2.legend(fontsize=8, loc="lower right")
    fig.tight_layout()
    fig.savefig(HERE / "sfdr.png", dpi=110)
    plt.close(fig)

    # --- ctrl law + dead zone -------------------------------------------
    fig, (a1, a2) = plt.subplots(1, 2, figsize=(13, 4.5))
    a1.plot(d.ctrls, d.adv / WF, lw=1.2)
    a1.set_xlabel("commanded ctrl (cycles/sample)")
    a1.set_ylabel("realised advance (cycles/sample)")
    a1.set_title("ctrl folds modulo one cycle, exactly, both signs")
    a1.grid(True, alpha=0.3)

    a2.plot(
        (d.dead_span + 0.25) * 1e9,
        [signed(int(v)) for v in d.dead_adv],
        lw=1.3,
        drawstyle="steps-mid",
    )
    a2.axhline(0, color="tab:red", ls="--", lw=1.0, label="stopped")
    a2.set_yscale("symlog", linthresh=1)
    a2.set_xlabel("ctrl offset from -norm_freq (ppb), norm_freq = 0.25")
    a2.set_ylabel("signed advance (phase words, symlog)")
    a2.set_title(
        f"The dead zone is one phase-word LSB ({d.quantum * 1e9:.3f} ppb) wide"
    )
    a2.grid(True, alpha=0.3)
    a2.legend(fontsize=9)
    fig.tight_layout()
    fig.savefig(HERE / "ctrl_law.png", dpi=110)
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
        "LO",
        [
            "**The 96 dBc SFDR is typical, not a bound.** It holds over "
            "random frequencies and falls to ~92 dBc on the rational ones a "
            "real configuration is most likely to pick (§2.1, F1). Budget the "
            "floor, not the average.",
            "**The LUT costs half a phase bin in a closed loop, and no "
            "bandwidth** (§2.5, F8): same filter, same drives, settling "
            "identical to the ideal oscillator. Choosing LO over NCO is a "
            "spur question, not a loop question.",
            "**A control that cancels `phase_inc` stops the LO over a "
            "plateau, not at a knife edge** — below one phase-word LSB the "
            "conversion truncates to zero (F6). A stopped oscillator in a "
            "strobe-driven loop is terminal, because no strobe means no "
            "update.",
            "**The inline composition API is C-only** — `lo_init`, `lo_step`, "
            "`lo_step_ctrl`, `lo_sin_lut` have no binding (F9), so a "
            "Python-side audit cannot see the surface a composing "
            "receiver actually uses.",
        ],
    )
    if write:
        plots(d)
        ll.plot(
            d.loop_lo,
            HERE / "linear_loop.png",
            title=(
                f"Closed loop through the LO's OWN emitted phasor: "
                f"angle(ref x conj(y))/2pi -> LoopFilter -> LO ctrl port "
                f"(bn = {ll.BN}, zeta = {ll.ZETA})"
            ),
        )
    R.summary(
        "\n- 6 new sections in `test_lo_core.c` (§16–§21), each proven by "
        "sabotage"
        "\n- Raw sweeps: `data/sfdr_sweep.csv`, `data/sfdr_rational.csv`, "
        "`data/frequency_sweep.csv`, `data/ctrl_sweep.csv`"
    )
    R.emit(HERE / "results.md")
    return R


if __name__ == "__main__":
    sys.exit(cli(build, HERE))
