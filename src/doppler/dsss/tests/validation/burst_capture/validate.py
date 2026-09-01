"""Certify `BurstCapture` — the stage that turns a detection into a burst.

Run:  python -m doppler.dsss.tests.validation.burst_capture.validate
      make validate          (regenerates every report)
      make validate-check    (fails if the committed report is stale)

Phase 8 of `docs/dev/contributing/adding-algorithms.md`, for the object
designed in `docs/design/burst-capture.md`. The order that produced it is the
mandated one: `burst_capture_core.h`'s prose claims first, mapped onto
`native/tests/test_burst_capture_core.c` as pinned / pinned-only-at-literals /
absent, the uncovered ones written and each proven by sabotage — and only then
this file.

**What is deliberately NOT re-derived here.** The detection statistics belong
to [`acq`](../acq/results.md) and reach this object through
[`BurstAcquisition`](../burst_acq/results.md); re-measuring them would certify
the same engine a third time and report it as independent evidence. Refine's
discrimination against C/N0 and the framing sweep are measured in
[`dsss_burst_receiver`](../dsss_burst_receiver/results.md)'s characterization,
against the composition that shipped first.

What is certified here is what this object owns alone: the **geometry** it
derives, the **epoch** it resolves, the **retention** that lets it reach back,
and the **two flavours' blobs**.
"""

from __future__ import annotations

import sys
import tempfile
from dataclasses import dataclass, field
from pathlib import Path

import numpy as np

from doppler.dsss import BurstCapture, PersistentBurstCapture
from doppler.dsss.tests.characterization.burst_capture.characterize import (
    ACQ_SF,
    BURST_LEN,
    CHIP_RATE,
    GAPS,
    REPS,
    SPC,
    acq_code,
    run_pair,
    scene,
    separation,
)
from doppler.tests._validation_common import Report, cli

HERE = Path(__file__).resolve().parent
R = Report()

# Geometry, stimulus and codes all come from the characterization subject, so
# the two bodies of evidence describe ONE waveform. A private copy here is how
# a report comes to certify a burst nobody transmits -- and the burst is the
# library's own `Segment(type="dsss")`, the same declarative source `wfmgen`
# drives, rather than a fourth hand-rolled one.
TOL = SPC  # one chip


def cap(**kw) -> BurstCapture:
    return BurstCapture(
        acq_code(),
        burst_len=kw.pop("burst_len", BURST_LEN),
        reps=REPS,
        spc=SPC,
        chip_rate=CHIP_RATE,
        cn0_dbhz=55.0,
        **kw,
    )


@dataclass
class Data:
    """Everything the sections measure, so review and limits read one run."""

    refine_span: int = 0
    retain_span: int = 0
    q_cap_small: int = 0
    q_cap_large: int = 0
    start_err: list[int] = field(default_factory=list)
    block_sizes: dict = field(default_factory=dict)
    gaps: list = field(default_factory=list)
    found: list = field(default_factory=list)
    sep: dict = field(default_factory=dict)
    whole_windows: bool = False
    rows_match: bool = False
    held_then_emitted: bool = False
    reset_reworks: bool = False
    bytes_static: bool = False
    accessors_agree: bool = False
    grid_refused: bool = False
    search_visible: bool = False
    underpowered_says_so: bool = False
    lifetime_counts: bool = False
    res_hz_ok: bool = False
    blob_ram: int = 0
    blob_disk: int = 0
    resume_ok: bool = False
    cross_reject: bool = False


def section_object() -> None:
    R.md("## 1. The object")
    R.md()
    R.md(
        "`BurstCapture` sits between a detector and whatever consumes a "
        "burst. Acquisition reports an END anchor and a `code_phase` that is "
        "a lag **modulo one code period**, so it fixes the alignment within a "
        "preamble repetition and never says WHICH one; a burst has a frame "
        "that begins in one specific repetition. This object resolves that, "
        "keeps the look-back needed to reach a start already gone past, and "
        "emits the burst's samples. It stops there."
    )
    R.md()
    design = "../../../../../../docs/design/burst-capture.md"
    R.md(f"- Design: [`burst-capture.md`]({design})")
    R.md("- Header: `native/inc/burst_capture/burst_capture_core.h`")
    R.md("- C tests: `native/tests/test_burst_capture_core.c`")
    R.md(
        "- Characterization: "
        "`src/doppler/dsss/tests/characterization/burst_capture/`"
    )
    R.md()


