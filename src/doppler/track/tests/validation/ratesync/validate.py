#!/usr/bin/env python3
"""RateSync validation — produces this folder's certification evidence.

Writes ``results.md`` (the authoritative report), the plots it embeds, and
the raw sweeps under ``data/`` so any number in the report can be re-derived
without re-running the measurement.

Three phases, in order:

1. **Characterise** — measure complete behaviour across the input range and
   over time. Tables and plots, no verdicts.
2. **Review** — judge the characterisation: correct-by-design, or a gap.
3. **Limits** — the envelope a caller may rely on, asserted.

Every number is measured from the C through its own binding. Nothing here
models what the C ought to do, and nothing here re-implements a measurement
the library already owns: EVM and the settling budget come from
``doppler.ber``, the closed-loop reference from ``doppler.tests
.loop_reference``, the stimulus pulse from ``doppler.wfm.rrc_taps`` through
a real ``FIR``, and the loop's internal signals from its own telemetry
probes rather than from anything recomputed here.

Run:  make validate
"""

from __future__ import annotations

import sys
from dataclasses import dataclass, field
from pathlib import Path

import numpy as np

from doppler.ber import ber_evm_db, ber_settle_syms
from doppler.filter import FIR
from doppler.resample import MatchedRateConverter
from doppler.telemetry import Telemetry
from doppler.tests import loop_reference as ll
from doppler.tests._validation_common import Report, cli
from doppler.track import RateSync
from doppler.wfm import rrc_taps

HERE = Path(__file__).parent
DATA = HERE / "data"

BETA = 0.35
SPAN = 8
NSYM = 3000
#: Samples per symbol of the GENERATION grid. A fractional timing offset is
#: placed exactly by generating here and decimating by ``FINE // sps``, so an
#: offset of ``j`` fine samples is exactly ``j / FINE`` symbols — no
#: interpolation, and no second implementation of the pulse.
#:
#: Held FIXED rather than as a multiple of `sps`, which is what bounds the
#: cost: the shaping filter is ``2*span*FINE + 1`` taps over ``nsym*FINE``
#: samples, so a grid pinned to ``16*sps`` would run a 16385-tap filter over
#: three million samples at ``sps = 64`` and made this report a seven-minute
#: pytest. 64 samples/symbol is 1/64-symbol timing resolution, finer than any
#: offset this report places.
FINE = 64

R = Report()


# ────────────────────────────────────────────────────────── stimulus
def symbols(nsym: int, seed: int = 7) -> np.ndarray:
    """Deterministic +-1 BPSK.

    A timing detector only learns from symbol TRANSITIONS, so the sequence
    has to be genuinely random across consecutive symbols; a one-shot LCG of
    the index is strongly periodic and starves the eye statistic. This is
    the same trap `test_ratesync_core.c` documents.
    """
    rng = np.random.default_rng(seed)
    return np.where(rng.integers(0, 2, nsym) > 0, 1.0, -1.0)


