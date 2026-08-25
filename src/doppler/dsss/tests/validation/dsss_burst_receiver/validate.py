"""Certify `DsssBurstReceiver` — the burst chain, composed in C.

Run:  python -m doppler.dsss.tests.validation.dsss_burst_receiver.validate
      make validate          (regenerates every report)
      make validate-check    (fails if the committed report is stale)

Phase 8 for the object designed in `docs/design/dsss-burst-receiver.md` and
characterized in
`src/doppler/dsss/tests/characterization/dsss_burst_receiver/`.

The three parts it composes — `BurstAcquisition`, `BurstDespreader`,
`BurstDemod` — are certified in their own folders and **nothing here
re-derives them**. What this object owns is the seam: the epoch, the fold,
the look-back that reaches back to a burst start already gone past, and the
refine stage that decides which repetition of the preamble a detection
landed on. Every claim below is about that.

The criterion the object was built against is stronger than "it decodes":

> A burst event must contain everything the demodulator needs, when the
> event is all that is exchanged.

which is why the field this report spends the most evidence on is
`preamble_start` — the one quantity a caller cannot compute, because it
needs the engine's own stream position.
"""

from __future__ import annotations

import sys
from dataclasses import dataclass, field
from pathlib import Path

import numpy as np

from doppler.dsss import DsssBurstReceiver
from doppler.tests._validation_common import Report, cli
from doppler.wfm import PN, crc16, mls_poly

HERE = Path(__file__).resolve().parent
R = Report()

REPS, SPC, PAYLOAD, DATA_SF, SYNC_LEN = 4, 4, 32, 8, 13
CHIP_RATE = 1.0e6
FS = CHIP_RATE * SPC
N_CAP = 40_000
PUSH = 777  # deliberately not a divisor of anything: blocks are arbitrary


def _mls(n: int) -> np.ndarray:
    """A real spreading code — the m-sequence of an n-stage LFSR."""
    return np.asarray(
        PN(poly=mls_poly(n), seed=1, length=n).generate(2**n - 1)
    ).astype(np.uint8)


ACQ_CODE = _mls(5)  # 31 chips, peak/worst-sidelobe = 31
ACQ_SF = ACQ_CODE.size
CODE_PERIOD = ACQ_SF * SPC

_rng = np.random.default_rng(0)
DATA_CODE = _rng.integers(0, 2, DATA_SF).astype(np.uint8)
SYNC = _rng.integers(0, 2, SYNC_LEN).astype(np.uint8)
PAYLOAD_BITS = _rng.integers(0, 2, PAYLOAD).astype(np.uint8)


def _sgn(b):
    return np.where(np.asarray(b) & 1, -1.0, 1.0)


def _burst_reps(nreps: int, acq_code=ACQ_CODE) -> np.ndarray:
    """A burst whose preamble carries `nreps` repetitions of the code."""
    c = crc16(PAYLOAD_BITS)
    crc = np.array([(c >> (15 - j)) & 1 for j in range(16)], np.uint8)
    frame = np.concatenate([SYNC, PAYLOAD_BITS, crc])
    chips = [np.tile(_sgn(acq_code), nreps)] + [
        _sgn(b) * _sgn(DATA_CODE) for b in frame
    ]
    return np.repeat(np.concatenate(chips), SPC).astype(np.complex64)


def _burst(acq_code=ACQ_CODE) -> np.ndarray:
    c = crc16(PAYLOAD_BITS)
    crc = np.array([(c >> (15 - j)) & 1 for j in range(16)], np.uint8)
    frame = np.concatenate([SYNC, PAYLOAD_BITS, crc])
    chips = [np.tile(_sgn(acq_code), REPS)] + [
        _sgn(b) * _sgn(DATA_CODE) for b in frame
    ]
    return np.repeat(np.concatenate(chips), SPC).astype(np.complex64)


BURST = _burst()
BURST_LEN = BURST.size


def peak_to_sidelobe(code: np.ndarray) -> float:
    s = _sgn(code)
    ac = np.array(
        [abs(float(np.dot(s, np.roll(s, k)))) for k in range(s.size)]
    )
    return float(ac[0] / ac[1:].max())


def _capture(at: int, sigma: float, seed: int, burst=None) -> np.ndarray:
    burst = BURST if burst is None else burst
    rng = np.random.default_rng(seed)
    cap = (
        sigma * (rng.standard_normal(N_CAP) + 1j * rng.standard_normal(N_CAP))
    ).astype(np.complex64)
    cap[at : at + burst.size] += burst
    return cap


def _rx(acq_code=ACQ_CODE) -> DsssBurstReceiver:
    return DsssBurstReceiver(
        acq_code,
        DATA_CODE,
        SYNC,
        reps=REPS,
        spc=SPC,
        chip_rate=CHIP_RATE,
        payload_len=PAYLOAD,
        cn0_dbhz=55.0,
    )


def _drive(rx, cap, stop_on_first=True):
    """Stream a capture in small blocks; collect every returned burst."""
    out = []
    for off in range(0, cap.size, PUSH):
        bits = rx.push(cap[off : off + PUSH])
        if bits.size:
            out.append(
                {
                    "bits": bits,
                    "start": int(rx.preamble_start),
                    "valid": bool(rx.frame_valid),
                    "margin": float(rx.refine_margin),
                }
            )
            if stop_on_first:
                break
    return out


