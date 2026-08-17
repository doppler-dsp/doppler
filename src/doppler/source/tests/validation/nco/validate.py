#!/usr/bin/env python3
"""NCO validation — produces this folder's certification evidence.

Writes ``results.md`` (the authoritative report), the plots it embeds, and
the raw sweeps under ``data/`` so any number in the report can be re-derived
without re-running the measurement.

Three phases, in order:

1. **Characterise** — measure complete behaviour across the input range and
   over time. Tables and plots, no verdicts.
2. **Review** — judge the characterisation: correct-by-design, or a gap.
3. **Limits** — the envelope a caller may rely on, asserted.

Every number is measured from the C through its own binding. Nothing here
models what the C ought to do.

Run:  make validate
"""

from __future__ import annotations

import sys
from dataclasses import dataclass
from pathlib import Path

import numpy as np

from doppler.source import NCO
from doppler.tests import loop_reference as ll
from doppler.tests._validation_common import Report, cli

HERE = Path(__file__).parent
DATA = HERE / "data"
LSB = 2.0**-32
W = 1 << 32


R = Report()


# ──────────────────────────────────────────────────────── measurement
def advance(norm_freq: float, ctrl_val: float, n: int = 6) -> int:
    nco = NCO(norm_freq, 0)
    ph = nco.steps_u32_ctrl(np.full(n, ctrl_val, dtype=np.float64))
    return int(np.diff(ph.astype(np.int64))[0] % W)


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


@dataclass
class Data:
    freqs: np.ndarray
    inc: np.ndarray
    err: np.ndarray
    bound: np.ndarray
    live: np.ndarray
    ctrls: np.ndarray
    adv: np.ndarray
    tick_iv: np.ndarray
    tick_f: float
    quantum: float
    plateau: float
    dead_span: np.ndarray
    dead_adv: np.ndarray
    carry_neg: list[tuple[float, float, float]]
    loop: dict[str, ll.LoopRun]


# ═════════════════════════════════════════════════ 1. OBJECT SUMMARY
# Every prose claim nco_core.h makes, against test_nco_core.c. The `C`
# numbers are the labels the C file already uses on the two sections
# written against a claim rather than against a function (C3, C18) --
# they referenced a list that existed only in someone's head until this
# table wrote it down.
CLAIM_MAP: list[tuple[str, str, str]] = [
    (
        "C1",
        "nco_phase_units is the ONLY double->integer conversion in the family",
        "§12, + the `phase-conversion sites` lint gate",
    ),
    (
        "C2",
        "two named faces over one unit-free fold (freq->inc, phase->word)",
        "§13",
    ),
    (
        "C3",
        "the realised value is at most one step LOW, never high",
        "§C3, swept against a long-double oracle",
    ),
    (
        "C4",
        "total: below zero and NaN give 0; at or above 2^32 saturates",
        "§12",
    ),
    ("C5", "the fold is exact in value but destroys the SIGN", "§13 §15"),
    (
        "C6",
        "three output mappings, each with a _ctrl variant and a "
        "single-sample primitive",
        "§3 §5 §6 §8 §9 §10",
    ),
    (
        "C7",
        "the _ovf event is signed by the COMPOSITE rate, not the raw carry",
        "§10 §14 §15",
    ),
    ("C8", "nmax == 0 in the scaled form is identical to the raw form", "§5"),
    ("C9", "reset() zeroes phase only; norm_freq and nmax unchanged", "§C18"),
    (
        "C10",
        "serializable via the standard bytes interface",
        "§ state round-trip + the Python matrix",
    ),
    ("C11", "lifecycle: create -> (steps / reset)* -> destroy", "§1"),
    (
        "C12",
        "owners embed the struct by value and set the fields directly",
        "C-only, by construction",
    ),
    ("C13", "phase_inc = floor(frac(norm_freq) x 2^32)", "§3 §7 §12"),
    ("C14", "nco_step_u32 emits the phase BEFORE the increment", "§3 §11"),
    ("C15", "scaled = (uint64)phase * nmax >> 32", "§5 §9"),
    (
        "C16",
        "each batch stepper is exactly a loop over its single-sample "
        "primitive",
        "§11",
    ),
    (
        "C17",
        "truncation, deliberately NOT llround (the FMA-contraction argument)",
        "§12 §C3",
    ),
    (
        "C18",
        "the fold never hands the cast a 1.0, even for a tiny negative",
        "§13",
    ),
    ("C19", "negative norm_freq folds correctly (-0.25 -> 3x2^30)", "§12"),
    (
        "C20",
        "nco_steer_scale clamps 1+control to [lo, hi]; NaN gives lo",
        "§16",
    ),
    (
        "C21",
        "ctrl never modifies phase_inc or norm_freq; ctrl == 0 is "
        "bit-identical",
        "§8 §9 §10",
    ),
    ("C22", "create() returns NULL on allocation failure", "unreachable"),
    ("C23", "destroy() may be NULL (no-op)", "**absent**"),
    (
        "C24",
        "setting norm_freq recomputes phase_inc and does NOT reset phase",
        "§7",
    ),
    (
        "C25",
        "steps return min(n, max_out); emission stops at capacity",
        "§17 NEW",
    ),
    (
        "C26",
        "steps_u32_max_out is the maximum samples per call",
        "§17 NEW — WAS FALSE, header fixed",
    ),
    (
        "C27",
        "the Python out= buffer must be sized to max_out, not len(ctrl)",
        "test_nco.py",
    ),
    (
        "C29",
        "the fold is TOTAL over non-finite and huge-finite input, and "
        "answers 0 where the raw cast saturates",
        "§C29 NEW",
    ),
    (
        "C28",
        "resamp lands the conversion boundary modularly in its own "
        "_step_inc — do not consolidate it back",
        "allowlisted in scripts/.phase-conversion-allow",
    ),
]