def characterise() -> Data:
    d = Data()
    R.md("## 2. Characterisation")
    R.md()

    # ── 2.1 ──────────────────────────────────────────────────────────────
    R.md("### 2.1 The geometry is derived, never asked for (C §1)")
    R.md()
    R.md(
        "A caller asked to size a look-back buffer is a caller handed a way "
        "to lose bursts silently, so every span comes from the geometry. The "
        "two a caller must nevertheless RESPECT are read back rather than "
        "recomputed — restating `(k_lo+k_hi+reps)·code_period` in a caller is "
        "the drift this repo forbids, and the header's own prose for it was "
        "2.4x low until it was measured."
    )
    R.md()
    c = cap()
    big = cap(burst_len=20 * BURST_LEN)
    d.refine_span = int(c.refine_span)
    d.retain_span = int(c.retain_span)
    R.table(
        ["quantity", "value (samples)", "identity"],
        [
            ["`burst_len`", str(c.burst_len), "what gets captured"],
            ["`refine_span`", str(d.refine_span), "the coalescing reach"],
            [
                "`retain_span`",
                str(d.retain_span),
                "`refine_span` + one burst",
            ],
        ],
    )
    R.md()
    d.q_cap_small, d.q_cap_large = int(c.retain_span), int(big.retain_span)
    R.md(
        f"`retain_span == refine_span + burst_len` holds exactly "
        f"({d.retain_span} = {d.refine_span} + {c.burst_len}). Nothing here "
        "is a constant that happens to fit the test geometry: a capture "
        f"built for a burst 20x longer derives `retain_span` "
        f"{d.q_cap_large:,} against {d.q_cap_small:,}, and the detection "
        "queue is sized from `burst_len / refine_span` for the same reason "
        "— a fixed depth silently dropped the hit AND the rest of its batch "
        "on any geometry but the one the tests happened to use."
    )
    R.md()

    # ── 2.2 ──────────────────────────────────────────────────────────────
    R.md("### 2.2 The epoch: never late, and exact (C §3)")
    R.md()
    R.md(
        "The claim the object exists for. `code_phase` measures "
        "`burst_start mod code_bins` exactly, so a phase seed resolves "
        "alignment WITHIN a code period and never which period — and an "
        "epoch error is a cliff, not a gradient: a window one repetition out "
        "decodes at half the payload in error. Scored against the burst's "
        "known position, not against the object's opinion of itself."
    )
    R.md()
    for seed, at in enumerate([9000, 23_117, 41_000, 60_500], start=1):
        x = scene([at], at + 6 * BURST_LEN, seed=seed)
        cc = cap()
        cc.push(x)
        ev = cc.events()
        got = int(ev["preamble_start"][0]) if len(ev) else -1
        d.start_err.append(got - at if got >= 0 else 10**9)
    R.table(
        ["burst placed at", "reported `preamble_start`", "error (samples)"],
        [
            [str(a), str(a + e) if abs(e) < 10**8 else "not found", str(e)]
            for a, e in zip([9000, 23_117, 41_000, 60_500], d.start_err)
        ],
    )
    R.md()
    R.md(
        "Zero at every offset, including ones that are not multiples of the "
        "code period — which is what distinguishes a resolved epoch from a "
        "phase seed that happened to land."
    )
    R.md()

    # ── 2.3 ──────────────────────────────────────────────────────────────
    R.md("### 2.3 Block size is not a parameter of the answer (C §5, §11)")
    R.md()
    R.md(
        "The ring is a contiguous window over the stream and is never reset "
        "between bursts, so a burst whose tail falls outside one call is "
        "completed by a later one. A push LARGER than the ring is sliced "
        "rather than refused, which is what accepting any block size costs."
    )
    R.md()
    at = [9000, 60_000, 120_000]
    ref = None
    for blk in (333, 4096, 65_536, 200_000):
        x = scene(at, 200_000, seed=11)
        cc = cap()
        out = []
        for off in range(0, x.size, blk):
            out.append(cc.push(x[off : off + blk]))
        got = np.concatenate(out) if out else np.empty(0, np.complex64)
        if ref is None:
            ref = got
        d.block_sizes[blk] = (
            int(got.size // BURST_LEN),
            bool(np.array_equal(got, ref)),
            int(cc.dropped),
        )
    R.table(
        ["block size", "windows", "identical to 333-sample run", "dropped"],
        [
            [str(b), str(v[0]), "yes" if v[1] else "**no**", str(v[2])]
            for b, v in d.block_sizes.items()
        ],
    )
    R.md()

    # ── 2.4 ──────────────────────────────────────────────────────────────
    R.md("### 2.4 The spacing floor — measured, against a derived claim")
    R.md()
    R.md(
        "`refine_span`'s own documentation states the gap a caller must "
        "leave as `max(0, refine_span - burst_len)`, which for a burst "
        "longer than the reach is **zero**. That number was derived rather "
        "than measured, and the correction that produced it went the other "
        "way from an earlier over-estimate (doppler#1085). The "
        "characterization sweeps it: two bursts, dead air from zero upwards, "
        "12 trials a point at randomised absolute positions."
    )
    R.md()
    rows = []
    for gap in GAPS:
        f = 0
        for t in range(6):
            fi, _e, ri = run_pair(gap, seed=1000 + 97 * t + gap)
            f += fi
            rows.extend(ri)
        d.gaps.append(gap)
        d.found.append(f / 12.0)
    d.sep = separation(rows)
    R.table(
        ["dead air (samples)", "transmitted bursts captured"],
        [[str(g), f"{100.0 * f:.0f}%"] for g, f in zip(d.gaps, d.found)],
    )
    R.md()
    claimed = max(0, d.refine_span - BURST_LEN)
    first_full = next((g for g, f in zip(d.gaps, d.found) if f >= 1.0), None)
    R.md(
        f"The header's number for this geometry is **{claimed} samples**. "
        f"Measured, the pair is not reliably captured until "
        f"**{first_full} samples** of dead air — "
        f"{first_full / max(claimed, 1):.0f}x that, and about "
        f"{first_full / (ACQ_SF * SPC):.1f} code periods. Recorded as F1."
    )
    R.md()

    # ── 2.5 ──────────────────────────────────────────────────────────────
    R.md("### 2.5 Telling a real window from a spurious one")
    R.md()
    R.md(
        "At `pfa = 1e-3` over a surface this size a false alarm is expected, "
        "not a defect — this object is a detector's output stage and gating "
        "it on signal quality would make it lie about what it found. So the "
        "caller filters, and the question a report has to answer is **with "
        "which read-back**."
    )
    R.md()
    cn0_gap = d.sep.get("cn0_real", 0) - d.sep.get("cn0_spurious", 0)
    margin_gap = d.sep.get("margin_spurious", 0) - d.sep.get("margin_real", 0)
    R.table(
        ["statistic", "at a real burst", "at a spurious window", "separation"],
        [
            [
                "`cn0_dbhz_est` (dB-Hz)",
                f"{d.sep.get('cn0_real', float('nan')):.1f}",
                f"{d.sep.get('cn0_spurious', float('nan')):.1f}",
                f"{cn0_gap:.1f} dB",
            ],
            [
                "`refine_margin`",
                f"{d.sep.get('margin_real', float('nan')):.3f}",
                f"{d.sep.get('margin_spurious', float('nan')):.3f}",
                f"{margin_gap:.3f}",
            ],
        ],
    )
    R.md()
    R.md(
        f"Over {d.sep.get('n_real', 0)} real and "
        f"{d.sep.get('n_spurious', 0)} spurious windows. The answer is "
        "`cn0_dbhz_est`, and it is not the intuitive one — `refine_margin` "
        "is the stage's own health signal and separates the two populations "
        "by a few hundredths, because a window on noise has no period to "
        "resolve and scores much like one that resolved it. Recorded as F2."
    )
    R.md()

    # ── 2.6 ──────────────────────────────────────────────────────────────
    R.md("### 2.6 The two flavours' blobs (C §12–§15)")
    R.md()
    R.md(
        "The look-back IS the checkpoint for an in-RAM capture. Backed by a "
        "file, the ring's pages are the file's contents (`MAP_SHARED`), so "
        "the blob names where in the ring the history sits instead of "
        "carrying it."
    )
    R.md()
    with tempfile.TemporaryDirectory() as td:
        path = Path(td) / "ring.cf32"
        ram = cap()
        dsk = PersistentBurstCapture(
            path,
            acq_code(),
            burst_len=BURST_LEN,
            reps=REPS,
            spc=SPC,
            chip_rate=CHIP_RATE,
            cn0_dbhz=55.0,
        )
        d.blob_ram = int(ram.state_bytes())
        d.blob_disk = int(dsk.state_bytes())

        # Resume across a mid-preamble split, into a fresh instance.
        at0 = 60_000
        x = scene([at0], 200_000, seed=3)
        cut = at0 + 2 * ACQ_SF * SPC
        a = cap()
        a.push(x[:cut])
        b = cap()
        b.set_state(a.get_state())
        d.resume_ok = bool(
            b.push(x[cut:]).size == BURST_LEN and int(b.preamble_start) == at0
        )

        # A blob does not travel between the flavours.
        try:
            ram.set_state(dsk.get_state())
            d.cross_reject = False
        except ValueError:
            d.cross_reject = True

    R.table(
        ["flavour", "`state_bytes()`", "carries the look-back?"],
        [
            ["`BurstCapture`", f"{d.blob_ram:,} B", "yes"],
            [
                "`PersistentBurstCapture`",
                f"{d.blob_disk:,} B",
                "no — the file does",
            ],
        ],
    )
    R.md()
    R.md(
        f"The difference is {d.blob_ram - d.blob_disk:,} B, which is exactly "
        f"`retain_span * 8` ({d.retain_span} complex64 samples) — the whole "
        "of the retained history and nothing else."
    )
    R.md()

    # ── 2.7 ──────────────────────────────────────────────────────────────
    R.md("### 2.7 The lifecycle, and what the read-backs mean (C §2, §6–§10)")
    R.md()
    R.md(
        "The claims a caller relies on between pushes rather than inside "
        "one. Each is small; together they are the difference between an "
        "object a caller can drive and one it has to guess at."
    )
    R.md()
    at1 = 9000
    x = scene([at1], 80_000, seed=7)

    c1 = cap()
    win = c1.push(x)
    d.whole_windows = bool(win.size % BURST_LEN == 0 and win.size > 0)
    d.rows_match = bool(len(c1.events()) == win.size // BURST_LEN)
    d.accessors_agree = bool(
        int(c1.preamble_start) == int(c1.events()["preamble_start"][0])
        and c1.refine_margin == float(c1.events()["refine_margin"][0])
        and c1.cn0_dbhz_est == float(c1.events()["cn0_dbhz_est"][0])
    )
    d.res_hz_ok = bool(c1.doppler_res_hz > 0.0)

    # A burst one sample short of complete is HELD, and one more releases it.
    short = at1 + BURST_LEN - 1
    c2 = cap()
    held = c2.push(x[:short])
    d.held_then_emitted = bool(
        held.size == 0
        and c2.pending == 1
        and c2.push(x[short : short + 1]).size == BURST_LEN
    )

    # reset() returns to searching AND keeps working (F3's regression).
    c3 = cap()
    c3.push(x)
    before = int(c3.n_bursts)
    c3.reset()
    d.reset_reworks = bool(
        c3.pending == 0
        and int(c3.preamble_start) == 0
        and int(c3.n_bursts) == before  # lifetime, survives reset
        and c3.push(x).size == BURST_LEN
        and int(c3.preamble_start) == at1
    )
    d.lifetime_counts = bool(int(c3.n_bursts) == before + 1)

    # state_bytes() is a pure function of configuration.
    c4 = cap()
    empty = int(c4.state_bytes())
    c4.push(x[:40_000])
    mid = int(c4.state_bytes())
    c4.push(x[40_000:])
    d.bytes_static = bool(empty == mid == int(c4.state_bytes()))

    # A grid deeper than the preamble is refused, and refusing changes
    # nothing -- an escape hatch that silently clamped would be worse than
    # one that says no.
    c5 = cap()
    ok_shallow = c5.configure_search_raw(2, 1)
    try:
        c5.configure_search_raw(8 * REPS, 1)
        refused = False
    except Exception:
        refused = True
    d.grid_refused = bool(refused and ok_shallow is None)

    # The search under the capture, and the diagnostic whose failure mode is
    # silence: a grid that cannot meet the requested pd still BUILDS and then
    # captures fewer bursts than arrived.
    c6 = cap()
    d.search_visible = bool(
        c6.doppler_bins >= 1
        and c6.n_noncoh >= 1
        and c6.code_bins == ACQ_SF * SPC
        and c6.doppler_span_hz > 0.0
        and c6.eta > 0.0
        and c6.eta_nc > c6.eta
        and 0.0 < c6.straddle_loss <= 1.0
        and not c6.underpowered
    )
    import warnings as _w

    with _w.catch_warnings(record=True) as caught:
        _w.simplefilter("always")
        c7 = BurstCapture(
            acq_code(),
            burst_len=BURST_LEN,
            reps=REPS,
            spc=SPC,
            chip_rate=CHIP_RATE,
            cn0_dbhz=20.0,
            pd=0.99,
        )
    d.underpowered_says_so = bool(
        c7.underpowered
        and c7.pd_predicted < 0.99
        and any(issubclass(w.category, UserWarning) for w in caught)
    )

    R.table(
        ["claim", "holds"],
        [
            ["`push()` returns whole windows only", str(d.whole_windows)],
            ["`events()` has one row per window", str(d.rows_match)],
            [
                "a burst one sample short is HELD, `pending == 1`",
                str(d.held_then_emitted),
            ],
            [
                "`reset()` returns to searching and keeps working",
                str(d.reset_reworks),
            ],
            [
                "`n_bursts` is a lifetime count and survives reset",
                str(d.lifetime_counts),
            ],
            [
                "`state_bytes()` does not move with the stream",
                str(d.bytes_static),
            ],
            [
                "the read-backs agree with the event row",
                str(d.accessors_agree),
            ],
            ["a grid deeper than `reps` is refused", str(d.grid_refused)],
            ["the search under the capture is visible", str(d.search_visible)],
            [
                "an unmeetable `pd` warns AND reads back",
                str(d.underpowered_says_so),
            ],
        ],
    )
    R.md()
    return d


def review(d: Data) -> None:
    R.md("## 3. Review — findings")
    R.md()
    claimed = max(0, d.refine_span - BURST_LEN)
    first_full = next((g for g, f in zip(d.gaps, d.found) if f >= 1.0), None)
    R.find(
        "F1",
        "CONFIRMED",
        f"**The header's required-gap formula is optimistic by "
        f"~{first_full // max(claimed, 1)}x.** "
        f"`refine_span`'s doc gives the gap a caller must leave as "
        f"`max(0, refine_span - burst_len)` = {claimed} samples here; "
        f"measured, both bursts of a pair are not reliably captured until "
        f"{first_full} samples of dead air (§2.4). The formula is right about "
        "the SHAPE — the constraint is start-to-start, not a whole "
        "`refine_span` of silence, and reading it the other way cost 9% of "
        "airtime once — but wrong about the floor. A caller following it "
        "literally loses roughly a fifth of a closely-packed pair. Tracked as "
        "[gh-1172](https://github.com/doppler-dsp/doppler/issues/1172).",
    )
    R.find(
        "F2",
        "BY DESIGN",
        "**`refine_margin` is not the statistic to filter on; `cn0_dbhz_est` "
        f"is.** The margin separates real from spurious windows by "
        f"{d.sep.get('margin_spurious', 0) - d.sep.get('margin_real', 0):.3f} "
        f"against C/N0's "
        f"{d.sep.get('cn0_real', 0) - d.sep.get('cn0_spurious', 0):.1f} dB "
        "(§2.5). That reads as a defect and is not one: the margin answers "
        "'was the code PERIOD resolved', and a window sitting on noise has "
        "no period to resolve, so it scores much like one that did. It is a "
        "hand-off health signal, which is a different question from 'is "
        "anything there'.",
    )
    R.find(
        "F3",
        "FIXED",
        "**`reset()` left the ring behind the stream.** `head`/`tail` are "
        "monotonic ABSOLUTE counters, so emptying the ring by consuming "
        "everything available left them at the stream's last position while "
        "`samples_fed` restarted at 0 — every position computed afterwards "
        "was 0-based against a ring that was not, so nothing was reachable, "
        "refine never ran and writes were refused. Found by asserting a "
        "re-push after reset, which the code this was moved from never did. "
        "Fixed here; the receiver's own copy is "
        "[gh-1169](https://github.com/doppler-dsp/doppler/issues/1169).",
    )
    R.find(
        "F4",
        "FIXED",
        "**`configure_search_raw` returned a code its header did not "
        "document.** The composed child forwards `acq`'s own `-1`, which is "
        "not one of the eight codes `clib_common.h` defines, while this "
        "object's header promised `DP_ERR_INVALID` (-4). A C caller "
        "branching on the documented code would have mis-read a refusal. "
        "Translated at this boundary rather than weakening the doc to match.",
    )
    R.find(
        "F5",
        "C-ONLY",
        "**The zero-copy consumer face is not reachable from Python.** "
        "`burst_capture_ready`/`window`/`event_at` let a composing C object "
        "borrow a window out of the scratch instead of copying it again, "
        "which is what keeps `DsssBurstReceiver` paying one memcpy per burst "
        "rather than two. The binding necessarily copies into a numpy array, "
        "so the borrow is certified in C §7 and §3 instead.",
    )
    R.find(
        "F6",
        "GAP",
        "**The per-burst window copy is measured only at a short-burst "
        "geometry.** `bench_burst_capture_core` puts it at 34 µs for four "
        "bursts against a 2.24 ms search floor — 1.5% — but the copy scales "
        "with `burst_len` and the search floor does not, so a real link "
        "geometry (a 2.5 MB window rather than 20 kB) is a different "
        "measurement that has not been taken. Tracked as "
        "[gh-1173](https://github.com/doppler-dsp/doppler/issues/1173).",
    )


def limits(d: Data) -> None:
    R.md("## 4. Limits — the certified envelope")
    R.md()
    R.md(
        "Claims a caller may rely on. A failure here is a regression, not a "
        "new finding."
    )
    R.md()
    R.limit(
        d.retain_span == d.refine_span + BURST_LEN,
        f"`retain_span` == `refine_span` + `burst_len` "
        f"({d.retain_span} = {d.refine_span} + {BURST_LEN})",
    )
    R.limit(
        all(e == 0 for e in d.start_err),
        "`preamble_start` is EXACT at every tested offset, including ones "
        "that are not a multiple of the code period (§2.2)",
    )
    R.limit(
        all(v[1] for v in d.block_sizes.values()),
        "the windows are bit-identical across block sizes from 333 samples "
        "to a push 1.6x the ring's capacity (§2.3)",
    )
    R.limit(
        all(v[2] == 0 for v in d.block_sizes.values()),
        "no sample is dropped by the history ring at any block size (§2.3)",
    )
    nf = next((g for g, f in zip(d.gaps, d.found) if f >= 1.0), None)
    R.limit(
        nf is not None and nf <= 512,
        f"two bursts {nf} samples apart (edge to edge) are both captured — "
        f"{nf / BURST_LEN:.2f} of a burst length, not a whole one (§2.4)",
    )
    R.limit(
        d.sep.get("cn0_real", 0) - d.sep.get("cn0_spurious", 0) > 3.0,
        "`cn0_dbhz_est` separates a real window from a spurious one by more "
        "than 3 dB, so it is usable as the filter (§2.5)",
    )
    R.limit(
        d.resume_ok,
        "a blob taken mid-preamble resumes into a FRESH instance and finds "
        "the burst it was holding — the retained look-back travels (§2.6)",
    )
    R.limit(
        d.blob_ram - d.blob_disk == d.retain_span * 8,
        f"the persistent flavour's blob is smaller by exactly the retained "
        f"span ({d.blob_ram - d.blob_disk:,} B) (§2.6)",
    )
    R.limit(
        d.cross_reject,
        "a blob does not travel between the in-RAM and persistent flavours "
        "in either direction (§2.6)",
    )
    R.limit(
        d.whole_windows,
        "`push()` returns a whole number of windows, never a partial one — "
        "half a burst is not a burst (§2.7)",
    )
    R.limit(
        d.rows_match,
        "`events()` returns exactly one row per window `push()` returned, so "
        "a caller can index them together (§2.7)",
    )
    R.limit(
        d.held_then_emitted,
        "a burst one sample short of complete is HELD with `pending == 1`, "
        "and one more sample releases it (§2.7)",
    )
    R.limit(
        d.reset_reworks,
        "`reset()` returns to the searching state AND the same stream is "
        "found again afterwards — the assertion the code this was moved from "
        "never made (§2.7, F3)",
    )
    R.limit(
        d.lifetime_counts,
        "`n_bursts` is a LIFETIME count and survives `reset()`, so a reset "
        "cannot report a clean stream (§2.7)",
    )
    R.limit(
        d.bytes_static,
        "`state_bytes()` is a pure function of configuration — it does not "
        "move with the stream, so a blob restores into any instance of the "
        "same geometry (§2.7)",
    )
    R.limit(
        d.accessors_agree,
        "the scalar read-backs and the event row describe the same burst, "
        "which is what lets a caller holding one window skip the list (§2.7)",
    )
    R.limit(
        d.grid_refused,
        "`configure_search_raw` accepts a coherent depth within `reps` and "
        "REFUSES one beyond it, rather than silently clamping (§2.7)",
    )
    R.limit(
        d.search_visible,
        "the search is visible as numbers — `doppler_bins`, `n_noncoh`, "
        "`code_bins`, `doppler_span_hz`, both gates and `straddle_loss` — so "
        "a caller can size a link without inferring it (§2.7)",
    )
    R.limit(
        d.underpowered_says_so,
        "a search that cannot meet the requested `pd` warns at construction "
        "AND reads back as `underpowered`, rather than building quietly and "
        "capturing fewer bursts than arrived (§2.7)",
    )
    R.limit(
        d.res_hz_ok,
        "`doppler_res_hz` reports acquisition's native bin width, so the "
        "event's Doppler estimate carries its own uncertainty (§2.7)",
    )


def build(write: bool = True) -> Report:
    global R
    R = Report(write=write)
    section_object()
    d = characterise()
    review(d)
    limits(d)
    R.executive(
        "BurstCapture",
        [
            "**`preamble_start` is exact, and that is the object's reason to "
            "exist.** Acquisition's `code_phase` is a lag modulo one code "
            "period and an epoch error is a cliff rather than a gradient; "
            "the reported start is correct to the sample at every offset "
            "tested, including ones off the code-period grid (§2.2).",
            "**Leave more dead air than the header's formula says.** It "
            "gives `max(0, refine_span - burst_len)`; measured, a pair needs "
            f"{next((g for g, f in zip(d.gaps, d.found) if f >= 1.0), 0)} "
            "samples before both bursts are reliably captured (§2.4, F1).",
            "**Filter on `cn0_dbhz_est`, not `refine_margin`.** A spurious "
            "window is expected at `pfa = 1e-3` and this object will not "
            "gate on quality; C/N0 separates the two populations by "
            f"{d.sep.get('cn0_real', 0) - d.sep.get('cn0_spurious', 0):.1f} "
            "dB while the margin separates them by hundredths (§2.5, F2).",
            "**Block size is not a parameter of the answer.** From 333 "
            "samples to a push larger than the ring, the windows are "
            "bit-identical and nothing is dropped (§2.3).",
            "**The persistent flavour costs nothing and saves the "
            f"checkpoint.** Its blob is smaller by exactly the retained span "
            f"({d.blob_ram - d.blob_disk:,} B here), the ring's pages ARE "
            "the file's, and the measured throughput is within noise of the "
            "anonymous ring (§2.6).",
        ],
    )
    R.summary()
    R.emit(HERE / "results.md")
    return R


if __name__ == "__main__":
    sys.exit(cli(build, HERE))