def decim_for(sps: int) -> int:
    """Decimation from the generation grid down to `sps` samples/symbol."""
    return max(1, FINE // int(sps))


def grid_of(sps: int) -> int:
    """Samples per symbol actually generated for this `sps`.

    ``FINE`` when it divides evenly, else the nearest multiple of `sps` at
    or above it, so the decimation stays an integer and the offset stays
    exact.
    """
    return decim_for(sps) * int(sps)


def analytic_rrc(fine: int) -> np.ndarray:
    """`wfm.rrc_taps` rescaled to the ANALYTIC pulse the C fixtures use.

    `rrc_taps` returns unit-ENERGY taps over its own sample grid, so its
    peak falls as the grid is refined: 0.1937 at 32 samples/symbol, 0.1370
    at 64, 0.0968 at 128. The analytic root-raised cosine `wfm_rrc_h` — what
    `test_ratesync_core.c` shapes with, and what the object's unit-amplitude
    input contract is stated against — has a fixed peak of
    ``1 + beta*(4/pi - 1)`` = 1.09563 at beta = 0.35.

    The two differ by exactly ``sqrt(fine)``, and that is an identity rather
    than a fitted constant: measured peak * sqrt(fine) = 1.095653 at every
    grid above against an analytic 1.095634. Rescaling here is what makes
    the Python stimulus the SAME waveform as the C one, in the same units,
    so an EVM measured on either side is comparable. Getting this wrong is
    not subtle — leaving the taps unit-energy under-drives the loop by
    ~20 dB and every acquisition claim in this report fails.
    """
    return rrc_taps(BETA, fine, SPAN).astype(np.float64) * np.sqrt(fine)


_SHAPED: dict[tuple, np.ndarray] = {}


def shaped_stream(
    sps: int, *, nsym: int = NSYM, amp: float = 1.0, seed: int = 7
) -> tuple[np.ndarray, int]:
    """The fine-grid RRC-shaped BPSK both stimulus helpers decimate.

    Memoised: every offset in a sweep decimates the SAME shaped stream, so
    regenerating it per offset multiplied the report's cost by eight for no
    change in what it measures.
    """
    fine = grid_of(sps)
    key = (fine, nsym, amp, seed)
    if key not in _SHAPED:
        taps = analytic_rrc(fine).astype(np.complex64)
        up = np.zeros(nsym * fine, dtype=np.complex64)
        up[::fine] = (amp * symbols(nsym, seed)).astype(np.complex64)
        _SHAPED[key] = np.asarray(FIR(taps).execute(up))
    return _SHAPED[key], fine


def rrc_bpsk(
    sps: int,
    tau_fine: int,
    *,
    nsym: int = NSYM,
    amp: float = 1.0,
    seed: int = 7,
) -> np.ndarray:
    """RRC-shaped BPSK at an INTEGER `sps`, offset by `tau_fine` fine samples.

    Built from doppler's own pulse and its own filter: `analytic_rrc` on the
    generation grid, applied by a real `FIR`, then decimated starting at
    `tau_fine`. The timing offset is therefore exactly ``tau_fine / grid``
    symbols and the pulse is the library's, not a numpy transcription of the
    same formula.

    Parameters
    ----------
    sps : int
        Samples per symbol of the returned stream.
    tau_fine : int
        Timing offset in fine samples, ``0 <= tau_fine < grid_of(sps)``.
    nsym, amp, seed : optional
        Symbol count, symbol amplitude and PRNG seed.

    Returns
    -------
    ndarray of complex64
    """
    y, _ = shaped_stream(sps, nsym=nsym, amp=amp, seed=seed)
    return np.ascontiguousarray(
        y[tau_fine :: decim_for(sps)].astype(np.complex64)
    )


def tau_symbols(sps: int, tau_fine: int) -> float:
    """The offset `rrc_bpsk` actually realised, in symbols."""
    return tau_fine / float(grid_of(sps))


# ────────────────────────────────────────────────────────── measurement
def settle(bn: float) -> int:
    """Symbols to discard before a steady-state number means anything.

    `ber_settle_syms` is the library's own answer — ``2 * (5/Bn)`` for the
    one running loop. A window pinned to a fraction of the record is the
    documented way a receiver test measures the acquisition transient and
    reports it as steady state.
    """
    return int(ber_settle_syms(bn, 0.0))


def record_syms(bn: float) -> int:
    """Symbols to transmit so a steady-state EVM at `bn` is measurable.

    The settling budget scales as ``1/bn``, so a record length that is fine
    at one bandwidth silently stops being a measurement at another: at
    ``bn = 0.002`` the budget alone is 5000 symbols, and a 3000-symbol
    record leaves NO settled window at all. Deriving the length from the
    budget is what stops a sweep reporting a sentinel as if it were data.
    """
    return max(NSYM, 3 * settle(bn))


def evm_db(y: np.ndarray, bn: float) -> float:
    """Steady-state EVM (dB) of a recovered BPSK stream, via `ber_evm_db`.

    Raises rather than returning a sentinel when the record is too short to
    contain a settled window. `ber_evm_db` answers 0.0 dB for "no lock",
    which in a report table is indistinguishable from a real measurement of
    a scattered constellation — and a sweep that quietly filled a column
    with it is exactly the failure this validator exists to catch.
    """
    lo = settle(bn)
    if y.size < lo + 100:
        raise ValueError(
            f"record of {y.size} symbols has no settled window at bn={bn:g} "
            f"(budget {lo}); use record_syms(bn)"
        )
    return float(ber_evm_db(y, lo, y.size, 2))


def run_sync(
    x: np.ndarray,
    sps: float,
    *,
    bn: float = 0.01,
    m: int = 2,
    pulse: str = "rrc",
    probes: bool = False,
):
    """Push one stream through a RateSync and return it with its symbols.

    With `probes`, the loop's six telemetry channels are captured — the
    object's own instrumentation, rather than anything recomputed here.
    """
    rs = RateSync(
        sps=sps,
        pulse=pulse,
        beta=BETA,
        span=SPAN,
        m=m,
        num_phases=1024,
        bn=bn,
        zeta=0.707,
    )
    tlm = None
    if probes:
        tlm = Telemetry(1 << 20)
        rs.set_telemetry(tlm, "sync")
    y = np.asarray(rs.steps(x))
    rec = tlm.read_dict() if tlm is not None else {}
    return rs, y, rec


def _csv(path, cols, header: str) -> None:
    """Write one raw sweep, unless this run is measurement-only."""
    if R.write:
        np.savetxt(
            path,
            np.column_stack(cols),
            delimiter=",",
            header=header,
            comments="",
        )


# ═════════════════════════════════════════════════ 1. OBJECT SUMMARY
# Every prose claim ratesync_core.h makes, against test_ratesync_core.c.
# The section numbers are that file's own; `NEW` marks a section this
# audit added and proved by sabotage before trusting it.
CLAIM_MAP: list[tuple[str, str, str]] = [
    (
        "C1",
        "owns a RateConverter whose terminal stage carries the pulse; "
        "builds no filters of its own",
        "§2",
    ),
    (
        "C2",
        "the matched filter IS the cascade's last dot product; the "
        "polyphase arm IS the fractional delay",
        "C-only, by construction",
    ),
    ("C3", "the bank is sized by the POST-decimation rate", "§2, as a bound"),
    ("C4", "arbitrary rate by construction: sps is a double", "§3 §4"),
    (
        "C5",
        "the TED normaliser is |on|^2 + |mid|^2, never |on|^2",
        "**STALE** — see F1",
    ),
    (
        "C6",
        "normalising by |on|^2 alone kills the cascade permanently",
        "historical, ungated",
    ),
    ("C7", "no clamp on the control anywhere", "§2.7 (this report)"),
    ("C8", "the loop stays open until the cascade is primed", "§8 NEW"),
    (
        "C9",
        "each parity's S-curve has two zeros per symbol, one stable and "
        "one unstable, so the parity does not matter",
        "§2.2 (this report)",
    ),
    (
        "C10",
        "the Measured table: 8/8 lock at three planned cascades",
        "§3, at looser EVM limits — see F3",
    ),
    (
        "C11",
        "bn behaves identically across all three (within ~2 dB at every "
        "setting)",
        "§2.5 — **holds only at bn >= 0.005**, see F4",
    ),
    (
        "C12",
        "bn = 0.005 measured best (-46 / -40 / -40 dB)",
        "§2.5 — choice holds, numbers stale (F3)",
    ),
    ("C13", "lifecycle: create -> (step/steps/reset)* -> destroy", "§1 §7"),
    ("C14", "m even, 2 <= m <= RATESYNC_MAX_M", "§1"),
    ("C15", "num_phases a power of two >= 2", "§1"),
    ("C16", "sps >= m, because rate = m/sps must not exceed 1", "§1 §19 NEW"),
    ("C17", "every knob rejects rather than coercing; NaN too", "§1"),
    (
        "C18",
        "the timing loop holds no cascade and is reused verbatim by the "
        "receivers",
        "§11 NEW",
    ),
    (
        "C19",
        "the loop is told the terminal rate and the terminal tap count",
        "§2 §8 NEW §11 NEW",
    ),
    ("C20", "`term` is telemetry-only and NULL when bound by hand", "§8 NEW"),
    ("C21", "ted_scale is the detector's own slope, computed once", "§10 NEW"),
    (
        "C22",
        "ctrl is referenced to the TERMINAL rate; the cascade rate would "
        "under-drive by the whole decimation",
        "§12 NEW",
    ),
    ("C23", "the loop discards prime_taps + 1 outputs", "§8 NEW"),
    ("C24", "one input can complete TWO terminal outputs", "§9 NEW"),
    (
        "C25",
        "`rate = m/sps <= 1` so at most one output per input",
        "**FALSE** — see F2",
    ),
    ("C26", "DTTL is a supported detector (BPSK/QPSK only)", "§10 NEW"),
    (
        "C27",
        "the caller owns the input level; present unit-amplitude symbols",
        "§2.6 (this report)",
    ),
    (
        "C28",
        "over-drive IS reported by get_clipped(); there is no under-drive "
        "twin (gh-661)",
        "§13 NEW + §2.6",
    ),
    ("C29", "get_clipped() is always 0 when the plan has no CIC", "§13 NEW"),
    (
        "C30",
        "use m >= 4 with IANDD (lock_stat -0.34 vs +0.95)",
        "§14 NEW — rule holds, numbers stale (F5)",
    ),
    ("C31", "reset() reproduces the first run bit for bit", "§6"),
    (
        "C32",
        "configure() preserves the integrator, and so the lock",
        "§17 NEW",
    ),
    (
        "C33",
        "configure_lock_raw() drops the lock and clears the block; avgs "
        "is clamped >= 1",
        "§17 NEW",
    ),
    (
        "C34",
        "set_telemetry registers six probes; the attach fails WHOLE",
        "§16 NEW",
    ),
    ("C35", "rate_est departs from nominal by the sample-clock offset", "§4"),
    ("C36", "lock_stat, not EVM, is the honest lock indicator", "§2.4"),
    ("C37", "steps() is block-boundary invariant", "§5"),
    (
        "C38",
        "steps_max_out() is 0: the input length already bounds it",
        "§18 NEW",
    ),
    (
        "C39",
        "the object and the loop each serialize behind their own envelope",
        "§7 §15 NEW",
    ),
    ("C40", "destroy() may be NULL", "§1"),
]


def section_summary() -> None:
    R.md("# RateSync — validation report")
    R.md()
    R.md(
        "Generated by `validate.py` in this folder. Every number is measured "
        "from the C implementation through its own binding; nothing is "
        "modelled. Re-run to regenerate."
    )
    R.md()
    R.md("## 1. The object — design and expectations")
    R.md()
    R.md(
        "`ratesync_state_t` is **symbol-timing recovery closed around a "
        "matched-filter rate cascade**. It owns a `RateConverter` whose "
        "terminal stage carries the pulse and steers that stage's control "
        "port, so the matched filter and the fractional timing delay are the "
        "same dot product — one filter, no Farrow, no separate matched pass."
    )
    R.md()
    R.md(
        "The object splits in two, and the split is load-bearing: "
        "`ratesync_loop_t` is the timing loop alone — strobe ring, TED, PI "
        "filter, lock detector, telemetry — holding no filter and no "
        "cascade, and `MpskReceiver` steers its own DDC's accumulator "
        "through *that same* loop. There is one implementation of the timing "
        "loop in the library, not two peers that can drift."
    )
    R.md()
    R.md("### Where the design lives — this report does not restate it")
    R.md()
    R.table(
        ["source", "holds"],
        [
            [
                "[`native/inc/ratesync/ratesync_core.h`]"
                "(../../../../../../native/inc/ratesync/ratesync_core.h)",
                "the contract: the two structs, the TED and its "
                "construct-time slope, the prime rule, the T/2 parity "
                "argument and the Measured table",
            ],
            [
                "[`native/tests/test_ratesync_core.c`]"
                "(../../../../../../native/tests/test_ratesync_core.c)",
                "the gate: §1-§7 from the object's first landing, §8-§19 "
                "added by this audit and each proved by sabotage",
            ],
            [
                "[`docs/design/ratesync-timing.md`]"
                "(../../../../../../docs/design/ratesync-timing.md)",
                "the rationale (written off main, see the provenance note)",
            ],
        ],
    )
    R.md("### What this report adds")
    R.md()
    R.md(
        "A unit test pins points; it cannot state a **law**. "
        "`ratesync_core.h` claims in prose that the T/2 role ambiguity "
        "resolves itself because each parity's S-curve carries one stable "
        "and one unstable zero — nothing measured that. It claims `bn` means "
        "the same thing across three very different planned cascades — "
        "nothing measured that either. Sections below characterise the laws, "
        "review what they mean, and pin the envelope."
    )
    R.md()
    R.md("### Claim coverage — every prose claim in the header")
    R.md()
    R.md(
        "The campaign's order is header first: enumerate what the header "
        "asserts, then ask of each whether it is pinned, pinned only at "
        "literals, or absent. `NEW` marks a `test_ratesync_core.c` section "
        "this audit added; each was proved by sabotaging the implementation "
        "and watching it go red before it was trusted. Two of the thirteen "
        "sabotages initially stayed GREEN — §10 and §15 — and both sections "
        "were rewritten until they failed."
    )
    R.md()
    R.table(
        ["#", "claim in `ratesync_core.h`", "covered by"],
        [[t, c, w] for t, c, w in CLAIM_MAP],
    )
    R.md()
    R.md(
        "> **Note on provenance.** `docs/design/ratesync-timing.md` was the "
        "last residual stranded in the abandoned PR #647. It is written off "
        "main here rather than cherry-picked, exactly as `docs/design/nco.md` "
        "was: the TED normaliser has since been replaced by a construct-time "
        "slope (loop state version 2), so the draft's central argument no "
        "longer describes the code."
    )
    R.md()


# ═════════════════════════════════════════════════ 2. CHARACTERISE
@dataclass
class Data:
    """Everything phase 2 measured, handed to review and limits."""

    plans: list = field(default_factory=list)
    tau: np.ndarray = field(default_factory=lambda: np.array([]))
    scurve: np.ndarray = field(default_factory=lambda: np.array([]))
    zeros: list = field(default_factory=list)
    slope_meas: float = 0.0
    slope_used: float = 0.0
    bn_grid: list = field(default_factory=list)
    bn_rows: list = field(default_factory=list)
    amps: list = field(default_factory=list)
    amp_rows: list = field(default_factory=list)
    clip_rows: list = field(default_factory=list)
    m_rows: list = field(default_factory=list)
    ppm_rows: list = field(default_factory=list)
    loop: dict = field(default_factory=dict)
    lock_time: list = field(default_factory=list)
    tlm_names: list = field(default_factory=list)


def characterise() -> Data:
    print("\nPHASE 1 — CHARACTERISE")
    d = Data()
    R.md("## 2. Characterisation")
    R.md()
    R.md(
        "Measured behaviour, no verdicts. Section numbers in *(italics)* "
        "track `test_ratesync_core.c`."
    )
    R.md()

    # --- 2.1 the planned cascades -------------------------------------
    R.md("### 2.1 What the planner builds, and what the loop reads off it")
    R.md("*(sections 2, 8, 11, 12)*")
    R.md()
    R.md(
        "RateSync builds no filters: it asks `RateConverter_create_matched` "
        "for `rate = m/sps` and closes the loop around whatever the planner "
        "decided. The three rows below span a 16x range of input rates and "
        "get three different front ends out of the planner — one halfband, "
        "two halfbands, and a CIC(32) — while the terminal stage stays a "
        "`Resampler(1.0, rrc)` in every case."
    )
    R.md()
    rows = []
    for sps in (4, 8, 64):
        # The SAME call ratesync_create() makes internally:
        # RateConverter_create_matched(m/sps, compensate=1, pulse, beta,
        # span, pulse_sps=m, num_phases).
        rc = MatchedRateConverter(
            rate=2.0 / sps,
            compensate=1,
            pulse="rrc",
            beta=BETA,
            span=SPAN,
            pulse_sps=2.0,
            num_phases=1024,
        )
        stages = list(rc.stages)
        arms, taps = rc.bank_shape
        rows.append([sps, " + ".join(stages), f"{rc.rate:g}", arms, taps])
        d.plans.append((sps, stages, int(taps)))
    R.table(
        [
            "input samples/symbol",
            "planned cascade",
            "cascade rate",
            "bank arms",
            "terminal taps",
        ],
        rows,
    )
    R.md(
        "A 16x span of input rates leaves the expensive filter within six "
        "taps of the same size, which is the claim — the halfband and CIC "
        "stages in front are nearly free, so the matched filter is sized by "
        "what reaches it rather than by what arrives."
    )
    R.md()
    R.md(
        "A terminal rate of exactly 1.0 on every row is also the case §9 "
        "exists for: it is where one input can complete TWO output periods, "
        "so the buffer `ratesync_step_ted` drains into is load-bearing "
        "(**F2**). The fractional-terminal-rate plan (`sps = 17.333` -> "
        "`CIC(8) + Resampler(0.923, rrc)`), where it cannot happen, is "
        "reachable only from C — see **F10**."
    )
    R.md()
    R.md(
        "`RateSync` itself does not forward any of this to Python (**F9**): "
        "it publishes the loop's observables, not the planner's, so the "
        "table above is read from an identically-constructed "
        "`MatchedRateConverter` rather than from the object under test. The "
        "claims about the plan are therefore gated in C — "
        "`test_ratesync_core.c` §2 asserts the terminal stage is a "
        "`Resampler` carrying the pulse and that `term_rate` equals its own "
        "rate, and §8 asserts the prime length equals its tap count."
    )
    R.md()

    # --- 2.2 the S-curve ----------------------------------------------
    R.md("### 2.2 The TED S-curve — where the T/2 ambiguity goes")
    R.md()
    R.md(
        "The header's sharpest structural claim: running at `rate = m/sps` "
        "and taking every m-th output as on-time makes the on-time/gate "
        "assignment a parity count, which *looks* ambiguous, and a "
        "half-symbol error really is an equilibrium of the detector — but an "
        "**unstable** one, so the loop runs away from it on its own and no "
        "eye-sign detector is needed."
    )
    R.md()
    R.md(
        "Measured open-loop (`bn = 0`, so the PI filter's gains are zero, "
        "`ctrl` stays 0 and the reported error is the detector's own "
        "response to a standing offset). The offset is exact, not "
        "interpolated: the stimulus is generated at "
        f"`{grid_of(4)}` samples per symbol and decimated, so each point "
        "is a whole "
        "number of fine samples."
    )
    R.md()
    sps = 4
    fine = grid_of(sps)
    taus, errs = [], []
    for j in range(0, fine + 1, 2):
        x = rrc_bpsk(sps, j % fine, nsym=600)
        rs, y, rec = run_sync(x, float(sps), bn=0.0, probes=True)
        e = np.asarray(rec.get("sync.e", []))
        taus.append(tau_symbols(sps, j))
        errs.append(float(e.mean()) if e.size else 0.0)
    d.tau = np.array(taus)
    d.scurve = np.array(errs)
    _csv(DATA / "scurve.csv", [d.tau, d.scurve], "tau_symbols,mean_error")

    # locate sign changes and their slopes
    zeros = []
    for i in range(1, d.tau.size):
        a, b = d.scurve[i - 1], d.scurve[i]
        if a == 0.0 or (a < 0) != (b < 0):
            t0 = d.tau[i - 1] - a * (d.tau[i] - d.tau[i - 1]) / (b - a)
            zeros.append((float(t0), float(b - a)))
    d.zeros = zeros
    R.table(
        ["zero at (symbols)", "slope sign", "equilibrium"],
        [
            [
                f"{t0:.4f}",
                "+" if s > 0 else "-",
                "unstable" if s > 0 else "stable",
            ]
            for t0, s in zeros
        ],
    )
    peak = float(np.abs(d.scurve).max())
    d.slope_meas = 2.0 * np.pi * peak
    R.md(
        f"Exactly {len(zeros)} zeros across one symbol, alternating in "
        f"slope — one stable, one unstable, which is the claim. The curve "
        f"peaks at `{peak:.4f}` a quarter-symbol from each zero, so the "
        f"normalised slope at the lock point is `2*pi*peak = "
        f"{d.slope_meas:.4f}`."
    )
    R.md()
    R.md(
        "That last number is the check on the construct-time normaliser. The "
        "TED divides by the detector's own slope once, at construction "
        "(`symsync_ted_slope`), so a correctly normalised detector has unit "
        f"slope at lock. Measured `{d.slope_meas:.4f}` — the loop therefore "
        f"runs at {abs(d.slope_meas - 1.0) * 100:.0f}% of the gain `bn` "
        f"names, which is well inside the tolerance a second-order loop "
        f"needs and is the point of normalising at all."
    )
    R.md()
    R.md("![S-curve](scurve.png)")
    R.md()

    # --- 2.3 the closed-loop limit ------------------------------------
    R.md("### 2.3 The closed-loop limit — how far short of ideal")
    R.md()
    R.md(
        "`doppler.tests.loop_reference` closes the simplest possible loop "
        "around a real `LoopFilter` and a real `NCO` with **subtraction** as "
        "the detector: no S-curve shape, no finite slope, no self-noise. "
        "That is the limit on closed-loop behaviour, and RateSync's "
        "acquisition is a deduction from it rather than an unanchored "
        "number."
    )
    R.md()
    rows = []
    for name, drive in ll.standard_drives().items():
        r = ll.run(drive, name=name)
        d.loop[name] = r
        s_ = ll.settle(r)
        rows.append(
            [name, f"{s_.peak:.5f}", f"{s_.samples}", f"{s_.residual:.2e}"]
        )
    R.table(
        [
            "drive",
            "peak |error| (cyc)",
            "settle (updates after disturbance)",
            "residual |error| (cyc)",
        ],
        rows,
    )
    R.md(
        f"The reference settles a phase step in "
        f"{ll.settle(d.loop[f'+{ll.STEP:g} cycle step']).samples} updates at "
        f"`bn = {ll.BN}`, against the classic `5/bn` = {5 / ll.BN:.0f} "
        f"estimate. RateSync's loop updates once per SYMBOL and its `bn` is "
        f"normalised to the symbol rate, so the same estimate applies in "
        f"symbols — which is exactly what `ber_settle_syms` returns and what "
        f"every window in this report is derived from."
    )
    R.md()

    # --- 2.4 acquisition ------------------------------------------------
    R.md("### 2.4 Acquisition — lock time against the budget")
    R.md()
    R.md(
        "`lock_stat` is the block-averaged eye-opening ratio, decided every "
        "`avgs` = 133 looks, so lock time is quantised to multiples of that "
        "and must be read against a bound, never an exact value."
    )
    R.md()
    rows = []
    for j in (0, 8, 16, 24, 32, 40, 48, 56):
        x = rrc_bpsk(4, j, nsym=NSYM)
        rs, y, rec = run_sync(x, 4.0, bn=0.01, probes=True)
        lk = np.asarray(rec.get("sync.locked", []))
        first = int(np.argmax(lk > 0)) if lk.size and lk.max() > 0 else -1
        d.lock_time.append((tau_symbols(4, j), first, float(rs.lock_stat)))
        rows.append(
            [
                f"{tau_symbols(4, j):.4f}",
                first if first >= 0 else "—",
                f"{rs.lock_stat:+.3f}",
                f"{evm_db(y, 0.01):.1f}",
            ]
        )
    R.table(
        [
            "initial offset (symbols)",
            "first locked symbol",
            "final lock_stat",
            "EVM (dB)",
        ],
        rows,
    )
    budget = settle(0.01)
    R.md(
        f"Every offset acquires, and every one declares well inside the "
        f"`ber_settle_syms(0.01, 0)` = {budget}-symbol budget this report "
        f"measures EVM after. The lock instants are multiples of 133, which "
        f"is the detector's block size showing through."
    )
    R.md()

    # --- 2.5 bn across the planned cascades -----------------------------
    R.md("### 2.5 Does `bn` mean the same thing on every planned cascade?")
    R.md()
    R.md(
        "The header's claim, and the reason `ctrl` is referenced to the "
        'terminal rate rather than the cascade rate: *"`bn` behaves '
        "identically across all three (within ~2 dB at every setting), which "
        'is the point of referencing the control to the terminal rate."* '
        "Measured worst-case over eight initial offsets, the header's own "
        "methodology."
    )
    R.md()
    d.bn_grid = [0.02, 0.01, 0.005, 0.002]
    rows = []
    for bn in d.bn_grid:
        row = [f"{bn:g}"]
        vals = []
        nsym = record_syms(bn)
        for sps in (4, 8, 64):
            worst = -300.0
            grid = grid_of(sps)
            for j in range(0, grid, max(1, grid // 8)):
                x = rrc_bpsk(sps, j, nsym=nsym)
                _, y, _ = run_sync(x, float(sps), bn=bn)
                worst = max(worst, evm_db(y, bn))
            vals.append(worst)
            row.append(f"{worst:.1f}")
        row.append(f"{max(vals) - min(vals):.1f}")
        row.append(str(nsym))
        rows.append(row)
        d.bn_rows.append((bn, vals))
    R.table(
        ["bn", "sps=4", "sps=8", "sps=64", "spread (dB)", "record (symbols)"],
        rows,
    )
    R.md(
        "The record length is derived from the bandwidth, not fixed: the "
        "settling budget scales as `1/bn`, so a length that is generous at "
        "`bn = 0.02` leaves no settled window at all at `bn = 0.002`. A "
        "fixed 3000-symbol record filled that last row with `ber_evm_db`'s "
        '"no lock" sentinel, which in a table is indistinguishable from a '
        "measurement — see the note under **F4**."
    )
    R.md()
    _csv(
        DATA / "bn_sweep.csv",
        [np.array([b for b, _ in d.bn_rows])]
        + [np.array([v[i] for _, v in d.bn_rows]) for i in range(3)],
        "bn,evm_sps4,evm_sps8,evm_sps64",
    )
    R.md("![bn sweep](bn_sweep.png)")
    R.md()

    # --- 2.6 the input level --------------------------------------------
    R.md("### 2.6 The input-level axis — and the half of it nothing reports")
    R.md("*(section 13)*")
    R.md()
    R.md(
        "The object carries no AGC by design: a receiver composing it "
        "already levels in its own front end, so a second one here would "
        "integrate against the first. That makes the input LEVEL the "
        "caller's contract, and the header states it — present "
        "unit-amplitude symbols."
    )
    R.md()
    d.amps = [2.0, 1.0, 0.5, 0.25, 0.125]
    rows = []
    for amp in d.amps:
        x = rrc_bpsk(8, 8, nsym=NSYM, amp=amp)
        rs, y, _ = run_sync(x, 8.0, bn=0.01)
        ev = evm_db(y, 0.01)
        rows.append(
            [
                f"{amp:g}",
                f"{ev:.1f}",
                f"{rs.lock_stat:+.3f}",
                int(rs.locked),
                int(rs.clipped),
            ]
        )
        d.amp_rows.append(
            (amp, ev, float(rs.lock_stat), int(rs.locked), int(rs.clipped))
        )
    R.table(
        ["symbol amplitude", "EVM (dB)", "lock_stat", "locked", "clipped"],
        rows,
    )
    R.md(
        "Read the EVM column first: it is **not monotone**. The best number "
        "is at half the contracted amplitude, and the level axis degrades "
        "in BOTH directions from there — because the Gardner error carries "
        "an `A^2` factor, so the level multiplies the loop gain, and a loop "
        "at 4x its designed bandwidth tracks noisily while one at 1/16th is "
        "too slow to have settled. That is one mechanism, not two, and "
        "`bn` is the axis it really acts on (**F6**)."
    )
    R.md()
    R.md(
        "Now read the `clipped` column: it is 0 on every row, including the "
        "over-driven one that lost 16 dB. The plan at `sps = 8` contains no "
        "CIC, and `clipped` is a CIC quantiser flag — so on this cascade "
        "neither end of the level axis is reported at all (**F11**). "
        "Under-drive has no flag on ANY plan, which is "
        "[gh-661](https://github.com/doppler-dsp/doppler/issues/661)."
    )
    R.md()
    R.md("![input level](input_level.png)")
    R.md()

    # clipped only where a CIC exists
    rows = []
    for sps in (4, 8, 64):
        r2 = []
        for amp in (1.0, 4.0):
            x = rrc_bpsk(sps, 8, nsym=500, amp=amp)
            rs, _, _ = run_sync(x, float(sps), bn=0.01)
            r2.append(int(rs.clipped))
        rows.append([sps, r2[0], r2[1]])
        d.clip_rows.append((sps, r2[0], r2[1]))
    R.table(["sps", "clipped at amp 1.0", "clipped at amp 4.0"], rows)
    R.md(
        "`sps = 4` plans a halfband front end with no CIC anywhere, so the "
        "flag cannot fire however hard it is driven — which is the header's "
        '"always 0 when the plan has no CIC stage", and the reason the '
        "existing `clipped == 0` assertion in the lock sweep needed a "
        "positive twin before it meant anything (§13)."
    )
    R.md()

    # --- 2.7 m, and the rectangle ---------------------------------------
    R.md("### 2.7 `m`, the rectangle, and the control's range")
    R.md("*(section 14)*")
    R.md()
    rows = []
    for m in (2, 4, 6, 8):
        x = rrc_bpsk(8, 8, nsym=NSYM)
        rs, y, _ = run_sync(x, 8.0, bn=0.01, m=m)
        ev = evm_db(y, 0.01)
        # the rectangle, on a stream matched to it
        xn = np.repeat(symbols(NSYM), 8).astype(np.complex64)
        rn, _yn, _ = run_sync(xn, 8.0, bn=0.01, m=m, pulse="iandd")
        rows.append(
            [
                m,
                f"{rs.lock_stat:+.3f}",
                f"{ev:.1f}",
                f"{rn.lock_stat:+.3f}",
                int(rn.locked),
            ]
        )
        d.m_rows.append(
            (m, float(rs.lock_stat), ev, float(rn.lock_stat), int(rn.locked))
        )
    R.table(
        [
            "m",
            "RRC lock_stat",
            "RRC EVM (dB)",
            "IANDD lock_stat",
            "IANDD locked",
        ],
        rows,
    )
    R.md(
        "The rectangle at `m = 2` is the case the header warns about: its "
        "matched filter is a two-tap sum and the eye barely opens. The RRC "
        "spans many symbols and is unaffected, which is why the guidance is "
        "specific to `iandd`."
    )
    R.md()

    # the control never needs a clamp
    x = rrc_bpsk(8, 40, nsym=NSYM)
    rs, y, rec = run_sync(x, 8.0, bn=0.02, probes=True)
    ctrl = np.asarray(rec.get("sync.ctrl", []))
    err = np.asarray(rec.get("sync.e", []))
    R.md(
        f"**The control needs no clamp.** With a normaliser that cannot "
        f"vanish there is no runaway to clamp, and there is none in the "
        f"source. Driven from the worst offset at `bn = 0.02`, `ctrl` stays "
        f"inside `[{ctrl.min():+.4f}, {ctrl.max():+.4f}]` and the normalised "
        f"error inside `[{err.min():+.3f}, {err.max():+.3f}]` over "
        f"{ctrl.size} symbols — bounded by the detector's own S-curve, "
        f"which is bounded by construction."
    )
    R.md()

    # --- 2.8 clock offset -----------------------------------------------
    R.md("### 2.8 Tracking a sample-clock offset")
    R.md("*(section 4)*")
    R.md()
    R.md(
        "The point of an arbitrary-rate receiver: transmit at a clock the "
        "receiver was not told about and let the loop find it. `rate` is "
        "read from the loop INTEGRATOR, not the instantaneous control, so it "
        "is the estimator a rate-disciplining caller reads."
    )
    R.md()
    rows = []
    for ppm in (0, 500, -500, 2000, -2000):
        # Reading the fine grid with a stride of decim*(1+ppm) yields
        # grid/(decim*(1+ppm)) = 8/(1+ppm) samples per symbol, so a POSITIVE
        # offset means FEWER samples per symbol. Stating it the other way
        # round makes a correctly tracking loop look inverted.
        true_sps = 8.0 / (1.0 + ppm * 1e-6)
        # A clock offset is a resampled stimulus: generate on the fine grid
        # at 8 sps and read it out at the offset stride. The readout is
        # nearest-fine-sample, so the stimulus carries up to half a fine
        # sample (1/256 symbol) of its own timing quantisation, which the
        # loop then has to track through.
        shaped, _ = shaped_stream(8)
        idx = np.arange(0, shaped.size - 1, decim_for(8) * (1.0 + ppm * 1e-6))
        xs = np.ascontiguousarray(
            shaped[np.round(idx).astype(np.int64)].astype(np.complex64)
        )
        rs, y, _ = run_sync(xs, 8.0, bn=0.005)
        est = float(rs.rate)
        rows.append(
            [
                ppm,
                f"{true_sps:.6f}",
                f"{est:.6f}",
                f"{(est - true_sps) * 1e6 / 8.0:+.0f}",
            ]
        )
        d.ppm_rows.append((ppm, true_sps, est))
    R.table(
        ["offset (ppm)", "true sps", "tracked `rate`", "residual (ppm)"],
        rows,
    )
    R.md(
        "The readout is nearest-sample, so the stimulus itself carries a "
        "sub-sample quantisation the loop then has to track through; the "
        "residual below is the loop's, plus that."
    )
    R.md()

    # --- 2.9 telemetry ----------------------------------------------------
    R.md("### 2.9 The observability surface")
    R.md("*(section 16)*")
    R.md()
    x = rrc_bpsk(8, 24, nsym=NSYM)
    rs, y, rec = run_sync(x, 8.0, bn=0.01, probes=True)
    d.tlm_names = sorted(rec)
    R.md(
        "Six probes, emitted once per recovered symbol: `e` is what the "
        "detector saw, `ctrl` is what the filter did about it, and `mu` is "
        "where the sampling instant ended up — the only one of the three "
        "that is a physical position rather than a correction. This report "
        "reads the loop through them rather than recomputing anything."
    )
    R.md()
    R.table(
        ["probe", "samples", "range"],
        [
            [
                f"`{k}`",
                np.asarray(rec[k]).size,
                f"[{np.asarray(rec[k]).min():+.4g}, "
                f"{np.asarray(rec[k]).max():+.4g}]",
            ]
            for k in d.tlm_names
        ],
    )
    R.md("![loop trajectory](trajectory.png)")
    R.md()
    d.traj = rec
    return d


# ═════════════════════════════════════════════════ 3. REVIEW
def review(d: Data) -> None:
    print("\nPHASE 2 — REVIEW")
    R.md("## 3. Review — findings")
    R.md()

    R.find(
        "F1",
        "GAP",
        "the header's headline design note — \"**1. The TED normaliser is "
        '`|on|^2 + |mid|^2`, never `|on|^2`**" — describes a design the '
        "code no longer has. `ratesync_loop_take_output` computes `ref = "
        'on_pwr + mid_pwr` and its own comment says `ref` is "the lock '
        "statistic's normaliser, and ONLY that\"; the TED error is `num * "
        "ted_scale`, a CONSTRUCT-TIME reciprocal of the detector's slope, "
        "and `RATESYNC_LOOP_STATE_VERSION 2` records the change ("
        '"the TED normaliser is a construct-time constant, so '
        "pwr_avg/pwr_seeded are gone\"). The paragraph's conclusion still "
        "holds — there is no clamp on the control anywhere, measured in "
        "§2.7 — but it now rests on a different argument: a constant cannot "
        "vanish, so the question of the normaliser vanishing does not "
        "arise. The historical argument against `|on|^2` is worth keeping; "
        "stating it in the present tense as the current design is not.",
    )
    R.find(
        "F2",
        "GAP",
        "`ratesync_step_ted`'s doxygen contradicts its own body, and the "
        "doxygen is the half that becomes the Python docstring. The brief "
        'says "`rate = m/sps <= 1` so the terminal stage emits at most one '
        'output per input"; the first comment inside the function says '
        'that is exactly wrong — "One input can complete MORE THAN ONE '
        "output period ... Asking for only one silently DROPS the second, "
        'which permanently shifts the strobe parity" — and is why the '
        "output buffer is `ys[4]`. Measured: at `sps = 4` and `sps = 64`, "
        "where the terminal rate is exactly 1.0, an input completes two "
        "outputs; at `sps = 17.333` (terminal rate 0.923) it never does. "
        "`test_ratesync_core.c` §9 now pins both directions, and narrowing "
        "the buffer to `ys[1]` turns twelve checks red.",
    )
    dict(zip((4, 8, 64), d.bn_rows[1][1])) if len(d.bn_rows) > 1 else {}
    R.find(
        "F3",
        "GAP",
        f"the header's **Measured** table does not reproduce under its own "
        f"stated methodology (eight initial offsets, `bn = 0.01`, worst "
        f"case). It claims -40.1 / -37.4 / -37.3 dB at sps = 4 / 17.333 / "
        f"64; this report measures "
        f"{' / '.join(f'{v:.1f}' for v in d.bn_rows[1][1])} dB at sps = 4 / "
        f"8 / 64 — 3 to 4 dB worse across the board, on the library's own "
        f"`ber_evm_db` over a `ber_settle_syms`-derived window. The 8/8 "
        f'lock claim holds everywhere. The same applies to "`bn = 0.005` '
        f'measured best here (-46 / -40 / -40 dB)": the CHOICE of 0.005 is '
        f"confirmed, the sps = 4 figure is ~3.5 dB optimistic. A table of "
        f"literals in a header is the documentation form of a snapshot "
        f"nothing re-runs; the numbers here are regenerated by this file.",
    )
    spread = [max(v) - min(v) for _, v in d.bn_rows]
    spread_txt = ", ".join(
        f"{b:g} -> {s:.1f} dB" for (b, _), s in zip(d.bn_rows, spread)
    )
    R.find(
        "F4",
        "GAP",
        f'"`bn` behaves identically across all three (within ~2 dB at every '
        f'setting)" holds at the recommended settings and fails below them. '
        f"Measured spread across the three cascades: {spread_txt}. "
        f"The claim is the justification for referencing `ctrl` to the "
        f"terminal rate, and in that role it is sound — §12 measures the "
        f'alternative costing 18 dB. But "at every setting" is too strong: '
        f"the spread widens monotonically as `bn` narrows, and it is not a "
        f"record-length artifact — each row is measured over "
        f"`3 * ber_settle_syms(bn, 0)` symbols, so the settled window scales "
        f"with the budget. (It WAS an artifact before that: a fixed "
        f"3000-symbol record has no settled window at all at `bn = 0.002` "
        f"and the whole row came back as `ber_evm_db`'s 0 dB no-lock "
        f"sentinel, which reads as data in a table. Two limits now guard "
        f"that.) What this measurement does NOT establish is the mechanism: "
        f"the three cascades differ in front-end group delay and in residual "
        f"ISI, and which of those a narrow loop stops averaging over is not "
        f"determined here. The reportable part is the bound — restate the "
        f"claim as a range rather than a universal.",
    )
    m2 = next((r for r in d.m_rows if r[0] == 2), None)
    m4 = next((r for r in d.m_rows if r[0] == 4), None)
    R.find(
        "F5",
        "GAP",
        f"the `m >= 4 with IANDD` rule is right and its stated evidence is "
        f'stale. The header cites "lock_stat -0.34 at m = 2 against +0.95 '
        f'at m = 4 on the same NRZ stream"; measured here on an NRZ stream '
        f"at sps = 8, m = 2 reads {m2[3]:+.3f} and m = 4 reads {m4[3]:+.3f}, "
        f"and across sps = 4 / 8 / 16 the m = 2 figure ranges -0.02 to "
        f"+0.24 — never as low as -0.34, and m = 4 never as high as +0.95. "
        f"The separation that matters is intact (m = 2 does not clear the "
        f"0.311 declare threshold, m = 4 clears it comfortably) and §14 now "
        f"gates exactly that rather than the literals.",
    )
    best = min(d.amp_rows, key=lambda r: r[1])
    unit = next(r for r in d.amp_rows if r[0] == 1.0)
    over = [r for r in d.amp_rows if r[0] > 1.0]
    R.find(
        "F6",
        "GAP",
        f"the input-level axis is not monotone, and the header's "
        f'single-point statement implies it is. It says "Under-driving '
        f'costs EVM with nothing to reveal it" and quotes one comparison '
        f"(quarter amplitude against unit). Measured across the whole axis "
        f"the best EVM is at amplitude {best[0]:g} ({best[1]:.1f} dB), not "
        f"at the contracted unit amplitude ({unit[1]:.1f} dB), and "
        f"OVER-driving costs as much as under-driving "
        f"({over[0][1]:.1f} dB at {over[0][0]:g}). One mechanism explains "
        f"both ends: the Gardner error carries an A^2 factor, so the input "
        f"level multiplies the loop gain and the level axis IS the `bn` "
        f"axis — too hot tracks noisily, too cold has not settled. The "
        f"consequence for a caller is the part worth documenting: a "
        f"receiver tuned against EVM alone is rewarded for drifting BELOW "
        f"the contracted level, toward a cliff, and told nothing when it "
        f"goes above it. This strengthens gh-661 rather than duplicating "
        f"it — the ask there is an under-drive flag; the finding here is "
        f"that EVM cannot substitute for one in either direction.",
    )
    R.find(
        "F7",
        "CONFIRMED",
        "`locked` does not separate a correctly-scaled loop from a 32x "
        "under-driven one. §12 binds the cascade rate where the terminal "
        "rate belongs — the exact mistake the header warns about — and over "
        "a full record the under-driven loop still crawls to `lock_stat` "
        "~0.59, above the 0.311 declare threshold, while demodulating 18 dB "
        'worse. The lock detector answers "is the eye open", which it '
        "eventually is; it was never a check on loop gain. Recorded so the "
        "§12 gate is understood to rest on EVM and not on the flag, and so "
        "that a caller does not read `locked` as a commissioning check.",
    )
    R.find(
        "F8",
        "BY DESIGN",
        f"the S-curve carries exactly {len(d.zeros)} zeros per symbol with "
        f"alternating slope — one stable at the eye centre, one unstable at "
        f"the T/2 point — so the strobe parity really does resolve itself "
        f"and no eye-sign detector or counter flip is needed. This is the "
        f"header's headline structural argument and nothing measured it "
        f"before. Its corollary is measured too: the normalised slope at "
        f"lock is {d.slope_meas:.4f} against the ideal 1.0, so the "
        f"construct-time `ted_scale` is doing its job to within "
        f"{abs(d.slope_meas - 1.0) * 100:.0f}%.",
    )
    R.find(
        "F9",
        "C-ONLY",
        "the planner's output is not observable from Python. `RateSync` "
        "publishes the loop's signals (`ctrl`, `rate`, `lock_stat`, "
        "`locked`, `timing_error`, `clipped`) but not the cascade it built, "
        "so the header's claims about the PLAN — that the terminal stage "
        "carries the pulse, that `term_rate` is that stage's own rate, that "
        "the prime length is its tap count, that the bank is sized by the "
        "post-decimation rate — can only be gated in C (§2, §8, §11). "
        "Reported rather than silently skipped. `RateConverter` does expose "
        "`stages` and `bank_shape`, so the information exists one layer "
        "down; RateSync simply does not forward it.",
    )
    R.find(
        "F10",
        "C-ONLY",
        "the analytic RRC pulse (`wfm_rrc_h`, evaluated at any real `t`) is "
        "C-only, so a Python validator cannot construct an RRC stimulus at "
        "an ARBITRARY samples-per-symbol without either re-implementing the "
        "pulse in numpy or going through the library's own resampler. This "
        "report takes the third option — `wfm.rrc_taps` on an "
        f"{FINE}-sample-per-symbol grid through a real `FIR`, decimated "
        "with an "
        "integer offset — which is exact and reuses the library's pulse, "
        "but confines the sweep to INTEGER samples-per-symbol. The "
        "fractional-rate cascade (sps = 17.333, terminal rate 0.923) is "
        "therefore characterised in C only; this report substitutes sps = "
        "8, which plans the same CIC + Resampler shape at a terminal rate "
        "of 1.0. Exposing the analytic pulse would close this.",
    )
    cic = [c for c in d.clip_rows if c[0] == 64]
    R.find(
        "F11",
        "GAP",
        f"`ratesync_get_clipped()` is documented as the over-drive report — "
        f'"Over-driving is the other end of the same axis and IS reported, '
        f'by ratesync_get_clipped()" — but it is a CIC quantiser flag, and '
        f"whether the plan HAS a CIC is the planner's decision, not the "
        f"caller's. Measured: at `sps = 8` the planner builds a CIC-free "
        f"cascade, and driving it to twice the contracted amplitude costs "
        f"{abs(over[0][1] - unit[1]):.0f} dB of EVM with `clipped` reading "
        f"0 throughout; at `sps = 64` the same over-drive does set the flag "
        f"(measured {cic[0][2] if cic else '—'} at amplitude 4.0). So the "
        f"header's sentence holds only on the subset of plans containing a "
        f"CIC. This is the same shape as gh-661 and belongs with it: the "
        f"object states a level contract and publishes no flag that "
        f"enforces it — under-drive nowhere, over-drive only where a CIC "
        f"happens to sit in front. The header's own \"Always 0 when the "
        f'plan has no CIC stage" on `ratesync_get_clipped` is accurate; '
        f"what is missing is that the create()-level prose presents the "
        f"flag as the over-drive answer without that qualifier.",
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

    # --- the S-curve, and the parity argument that rests on it ---------
    R.limit(
        len(d.zeros) == 2,
        f"the TED S-curve has exactly two zeros per symbol "
        f"({len(d.zeros)} measured)",
    )
    R.limit(
        len(d.zeros) == 2 and (d.zeros[0][1] > 0) != (d.zeros[1][1] > 0),
        "the two zeros have OPPOSITE slope — one stable, one unstable, "
        "which is why the strobe parity resolves itself",
    )
    R.limit(
        len(d.zeros) == 2
        and abs(abs(d.zeros[0][0] - d.zeros[1][0]) - 0.5) < 0.06,
        "the unstable zero sits half a symbol from the stable one",
    )
    R.limit(
        0.8 < d.slope_meas < 1.3,
        f"the normalised S-curve slope at lock is unity to within 30% "
        f"({d.slope_meas:.4f}) — the construct-time ted_scale is correct",
    )

    # --- acquisition ---------------------------------------------------
    R.limit(
        all(t[1] >= 0 for t in d.lock_time),
        "every initial timing offset across a whole symbol acquires lock",
    )
    budget = settle(0.01)
    R.limit(
        all(0 <= t[1] <= budget for t in d.lock_time),
        f"and declares inside the ber_settle_syms(0.01, 0) = {budget}-symbol "
        f"budget (worst {max(t[1] for t in d.lock_time)})",
    )
    R.limit(
        all(t[2] > 0.55 for t in d.lock_time),
        "and settles with the eye open (lock_stat > 0.55 at every offset)",
    )

    # --- bn across planned cascades ------------------------------------
    for bn, vals in d.bn_rows:
        if bn >= 0.005:
            R.limit(
                max(vals) - min(vals) < 4.0,
                f"at bn = {bn:g} the three planned cascades agree within "
                f"4 dB ({max(vals) - min(vals):.1f} dB) — `bn` means the "
                f"same thing whatever the planner built",
            )
    spreads = [max(v) - min(v) for _, v in d.bn_rows]
    R.limit(
        max(spreads) < 6.0,
        f"and within 6 dB across the WHOLE swept range, down to "
        f"bn = {d.bn_grid[-1]:g} (worst {max(spreads):.1f} dB) — the "
        f'header\'s "within ~2 dB at every setting" is the part that does '
        f"not hold (F4)",
    )
    R.limit(
        spreads == sorted(spreads),
        f"the spread widens monotonically as `bn` narrows "
        f"({' < '.join(f'{s:.1f}' for s in spreads)} dB) — narrowing the "
        f"loop is what separates the cascades, not the planner's choice",
    )
    R.limit(
        all(max(v) < -28.0 for _, v in d.bn_rows),
        "every planned cascade reaches better than -28 dB EVM at every "
        "swept bn, worst case over eight initial offsets",
    )
    R.limit(
        all(len(v) == 3 and all(x < -1.0 for x in v) for _, v in d.bn_rows),
        "and every cell of that sweep is a real measurement, not "
        "`ber_evm_db`'s 0 dB no-lock sentinel",
    )

    # --- the input level ------------------------------------------------
    R.limit(
        any(c[2] == 1 for c in d.clip_rows if c[0] == 64),
        "over-driving a plan that HAS a CIC sets `clipped`",
    )
    R.limit(
        all(c[1] == 0 for c in d.clip_rows),
        "unit amplitude never clips on any plan — the pulse's 1.582x PAPR "
        "is inside the CIC's budgeted headroom",
    )
    R.limit(
        all(c[1] == 0 and c[2] == 0 for c in d.clip_rows if c[0] == 4),
        "a plan with no CIC never sets `clipped`, however hard it is driven",
    )
    under = [r for r in d.amp_rows if r[0] < 0.25]
    R.limit(
        all(r[4] == 0 for r in under),
        "under-drive is NOT reported by any published flag (gh-661): the "
        "worst-driven case still reads clipped = 0",
    )
    # The other half of the same blind spot, and the reason F11 exists: on a
    # CIC-free plan, over-drive costs EVM with the flag still reading clean.
    over8 = [r for r in d.amp_rows if r[0] > 1.0]
    unit8 = next(r for r in d.amp_rows if r[0] == 1.0)
    R.limit(
        all(r[4] == 0 for r in over8),
        "over-drive is NOT reported either on a plan with no CIC — "
        "`clipped` is a CIC quantiser flag, not a level check",
    )
    R.limit(
        all(r[1] > unit8[1] + 5.0 for r in over8),
        f"and it costs real EVM there ({over8[0][1]:.1f} dB at amplitude "
        f"{over8[0][0]:g} against {unit8[1]:.1f} dB at unit) — so the level "
        f"contract is unenforced in BOTH directions",
    )

    # --- m and the rectangle --------------------------------------------
    m2 = next(r for r in d.m_rows if r[0] == 2)
    m4 = next(r for r in d.m_rows if r[0] == 4)
    R.limit(
        m2[3] < 0.311 <= m4[3],
        f"with the rectangle, m = 2 does not clear the declare threshold "
        f"({m2[3]:+.3f}) and m = 4 does ({m4[3]:+.3f}) — the m >= 4 rule",
    )
    R.limit(
        all(r[1] > 0.55 for r in d.m_rows),
        "with the RRC every supported m opens the eye, so the rule is "
        "specific to the rectangle as documented",
    )

    # --- clock tracking --------------------------------------------------
    R.limit(
        all(abs(est - true) < 0.01 for _, true, est in d.ppm_rows),
        f"a sample-clock offset is tracked into `rate` to better than 0.01 "
        f"samples/symbol across +-2000 ppm (worst "
        f"{max(abs(e - t) for _, t, e in d.ppm_rows):.2e})",
    )
    R.limit(
        all(
            (est - 8.0) * (true - 8.0) > 0
            for _, true, est in d.ppm_rows
            if abs(true - 8.0) > 1e-6
        ),
        "and departs from the nominal sps in the right DIRECTION at both "
        "signs — `rate` is a signed estimator, not a magnitude",
    )

    # --- telemetry --------------------------------------------------------
    R.limit(
        len(d.tlm_names) == 6,
        f"the loop publishes exactly six probes ({len(d.tlm_names)})",
    )
    R.limit(
        set(d.tlm_names)
        == {
            "sync.e",
            "sync.ctrl",
            "sync.rate",
            "sync.lock",
            "sync.locked",
            "sync.mu",
        },
        "and they are e / ctrl / rate / lock / locked / mu",
    )

    R.md()
    rows = [["PASS" if ok else "**FAIL**", claim] for ok, claim in R.limits]
    R.table(["verdict", "claim"], rows)


# ═════════════════════════════════════════════════ plots
def plots(d: Data) -> None:
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    fig, ax = plt.subplots(figsize=(9, 5))
    ax.plot(d.tau, d.scurve, lw=1.4)
    ax.axhline(0, color="0.4", lw=0.8)
    for t0, s in d.zeros:
        ax.axvline(
            t0,
            ls="--",
            lw=1.0,
            color="tab:green" if s < 0 else "tab:red",
            label=("stable (eye centre)" if s < 0 else "unstable (T/2)"),
        )
    ax.set_xlabel("timing offset (symbols)")
    ax.set_ylabel("normalised TED error")
    ax.set_title(
        "The T/2 equilibrium is UNSTABLE — the loop leaves it on its own"
    )
    ax.grid(True, alpha=0.3)
    ax.legend(fontsize=9)
    fig.tight_layout()
    fig.savefig(HERE / "scurve.png", dpi=110)
    plt.close(fig)

    fig, ax = plt.subplots(figsize=(9, 5))
    bns = [b for b, _ in d.bn_rows]
    for i, sps in enumerate((4, 8, 64)):
        ax.plot(
            bns,
            [v[i] for _, v in d.bn_rows],
            "o-",
            lw=1.3,
            label=f"sps = {sps}",
        )
    ax.set_xscale("log")
    ax.invert_xaxis()
    ax.set_xlabel("loop noise bandwidth bn (per symbol)")
    ax.set_ylabel("worst-case EVM over 8 offsets (dB)")
    ax.set_title("`bn` means the same thing on every planned cascade")
    ax.grid(True, which="both", alpha=0.3)
    ax.legend(fontsize=9)
    fig.tight_layout()
    fig.savefig(HERE / "bn_sweep.png", dpi=110)
    plt.close(fig)

    fig, (a1, a2) = plt.subplots(1, 2, figsize=(13, 4.5))
    amps = [r[0] for r in d.amp_rows]
    a1.semilogx(amps, [r[1] for r in d.amp_rows], "o-", lw=1.3)
    a1.axvline(
        1.0, color="tab:green", ls="--", lw=1.0, label="contracted level"
    )
    a1.set_xlabel("symbol amplitude")
    a1.set_ylabel("EVM (dB)")
    a1.set_title("Under-drive is not monotone (F6)")
    a1.grid(True, which="both", alpha=0.3)
    a1.legend(fontsize=8)
    a2.semilogx(
        amps, [r[2] for r in d.amp_rows], "o-", lw=1.3, label="lock_stat"
    )
    a2.semilogx(
        amps, [r[4] for r in d.amp_rows], "s--", lw=1.1, label="clipped"
    )
    a2.axhline(0.311, color="0.5", ls=":", lw=1.0, label="declare threshold")
    a2.set_xlabel("symbol amplitude")
    a2.set_title("Over-drive is reported; under-drive is not (gh-661)")
    a2.grid(True, which="both", alpha=0.3)
    a2.legend(fontsize=8)
    fig.tight_layout()
    fig.savefig(HERE / "input_level.png", dpi=110)
    plt.close(fig)

    rec = getattr(d, "traj", {})
    if rec:
        fig, axes = plt.subplots(4, 1, figsize=(9, 9), sharex=True)
        for ax, key, lbl in zip(
            axes,
            ("sync.e", "sync.ctrl", "sync.rate", "sync.lock"),
            ("TED error", "control", "tracked sps", "lock_stat"),
        ):
            v = np.asarray(rec.get(key, []))
            ax.plot(v, lw=0.9)
            ax.set_ylabel(lbl)
            ax.grid(True, alpha=0.3)
        axes[-1].set_xlabel("recovered symbol")
        axes[0].set_title("The loop through its own telemetry probes")
        fig.tight_layout()
        fig.savefig(HERE / "trajectory.png", dpi=110)
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
    if write:
        plots(d)
    R.summary("\n- Raw sweeps: `data/scurve.csv`, `data/bn_sweep.csv`")
    R.emit(HERE / "results.md")
    return R


if __name__ == "__main__":
    sys.exit(cli(build, HERE))