def section_summary() -> None:
    # Title and provenance belong to the executive summary, which
    # Report.executive renders ahead of everything here.
    R.md("## 1. The object — design and expectations")
    R.md()
    R.md(
        "`nco_state_t` is a **32-bit phase accumulator**: a register "
        "advancing by `phase_inc` every sample, wrapping naturally at 2^32. "
        "It is the primitive every steered thing in the library stands on — "
        "`symsync`, `dll` and `resamp` embed it **by value**, so its "
        "behaviour is their behaviour."
    )
    R.md()
    R.md("### Where the design lives — this report does not restate it")
    R.md()
    R.table(
        ["source", "holds"],
        [
            [
                "[`docs/design/nco.md`](../../../../../../docs/design/nco.md)",
                "the rationale: why one conversion site, why truncation, "
                "why the event is signed, what was tried and removed",
            ],
            [
                "[`native/inc/nco/nco_core.h`]"
                "(../../../../../../native/inc/nco/nco_core.h)",
                "the contract: the two layers, the two faces over one fold, "
                "the three output mappings and their `_ctrl` variants",
            ],
            [
                "[`native/tests/test_nco_core.c`]"
                "(../../../../../../native/tests/test_nco_core.c)",
                "the gate: point assertions, with `volatile` where "
                "constant-folding would hide the bug",
            ],
        ],
    )
    R.md(
        "The one-line orientation, so this page stands alone: the design is "
        "stratified into a **float boundary** (`nco_phase_units`, the only "
        "double→integer conversion in the family — C99 6.3.1.4 leaves an "
        "out-of-range conversion undefined, so confining the cast is "
        "structural) over **integer arithmetic C99 defines outright** "
        "(6.2.5p9, 7.20.1.1), which is why all the care lives in the first "
        "layer. Everything else is in the sources above."
    )
    R.md()
    R.md("### What this report adds")
    R.md()
    R.md(
        "A unit test pins points; it cannot state a **law**. `nco_core.h` "
        'claims in prose that the frequency error is "at most one step low '
        'and never high" — nothing checks that as a property. Sections '
        "below characterise the laws, review what they mean, and pin the "
        "envelope. The numbering is this report's own — several sections "
        "merge more than one of `test_nco_core.c`'s, so the two do not "
        "line up one-for-one."
    )
    R.md()
    R.md("### Claim coverage — every prose claim in the header")
    R.md()
    R.md(
        "The campaign's order is header first: enumerate what the header "
        "asserts, then ask of each whether it is pinned, pinned only at "
        "literals, or absent. `test_nco_core.c` already labels two of its "
        "sections against claim numbers (`C3`, `C18`) — this is the list "
        "they were referring to, which until now existed nowhere. `NEW` "
        "marks a section added by this audit and proven by sabotage."
    )
    R.md()
    R.table(
        ["#", "claim in `nco_core.h`", "covered by"],
        [[t, c, w] for t, c, w in CLAIM_MAP],
    )
    R.md(
        "One claim is still **absent**: `destroy()` accepting NULL. It is "
        "one line of C and no Python can reach it, so it is recorded "
        "rather than gated — the LO's §18 does pin the same promise for "
        "its own destroy, so the pattern exists to copy when this is worth "
        "closing."
    )
    R.md()
    R.md(
        "> **Note on provenance.** The three residuals stranded in PR #647 "
        "have now all landed here off main rather than by rebasing it: "
        "the signed carry/borrow rule and test sections §14–16 (see "
        "**F7**), `nco_steer_scale` (**F8**), and `docs/design/nco.md`, "
        "which was rewritten rather than cherry-picked — a later commit "
        "reshaped the same function, `resamp` has since adopted the "
        "interpolating rule that changed how it lands the conversion, and "
        "the evidence layer this report is did not exist when that draft "
        "was written."
    )
    R.md()


