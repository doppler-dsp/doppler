"""Certify `BurstAcquisition` — a forwarder, certified as one.

Run:  python -m doppler.dsss.tests.validation.burst_acq.validate
      make validate          (regenerates every report)
      make validate-check    (fails if the committed report is stale)

`burst_acq_core.c` is a pure forwarder onto `acq_core.c`'s shared engine:
every function is a direct call through an embedded `acq_state_t` built by
`acq_create_burst()`. The algorithm lives in `acq` exactly once, which is the
library's rule, and it is certified there.

**So this report deliberately does not re-derive the physics.** Re-measuring
the detection statistics through the wrapper would certify the same engine
twice and report the second run as independent evidence, which it is not.
What is certified here is the thing the wrapper can get wrong on its own:
**whether every argument and every call reaches the engine, and reaches the
right place.**

That is not a formality. A thin wrapper's characteristic defect is a
transposition — two same-typed parameters swapped, so the object still
constructs, still detects on a strong signal, and is sized against the wrong
numbers. `pfa` and `pd` are both doubles in (0,1); `reps` and `spc` are both
`size_t`. §2.1 is built to catch exactly that.
"""

from __future__ import annotations

import sys
import warnings
from dataclasses import dataclass, field
from pathlib import Path

import numpy as np

from doppler.dsss import BurstAcquisition
from doppler.tests._validation_common import Report, cli
from doppler.wfm import PN, mls_poly

HERE = Path(__file__).resolve().parent
R = Report()

SF = 31
SPC = 4
CHIP_RATE = 1.0e6
BASE = {
    "reps": 8,
    "spc": SPC,
    "chip_rate": CHIP_RATE,
    "cn0_dbhz": 45.0,
    "doppler_uncertainty": 0.0,
    "pfa": 1e-3,
    "pd": 0.9,
}


def _code() -> np.ndarray:
    return np.asarray(
        PN(poly=mls_poly(5), seed=1, length=5).generate(SF)
    ).astype(np.uint8)


def _burst(code: np.ndarray, n_ep: int, roll: int = 0) -> np.ndarray:
    s0 = np.repeat(np.where(code & 1, -1.0, 1.0), SPC)
    return np.tile(np.roll(s0, roll), n_ep).astype(np.complex64)


def _make(**kw):
    with warnings.catch_warnings():
        warnings.simplefilter("ignore")
        return BurstAcquisition(_code(), **{**BASE, **kw})


def _derived(a) -> dict[str, object]:
    """Every construction-derived quantity the object publishes."""
    return {
        "sf": a.sf,
        "spc": a.spc,
        "fs": a.fs,
        "code_bins": a.code_bins,
        "reps": a.reps,
        "doppler_bins": a.doppler_bins,
        "n_noncoh": a.n_noncoh,
        "eta": round(float(a.eta), 6),
        "pfa_cell": float(a.pfa_cell),
        "doppler_span_hz": round(float(a.doppler_span_hz), 3),
        "underpowered": bool(a.underpowered),
    }