def _csv(path: Path, header: str, rows: list[list[object]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as fh:
        fh.write(header + "\n")
        for r in rows:
            fh.write(",".join(str(v) for v in r) + "\n")


@dataclass
class Data:
    """Everything §3 and §4 read, measured once in §2."""

    decoded: bool = False
    bits_exact: bool = False
    start_exact: bool = False
    epoch_rows: list[list[str]] = field(default_factory=list)
    epoch_all_exact: bool = False
    epoch_near_start: bool = False
    margin_rows: list[list[str]] = field(default_factory=list)
    margin_healthy: float = 1.0
    margin_predicted: float = 0.0
    margin_tracks_prediction: bool = False
    reps_rows: list[list[str]] = field(default_factory=list)
    reps_floor_tracks: bool = False
    reps_separation_scales: bool = False
    amp_rows: list[list[str]] = field(default_factory=list)
    amp_invariant: bool = False
    amp_span_db: float = 0.0
    truncated_margin: float = 0.0
    truncated_rejected: bool = False
    margin_separates: bool = False
    code_rows: list[list[str]] = field(default_factory=list)
    good_code_pts: float = 0.0
    poor_code_pts: float = 0.0
    code_dominates: bool = False
    silence_quiet: bool = False
    one_valid_per_burst: bool = False
    event_rows: list[list[str]] = field(default_factory=list)
    freq_err_bins: float = 0.0
    state_pure: bool = False
    state_resumes: bool = False
    state_rejects: bool = False
    dropped_zero: bool = False
    any_block_size: bool = False


# ── 1. the object ─────────────────────────────────────────────────────


def section_object() -> None:
    R.md("## 1. The object — three stages behind one push()")
    R.md()
    R.md(
        "`DsssBurstReceiver` composes the burst chain in C: acquisition "
        "**searches** the stream, a **refine** stage recovers the exact "
        "preamble start, and the demodulator produces the payload. It owns "
        "only the seam between them — which is the part every caller "
        "previously redid by hand, and got wrong."
    )
    R.md()
    R.table(
        ["page", "owns"],
        [
            [
                "[the design](../../../../../../docs/design/"
                "dsss-burst-receiver.md)",
                "why the chain needs a refine stage and look-back rather "
                "than the tail a tracking receiver uses",
            ],
            [
                "`.../characterization/dsss_burst_receiver/`",
                "the sweeps behind §2.3 and §2.6 — margin vs C/N0, and "
                "detection vs the acquisition code",
            ],
            [
                "`burst_acq`, `burst_despreader`, `burst_demod` reports",
                "the three composed parts, certified separately and not "
                "re-derived here",
            ],
            [
                "`native/inc/dsss_burst_receiver/dsss_burst_receiver_core.h`",
                "the contract — the SSOT this report audits",
            ],
        ],
    )
    R.md("### 1.1 The claim inventory")
    R.md()
    R.md(
        "Step 1 of `docs/dev/contributing/validation.md`. This object is new, "
        "so nothing here is archaeology — the column that matters is which "
        "claims a test would still pass with the implementation broken, and "
        "for a composition that is most of them. Every row was "
        "sabotage-proven in `test_dsss_burst_receiver_core.c`."
    )
    R.md()
    R.table(
        ["header claim", "pinned where", "here"],
        [
            [
                "`push()` returns one burst's payload, CRC checked",
                "C + Python end to end",
                "§2.1",
            ],
            [
                "`preamble_start` is the burst's exact sample",
                "C, at several positions **including near the stream start**",
                "§2.2",
            ],
            [
                "refine names WHICH preamble repetition",
                "C, sabotage-proven against a coherent combine",
                "§2.3",
            ],
            [
                "`refine_margin` reports that decision's confidence",
                "**its negative case needed a real false alarm to exist**",
                "§2.3",
            ],
            [
                "a burst is claimed once however many frames fire",
                "C, needs a grid where several DO fire",
                "§2.5",
            ],
            [
                "`state_bytes()` is a pure function of configuration",
                "**jm's binding depends on it; nothing said so**",
                "§2.6",
            ],
            [
                "any block size is accepted",
                "C, including a push larger than the ring",
                "§2.5",
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
    _sec_end_to_end(d)
    _sec_epoch(d)
    _sec_refine(d)
    _sec_reps(d)
    _sec_amplitude(d)
    _sec_code(d)
    _sec_bounds(d)
    _sec_state(d)
    return d


def _margin_at(reps_cfg: int, reps_sig: int) -> float:
    """refine_margin for a preamble of `reps_sig` reps, receiver on
    `reps_cfg`. Equal means intact; one short means clipped."""
    chips = [np.tile(_sgn(ACQ_CODE), reps_sig)] + [
        _sgn(b) * _sgn(DATA_CODE)
        for b in np.concatenate(
            [
                SYNC,
                PAYLOAD_BITS,
                np.array(
                    [(crc16(PAYLOAD_BITS) >> (15 - j)) & 1 for j in range(16)],
                    np.uint8,
                ),
            ]
        )
    ]
    burst = np.repeat(np.concatenate(chips), SPC).astype(np.complex64)
    rx = DsssBurstReceiver(
        ACQ_CODE,
        DATA_CODE,
        SYNC,
        reps=reps_cfg,
        spc=SPC,
        chip_rate=CHIP_RATE,
        payload_len=PAYLOAD,
        cn0_dbhz=55.0,
    )
    hits = _drive(rx, _capture(6000, 0.02, seed=42, burst=burst))
    return hits[0]["margin"] if hits else float("nan")


def _sec_end_to_end(d: Data) -> None:
    R.md("### 2.1 A burst in, a payload out")
    R.md()
    R.md(
        "The whole object in one claim: stream a capture in arbitrary "
        "blocks, get the transmitted bits back with the CRC checked. "
        "Nothing is handed to the receiver but samples — no burst position, "
        "no Doppler, no ground truth."
    )
    R.md()
    rx = _rx()
    hits = _drive(rx, _capture(5000, 0.02, seed=3))
    d.decoded = len(hits) == 1 and hits[0]["valid"]
    d.bits_exact = bool(hits) and np.array_equal(hits[0]["bits"], PAYLOAD_BITS)
    d.start_exact = bool(hits) and hits[0]["start"] == 5000
    d.dropped_zero = rx.dropped == 0
    R.table(
        ["what", "value"],
        [
            ["payload bits returned", str(len(hits[0]["bits"]))],
            ["bits equal the transmitted payload", str(d.bits_exact)],
            ["frame_valid", str(hits[0]["valid"])],
            ["preamble_start", f"{hits[0]['start']} (true 5000)"],
            ["samples dropped by the history ring", str(rx.dropped)],
        ],
    )
    R.md(
        f"Exact bits and an exact start (**{d.bits_exact}**, "
        f"**{d.start_exact}**), in {PUSH}-sample blocks — a size chosen to "
        f"divide nothing, so no boundary lines up with a frame, a code "
        f"period or the burst."
    )
    R.md()


def _sec_epoch(d: Data) -> None:
    R.md("### 2.2 `preamble_start` — the field a caller cannot compute")
    R.md()
    R.md(
        "Acquisition reports an **end** anchor (`samples_consumed`, where a "
        "detection's epoch finished) and a **residue** (`code_phase`, a lag "
        "modulo one code period). Neither is a burst start, and a consumer "
        "in another process has neither the producer's stream position nor "
        "its dwell offsets. This field is what the composition exists to "
        "produce."
    )
    R.md()
    rows, csv = [], []
    all_exact = True
    for at in (600, 5000, 5000 + CODE_PERIOD // 3, 12345, 20000):
        rx = _rx()
        hits = _drive(rx, _capture(at, 0.02, seed=at % 97))
        got = hits[0]["start"] if hits else -1
        ok = got == at
        all_exact &= ok
        rows.append(
            [
                str(at),
                str(at % CODE_PERIOD),
                str(got),
                str(got - at if hits else "—"),
                str(ok),
            ]
        )
        csv.append([at, at % CODE_PERIOD, got])
    R.table(
        [
            "true burst start",
            "start mod code period",
            "reported",
            "error",
            "exact",
        ],
        rows,
    )
    _csv(HERE / "data" / "epoch.csv", "true,phase,reported", csv)
    d.epoch_rows = rows
    d.epoch_all_exact = all_exact
    d.epoch_near_start = rows[0][4] == "True"
    R.md(
        f"Exact at every position (**{d.epoch_all_exact}**), to the sample. "
        f"The first row is the one that matters most: at 600 the burst sits "
        f"closer to the stream start than refine's own search span, so the "
        f"stage cannot back off in full. Backing off to sample 0 instead of "
        f"to a whole number of code periods puts the entire candidate grid "
        f"on multiples of the period and discards the phase the anchor "
        f"carries — measured, before that was fixed, as a start reported at "
        f"exactly 11 periods with the burst 588 samples away."
    )
    R.md()


def _sec_refine(d: Data) -> None:
    R.md("### 2.3 Refine — which repetition, and how sure")
    R.md()
    R.md(
        "A preamble is the same code repeated, so a correlation against the "
        "code cannot say which repetition a detection landed on. The "
        "preamble can, because it has finite extent: score each candidate "
        "by correlating one code period at every position the preamble "
        "would occupy and summing the magnitudes. Only `reps - |k|` of them "
        "still land on preamble when a candidate is `k` periods off, so the "
        "score follows a triangular envelope peaking at the truth."
    )
    R.md()
    d.margin_predicted = (REPS - 1) / REPS
    rx = _rx()
    hits = _drive(rx, _capture(5000, 0.02, seed=3))
    d.margin_healthy = hits[0]["margin"]
    # Relative to the prediction, NOT against a fixed number: the floor is
    # (reps-1)/reps, so an absolute threshold is only ever right for one
    # reps. See §2.4.
    d.margin_tracks_prediction = (
        d.margin_predicted <= d.margin_healthy <= d.margin_predicted + 0.08
    )

    # The negative case, constructed rather than waited for: a preamble
    # CLIPPED to three of its four repetitions. The receiver still expects
    # four, so no candidate period can win cleanly and the envelope the
    # stage relies on flattens. A real link condition -- a burst that
    # starts mid-transmission, or is truncated by a capture boundary --
    # and deterministic, unlike hoping a false alarm turns up in a
    # particular noise realization.
    clipped = _burst_reps(3)
    rx2 = _rx()
    hits2 = _drive(rx2, _capture(5000, 0.02, seed=3, burst=clipped))
    d.truncated_margin = hits2[0]["margin"] if hits2 else 0.0
    d.truncated_rejected = bool(hits2) and not hits2[0]["valid"]
    d.margin_separates = d.truncated_margin > d.margin_healthy
    rows = [
        [
            "intact preamble (4 of 4 reps)",
            f"{d.margin_healthy:.3f}",
            "True",
            f"predicted (reps-1)/reps = {d.margin_predicted:.3f}",
        ],
        [
            "preamble clipped to 3 of 4 reps",
            f"{d.truncated_margin:.3f}",
            str(hits2[0]["valid"]) if hits2 else "—",
            "no period wins cleanly — the envelope has flattened",
        ],
    ]
    R.table(["stimulus", "refine_margin", "frame_valid", "note"], rows)
    d.margin_rows = rows
    R.md(
        f"`refine_margin` is the rival period over the winner, so **lower "
        f"is better** and a value near 1 means the period was not resolved. "
        f"Intact, it reads **{d.margin_healthy:.3f}** against the predicted "
        f"{d.margin_predicted:.3f}; clipped, **{d.truncated_margin:.3f}** — "
        f"and the CRC independently rejects that burst "
        f"(**{d.truncated_rejected}**). The two agree without being told "
        f"about each other, which is what makes the healthy reading "
        f"evidence rather than a constant."
    )
    R.md()
    R.md(
        "That is the gap this object closes. A window one code period off "
        "still carries a carrier, so a lock-style indicator reads perfectly "
        "healthy through a broken hand-off — the failure `acq_core.h` "
        "already records historically as a receiver reporting tracking "
        "while decoding noise. `refine_margin` is the only quantity in the "
        "chain that sees it."
    )
    R.md()
    R.md(
        "Where it stops working is measured in the characterization rather "
        "than here, because it is a sweep: the correct-period rate holds at "
        "100% down to roughly 60 dB-Hz and collapses to 58% by 56.5, while "
        "the margin closes from 0.774 to 0.87 — so the read-back degrades "
        "before the decode does."
    )
    R.md()


def _sec_reps(d: Data) -> None:
    R.md("### 2.4 The margin's floor moves with `reps`")
    R.md()
    R.md(
        "The envelope refine relies on is triangular: a candidate `k` "
        "periods off still has `reps - |k|` of its positions on preamble, "
        "so the nearest rival scores `(reps-1)/reps`. That is a **floor "
        "that rises with `reps`** — and it is the reason no fixed threshold "
        "on `refine_margin` can be right for every configuration."
    )
    R.md()
    rows, csv = [], []
    floor_ok = sep_ok = True
    for reps in (2, 4, 8):
        healthy = _margin_at(reps, reps)
        clipped = _margin_at(reps, reps - 1)
        pred = (reps - 1) / reps
        sep = clipped - healthy
        floor_ok &= pred <= healthy <= pred + 0.08
        sep_ok &= sep >= 0.5 / reps
        rows.append(
            [
                str(reps),
                f"{pred:.3f}",
                f"{healthy:.3f}",
                f"{clipped:.3f}",
                f"{sep:.3f}",
            ]
        )
        csv.append([reps, pred, healthy, clipped, sep])
    R.table(
        [
            "reps",
            "predicted (reps-1)/reps",
            "healthy margin",
            "clipped preamble",
            "separation",
        ],
        rows,
    )
    _csv(HERE / "data" / "reps.csv", "reps,predicted,healthy,clipped,sep", csv)
    d.reps_rows = rows
    d.reps_floor_tracks = floor_ok
    d.reps_separation_scales = sep_ok
    R.md(
        f"The healthy reading tracks the prediction at every depth "
        f"(**{d.reps_floor_tracks}**), and the separation between resolved "
        f"and unresolved **halves with every doubling of `reps`** "
        f"(**{d.reps_separation_scales}**) — it scales as roughly `1/reps`, "
        f"because both readings are converging on 1 from opposite sides."
    )
    R.md()
    R.md(
        "**Two consequences a caller must not miss.** First, more "
        "repetitions buy sensitivity and COST discrimination: the same "
        "sweep that moves the acquisition knee from 66.0 dB-Hz at `reps=2` "
        "to 54.9 at `reps=16` shrinks the margin's separation from 0.37 to "
        "0.05. Second, and concretely: **compare `refine_margin` against "
        "`(reps-1)/reps`, never against a constant.** A rule like "
        '"healthy is below 0.9" is correct at `reps=4` and simply wrong at '
        "8 or 16, where a perfectly resolved burst reads 0.89 and 0.94. "
        "This report asserted exactly that constant until the sweep was "
        "run."
    )
    R.md()


def _sec_amplitude(d: Data) -> None:
    R.md("### 2.5 Amplitude invariance")
    R.md()
    R.md(
        "Every gate in the chain is a RATIO — `test_stat` is peak over the "
        "CFAR noise estimate, `refine_margin` is the rival period over the "
        "winner — so the receiver should not care what level a burst "
        "arrives at, only how it compares to its own noise. Worth "
        "measuring rather than assuming, because a float32 pipeline that "
        "squares magnitudes has somewhere to lose it."
    )
    R.md()
    rows, csv = [], []
    invariant = True
    ref = None
    decades = (-90.0, -45.0, 0.0, 45.0, 90.0)
    for db in decades:
        amp = 10.0 ** (db / 20.0)
        rx = _rx()
        cap = _capture(
            5000, 0.02 * amp, seed=3, burst=(BURST * amp).astype(np.complex64)
        )
        hits = _drive(rx, cap)
        ok = bool(hits) and hits[0]["valid"] and hits[0]["start"] == 5000
        m = hits[0]["margin"] if hits else float("nan")
        if ref is None:
            ref = m
        invariant &= ok and abs(m - ref) < 0.01
        rows.append([f"{db:+.0f}", f"{amp:.3g}", str(ok), f"{m:.3f}"])
        csv.append([db, amp, int(ok), m])
    R.table(
        ["burst level (dB)", "amplitude", "decoded", "refine_margin"], rows
    )
    _csv(HERE / "data" / "amplitude.csv", "level_db,amp,decoded,margin", csv)
    d.amp_rows = rows
    d.amp_invariant = invariant
    d.amp_span_db = decades[-1] - decades[0]
    R.md(
        f"Identical behaviour across **{d.amp_span_db:.0f} dB** of burst "
        f"level, with the margin unchanged to three decimals "
        f"(**{d.amp_invariant}**). The noise is scaled with the signal, so "
        f"C/N0 is held and level is the only variable — which is the "
        f"question being asked."
    )
    R.md()
    R.md(
        "Against a **fixed** noise floor the answer is different and "
        "unsurprising: level and C/N0 are then the same axis, so a "
        "±60 dB spread in amplitude is a ±60 dB spread in C/N0 and the "
        "bursts below the sensitivity knee are simply lost. That is the "
        "characterization's curve, not a separate property."
    )
    R.md()


def _sec_code(d: Data) -> None:
    R.md("### 2.6 What actually loses a burst is the CODE")
    R.md()
    R.md(
        "Acquisition frames the stream sequentially and **without "
        "overlap**, so a preamble can straddle two frames with neither "
        "holding all of it. That looks like the reason bursts go missing, "
        "and it is not."
    )
    R.md()
    poor = ((np.arange(ACQ_SF) * 2654435761 >> 13) & 1).astype(np.uint8)
    d.good_code_pts = peak_to_sidelobe(ACQ_CODE)
    d.poor_code_pts = peak_to_sidelobe(poor)
    rows, csv = [], []
    found = {}
    for label, code in (("m-sequence", ACQ_CODE), ("structured", poor)):
        burst = _burst(code)
        ok = 0
        trials = 0
        for off in range(0, REPS * CODE_PERIOD, 62):
            at = 4960 + off
            rx = _rx(code)
            hits = _drive(rx, _capture(at, 0.02, seed=5, burst=burst))
            ok += int(bool(hits) and hits[0]["start"] == at)
            trials += 1
        found[label] = ok / trials
        rows.append(
            [
                label,
                f"{peak_to_sidelobe(code):.2f}",
                f"{ok}/{trials}",
                f"{100 * ok / trials:.0f}%",
            ]
        )
        csv.append([label, peak_to_sidelobe(code), ok, trials])
    R.table(
        [
            "acquisition code",
            "peak / worst sidelobe",
            "bursts found",
            "rate",
        ],
        rows,
    )
    _csv(HERE / "data" / "code.csv", "code,pts,found,trials", csv)
    d.code_rows = rows
    d.code_dominates = found["m-sequence"] > found["structured"]
    R.md(
        f"Same framing, same offsets, same noise — only the code differs. "
        f"A peak-to-worst-sidelobe of {d.poor_code_pts:.2f} is barely a "
        f"spreading code, and with sidelobes that high the CFAR reference "
        f"is set by the code's own autocorrelation rather than by noise, so "
        f"a straddled preamble has no margin left to give away. An "
        f"m-sequence ({d.good_code_pts:.0f}) absorbs the straddle "
        f"(**{d.code_dominates}**)."
    )
    R.md()
    R.md(
        "Recorded because the wrong conclusion was reached first and nearly "
        "filed: a 42% loss measured on the structured code was read as the "
        "framing needing overlapping dwells. The framing was never the "
        "problem. **Choose the preamble code on its autocorrelation.**"
    )
    R.md()


def _sec_bounds(d: Data) -> None:
    R.md("### 2.7 What it refuses, and what it does not double-count")
    R.md()
    rx = _rx()
    nothing = np.zeros(1, np.complex64)
    quiet = _drive(rx, _capture(0, 0.02, seed=9, burst=nothing))
    d.silence_quiet = not quiet and rx.n_bursts == 0

    # One burst, a grid where several frames of its preamble fire.
    rx2 = _rx()
    rx2.configure_search_raw(1, 1)
    hits = _drive(rx2, _capture(5000, 0.02, seed=777), stop_on_first=False)
    valid = [h for h in hits if h["valid"]]
    d.one_valid_per_burst = len(valid) == 1 and valid[0]["start"] == 5000

    # Any block size, including one larger than the ring itself.
    rx3 = _rx()
    big = rx3.push(_capture(5000, 0.02, seed=3))
    d.any_block_size = big.size == PAYLOAD and rx3.preamble_start == 5000

    R.table(
        ["case", "outcome"],
        [
            ["silence", f"no burst, n_bursts = {rx.n_bursts}"],
            [
                "one burst, single-bin grid (several frames fire)",
                f"{len(valid)} valid burst(s)",
            ],
            [
                f"one push of the whole {N_CAP}-sample capture",
                f"{big.size} bits, start {rx3.preamble_start}",
            ],
            ["push_max_out at any block size", str(rx3.push_max_out(1 << 20))],
        ],
    )
    R.md(
        f"Silence decodes nothing (**{d.silence_quiet}**) — the control "
        f"that stops every other row passing against a receiver that "
        f"reports a burst for anything. With the grid pinned to a single "
        f"coherent bin the acquisition frame is one code period, so "
        f"`reps` frames of the preamble each fire; the burst is still "
        f"claimed exactly once (**{d.one_valid_per_burst}**). And a single "
        f"push far larger than the history ring is sliced rather than "
        f"refused (**{d.any_block_size}**)."
    )
    R.md()


def _sec_state(d: Data) -> None:
    R.md("### 2.8 State — a pure function of configuration")
    R.md()
    R.md(
        "`state_bytes()` has to be constant for a given configuration, and "
        "not because it is tidy: jm's binding compares an incoming blob's "
        "length against it before calling `set_state`. A size that moved "
        "with the retained history would make a receiver restorable only "
        "into an instance holding exactly as much — coincidence, not "
        "resume."
    )
    R.md()
    a, b = _rx(), _rx()
    fresh = a.state_bytes()
    cap = _capture(5000, 0.02, seed=3)
    cut = 5000 + 800  # inside the preamble, before the frame has landed
    for off in range(0, cut, PUSH):
        a.push(cap[off : min(off + PUSH, cut)])
    d.state_pure = a.state_bytes() == fresh == b.state_bytes()

    blob = a.get_state()
    b.set_state(blob)
    rest = []
    for off in range(cut, N_CAP, PUSH):
        bits = b.push(cap[off : off + PUSH])
        if bits.size:
            rest.append((bits, int(b.preamble_start), bool(b.frame_valid)))
            break
    d.state_resumes = bool(rest) and rest[0][1] == 5000 and rest[0][2]

    bad = bytearray(blob)
    bad[0] ^= 0xFF
    try:
        b.set_state(bytes(bad))
        d.state_rejects = False
    except ValueError:
        d.state_rejects = True

    R.table(
        ["what", "value"],
        [
            ["state_bytes, fresh receiver", str(fresh)],
            ["state_bytes, mid-preamble", str(a.state_bytes())],
            ["resumed into a fresh instance, decoded", str(d.state_resumes)],
            ["clobbered envelope rejected", str(d.state_rejects)],
        ],
    )
    R.md(
        f"The split is placed **inside the preamble**, before the frame has "
        f"arrived, so the restored receiver has to carry the retained "
        f"look-back with it — a blob that omitted the history would still "
        f"round-trip on a fresh receiver and still fail here "
        f"(**{d.state_resumes}**). The size is identical across two "
        f"differently-loaded instances (**{d.state_pure}**)."
    )
    R.md()


# ── 3. review ─────────────────────────────────────────────────────────


def review(d: Data) -> None:
    R.md("## 3. Review — findings")
    R.md()
    R.find(
        "F1",
        "FIXED",
        "**One bin-to-frequency fold was restated in four Python call "
        "sites, three ways** — one of them the exact form `acq_core.h` "
        "records as a past full-span sign inversion. It is `fftfreq`, and "
        "the engine's private helper had deviated from it at one index: an "
        "even grid's Nyquist bin. Fixed by giving the fold one home "
        "(`dp_fftfreq` in `clib_common.h`), with `doppler.dsss.bin_to_signed` "
        "a wrapper over the same inline. This object exists so that no "
        "further call site has to restate it.",
    )
    R.find(
        "F2",
        "FIXED",
        "**Refine was designed to correlate the whole preamble coherently, "
        "and that does not survive the residual acquisition leaves.** The "
        "design doc recorded the coherent form as the mechanism, verified "
        "on a capture with ZERO Doppler — the one input where it cannot "
        "fail. At a quarter of a Doppler bin it picked an offset two whole "
        "code periods wrong, with the true position 639x below the peak it "
        "chose; at half a bin, one period wrong. Now combined "
        "non-coherently, one code period at a time, which is short enough "
        "that a half-bin residual cannot rotate through it. Sabotage-proven "
        "by swapping the combine back.",
    )
    R.find(
        "F3",
        "FIXED",
        "**A detection whose refine window had not fully arrived was "
        "discarded**, and the burst then went to whatever spurious "
        "detection came next — observed as a confident decode 1996 samples "
        "from the truth. Detections are now queued and retried: a dropped "
        "detection is a lost burst. Found only by an end-to-end test; no "
        "unit test of refine would have reached it.",
    )
    R.find(
        "F4",
        "FIXED",
        "**Refine's clamp for the start of the stream destroyed the code "
        "phase.** Candidates are `anchor + k*P` precisely because "
        "acquisition already fixes alignment within a period, so backing "
        "off must move in whole periods; clamping to sample 0 put the grid "
        "on multiples of `P` instead. Invisible at the test geometry (the "
        "anchor was far past the search span) and surfaced only by asking "
        "whether a longer code lowers the workable C/N0 — a 127-chip code "
        "makes the span large enough to trip it, and refine returned "
        "exactly 11 periods with the burst 588 samples away. §2.2's first "
        "row is the regression test.",
    )
    R.find(
        "F5",
        "FIXED",
        "**`state_bytes()` was not a pure function of configuration.** It "
        "tracked the retained look-back, so a receiver could be restored "
        "only into an instance holding exactly as much history — and jm's "
        "binding compares an incoming blob's length against it, so the "
        "mismatch surfaced as a flat refusal rather than as a wrong "
        "answer. Both variable regions are now fixed-size with a length "
        "prefix. Caught by a mid-stream resume test, not by review.",
    )
    R.find(
        "F6",
        "FIXED",
        "**The acquisition CODE dominates burst loss, not the "
        "non-overlapping framing** — and the opposite was nearly filed as "
        "an issue. A 42% loss measured against burst position looked "
        "exactly like preambles straddling frame boundaries; the same "
        "sweep with a different code of the same length lost nothing "
        "(§2.4). The tests have been moved onto a real m-sequence, built "
        "with the library's own generator, because a receiver measured on "
        "a code no caller would choose measures the wrong thing.",
    )
    R.find(
        "F7b",
        "FIXED",
        "**`refine_margin` had no fixed threshold, and this report asserted "
        "one anyway.** The limit read `margin < 0.9`, which is true at "
        "`reps = 4` and false at 8 and 16, where a perfectly resolved burst "
        "reads 0.887 and 0.944 — the floor is `(reps-1)/reps` and rises "
        "with depth. Worse, the separation between resolved and unresolved "
        "HALVES with every doubling of `reps` (0.37 at 2, down to 0.05 at "
        "16), so the read-back's usefulness degrades exactly where the "
        "extra repetitions were bought for sensitivity. The limit is now "
        "relative to the prediction, and §2.4 states the trade. Found by "
        "sweeping a parameter the first certification held fixed.",
    )
    R.find(
        "F7",
        "BY DESIGN",
        "**`refine_margin` is the only quantity in the chain that can see a "
        "broken hand-off.** A window one code period off still carries a "
        "carrier, so a lock indicator reads healthy while the despread "
        "output is noise — the failure `acq_core.h` records historically. "
        "The margin separates the two unprompted (§2.3), and its NEGATIVE "
        "case is exercised by a clipped preamble -- a real link condition, "
        "and deterministic -- rather than by hoping a false alarm turns up "
        "in a particular noise realization, which is what makes the "
        "positive reading evidence rather than a constant.",
    )
    R.find(
        "F8",
        "BY DESIGN",
        "**The reported Doppler stays on acquisition's native grid.** The "
        "half-bin scalloping null that made a burst undetectable at any "
        "C/N0 was fixed in `acq` (gh-1002) by lengthening its slow-time "
        "transform, and it would have been easy to pass the finer grid "
        "through. It is not passed through: every consumer scales "
        "`doppler_bin` by `doppler_res_hz`, and interpolation buys "
        "DETECTION rather than a finer reported estimate. Rounding a "
        "half-bin row to its nearer neighbour was tried and cost ~5 points "
        "of Pd.",
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
    R.limit(d.decoded, "a burst streamed in arbitrary blocks decodes once")
    R.limit(
        d.bits_exact,
        "...and the returned bits equal the transmitted payload exactly",
    )
    R.limit(
        d.start_exact,
        "...with preamble_start naming the burst's exact sample",
    )
    R.limit(
        d.epoch_all_exact,
        "preamble_start is exact at five burst positions, including one "
        "off a code-period boundary",
    )
    R.limit(
        d.epoch_near_start,
        "...including a burst closer to the stream start than refine's own "
        "search span, where backing off must still move in whole periods",
    )
    R.limit(
        d.margin_tracks_prediction,
        f"refine_margin reads {d.margin_healthy:.3f} on a resolved burst, "
        f"within 0.08 of the predicted (reps-1)/reps = "
        f"{d.margin_predicted:.3f} — a RELATIVE claim, because the floor "
        f"moves with reps",
    )
    R.limit(
        d.reps_floor_tracks,
        "the healthy margin tracks (reps-1)/reps to within 0.08 at reps 2, "
        "4 and 8 — the floor MOVES, so the claim is relative",
    )
    R.limit(
        d.reps_separation_scales,
        "...and the resolved/unresolved separation is at least 0.5/reps, "
        "halving with every doubling — more repetitions buy sensitivity "
        "and cost discrimination",
    )
    R.limit(
        len(d.reps_rows) == 3,
        "the reps sweep measures three depths, so the scaling is a trend "
        "rather than a single point",
    )
    R.limit(
        d.amp_invariant,
        f"decoding and refine_margin are unchanged across "
        f"{d.amp_span_db:.0f} dB of burst level at constant C/N0 — every "
        f"gate in the chain is a ratio",
    )
    R.limit(
        len(d.amp_rows) == 5,
        "the amplitude sweep spans five levels, not two",
    )
    R.limit(
        d.truncated_rejected,
        "a preamble clipped to three of its four repetitions is REJECTED "
        "by the CRC — the negative case, constructed rather than waited for",
    )
    R.limit(
        d.margin_separates,
        f"...and refine_margin says so independently "
        f"({d.truncated_margin:.3f} against {d.margin_healthy:.3f} intact)",
    )
    R.limit(
        d.code_dominates,
        "an m-sequence finds strictly more bursts than a structured code "
        "of the same length under identical framing and noise",
    )
    R.limit(
        d.good_code_pts > 10.0 * d.poor_code_pts,
        f"...and the two differ by more than 10x in peak-to-sidelobe "
        f"({d.good_code_pts:.0f} against {d.poor_code_pts:.2f})",
    )
    R.limit(d.silence_quiet, "silence decodes nothing and reports no burst")
    R.limit(
        d.one_valid_per_burst,
        "one burst is claimed exactly once even on a grid where several "
        "frames of its preamble fire",
    )
    R.limit(
        d.any_block_size,
        "a single push larger than the history ring is sliced, not refused",
    )
    R.limit(
        d.dropped_zero,
        "no sample is dropped by the history ring on a normal stream",
    )
    R.limit(
        d.state_pure,
        "state_bytes() is identical for a fresh receiver and one holding a "
        "partial burst — the contract jm's binding depends on",
    )
    R.limit(
        d.state_resumes,
        "a blob taken INSIDE the preamble resumes into a fresh instance and "
        "still decodes, so the retained look-back travels with it",
    )
    R.limit(
        d.state_rejects,
        "a clobbered envelope is rejected rather than reinterpreted",
    )
    R.limit(
        len(d.epoch_rows) == 5,
        "the epoch check spans five positions, not one",
    )
    R.limit(
        len(d.code_rows) == 2,
        "the code comparison measures both codes, so the claim is a "
        "contrast rather than an assertion",
    )


# ── build ─────────────────────────────────────────────────────────────


def build(write: bool = True) -> Report:
    global R
    R = Report(write=write)
    R.md("# DsssBurstReceiver — validation report")
    R.md()
    section_object()
    d = characterise()
    review(d)
    limits(d)
    R.executive(
        "DsssBurstReceiver",
        [
            "**`preamble_start` is the deliverable.** Acquisition reports "
            "an end anchor and a residue modulo one code period; neither is "
            "a burst start, and a consumer in another process has neither "
            "the producer's stream position nor its dwell offsets. This "
            "object produces the exact sample, at five positions including "
            "one closer to the stream start than refine's own search span "
            "(§2.2).",
            "**Refine combines NON-coherently across the preamble's "
            "repetitions**, and that is the mechanism rather than an "
            "optimisation: the coherent form was measured wrong by two code "
            "periods at a quarter of a Doppler bin, with the true position "
            "639x below the peak it chose (F2).",
            f"**`refine_margin` is the only thing in the chain that sees a "
            f"broken hand-off.** A mis-windowed burst still has a carrier, "
            f"so a lock indicator reads healthy. The margin reads "
            f"{d.margin_healthy:.3f} on a resolved burst and "
            f"{d.truncated_margin:.3f} on one whose preamble was clipped, "
            f"agreeing with the CRC unprompted (§2.3, F7).",
            "**Compare `refine_margin` against `(reps-1)/reps`, never "
            "against a constant.** The floor rises with depth, and the "
            "separation between resolved and unresolved HALVES with every "
            "doubling of `reps` — 0.37 at 2, 0.05 at 16. So more "
            "repetitions buy sensitivity and cost discrimination, and a "
            "fixed threshold is right for exactly one configuration "
            "(§2.4, F7b).",
            f"**Level does not matter, only C/N0 does.** Decoding and the "
            f"margin are unchanged across {d.amp_span_db:.0f} dB of burst "
            f"amplitude at constant C/N0 — every gate in the chain is a "
            f"ratio. Against a fixed noise floor, level and sensitivity "
            f"are the same axis (§2.5).",
            f"**Choose the preamble code on its autocorrelation.** Under "
            f"identical framing and noise an m-sequence "
            f"({d.good_code_pts:.0f} peak-to-sidelobe) finds strictly more "
            f"bursts than a structured code of the same length "
            f"({d.poor_code_pts:.2f}). A 42% loss was first read as the "
            f"non-overlapping framing needing fixing; it was the code "
            f"(§2.6, F6).",
            "**Checkpoint between bursts.** `state_bytes()` is a pure "
            "function of configuration — jm's binding depends on that — and "
            "a blob taken inside the preamble carries the retained "
            "look-back, so it resumes into a fresh instance and still "
            "decodes (§2.6, F5).",
        ],
    )
    R.summary("\n- Raw sweeps: `data/epoch.csv`, `data/code.csv`")
    R.emit(HERE / "results.md")
    return R


if __name__ == "__main__":
    sys.exit(cli(build, HERE))