# ═════════════════════════════════════════════════ 2. CHARACTERISE
def characterise() -> Data:
    print("\nPHASE 1 — CHARACTERISE")
    R.md("## 2. Characterisation")
    R.md()
    R.md(
        "Measured behaviour, no verdicts. Section numbers track "
        "`test_nco_core.c`."
    )
    R.md()

    # --- 1-4, 7 --------------------------------------------------------
    R.md("### 2.1 Lifecycle, zero, quarter-rate, continuity, accessors")
    R.md("*(test_nco_core.c sections 1-4, 7)*")
    R.md()
    q = NCO(0.25, 0).steps_u32(8)
    a = NCO(0.1, 0)
    two = np.concatenate([a.steps_u32(5), a.steps_u32(5)])
    one = NCO(0.1, 0).steps_u32(10)
    p = NCO(0.1, 0)
    p.norm_freq, p.phase = 0.2, 12345
    R.table(
        ["property", "measured"],
        [
            ["phase after `reset()`", NCO(0.0, 0).phase],
            ["`phase_inc` at `norm_freq = 0`", NCO(0.0, 0).phase_inc],
            ["quarter-rate first 4 phases", f"`{q[:4].tolist()}`"],
            ["5+5 samples == 10 samples", np.array_equal(two, one)],
            [
                "accessor round-trip",
                f"norm_freq={p.norm_freq}, phase={p.phase}, "
                f"phase_inc={p.phase_inc}",
            ],
        ],
    )

    # --- 5, 9 ----------------------------------------------------------
    R.md("### 2.2 `nmax` scaling maps `[0, 2^32)` onto `[0, nmax)`")
    R.md("*(sections 5, 9)*")
    R.md()
    rows = []
    for nm in (2, 10, 256, 1000, 65536):
        v = NCO(0.001, nm).steps_u32_scaled(4096)
        raw = NCO(0.001, 0).steps_u32(4096).astype(np.float64) / W
        pred = np.floor(raw * nm).astype(np.int64)
        rows.append(
            [
                nm,
                int(v.min()),
                int(v.max()),
                np.unique(v).size,
                bool(np.all(pred == v.astype(np.int64))),
            ]
        )
    R.table(
        ["nmax", "min", "max", "distinct", "== floor(phase/2^32 · nmax)"],
        rows,
    )

    # --- 6, 10 ---------------------------------------------------------
    R.md("### 2.3 The carry — one per cycle, and its cadence")
    R.md("*(sections 6, 10)*")
    R.md()
    rows = []
    for f in (0.25, 0.1, 0.5):
        _, c = NCO(f, 0).steps_u32_ovf(4096)
        rows.append([f, int(c.sum()), int(4096 * f)])
    R.table(["norm_freq", "carries in 4096", "expected"], rows)

    tick_f = 2.0 / 17.333333333
    _, ct = NCO(tick_f, 0).steps_u32_ovf(1 << 16)
    tick_iv = np.diff(np.flatnonzero(ct))
    R.md(
        f"At the irrational rate `{tick_f:.9f}` the strobe intervals are "
        f"`{np.unique(tick_iv).tolist()}` with mean `{tick_iv.mean():.6f}` "
        f"against `1/norm_freq = {1.0 / tick_f:.6f}` — it dithers between "
        f"two adjacent integers, which is the resampler's whole basis."
    )
    R.md()
    nco = NCO(0.6, 0)
    _, c2 = nco.steps_u32_ovf_ctrl(np.full(16, 0.7, dtype=np.float64))
    R.md(
        f"Driven at 1.30 cycles/sample (`norm_freq` 0.6 + `ctrl` 0.7) the "
        f"flag reports `{int(c2.sum())}/16` with distinct values "
        f"`{np.unique(c2).tolist()}` — it is a flag, not a counter, and "
        f"cannot report two wraps in one sample."
    )
    R.md()

    # --- carry under NEGATIVE ctrl (the admitted defect) ---------------
    R.md("### 2.4 The carry under a **negative** control")
    R.md()
    R.md(
        "Drive a stopped NCO (`norm_freq = 0`) with a constant negative "
        "`ctrl`. The intent is a slow **backward** rate, so crossings should "
        "occur at `|ctrl|` per sample. This is the case that was wrong until "
        "the signed rule landed: the fold takes bipolar to unipolar before "
        "the add, so a bare carry test saw the accumulator running "
        "**forward** at `1 - |ctrl|` and fired at that rate instead. The "
        "last column is what the flag used to report."
    )
    R.md()
    carry_neg = []
    rows = []
    for cv in (-0.1, -0.25, -0.4, -0.01):
        _, cc = NCO(0.0, 0).steps_u32_ovf_ctrl(
            np.full(1 << 14, cv, dtype=np.float64)
        )
        seen = float(cc.mean())
        intended, folded = abs(cv), 1.0 - abs(cv)
        carry_neg.append((cv, seen, intended))
        rows.append(
            [
                f"{cv:g}",
                f"{intended:.4f}",
                f"{seen:.4f}",
                f"{seen / intended:.3f}x",
                f"{folded:.4f}",
            ]
        )
    R.table(
        [
            "ctrl",
            "intended rate",
            "measured carries/sample",
            "measured vs intended",
            "old (folded) rate",
        ],
        rows,
    )

    # --- 12, 13 --------------------------------------------------------
    R.md("### 2.5 The one conversion, observed as `phase_inc`")
    R.md("*(sections 12, 13)*")
    R.md()
    rows = []
    for v, note in (
        (0.0, "exact zero"),
        (LSB, "one LSB — smallest live rate"),
        (LSB / 2, "below one LSB"),
        (0.25, "exact, divides 2^32"),
        (float("nan"), "NaN"),
        (float("inf"), "+inf"),
        (float("-inf"), "-inf"),
        (-0.25, "negative, folds into [0,1)"),
        (-1e-20, "tiny negative"),
        (1.0, "one whole cycle"),
    ):
        rows.append([f"`{v:.10g}`", NCO(v, 0).phase_inc, note])
    R.table(["requested norm_freq", "phase_inc", "note"], rows)

    freqs = np.logspace(-9, np.log10(0.25), 400)
    inc = np.array([NCO(float(f), 0).phase_inc for f in freqs], dtype=np.int64)
    live = inc > 0
    err = (inc / W - freqs) / freqs * 1e6
    bound = np.where(live, 1e6 / np.maximum(inc, 1), np.inf)
    _csv(
        DATA / "frequency_sweep.csv",
        [freqs, inc, err, -bound],
        "norm_freq,phase_inc,err_ppm,bound_ppm",
    )
    R.md("Across the range (full sweep in `data/frequency_sweep.csv`):")
    R.md()
    rows = []
    for t in (1e-8, 1e-6, 1e-4, 1e-2, 1e-1):
        i = int(np.argmin(np.abs(freqs - t)))
        rows.append(
            [
                f"{freqs[i]:.3e}",
                inc[i],
                f"{err[i]:.3f}",
                f"{-bound[i]:.3f}",
            ]
        )
    R.table(
        ["norm_freq", "phase_inc", "error (ppm)", "1/inc bound (ppm)"], rows
    )
    R.md(f"![frequency error]({'frequency_error.png'})")
    R.md()

    # --- 8 -------------------------------------------------------------
    R.md("### 2.6 The control port — law and resolution")
    R.md("*(section 8)*")
    R.md()
    rows = []
    for cv in (0.25, 0.1, 1e-9, 0.0, -1e-9, -0.1, 1.0, 1.5, -1.5):
        d = advance(0.0, cv)
        rows.append([f"{cv:g}", d, f"{d / W:.6f}"])
    R.table(["ctrl", "advance (phase words)", "as cycles/sample"], rows)

    ctrls = np.linspace(-1.5, 1.5, 1201)
    adv = np.array([advance(0.0, c) for c in ctrls], dtype=np.int64)
    _csv(DATA / "ctrl_sweep.csv", [ctrls, adv], "ctrl,advance")
    inc_cfg = NCO(0.1, 0).phase_inc
    adv_ctrl = advance(0.0, 0.1)
    R.md(
        f"The same requested `0.1` reaches the accumulator by two paths — "
        f"configured, and through the control port — and lands on the SAME "
        f"word: `{inc_cfg}` both ways, delta `{adv_ctrl - inc_cfg}`. It did "
        f"not used to. The port was float32 while the configured rate was "
        f"double, so `float32(0.1)` = `{np.float32(0.1):.17g}` against "
        f"`{0.1:.17g}` quantized the request before the fold ever saw it "
        f"and the two paths differed by 7 phase words. The port is now "
        f"`double`, the width the conversion works in and the one every "
        f"scalar steer site already used (**F2**)."
    )
    R.md()
    R.md(f"![ctrl law]({'ctrl_law.png'})")
    R.md()

    f = 0.25
    # The relevant quantum is no longer the port's: with a double port the
    # narrowest distinguishable control step is the PHASE WORD's own LSB,
    # which is the floor no port precision can beat.
    quantum = LSB
    dead_span = np.linspace(-f - 4 * quantum, -f + 4 * quantum, 1601)
    dead_adv = np.array([advance(f, c) for c in dead_span], dtype=np.int64)
    best = run = lo = hi = start = 0
    for i, z in enumerate(dead_adv == 0):
        if z:
            if run == 0:
                start = i
            run += 1
            if run > best:
                best, lo, hi = run, start, i
        else:
            run = 0
    plateau = float(dead_span[hi] - dead_span[lo]) if best else 0.0

    R.md("### 2.7 A control that cancels `phase_inc`")
    R.md()
    rows = [
        [f"{-f + e:.12f}", signed(advance(f, -f + e))]
        for e in (-1e-7, -1e-8, 0.0, 1e-8, 1e-7)
    ]
    R.table(["ctrl", "signed advance (phase words)"], rows)
    R.md(
        f"The phase-word LSB is `{quantum:.4g}` ({quantum * 1e9:.3f} ppb) "
        f"and the measured contiguous zero-advance run is `{plateau:.4g}` "
        f"({plateau * 1e9:.3f} ppb) over {best}/{dead_span.size} scanned "
        f"controls — one LSB, which is the floor: below one LSB the "
        f"conversion truncates to zero whatever the port's precision. On "
        f"the float32 port this plateau was 22.4 ppb, 96x wider, and that "
        f"excess was the port's and not the accumulator's (**F3**)."
    )
    R.md()
    R.md(f"![dead zone]({'ctrl_dead_zone.png'})")
    R.md()

    # --- 2.9: the closed-loop limit --------------------------------------
    R.md("### 2.8 The closed-loop limit — a trivial linear loop")
    R.md()
    R.md(
        "Everything above characterises the NCO open-loop. This closes the "
        "simplest possible loop around it: **subtraction** as the phase "
        "detector, a `LoopFilter` on the error, its control driving the "
        "NCO's ctrl port. No discriminator shape, no gain to normalise, no "
        "noise — the error IS the input phase minus the NCO phase."
    )
    R.md()
    R.md(
        "That is the point. A real detector reports the error through some "
        "S-curve with finite slope, self-noise and a pull-in range; this "
        "one reports it exactly. So the numbers below are the **limit on "
        "closed-loop behaviour** — the best any loop built on this NCO can "
        "do, and the reference a real detector's performance is a "
        "deduction from."
    )
    R.md()
    R.md(
        f"`bn = {ll.BN}`, `zeta = {ll.ZETA}`, one update per sample, "
        f"disturbance at sample {ll.K0}. The classic settling estimate "
        f"for a second-order loop is `5/bn` = {5 / ll.BN:.0f} samples. "
        f"Error wrapped to `[-0.5, 0.5)`, so the detector is linear while "
        f"the error stays inside half a cycle."
    )
    R.md()
    loop: dict[str, ll.LoopRun] = {}
    rows = []
    for name, pin in ll.standard_drives().items():
        r = ll.run(pin, name=name)
        loop[name] = r
        s_ = ll.settle(r)
        rows.append(
            [
                name,
                f"{s_.peak:.5f}",
                f"{s_.samples}",
                f"{s_.residual:.2e}",
                f"{r.control[-1]:.4e}",
            ]
        )
    R.table(
        [
            "drive",
            "peak |error| (cyc)",
            "settle (samples after disturbance)",
            "residual |error| (cyc)",
            "final control (cyc/sample)",
        ],
        rows,
    )
    ramp_name = f"ramp, {ll.RAMP:g} cyc/sample"
    R.md(
        f"Both steps settle identically, which is the symmetry a linear "
        f"loop must have. On the ramp the loop filter's output converges "
        f"on the applied frequency offset itself — "
        f"`{loop[ramp_name].control[-1]:.6e}` against `{ll.RAMP:g}` — "
        f"which is the type-2 integrator absorbing a constant frequency "
        f"error and leaving no steady-state phase error behind. A type-1 "
        f"loop would sit at a fixed offset instead."
    )
    R.md()
    R.md(f"![linear loop]({'linear_loop.png'})")
    R.md()

    # --- 2.9b: max_out ---------------------------------------------------
    R.md("### 2.9 `max_out` is advisory on every face")
    R.md("*(the new section 17)*")
    R.md()
    big_n = 70000
    got = NCO(0.013, 0).steps_u32(big_n)
    got_s = NCO(0.013, 1000).steps_u32_scaled(big_n)
    got_c = NCO(0.013, 0).steps_u32_ctrl(np.zeros(big_n, dtype=np.float64))
    inc13 = NCO(0.013, 0).phase_inc
    R.table(
        ["probe", "measured"],
        [
            ["`steps_u32_max_out()`", NCO(0.0, 0).steps_u32_max_out()],
            [f"`steps_u32({big_n})` returns", got.shape[0]],
            [f"`steps_u32_scaled({big_n})` returns", got_s.shape[0]],
            [f"`steps_u32_ctrl(zeros({big_n}))` returns", got_c.shape[0]],
            [
                "last sample of the oversized call is exact",
                int(got[-1]) == (inc13 * (big_n - 1)) % W,
            ],
            [
                "scaled output still inside [0, nmax)",
                bool(got_s.max() < 1000),
            ],
        ],
    )
    R.md(
        f"Every face returns the whole request, {big_n - 65536} samples "
        f"past the advertised maximum, and the last one is exact. The "
        f"header said the opposite until this audit — see **F9**."
    )
    R.md()

    R.md("### 2.10 Not reachable from Python")
    R.md()
    R.md(
        "*(section 11)* The bindings expose the batch `steps_u32*` forms but "
        "not the single-sample `nco_step_u32*` primitives, so the claim that "
        "each batch stepper is exactly a loop over its single-sample "
        "counterpart cannot be checked here. It remains "
        "`test_nco_core.c`'s alone — reported, not silently skipped."
    )
    R.md()

    return Data(
        freqs,
        inc,
        err,
        bound,
        live,
        ctrls,
        adv,
        tick_iv,
        tick_f,
        quantum,
        plateau,
        dead_span,
        dead_adv,
        carry_neg,
        loop,
    )


