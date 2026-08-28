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


FRAME_SYMS = len(SYNC) + PAYLOAD + 16  # sync | payload | CRC-16
PAYLOAD_OFF = len(SYNC)


def _payload_of(frame):
    """The payload's slice out of a returned frame."""
    return np.asarray(frame)[PAYLOAD_OFF : PAYLOAD_OFF + PAYLOAD]


def _frame_ok(frame) -> bool:
    """Does the frame's own trailer match its own payload?

    The receiver stops at decisions (doppler#1022) — it returns the frame's
    bits and holds no description, so the verdict this report needs is
    computed here, exactly as `wfm.Frame.deframe()` computes it for a caller
    that holds one.
    """
    from doppler.wfm import crc16

    frame = np.asarray(frame)
    if frame.size < FRAME_SYMS:
        return False
    rx = 0
    for b in frame[PAYLOAD_OFF + PAYLOAD :][:16]:
        rx = (rx << 1) | (int(b) & 1)
    return rx == int(crc16(_payload_of(frame)))


def _rx(acq_code=ACQ_CODE) -> DsssBurstReceiver:
    return DsssBurstReceiver(
        acq_code,
        DATA_CODE,
        SYNC,
        reps=REPS,
        spc=SPC,
        chip_rate=CHIP_RATE,
        frame_syms=FRAME_SYMS,
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
                    "valid": _frame_ok(bits),
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
    pd_rows: list[list[str]] = field(default_factory=list)
    pd_high: float = 0.0
    pd_low: float = 1.0
    pd_mid: float = 0.0
    pd_monotone: bool = False
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
    decoy_rows: list[list[str]] = field(default_factory=list)
    decoy_all_survive: bool = False
    decoy_detected_alone: bool = False
    block_rows: list[list[str]] = field(default_factory=list)
    block_invariant: bool = False
    block_multi_in_one: bool = False
    block_no_drops: bool = False


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
                "the sweeps behind §2.3 and §2.7 — margin vs C/N0, and "
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
                "the fraction of bursts it FINDS, at a given C/N0",
                "**nothing — no report in this chain gated a Pd**",
                "§2.6",
            ],
            [
                "a burst is claimed once however many frames fire",
                "C, needs a grid where several DO fire",
                "§2.8",
            ],
            [
                "`state_bytes()` is a pure function of configuration",
                "**jm's binding depends on it; nothing said so**",
                "§2.9",
            ],
            [
                "any block size is accepted",
                "C, including a push larger than the ring",
                "§2.8",
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
    _sec_pd(d)
    _sec_code(d)
    _sec_bounds(d)
    _sec_state(d)
    _sec_decoy(d)
    _sec_blocks(d)
    return d


def _cn0(sigma: float) -> float:
    """C/N0 of a unit-amplitude burst in complex noise of std `sigma`.

    The same relationship the acquisition engine sizes itself with, so the
    axis is comparable to a `cn0_dbhz` a caller would pass in.
    """
    return 10.0 * np.log10(1.0 / sigma**2) + 10.0 * np.log10(FS)


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
        frame_syms=FRAME_SYMS,
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
    d.bits_exact = bool(hits) and np.array_equal(
        _payload_of(hits[0]["bits"]), PAYLOAD_BITS
    )
    d.start_exact = bool(hits) and hits[0]["start"] == 5000
    d.dropped_zero = rx.dropped == 0
    R.table(
        ["what", "value"],
        [
            ["payload bits returned", str(len(hits[0]["bits"]))],
            ["bits equal the transmitted payload", str(d.bits_exact)],
            ["frame checks out", str(hits[0]["valid"])],
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
    R.table(["stimulus", "refine_margin", "frame checks out", "note"], rows)
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


def _sec_pd(d: Data) -> None:
    R.md("### 2.6 Detection probability — the fraction it actually finds")
    R.md()
    R.md(
        "Everything above asks whether the receiver handles a burst "
        "CORRECTLY. This asks how often it gets one at all, which is the "
        "number a link is sized with — and until now no report in this "
        "chain gated one. `acq`'s certification pins `underpowered`, the "
        "engine's own *prediction*; `Acquisition` has a measured Pd gate in "
        "`test_acq_characterization.py`; the burst path had neither."
    )
    R.md()
    R.md(
        "This is the **end-to-end** figure and it is stricter than "
        "acquisition's: a trial counts only when the burst is found at its "
        "exact sample AND the CRC passes. It therefore folds in "
        "acquisition's own Pd, refine's period discrimination, and the "
        "demodulator's margin — one number for the whole chain, which is "
        "what a caller has."
    )
    R.md()
    rows, csv = [], []
    trials = 24
    for sigma in (1.4, 2.2, 4.0):
        good = 0
        for k in range(trials):
            rx = _rx()
            at = 6000 + (k * 37) % CODE_PERIOD  # spread across a period
            hits = _drive(rx, _capture(at, sigma, seed=8000 + k))
            good += int(
                bool(hits) and hits[0]["valid"] and hits[0]["start"] == at
            )
        pd = good / trials
        rows.append([f"{_cn0(sigma):.1f}", f"{good}/{trials}", f"{pd:.2f}"])
        csv.append([_cn0(sigma), good, trials, pd])
    R.table(["C/N0 (dB-Hz)", "found and decoded", "Pd"], rows)
    _csv(HERE / "data" / "pd.csv", "cn0_dbhz,good,trials,pd", csv)
    d.pd_rows = rows
    d.pd_high = float(rows[0][2])
    d.pd_mid = float(rows[1][2])
    d.pd_low = float(rows[2][2])
    d.pd_monotone = d.pd_high >= d.pd_mid >= d.pd_low
    R.md(
        f"Saturated at the top (**{d.pd_high:.2f}**), on the knee in the "
        f"middle (**{d.pd_mid:.2f}**), and floored at the bottom "
        f"(**{d.pd_low:.2f}**). The floor is the half that makes the "
        f"saturation meaningful: a receiver that reported a burst for "
        f"anything would score 1.00 at every level, and only the bottom row "
        f"can catch it."
    )
    R.md()
    R.md(
        "**These numbers belong to this geometry**, not to the object: they "
        "move with the code length, `reps`, `spc` and the CFAR sizing. "
        "§2.4 shows the knee shifting 11 dB across `reps` alone. What is "
        "certified here is the SHAPE — saturate, transition, floor — and "
        "that the transition sits where the characterization's sweep puts "
        "it."
    )
    R.md()


def _sec_code(d: Data) -> None:
    R.md("### 2.7 What actually loses a burst is the CODE")
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
    R.md("### 2.8 What it refuses, and what it does not double-count")
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
    d.any_block_size = big.size == FRAME_SYMS and rx3.preamble_start == 5000

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
    R.md("### 2.9 State — a pure function of configuration")
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
            rest.append((bits, int(b.preamble_start), _frame_ok(bits)))
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


def _sec_decoy(d: Data) -> None:
    """A weaker detection arriving FIRST must not cost the burst behind it."""
    R.md("### 2.10 A spurious detection ahead of a real burst")
    R.md()
    R.md(
        "The chain-visible failure this object was certified without. "
        "Acquisition fires on more than preambles — noise crosses the CFAR "
        "gate at the priced rate, and a neighbouring burst's PAYLOAD "
        "correlates against the acquisition code too. The question is what "
        "one of those costs when it lands ahead of a real burst."
    )
    R.md()

    burst_len = (REPS * ACQ_SF + (SYNC_LEN + PAYLOAD + 16) * DATA_SF) * SPC
    at, amp = 5000, 0.35
    preamble = _burst_reps(REPS)[: REPS * CODE_PERIOD]

    # The vacuity guard: the decoy must actually CROSS the gate on its own,
    # or every row below passes against a receiver that simply ignores it.
    solo = _capture(at, 0.02, seed=41, burst=np.zeros(1, np.complex64))
    solo[at : at + preamble.size] += (amp * preamble).astype(np.complex64)
    seen = _drive(_rx(), solo, stop_on_first=False)
    d.decoy_detected_alone = len(seen) > 0 and not any(
        h["valid"] for h in seen
    )

    rows = []
    survived = []
    for gap in (400, 900, 1500, 2100):
        cap = _capture(at, 0.02, seed=41)
        lo = at - gap
        cap[lo : lo + preamble.size] += (amp * preamble).astype(np.complex64)
        hits = _drive(_rx(), cap, stop_on_first=False)
        good = [h for h in hits if h["valid"] and h["start"] == at]
        survived.append(len(good) == 1)
        rows.append(
            [
                str(gap),
                f"{gap / burst_len:.2f}",
                "yes" if good else "NO",
                f"{good[0]['margin']:.3f}" if good else "--",
            ]
        )
    d.decoy_rows = rows
    d.decoy_all_survive = all(survived)

    R.table(
        ["decoy lead (samples)", "of a burst", "burst decoded", "margin"],
        rows,
    )
    R.md()
    R.md(
        f"A bare preamble at **{amp:g}** amplitude — no payload, so it "
        f"cannot pass a CRC — placed ahead of a real burst. On its own it "
        f"crosses the gate and is reported, invalid (**"
        f"{d.decoy_detected_alone}**), which is what stops the rows above "
        f"passing vacuously. Ahead of a real burst it costs nothing: the "
        f"burst decodes at its exact sample at every lead "
        f"(**{d.decoy_all_survive}**)."
    )
    R.md()
    R.md(
        "It used to cost the burst entirely. The rule armed a "
        "`burst_len`-long suppression window on EVERY detection, at "
        "detection time, and took the first hit in it — so any spurious "
        "crossing blinded the search for a whole burst. Two jobs were "
        "conflated: *is this the same preamble* (identity, answered by "
        "`refine_span` proximity and settled by keeping the stronger peak) "
        "and *is this a burst's own payload* (answerable only once a burst "
        "has DECODED, which is now the only thing that arms the long "
        "window). See F9 and doppler#1004."
    )
    R.md()


def _sec_blocks(d: Data) -> None:
    """The block size is the caller's choice and must not change the answer."""
    R.md("### 2.11 Several bursts, and the caller's block size")
    R.md()
    R.md(
        "Every other section here streams ONE burst. That is the shape of "
        "every test this object had, and it is why three separate "
        "input-discarding defects survived certification: with a single "
        "burst, everything `push()` threw away was noise, so nothing "
        "observable was lost. This section puts three bursts in one capture "
        "and varies only how the caller hands them over."
    )
    R.md()

    burst_len = BURST.size
    gap = 3 * CODE_PERIOD
    starts = [5000]
    for _ in range(2):
        starts.append(starts[-1] + burst_len + gap)
    n_cap = starts[-1] + burst_len + 4000

    rng = np.random.default_rng(31)
    cap = (
        0.02 * (rng.standard_normal(n_cap) + 1j * rng.standard_normal(n_cap))
    ).astype(np.complex64)
    for t in starts:
        cap[t : t + burst_len] += BURST

    rows, sets, drops = [], [], []
    for block in (777, 4096, 16384, n_cap):
        rx = _rx()
        found = set()
        for off in range(0, n_cap, block):
            bits = rx.push(cap[off : off + block])
            if not bits.size:
                continue
            for i, ev in enumerate(rx.events()):
                frame = bits[i * FRAME_SYMS : (i + 1) * FRAME_SYMS]
                if int(ev[0]) in starts and _frame_ok(frame):
                    found.add(int(ev[0]))
        sets.append(found)
        drops.append(int(rx.dropped))
        rows.append(
            [
                "whole capture" if block >= n_cap else str(block),
                f"{len(found)}/{len(starts)}",
                str(int(rx.dropped)),
            ]
        )

    d.block_rows = rows
    d.block_invariant = all(f == sets[0] for f in sets) and len(
        sets[0]
    ) == len(starts)
    d.block_no_drops = all(x == 0 for x in drops)

    # One call carrying the whole capture must return all three, from THAT
    # call -- not one per call with the rest of the input abandoned.
    rx = _rx()
    bits = rx.push(cap)
    d.block_multi_in_one = bits.size == len(starts) * FRAME_SYMS

    R.table(["block size (samples)", "bursts decoded", "dropped"], rows)
    R.md()
    R.md(
        f"Identical at every block size (**{d.block_invariant}**), and no "
        f"sample refused at any of them (**{d.block_no_drops}**). A single "
        f"push of the whole capture returns all "
        f"{len(starts)} payloads at once (**{d.block_multi_in_one}**)."
    )
    R.md()
    R.md(
        "It used to depend on the block size entirely. `push()` returned at "
        "most one burst per call and abandoned the rest of its input to do "
        "it, so a block carrying several bursts lost all but the first -- "
        "6/6 decoded with 333-sample blocks against 1/6 with one large one "
        "on the example capture. `push()` now returns EVERY burst it "
        "completed, with `events()` giving each its own record, which is "
        "what makes consuming the whole input possible: draining fully is "
        "also what bounds retention, so `dropped` is 0 by construction "
        "rather than by luck. See F11 and doppler#1008."
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
        "(§2.7). The tests have been moved onto a real m-sequence, built "
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
    R.find(
        "F11",
        "FIXED",
        "**`push()` discarded the rest of its input once a burst was "
        "ready**, so a block carrying several bursts lost all but the "
        "first -- 6/6 decoded with 333-sample blocks against 1/6 with one "
        "large one. Three separate sites: an early return that never looked "
        "at `x` at all, a `break` that left the remainder unwritten, and a "
        "single `acq_push` per chunk, which stops once its result array is "
        "full and abandons its own input suffix. All three are gone: the "
        "loop runs to the end of `x`, acq is re-fed until it has absorbed "
        "the chunk, and `push()` returns EVERY burst it completed. "
        "**Nothing caught this because no test anywhere put two bursts in "
        "one stream** -- with a single burst, everything discarded after it "
        "was noise, so the loss was unobservable, and this report's own "
        "`any_block_size` limit had the same blind spot. §2.11 is that gate. "
        "Draining every arrived detection rather than one per chunk is also "
        "what bounds retention, so `dropped` is now 0 by construction. See "
        "doppler#1008.",
    )
    R.find(
        "F9",
        "FIXED",
        "**A spurious detection ahead of a real burst discarded it.** The "
        "dedup rule armed `suppress_until = epoch + burst_len` on EVERY "
        "detection, unconditionally and at detection time, and took the "
        "FIRST hit in that window — so a crossing from noise, or from a "
        "neighbouring burst's payload firing against the acquisition code, "
        "blinded the search for a whole burst length. On the 5-burst "
        "example capture that cost 2 of 5 bursts; it now finds 5/5, each at "
        "its exact sample. Two jobs were conflated and are now separate: "
        "identity (`refine_span` proximity, settled by the stronger PEAK) "
        "and payload exclusion (armed only by a burst that actually "
        "DECODED). The tie-break is on `peak_mag` and deliberately not "
        "`test_stat` — the latter is peak over a noise estimate averaged "
        "across the surface, so a BARE preamble, raising no floor, "
        "outscores a real burst whose payload does. §2.10 pins it. "
        "Note this report could not have caught it at its own geometry: "
        "with this payload `burst_len` (2448) is UNDER `refine_span` "
        "(2480), so the window cannot reach past the burst it belongs to. "
        "The defect needs `burst_len > refine_span`, which is every "
        "realistic link. See doppler#1004.",
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
        d.pd_high >= 0.9,
        f"Pd saturates above the knee: {d.pd_high:.2f} at "
        f"{d.pd_rows[0][0]} dB-Hz",
    )
    R.limit(
        d.pd_low <= 0.1,
        f"...and FLOORS below it: {d.pd_low:.2f} at {d.pd_rows[2][0]} "
        f"dB-Hz — the half that stops a receiver which reports a burst for "
        f"anything from scoring perfectly",
    )
    R.limit(
        d.pd_monotone,
        "...and is monotone across the three levels, so the curve has a "
        "shape rather than two endpoints",
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
    R.limit(
        d.decoy_detected_alone,
        "a bare 0.35-amplitude preamble crosses the gate on its own and is "
        "reported invalid -- without which the decoy limits below are "
        "vacuous",
    )
    R.limit(
        d.decoy_all_survive,
        "a weaker detection arriving ahead of a real burst does not cost "
        "it: the burst decodes at its exact sample at every lead from 400 "
        "to 2100 samples (doppler#1004)",
    )
    R.limit(
        d.block_invariant,
        "the same 3-burst capture decodes identically at every block size "
        "from 777 samples to the whole capture in one call (doppler#1008)",
    )
    R.limit(
        d.block_no_drops,
        "no sample is refused at any block size -- draining every arrived "
        "detection is what keeps retention inside the ring",
    )
    R.limit(
        d.block_multi_in_one,
        "one push carrying three bursts returns all three payloads from "
        "that call, not one with the rest of the input abandoned",
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
            # ORIENTATION FIRST. The bullets below this are subtleties, and
            # every one of them assumes the reader already knows what the
            # object is and how it is called. A caller arriving cold needs
            # the shape of the call and the envelope before the caveats --
            # otherwise the list answers questions nobody has asked yet.
            "**What it is.** A burst receiver: samples in, decoded "
            "payloads out. It owns the hand-off between acquisition, a "
            "refine stage and the demodulator, which is the part a "
            "hand-wired chain gets wrong (§1). Configure it once with the "
            "waveform -- the two codes, the sync word, and the geometry -- "
            "and stream; there is no per-burst setup.",
            "**The shape of the call.** `push(x)` returns the payload "
            "bits of EVERY burst that completed in that call, "
            "concatenated, and `events()` returns one record per payload "
            "in the same order: burst `i` is "
            "`bits[i*frame_syms:(i+1)*frame_syms]` with `events()[i]` "
            "describing it. Every sample is consumed whatever the block "
            "size, and a burst split across calls is completed by a later "
            "one, so a caller never sizes or aligns anything (§2.11).",
            f"**Whether it fits your link.** End to end -- exact sample "
            f"AND valid CRC -- this geometry decodes "
            f"{d.pd_high:.0%} of bursts at {d.pd_rows[0][0]} dB-Hz, "
            f"{d.pd_mid:.0%} at {d.pd_rows[1][0]} and {d.pd_low:.0%} at "
            f"{d.pd_rows[2][0]}: a knee a few dB wide, not a gentle "
            f"roll-off (§2.6). The numbers belong to the GEOMETRY rather "
            f"than to the object -- §2.4 moves that knee 11 dB on `reps` "
            f"alone -- so "
            f"read the SHAPE and re-measure for yours.",
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
            f"(§2.7, F6).",
            "**Checkpoint between bursts.** `state_bytes()` is a pure "
            "function of configuration — jm's binding depends on that — and "
            "a blob taken inside the preamble carries the retained "
            "look-back, so it resumes into a fresh instance and still "
            "decodes (§2.9, F5).",
        ],
    )
    R.summary(
        "\n- Raw sweeps: `data/epoch.csv`, `data/reps.csv`, "
        "`data/amplitude.csv`, `data/pd.csv`, `data/code.csv`"
    )
    R.emit(HERE / "results.md")
    return R


if __name__ == "__main__":
    sys.exit(cli(build, HERE))