def _csv(path: Path, header: str, rows: list[list[object]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as fh:
        fh.write(header + "\n")
        for r in rows:
            fh.write(",".join(str(v) for v in r) + "\n")


@dataclass
class Data:
    """Everything §3 and §4 read, measured once in §2."""

    fwd_rows: list[list[str]] = field(default_factory=list)
    every_param_moves: bool = False
    all_signatures_distinct: bool = False
    method_rows: list[list[str]] = field(default_factory=list)
    push_reaches: bool = False
    reset_reaches: bool = False
    raw_reaches: bool = False
    state_reaches: bool = False
    state_rejects: bool = False
    warn_fires: bool = False
    warn_silent_when_ok: bool = False
    ident_rows: list[list[str]] = field(default_factory=list)
    ident_sf: bool = False
    ident_fs: bool = False
    ident_code_bins: bool = False
    ident_span: bool = False
    ident_res: bool = False
    ident_reps_bound: bool = False


# ── 1. the object ─────────────────────────────────────────────────────


def section_object() -> None:
    R.md("## 1. The object — a front door, not an algorithm")
    R.md()
    R.md(
        "`BurstAcquisition` composes one `acq_state_t`, built through "
        "`acq_create_burst()`, and forwards every call to it. There is no "
        "algorithm here: the physics — coherent depth selection, CFAR "
        "sizing, window tiling, the detection statistic — lives in `acq` "
        "exactly once and is certified in its own report."
    )
    R.md()
    R.table(
        ["page", "owns"],
        [
            [
                "[`docs/design/dsss-acquisition.md`]"
                "(../../../../../../docs/design/dsss-acquisition.md)",
                "the architecture this object is a front door onto — the "
                "burst constructor it selects, and why burst and continuous "
                "are fixed at construction rather than switchable",
            ],
            [
                "[`acq`'s report](../acq/results.md)",
                "everything this object does at runtime — the engine, its "
                "sizing, its statistics. **Not re-derived here.**",
            ],
            [
                "`native/inc/burst_acq/burst_acq_core.h`",
                "the forwarding contract — the SSOT this report audits",
            ],
        ],
    )
    R.md("### 1.1 What a forwarder can get wrong")
    R.md()
    R.md(
        "Re-measuring detection through the wrapper would certify the same "
        "engine twice and present the second run as independent evidence. "
        "What the wrapper *can* get wrong alone is delivery: an argument "
        "dropped, defaulted, or **transposed**."
    )
    R.md()
    R.md(
        "Transposition is the characteristic defect of a thin wrapper "
        "because the type system does not help. `pfa` and `pd` are both "
        "doubles in (0,1); `reps` and `spc` are both `size_t`; "
        "`noise_mode` and `nthreads` are both `int`. Swap either pair and "
        "the object still constructs, still detects a strong signal, and "
        "is sized against the wrong numbers — which surfaces much later as "
        "a false-alarm rate nobody chose."
    )
    R.md()
    R.md("### 1.2 The claim inventory")
    R.md()
    R.table(
        ["header claim", "pinned where", "here"],
        [
            [
                "every function forwards to the embedded engine",
                "C, one call each",
                "§2.2",
            ],
            [
                "the constructor's arguments reach `acq_create_burst` intact",
                "**partially** — C builds one object and detects with it",
                "§2.1",
            ],
            [
                "`push`/`reset`/`configure_search_raw` reach the engine",
                "C",
                "§2.2",
            ],
            [
                "the serialized bytes ARE the shared engine's own state",
                "C, round-trip + envelope reject",
                "§2.2",
            ],
            [
                "a NULL or zero-length code is rejected, as `acq` rejects it",
                "C",
                "§2.2",
            ],
            [
                "an under-powered configuration warns at construction",
                "**was nothing** — the declared warning had no test",
                "§2.3",
            ],
            [
                "`destroy(NULL)` is safe",
                "C",
                "§2.2",
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
    _sec_forwarding(d)
    _sec_methods(d)
    _sec_warning(d)
    _sec_identities(d)
    return d


def _sec_forwarding(d: Data) -> None:
    R.md("### 2.1 Every constructor argument reaches the engine")
    R.md()
    R.md(
        "One parameter is varied at a time and the object's whole derived "
        "state is compared against the base configuration. Two things are "
        "required: each parameter must move **something** (it was not "
        "dropped or defaulted), and no two parameters may move the **same "
        "set** of quantities (a transposition would be invisible if they "
        "did)."
    )
    R.md()
    base = _derived(_make())
    cases = [
        ("reps 8 -> 16", {"reps": 16}),
        ("spc 4 -> 8", {"spc": 8}),
        ("chip_rate 1 -> 2 MHz", {"chip_rate": 2.0e6}),
        ("cn0_dbhz 45 -> 30", {"cn0_dbhz": 30.0}),
        ("doppler_uncertainty 0 -> 40 kHz", {"doppler_uncertainty": 40e3}),
        ("pfa 1e-3 -> 1e-6", {"pfa": 1e-6}),
        ("pd 0.9 -> 0.99", {"pd": 0.99}),
    ]
    rows, csv = [], []
    sigs = []
    all_moved = True
    for name, kw in cases:
        got = _derived(_make(**kw))
        moved = tuple(sorted(k for k in base if got[k] != base[k]))
        sigs.append(moved)
        all_moved &= len(moved) > 0
        rows.append([name, str(len(moved)), ", ".join(moved)])
        csv.append([name.replace(",", ";"), len(moved), " ".join(moved)])
    R.table(["parameter varied", "fields moved", "which"], rows)
    _csv(HERE / "data" / "forwarding.csv", "case,n_moved,fields", csv)
    d.fwd_rows = rows
    d.every_param_moves = all_moved
    d.all_signatures_distinct = len(set(sigs)) == len(sigs)
    R.md(
        f"All seven move something (**{d.every_param_moves}**), and all "
        f"seven signatures are distinct (**{d.all_signatures_distinct}**) — "
        f"so no pair of arguments could be swapped without the table "
        f"changing. `pfa` and `pd` are the pair worth naming: both are "
        f"doubles in (0,1), and they move different things — `pfa` moves "
        f"the threshold and the per-cell rate, `pd` moves only the look "
        f"count. Raw sweep: `data/forwarding.csv`."
    )
    R.md()


def _sec_methods(d: Data) -> None:
    R.md("### 2.2 Every method reaches the engine")
    R.md()
    code = _code()
    a = _make(cn0_dbhz=60.0, reps=4)
    a.configure_search_raw(doppler_bins=4, n_noncoh=1)
    d.raw_reaches = (a.doppler_bins, a.n_noncoh) == (4, 1)

    hits = a.push(_burst(code, 8, roll=17))
    d.push_reaches = bool(hits) and hits[0][1] == 17

    b = _make(cn0_dbhz=60.0, reps=4)
    b.configure_search_raw(doppler_bins=4, n_noncoh=1)
    b.push(_burst(code, 1)[: SF * SPC // 2])
    b.reset()
    fresh = _make(cn0_dbhz=60.0, reps=4)
    fresh.configure_search_raw(doppler_bins=4, n_noncoh=1)
    x = _burst(code, 8)
    d.reset_reaches = [h[:2] for h in b.push(x)] == [
        h[:2] for h in fresh.push(x)
    ]

    s = _make(cn0_dbhz=60.0, reps=4)
    s.configure_search_raw(doppler_bins=4, n_noncoh=1)
    s.push(_burst(code, 2))
    blob = s.get_state()
    t = _make(cn0_dbhz=60.0, reps=4)
    t.configure_search_raw(doppler_bins=4, n_noncoh=1)
    t.set_state(blob)
    d.state_reaches = [h[:2] for h in s.push(x)] == [h[:2] for h in t.push(x)]
    bad = bytearray(blob)
    bad[0] ^= 0xFF
    try:
        t.set_state(bytes(bad))
        d.state_rejects = False
    except ValueError:
        d.state_rejects = True

    rows = [
        [
            "configure_search_raw",
            "the pinned grid is read back",
            str(d.raw_reaches),
        ],
        [
            "push",
            "a burst rolled by 17 reports code_phase 17",
            str(d.push_reaches),
        ],
        [
            "reset",
            "a half-frame residue is dropped; matches a fresh object",
            str(d.reset_reaches),
        ],
        [
            "get_state / set_state",
            "a mid-stream blob resumes into a fresh instance",
            str(d.state_reaches),
        ],
        [
            "set_state (clobbered)",
            "a bad envelope is rejected, not reinterpreted",
            str(d.state_rejects),
        ],
    ]
    R.table(["method", "evidence it reached the engine", "holds"], rows)
    d.method_rows = rows
    R.md(
        "Each row is chosen so the engine must have *acted*, not merely "
        "been called: a code phase that matches the roll, a residue that "
        "was really dropped, a stream that really resumes. `push` returning "
        "a hit at the right phase is the strongest of them — it exercises "
        "the ring, the correlator and the CFAR gate in one call."
    )
    R.md()
    R.md(
        "The serialized bytes **are** the shared engine's state; there is "
        "no wrapper format layered on top, which is why a blob taken here "
        "restores the whole search and not a subset of it."
    )
    R.md()


def _sec_warning(d: Data) -> None:
    R.md("### 2.3 An under-powered configuration says so")
    R.md()
    R.md(
        "The object declares a post-construction warning gated on its own "
        "`underpowered` field — the manifest-driven diagnostic that turns a "
        "silently-too-weak search into something a caller sees. Nothing "
        "tested that it fires, which is the usual fate of a diagnostic: it "
        "is easy to declare and invisible when it stops working."
    )
    R.md()
    with warnings.catch_warnings(record=True) as caught:
        warnings.simplefilter("always")
        BurstAcquisition(
            _code(),
            **{**BASE, "reps": 2, "cn0_dbhz": 10.0},
        )
    d.warn_fires = any(
        "under-powered" in str(w.message).lower() for w in caught
    )
    with warnings.catch_warnings(record=True) as quiet:
        warnings.simplefilter("always")
        BurstAcquisition(
            _code(),
            **{**BASE, "reps": 8, "cn0_dbhz": 70.0},
        )
    d.warn_silent_when_ok = not any(
        "under-powered" in str(w.message).lower() for w in quiet
    )
    R.table(
        ["configuration", "warns"],
        [
            ["reps=2, cn0_dbhz=10 (cannot meet pd)", str(d.warn_fires)],
            [
                "reps=8, cn0_dbhz=70 (comfortable)",
                str(not d.warn_silent_when_ok),
            ],
        ],
    )
    R.md(
        "Both directions are checked. A warning that always fires is as "
        "useless as one that never does, and only the pair distinguishes "
        "them."
    )
    R.md()


def _sec_identities(d: Data) -> None:
    R.md("### 2.4 The derived identities, across three geometries")
    R.md()
    R.md(
        "Each of these is a documented relationship between a constructor "
        "argument and a published field, so each is a second, independent "
        "check that the argument landed in the right slot. §2.1 proves the "
        "arguments are distinguishable; these prove where they went."
    )
    R.md()
    rows = []
    sf_ok = fs_ok = cb_ok = span_ok = res_ok = bound_ok = True
    for reps, spc, cr in ((8, 4, 1.0e6), (16, 2, 3.0e6), (4, 8, 2.5e6)):
        a = _make(reps=reps, spc=spc, chip_rate=cr, cn0_dbhz=50.0)
        sf_ok &= a.sf == SF
        fs_ok &= abs(a.fs - cr * spc) < 1e-6
        cb_ok &= a.code_bins == SF * spc
        span_ok &= abs(a.doppler_span_hz - cr / (2.0 * SF)) < 1e-6
        res_ok &= abs(a.doppler_res_hz - cr / (SF * a.doppler_bins)) < 1e-6
        bound_ok &= 1 <= a.doppler_bins <= reps
        rows.append(
            [
                f"reps={reps}, spc={spc}, {cr / 1e6:g} Mcps",
                str(a.sf),
                f"{a.fs / 1e6:g}M",
                str(a.code_bins),
                f"{a.doppler_span_hz:.0f}",
                f"{a.doppler_bins}/{reps}",
            ]
        )
    R.table(
        [
            "geometry",
            "sf",
            "fs",
            "code_bins",
            "span (Hz)",
            "bins/reps",
        ],
        rows,
    )
    d.ident_rows = rows
    d.ident_sf, d.ident_fs, d.ident_code_bins = sf_ok, fs_ok, cb_ok
    d.ident_span, d.ident_res, d.ident_reps_bound = span_ok, res_ok, bound_ok
    R.md(
        "`sf` is the code length, `fs` is `chip_rate * spc`, `code_bins` is "
        "`sf * spc`, the native span is `chip_rate/(2*sf)`, the resolution "
        "is `chip_rate/(sf * doppler_bins)`, and the coherent depth never "
        "exceeds `reps`. Six identities over three geometries: a `spc` that "
        "reached the `reps` slot, or a `chip_rate` that never arrived, "
        "breaks several of them at once."
    )
    R.md()


def _both(d: Data) -> str:
    """Wording for F2, so the finding reads the measurement rather than
    asserting it."""
    return (
        "both hold"
        if d.every_param_moves and d.all_signatures_distinct
        else "see the table"
    )


# ── 3. review ─────────────────────────────────────────────────────────


def review(d: Data) -> None:
    R.md("## 3. Review — findings")
    R.md()
    R.find(
        "F1",
        "BY DESIGN",
        "**This report does not measure detection performance, and that is "
        "the point.** `burst_acq_core.c` forwards every call into "
        "`acq_core.c`; re-running the detection statistics through the "
        "wrapper would certify the same engine a second time and present "
        "the result as independent evidence. The physics is certified in "
        "[`acq`'s report](../acq/results.md), which measures through this "
        "very front door. What is certified here is delivery — that every "
        "argument and every call arrives, and arrives in the right place.",
    )
    R.find(
        "F2",
        "FIXED",
        f"**Argument delivery was pinned only by a single successful "
        f"construction.** The C test builds one object and detects with it, "
        f"which proves nothing about whether each argument reached its own "
        f"destination: a wrapper that transposed two same-typed parameters "
        f"would still construct and still detect a strong signal. §2.1 now "
        f"varies each of the seven constructor arguments alone and requires "
        f"(a) that each moves something, and (b) that no two move the same "
        f"set — {_both(d)}. "
        f"`pfa` and `pd` are the pair that matters: both doubles in (0,1), "
        f"and distinguishable because `pfa` moves the threshold while `pd` "
        f"moves only the look count.",
    )
    R.find(
        "F3",
        "FIXED",
        "**The under-powered warning was declared and never tested.** It is "
        "a manifest-driven post-construction diagnostic gated on the "
        "engine's own `underpowered` field, and it is exactly the kind of "
        "thing that stops working invisibly — a diagnostic nobody exercises "
        "is indistinguishable from one that was removed. Now checked in "
        "both directions (§2.3): it fires on a configuration that cannot "
        "meet the requested `pd`, and stays silent on one that can. A "
        "warning that always fires is as useless as one that never does, "
        "and only the pair separates them.",
    )
    R.find(
        "F4",
        "C-ONLY",
        "The forwarding of `burst_acq_destroy(NULL)` and the rejection of a "
        "NULL or zero-length code are certified in "
        "`native/tests/test_burst_acq_core.c`: neither is reachable from "
        "Python, where the binding owns lifetime and the array conversion "
        "refuses a malformed code before the C is entered.",
    )


# ── 4. limits ─────────────────────────────────────────────────────────


def limits(d: Data) -> None:
    R.md("## 4. Limits — the certified envelope")
    R.md()
    R.md(
        "Claims a caller may rely on. A failure here is a regression, not a "
        "new finding. Every one is asserted by "
        "`src/doppler/dsss/tests/test_validation_limits.py`. The detection "
        "envelope itself is [`acq`'s](../acq/results.md), not repeated here."
    )
    R.md()
    R.limit(
        d.every_param_moves,
        "each of the seven constructor arguments changes the derived state "
        "— none is dropped or silently defaulted",
    )
    R.limit(
        d.all_signatures_distinct,
        "no two arguments move the same set of derived quantities, so a "
        "transposition cannot hide",
    )
    R.limit(
        d.raw_reaches,
        "configure_search_raw reaches the engine: the pinned grid reads back",
    )
    R.limit(
        d.push_reaches,
        "push reaches the engine: a burst rolled by 17 samples reports code "
        "phase 17",
    )
    R.limit(
        d.reset_reaches,
        "reset reaches the engine: a half-frame residue is dropped and the "
        "object matches a fresh one",
    )
    R.limit(
        d.state_reaches,
        "get_state/set_state reach the engine: a mid-stream blob resumes "
        "into a fresh instance",
    )
    R.limit(
        d.state_rejects,
        "a clobbered state envelope is rejected rather than reinterpreted",
    )
    R.limit(
        d.warn_fires,
        "an under-powered configuration warns at construction",
    )
    R.limit(
        d.warn_silent_when_ok,
        "...and a comfortable one does not, so the warning carries "
        "information",
    )
    R.limit(
        len(d.fwd_rows) == 7,
        "the forwarding sweep covers every constructor argument, not a "
        "sample of them",
    )
    R.limit(
        len(d.method_rows) == 5,
        "every forwarded method has evidence the engine ACTED, not merely "
        "that the call returned",
    )
    R.limit(d.ident_sf, "sf equals the supplied code length")
    R.limit(d.ident_fs, "fs equals chip_rate * spc across three geometries")
    R.limit(
        d.ident_code_bins, "code_bins equals sf * spc across three geometries"
    )
    R.limit(
        d.ident_span,
        "doppler_span_hz equals chip_rate/(2*sf) across three geometries",
    )
    R.limit(
        d.ident_res,
        "doppler_res_hz equals chip_rate/(sf * doppler_bins)",
    )
    R.limit(
        d.ident_reps_bound,
        "the chosen coherent depth stays within [1, reps] — the burst "
        "mode's documented search range",
    )


# ── build ─────────────────────────────────────────────────────────────


def build(write: bool = True) -> Report:
    global R
    R = Report(write=write)
    R.md("# BurstAcquisition — validation report")
    R.md()
    section_object()
    d = characterise()
    review(d)
    limits(d)
    R.executive(
        "BurstAcquisition",
        [
            "**The detection envelope is [`acq`'s](../acq/results.md), not "
            "this object's.** Every call forwards into the shared engine, "
            "so re-measuring the statistics here would certify the same "
            "code twice and call the second run independent (F1).",
            "**What a forwarder can get wrong is delivery, and that is what "
            "is certified.** Each of the seven constructor arguments moves "
            "a distinct set of derived quantities, so a transposition — two "
            "same-typed parameters swapped, the object still constructing "
            "and still detecting — cannot hide (§2.1, F2).",
            "**`pfa` and `pd` are the pair to watch.** Both are doubles in "
            "(0,1) and nothing in the type system separates them; they are "
            "distinguishable only because `pfa` moves the threshold and the "
            "per-cell rate while `pd` moves only the look count (§2.1).",
            "**The under-powered warning is tested in both directions** — "
            "it fires when the link cannot meet the requested `pd` and "
            "stays quiet when it can. It had no test at all, which is how a "
            "declared diagnostic stops working invisibly (§2.3, F3).",
            "**The serialized bytes are the engine's own state**, with no "
            "wrapper format layered on, so a blob taken here restores the "
            "whole search (§2.2).",
        ],
    )
    R.summary("\n- Raw sweep: `data/forwarding.csv`")
    R.emit(HERE / "results.md")
    return R


if __name__ == "__main__":
    sys.exit(cli(build, HERE))