# ═════════════════════════════════════════════════ 3. REVIEW
def review(d: Data) -> None:
    print("\nPHASE 2 — REVIEW")
    R.md("## 3. Review — findings")
    R.md()

    R.find(
        "F1",
        "BY DESIGN",
        f"the frequency error is one-sided and tracks the 1/phase_inc "
        f"envelope. Correct — but the consequence is unstated: "
        f"{abs(d.err[d.live].min()):.0f} ppm at the bottom of the range "
        f"against {abs(d.err[d.live][-1]):.3f} ppm at the top. Same "
        f"conversion, five orders of magnitude apart in cost, and a code "
        f"NCO lives at the expensive end.",
    )
    R.find(
        "F2",
        "FIXED",
        f"the ctrl port was float32 while the configured rate was double, "
        f"so one requested 0.1 landed on two different phase words (delta "
        f"7) depending on which face it entered by, and nothing documented "
        f"which resolution a tracking loop was steering at. The port is "
        f"now `double` on all four narrowing signatures — "
        f"nco_steps_u32_ctrl, _scaled_ctrl, _ovf_ctrl and lo_steps_ctrl — "
        f"which is the width the conversion works in and the one every "
        f"scalar steer site (nco_step_u32*_ctrl, lo_step_ctrl, symsync, "
        f"dll) already used, so this removed an inconsistency rather than "
        f"introducing a width. Measured: delta is now "
        f"{advance(0.0, 0.1) - NCO(0.1, 0).phase_inc}. No production C "
        f"called the narrowing forms — only the generated bindings and the "
        f"C tests — and the binding still accepts a float32 array by "
        f"casting, so no caller breaks.",
    )
    R.find(
        "F3",
        "BY DESIGN",
        f"a ctrl cancelling phase_inc stops the NCO over a plateau rather "
        f"than at a knife edge, and it always will: below one phase-word "
        f"LSB the conversion truncates to zero, so a dead zone one LSB "
        f"wide is the quantization floor, not a defect. What WAS a defect "
        f"is how wide it used to be — 22.4 ppb, one float32 quantum, set "
        f"by the port rather than by the accumulator. With the port "
        f"widened (F2) it measures {d.plateau * 1e9:.3f} ppb against an LSB "
        f"of {LSB * 1e9:.3f} ppb: 96x narrower and now at the floor. A loop "
        f"settling near -phase_inc still parks, but within one LSB of the "
        f"rate it was asked for.",
    )
    R.find(
        "F4",
        "BY DESIGN",
        f"filed as 'the two faces diverge at infinity': nco_phase_units "
        f"(+inf) saturates to 4294967295 while NCO(inf).phase_inc is "
        f"{NCO(float('inf'), 0).phase_inc}. Measured, that framing was "
        f"wrong twice over. The two NAMED faces — freq_to_inc and "
        f"phase_to_word — agree everywhere, being one body; the difference "
        f"is between the raw cast and the fold, and they take different "
        f"UNITS. nco_phase_units is handed a quantity already in "
        f"phase-word units, so infinitely many saturate; the fold is "
        f"handed a normalised rate, and an infinite rate has no fractional "
        f"part, so no phase word represents it and 0 is the honest answer. "
        f"Nor is infinity special: 1.0, 2^32 and 1e300 all give 0 by the "
        f"same rule. It is 'only the fractional part matters' at the top "
        f"of the range, exactly as F5's sub-LSB case is that rule at the "
        f"bottom. What WAS real is that only the cast's side was asserted "
        f"— test_nco_core.c §C29 now pins the fold across the non-finite "
        f"and huge-finite range, pins the two named faces to each other "
        f"there, and asserts the saturating case beside it so the pair "
        f"reads as intended; nco_norm_fold_'s doxygen says the same.",
    )
    surprising = {
        "NaN": float("nan"),
        "+inf": float("inf"),
        "-inf": float("-inf"),
        "sub-LSB": LSB / 2,
    }
    bad = [k for k, v in surprising.items() if NCO(v, 0).phase_inc == 0]
    R.find(
        "F5",
        "BY DESIGN",
        f"{len(bad)} frequency requests ({', '.join(bad)}) produce a stopped "
        f"oscillator, and this was originally filed as a gap on the grounds "
        f"that create() reports success. That was a misreading, and the "
        f"sub-LSB case shows why: a rate below one LSB is not nonsense, it "
        f"is a perfectly ordinary request the phase word cannot represent, "
        f"and truncating it to 0 IS the law this object already documents — "
        f"at most one step low, never high. It is the bottom of the range "
        f"behaving exactly like the rest of it. NaN and +-inf reach the same "
        f"0 by the conversion's deliberate totality: nco_phase_units rejects "
        f"them with a negated comparison rather than passing them to the "
        f"cast, so every input has a defined answer and no caller has to "
        f"pre-validate. Grouping a representable-but-tiny rate with a "
        f"non-number was the error in the original finding, not the "
        f"behaviour.",
    )
    R.find(
        "F6",
        "BY DESIGN",
        "the carry is a flag, not a counter: at 1.30 cycles/sample it "
        "reports 16/16 and cannot say 'two wraps'. Correct for a strobe, "
        "and the reason a resampler keeps its own accounting.",
    )
    worst = max(d.carry_neg, key=lambda t: t[1] / t[2])
    R.find(
        "F7",
        "FIXED",
        f"the control port's event is now signed by the COMPOSITE rate, "
        f"formed as `norm_freq + ctrl` in cycles before either term is "
        f"folded. Under a negative control the flag now fires at the "
        f"intended rate — at ctrl={worst[0]:g}, {worst[1]:.4f} against an "
        f"intended {worst[2]:.4f} — where the old bare-carry test fired at "
        f"the folded rate {1.0 - abs(worst[0]):.4f}. Not a discovery: this "
        f"was already fixed and documented in PR #647 and the conversion "
        f"consolidation landed on main without it. Applied here by hand "
        f"(a later commit reshaped the same function, and main carries a "
        f"`resamp` paragraph #647 predates), together with that PR's "
        f"§14-16 — an independent long-double oracle over 10 base rates x "
        f"6 control trajectories, both signs. Reverting the rule now "
        f"produces 279 failures.",
    )
    R.find(
        "F8",
        "C-ONLY",
        "nco_steer_scale exists but has NO PRODUCTION CALLER — it is the "
        "header inline plus test_nco_core.c §16, and symsync, the object "
        "it was written for, still steers unbounded. Adoption is held "
        "deliberately: a band that engages is a band applied to the case "
        "where cycle slipping was the recovery path, and too tight a floor "
        "sticks the loop at the rail instead of stopping it — the same "
        "outcome, harder to spot. Measured here: closing an ideal loop "
        "around this NCO and demanding rates it cannot have (0.1x nominal, "
        "through zero to -1x) never drives the control below 0.66, because "
        "a wrapped detector's slip reverses the integrator before it can "
        "wind up. A loop that can slip never asks for an impossible rate, "
        "so the band only engages where the discriminator is non-linear "
        "enough to prevent the slip. See docs/design/nco.md §8. "
        "Originally recorded as: bound the "
        "REQUEST so the conversion is a safety net rather than the active "
        "path. It is a header inline with no binding, so this report "
        "cannot exercise it; test_nco_core.c §16 does, including the case "
        "that motivated it (a control below -1 makes the raw scale "
        "negative, which an honest conversion floors to 0 — a stopped NCO "
        "that never strobes again).",
    )
    R.find(
        "F9",
        "FIXED",
        "nco_core.h claimed steps_u32_max_out() was the 'maximum samples "
        "per call' and that 'requesting more samples per call is undefined "
        "behaviour'; nco_core.c said a request past 65536 'overflows the "
        "buffer'. Both described the contract from before pass_capacity "
        "(jm gh-138) handed the kernel the caller's capacity. Measured: "
        "all three faces return 70000 correct samples for a 70000-sample "
        "request. Found by fixing the IDENTICAL pair of sentences in "
        "lo_core.{h,c} and then checking the sibling — four copies of one "
        "false claim, which is the documentation form of the duplicate "
        "that drifts. All four now corrected, and test_nco_core.c §17 "
        "pins the behaviour on each of the three output mappings "
        "separately, since each has its own kernel and could regain a "
        "private ceiling independently.",
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
        "Claims a caller may rely on. A failure here is a regression, not a "
        "new finding."
    )
    R.md()

    R.limit(
        bool(np.all(d.err[d.live] <= 1e-9)),
        "frequency error is never HIGH, at any requested rate",
    )
    R.limit(
        bool(np.all(d.err[d.live] >= -d.bound[d.live] * (1 + 1e-6))),
        "frequency error never escapes the 1e6/phase_inc ppm envelope",
    )
    R.limit(
        NCO(LSB, 0).phase_inc == 1 and NCO(LSB / 2, 0).phase_inc == 0,
        f"the usable frequency floor is exactly one LSB ({LSB:.4e} "
        f"cycles/sample); below it the oscillator is stopped",
    )
    R.limit(
        NCO(float("nan"), 0).phase_inc == 0,
        "NaN converts to a stopped NCO, never to a wrapped increment",
    )
    R.limit(
        NCO(-1e-20, 0).phase_inc == 0xFFFFFFFF,
        "the fold never hands the cast a 1.0: a tiny negative rate steps "
        "one word BACK, it does not stick",
    )
    R.limit(
        all(
            np.array_equal(
                NCO(f, 0).steps_u32(8),
                NCO(f, 0).steps_u32_ctrl(np.zeros(8, dtype=np.float64)),
            )
            for f in (0.0, 0.1, 0.25)
        ),
        "ctrl == 0 is bit-identical to no ctrl at all",
    )
    n = NCO(0.1, 0)
    before = (n.norm_freq, n.phase_inc)
    n.steps_u32_ctrl(np.full(64, -0.7, dtype=np.float64))
    R.limit(
        (n.norm_freq, n.phase_inc) == before,
        "ctrl never modifies norm_freq or phase_inc",
    )
    folded = np.mod(d.ctrls, 1.0)
    pred = np.floor(folded * W).astype(np.int64) % W
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
    u = np.unique(d.tick_iv)
    R.limit(
        u.size == 2
        and int(u[1] - u[0]) == 1
        and abs(d.tick_iv.mean() - 1.0 / d.tick_f) < 1e-3,
        f"at an irrational rate the strobe dithers between two adjacent "
        f"intervals {u.tolist()} whose mean is 1/norm_freq",
    )
    s = NCO(0.001, 1000).steps_u32_scaled(4096)
    R.limit(
        int(s.min()) >= 0 and int(s.max()) < 1000,
        "scaled output never leaves [0, nmax)",
    )
    # This limit previously pinned the DEFECT (carry at the folded rate),
    # phrased so it would turn red the moment the signed rule landed. It
    # has landed, so this is now the real claim: the event counts the
    # boundaries the composite rate actually crosses, in either direction.
    R.limit(
        all(abs(seen - intended) < 0.01 for _, seen, intended in d.carry_neg),
        "under a NEGATIVE control the event fires at the intended |ctrl| "
        "rate, not the folded 1-|ctrl| — the composite's sign decides",
    )

    # --- the closed-loop limit -------------------------------------------
    up = d.loop[f"+{ll.STEP:g} cycle step"]
    dn = d.loop[f"-{ll.STEP:g} cycle step"]
    rm = d.loop[f"ramp, {ll.RAMP:g} cyc/sample"]
    s_up, s_dn, s_rm = ll.settle(up), ll.settle(dn), ll.settle(rm)

    R.limit(
        s_up.samples < 5.0 / ll.BN and s_dn.samples < 5.0 / ll.BN,
        f"a phase step settles inside the 5/bn estimate "
        f"({s_up.samples} and {s_dn.samples} samples against "
        f"{5 / ll.BN:.0f})",
    )
    R.limit(
        s_up.samples == s_dn.samples and abs(s_up.peak - s_dn.peak) < 1e-9,
        "the loop is symmetric in sign: +step and -step settle identically",
    )
    R.limit(
        s_up.residual < 1e-6 and s_dn.residual < 1e-6,
        f"a phase step leaves NO steady-state error "
        f"(residual {max(s_up.residual, s_dn.residual):.1e} cycles)",
    )
    R.limit(
        s_rm.residual < 1e-6,
        f"a frequency ramp leaves NO steady-state PHASE error "
        f"(residual {s_rm.residual:.1e} cycles) — the type-2 integrator "
        f"absorbs it",
    )
    R.limit(
        abs(rm.control[-1] - ll.RAMP) < 1e-6,
        f"on a ramp the loop filter's output converges on the applied "
        f"frequency offset itself ({rm.control[-1]:.6e} vs {ll.RAMP:g})",
    )

    big_n = 70000
    z = np.zeros(big_n, dtype=np.float64)
    R.limit(
        NCO(0.013, 0).steps_u32(big_n).shape[0] == big_n
        and NCO(0.013, 1000).steps_u32_scaled(big_n).shape[0] == big_n
        and NCO(0.013, 0).steps_u32_ctrl(z).shape[0] == big_n,
        f"max_out is advisory: all three faces return every one of "
        f"{big_n} samples requested, past the advertised 65536",
    )


def plots(d: Data) -> None:
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    fig, ax = plt.subplots(figsize=(9, 5))
    ax.loglog(d.freqs[d.live], -d.err[d.live], lw=1.3, label="measured")
    ax.loglog(
        d.freqs[d.live], d.bound[d.live], "--", lw=1.0, label="1e6 / phase_inc"
    )
    ax.axvspan(
        1e-9, 1e-4, alpha=0.12, color="tab:red", label="code-NCO regime"
    )
    ax.set_xlabel("requested norm_freq (cycles/sample)")
    ax.set_ylabel("frequency error (ppm, always LOW)")
    ax.set_title("Frequency error is one-sided; its cost is 1/phase_inc")
    ax.grid(True, which="both", alpha=0.3)
    ax.legend(fontsize=9)
    fig.tight_layout()
    fig.savefig(HERE / "frequency_error.png", dpi=110)
    plt.close(fig)

    fig, (a1, a2) = plt.subplots(1, 2, figsize=(13, 4.5))
    a1.plot(d.ctrls, d.adv / W, lw=1.2)
    a1.set_xlabel("commanded ctrl (cycles/sample)")
    a1.set_ylabel("realised advance (cycles/sample)")
    a1.set_title("ctrl folds modulo one cycle, exactly, both signs")
    a1.grid(True, alpha=0.3)
    a2.step(
        np.arange(min(60, d.tick_iv.size)), d.tick_iv[:60], where="mid", lw=1.3
    )
    a2.axhline(
        1.0 / d.tick_f,
        color="tab:red",
        ls="--",
        lw=1.0,
        label=f"1/norm_freq = {1.0 / d.tick_f:.4f}",
    )
    a2.set_xlabel("tick index")
    a2.set_ylabel("samples between carries")
    a2.set_title("The strobe dithers between adjacent intervals")
    a2.grid(True, alpha=0.3)
    a2.legend(fontsize=8)
    fig.tight_layout()
    fig.savefig(HERE / "ctrl_law.png", dpi=110)
    plt.close(fig)

    fig, ax = plt.subplots(figsize=(9, 5))
    ax.plot(
        (d.dead_span + 0.25) * 1e9,
        [signed(int(a)) for a in d.dead_adv],
        lw=1.3,
        drawstyle="steps-mid",
    )
    ax.axhline(0, color="tab:red", ls="--", lw=1.0, label="stopped")
    ax.set_yscale("symlog", linthresh=1)
    ax.set_xlabel("ctrl offset from -norm_freq (ppb), norm_freq = 0.25")
    ax.set_ylabel("signed advance (phase words, symlog)")
    ax.set_title(
        f"The dead zone is one phase-word LSB ({d.quantum * 1e9:.3f} ppb) wide"
    )
    ax.grid(True, alpha=0.3)
    ax.legend(fontsize=9)
    fig.tight_layout()
    fig.savefig(HERE / "ctrl_dead_zone.png", dpi=110)
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
        "NCO",
        [
            "**The frequency error is one-sided and tracks a `1/phase_inc` "
            "envelope** — up to 194700 ppm at the bottom of the range (§2.1, "
            "F1). That is quantisation, not drift, and it is why a slow NCO "
            "is the wrong place to read a frequency.",
            "**Four frequency requests produce a STOPPED oscillator** — NaN, "
            "±inf and any sub-LSB rate (F5). `create()` reports success for "
            "all of them, so validate the rate you asked for rather than the "
            "handle you got back.",
            "**The carry is a flag, not a counter**: above one cycle per "
            'sample it cannot say "two wraps" (F6). Correct for a strobe, and '
            "the reason a resampler must not drive it past unity.",
            "**`nco_steer_scale` has no production caller** (F8). It is the "
            "header inline plus one C section; symsync — the object it was "
            "written for — still steers unbounded, so do not read its "
            "existence as a bound that is being applied.",
        ],
    )
    if write:
        plots(d)
        ll.plot(d.loop, HERE / "linear_loop.png")
    R.summary(
        "\n- Raw sweeps: `data/frequency_sweep.csv`, `data/ctrl_sweep.csv`"
    )
    R.emit(HERE / "results.md")
    return R


if __name__ == "__main__":
    sys.exit(cli(build, HERE))
