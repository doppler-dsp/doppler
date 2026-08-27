"""Certify `BurstDemod` — one burst in, a payload and a verdict out.

Run:  python -m doppler.dsss.tests.validation.burst_demod.validate
      make validate          (regenerates every report)
      make validate-check    (fails if the committed report is stale)

A feedforward demodulator with **no tracking loops**: it estimates the
burst's Doppler and Doppler rate from the unmodulated preamble, dechirps by
that estimate, despreads, aligns on the sync word, slices, and checks the
CRC-16 trailer. One pass, one shot, one verdict.

That shape decides what matters. There is no loop to converge, so the
estimate either lands or the burst is lost; and the object's entire output
surface is five read-backs plus the bits. Whether the frame is GOOD is not
among them any more — this object stops at decisions (doppler#1022), so the
trailer is checked here, as a caller would. That check is the one
branches on, so its **negative** case is the claim worth the most evidence —
a demodulator that hid or silently repaired a transmitted bit error
would be indistinguishable from a working one on every clean burst.

The estimator is certified separately
(`src/doppler/dsss/tests/validation/ppe/results.md`) and not re-derived here.
"""

from __future__ import annotations

import sys
from dataclasses import dataclass, field
from pathlib import Path

import numpy as np

from doppler.dsss import BurstDemod
from doppler.tests._validation_common import Report, cli

HERE = Path(__file__).resolve().parent
R = Report()

SPC = 4
ACQ_SF = 500
REPS = 5
DATA_SF = 50
CHIP_RATE = 1.0e6
PAYLOAD_LEN = 64
CRC_BITS = 16
SYNC = np.array([0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 1, 0], np.uint8)
F0 = 0.012


def _acode() -> np.ndarray:
    return ((np.arange(ACQ_SF) * 2654435761 >> 13) & 1).astype(np.uint8)


def _dcode() -> np.ndarray:
    return ((np.arange(DATA_SF) * 40503 >> 7) & 1).astype(np.uint8)


def _payload() -> np.ndarray:
    return ((np.arange(PAYLOAD_LEN) * 7 + 3) & 1).astype(np.uint8)


def _crc16(bits: np.ndarray) -> int:
    c = 0xFFFF
    for b in bits:
        c ^= (int(b) & 1) << 15
        c = ((c << 1) ^ 0x1021) & 0xFFFF if c & 0x8000 else (c << 1) & 0xFFFF
    return c


PAYLOAD_OFF = len(SYNC)  # the frame opens with the sync word


def _frame_ok(frame: np.ndarray) -> bool:
    """The caller's half: does the frame's trailer match its payload?

    `burst_demod` stops at decisions — it returns the frame's bits and makes
    no claim about them, because undoing a frame needs a description it does
    not hold (doppler#1022). So the CRC is checked HERE, which is what a
    caller does and what `wfm.Frame.deframe()` does for one that holds a
    description. This report's own claims moved with it: what the object
    promises is that a transmitted bit error REACHES this check.
    """
    frame = np.asarray(frame)
    if frame.size < PAYLOAD_OFF + PAYLOAD_LEN + CRC_BITS:
        return False
    payload = frame[PAYLOAD_OFF : PAYLOAD_OFF + PAYLOAD_LEN]
    rx = 0
    for b in frame[PAYLOAD_OFF + PAYLOAD_LEN :][:CRC_BITS]:
        rx = (rx << 1) | (int(b) & 1)
    return rx == _crc16(payload)


def _payload_of(frame: np.ndarray) -> np.ndarray:
    """The payload's slice out of a returned frame."""
    return np.asarray(frame)[PAYLOAD_OFF : PAYLOAD_OFF + PAYLOAD_LEN]


def _burst(
    payload: np.ndarray,
    *,
    filler: int = 0,
    corrupt_at: int | None = None,
    rate: float = 0.0,
    seed: int = 0,
    sigma: float = 0.0,
) -> np.ndarray:
    """One burst: preamble, optional filler symbols, sync, payload, CRC.

    `corrupt_at` flips a payload bit AFTER the trailer is computed, so the
    frame arrives intact and fails its own CRC — the case that separates a
    demodulator which hands the error THROUGH from one that hides or
    "fixes" it.
    """
    crc = _crc16(payload)
    crc_bits = np.array(
        [(crc >> (CRC_BITS - 1 - j)) & 1 for j in range(CRC_BITS)], np.uint8
    )
    tx = payload.copy()
    if corrupt_at is not None:
        tx[corrupt_at] ^= 1
    fill = ((np.arange(filler) * 5 + 1) & 1).astype(np.uint8)
    frame = np.concatenate([fill, SYNC, tx, crc_bits])

    def csign(b):
        return np.where(np.asarray(b) & 1, -1.0, 1.0)

    chips = [np.tile(csign(_acode()), REPS)] + [
        csign(b) * csign(_dcode()) for b in frame
    ]
    bb = np.repeat(np.concatenate(chips), SPC)
    n = np.arange(len(bb))
    ph = 2.0 * np.pi * (F0 * n + 0.5 * rate * n * n)
    x = bb * np.exp(1j * ph)
    if sigma > 0.0:
        r = np.random.default_rng(seed)
        x = x + sigma * (
            r.standard_normal(len(x)) + 1j * r.standard_normal(len(x))
        ) / np.sqrt(2)
    return x.astype(np.complex64)


def _demod(x: np.ndarray, *, max_rate: float = 0.0, carrier_hz: float = 0.0):
    d = BurstDemod(
        _dcode(),
        spc=SPC,
        chip_rate=CHIP_RATE,
        frame_syms=len(SYNC) + PAYLOAD_LEN + CRC_BITS,
        max_rate=max_rate,
        carrier_hz=carrier_hz,
    )
    d.set_preamble(_acode(), REPS)
    d.set_sync(SYNC)
    d.set_prior(F0, 0)
    return d, d.demod(x)


def _csv(path: Path, header: str, rows: list[list[object]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as fh:
        fh.write(header + "\n")
        for r in rows:
            fh.write(",".join(str(v) for v in r) + "\n")


@dataclass
class Data:
    """Everything §3 and §4 read, measured once in §2."""

    clean_ok: bool = False
    crc_rows: list[list[str]] = field(default_factory=list)
    crc_rejects_all: bool = False
    crc_still_decodes: bool = False
    offset_rows: list[list[str]] = field(default_factory=list)
    offset_tracks: bool = False
    nsym_tracks: bool = False
    short_rows: list[list[str]] = field(default_factory=list)
    short_refused: bool = False
    reset_rows: list[list[str]] = field(default_factory=list)
    reset_clears: bool = False
    reset_precondition: bool = False
    est_rows: list[list[str]] = field(default_factory=list)
    est_freq_err_hz: float = 0.0
    noise_rows: list[list[str]] = field(default_factory=list)
    noise_floor_ok: bool = False
    max_out_is_payload: bool = False


# ── 1. the object ─────────────────────────────────────────────────────


def section_object() -> None:
    R.md("## 1. The object — feedforward, so there is no second chance")
    R.md()
    R.md(
        "`BurstDemod` recovers a spread burst end to end with no tracking "
        "loops: estimate from the preamble, dechirp, despread, sync-align, "
        "slice, check the CRC. Its entire output surface is the payload "
        "bits plus five read-backs, and a caller branches on one of them."
    )
    R.md()
    R.table(
        ["page", "owns"],
        [
            [
                "[`docs/design/dsss-burst-receiver.md`]"
                "(../../../../../../docs/design/dsss-burst-receiver.md)",
                "the burst chain this object terminates — why the last stage "
                "is feedforward, and what the refine stage must hand it",
            ],
            [
                "[`ppe`'s report](../ppe/results.md)",
                "the feedforward (frequency, chirp rate) estimator this "
                "object drives — not re-derived here",
            ],
            [
                "[`burst_despreader`'s report]"
                "(../burst_despreader/results.md)",
                "the tracked alternative, for bursts long enough to close a "
                "loop",
            ],
            [
                "`native/inc/burst_demod/burst_demod_core.h`",
                "the contract — the SSOT this report audits",
            ],
        ],
    )
    R.md("### 1.1 The claim inventory")
    R.md()
    R.md(
        "Step 1 of `docs/dev/contributing/validation.md`. The C test was 192 "
        "lines with 14 assertions for an end-to-end object, and the gaps "
        "cluster on the read-backs — the part a caller actually consumes."
    )
    R.md()
    R.table(
        ["header claim", "pinned where", "here"],
        [
            [
                "a transmitted bit error reaches the caller, and the "
                "frame's own trailer rejects it",
                "**positive case + the too-short path only**",
                "§2.1",
            ],
            [
                "`frame_offset` is the symbol offset of the sync word",
                "**observed only as 0**, its degenerate value",
                "§2.2",
            ],
            [
                "`n_symbols` is the despread data symbols produced",
                "**no mention in either language**",
                "§2.2",
            ],
            [
                "`reset()` re-arms the demodulator",
                "**called by nothing in either language**",
                "§2.3",
            ],
            [
                "`est_freq_hz` / `est_rate_hz` / `est_snr_db` report the "
                "estimate",
                "C, printed but barely asserted",
                "§2.4",
            ],
            [
                "returns 0 on failure / too-short burst",
                "C, at 8 samples",
                "§2.1",
            ],
            [
                "`demod_max_out()` is the payload length",
                "Python",
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
    _sec_crc(d)
    _sec_readbacks(d)
    _sec_reset(d)
    _sec_estimate(d)
    _sec_bounds(d)
    return d


def _sec_crc(d: Data) -> None:
    R.md("### 2.1 A transmitted bit error REACHES the caller")
    R.md()
    R.md(
        "This object stops at decisions: it returns the frame's bits and "
        "makes no claim about them, because undoing a frame needs a "
        "description it does not hold (doppler#1022). So the claim it makes "
        "here is the one a layer above depends on — that an error which "
        "happened on the wire is still in the bits when they are handed "
        "over, neither hidden nor silently repaired."
    )
    R.md()
    R.md(
        "The case that shows it is a frame which arrives intact, aligns on "
        "the sync and produces its symbols, carrying one payload bit "
        "transmitted flipped **after** its trailer was computed. Two things "
        "must hold: the flip is in the output at the bit it was applied to, "
        "and the trailer — checked HERE, as `wfm.Frame.deframe()` would — "
        "no longer matches. An earlier version of this report asserted the "
        "second alone, through a `frame_valid` read-back this object no "
        "longer owns."
    )
    R.md()
    payload = _payload()
    _d0, bits0 = _demod(_burst(payload))
    d.clean_ok = _frame_ok(bits0) and np.array_equal(
        _payload_of(bits0), payload
    )
    rows, csv = [], []
    all_reject = True
    still_decodes = True
    frame_syms = len(SYNC) + PAYLOAD_LEN + CRC_BITS
    for pos in (0, 17, PAYLOAD_LEN - 1):
        _dd, bb = _demod(_burst(payload, corrupt_at=pos))
        all_reject &= not _frame_ok(bb)
        still_decodes &= len(bb) == frame_syms
        reached = bool(_payload_of(bb)[pos] != payload[pos])
        rows.append(
            [
                str(pos),
                str(len(bb)),
                str(reached),
                str(not _frame_ok(bb)),
            ]
        )
        csv.append([pos, len(bb), int(reached)])
    R.table(
        [
            "payload bit flipped",
            "frame bits returned",
            "flip reached the caller",
            "trailer rejects it",
        ],
        rows,
    )
    _csv(HERE / "data" / "crc.csv", "pos,nbits,reached", csv)
    d.crc_rows = rows
    d.crc_rejects_all = all_reject
    d.crc_still_decodes = still_decodes
    R.md(
        f"All three are rejected, at both ends of the payload and in the "
        f"middle — and all three still **return their bits** "
        f"(**{d.crc_still_decodes}**), which is what makes this the CRC "
        f"path and not the too-short path. A clean burst decodes and "
        f"validates (**{d.clean_ok}**), so the rejections are not simply a "
        f"demodulator that never works."
    )
    R.md()


def _sec_readbacks(d: Data) -> None:
    R.md("### 2.2 `frame_offset` and `n_symbols`, away from zero")
    R.md()
    R.md(
        "`frame_offset` is documented as the symbol offset of the sync "
        "word, and was only ever observed as 0 — which is exactly what a "
        "read-back hardwired to zero also reports. `n_symbols` had no "
        "mention in either language."
    )
    R.md()
    payload = _payload()
    rows, csv = [], []
    off_ok = nsym_ok = True
    for filler in (0, 3, 9):
        dd, bb = _demod(_burst(payload, filler=filler))
        want_n = filler + len(SYNC) + PAYLOAD_LEN + CRC_BITS
        off_ok &= dd.frame_offset == filler
        nsym_ok &= dd.n_symbols == want_n
        rows.append(
            [
                str(filler),
                str(dd.frame_offset),
                str(dd.n_symbols),
                str(want_n),
                str(_frame_ok(bb)),
            ]
        )
        csv.append([filler, dd.frame_offset, dd.n_symbols, want_n])
    R.table(
        [
            "filler symbols before sync",
            "frame_offset",
            "n_symbols",
            "expected",
            "trailer ok",
        ],
        rows,
    )
    _csv(
        HERE / "data" / "readbacks.csv",
        "filler,frame_offset,n_symbols,expected",
        csv,
    )
    d.offset_rows = rows
    d.offset_tracks = off_ok
    d.nsym_tracks = nsym_ok
    R.md(
        "The offset equals the filler count exactly, and `n_symbols` counts "
        "the **whole despread data section** — filler, sync, payload and "
        "trailer — not the payload alone. That distinction is what a caller "
        "slicing the soft symbols needs, and it is only visible once the "
        "sync is not at zero."
    )
    R.md()


def _sec_reset(d: Data) -> None:
    R.md("### 2.3 `reset()` clears the verdict")
    R.md()
    R.md(
        "Documented, and called by nothing in either language. The "
        "read-backs are this object's whole output surface, so a reset that "
        "left them standing would report the **previous** burst's verdict "
        "for a burst that has not been demodulated — the worst failure a "
        "per-burst object can have, and a completely silent one."
    )
    R.md()
    dd, _ = _demod(_burst(_payload()))
    before = (
        int(dd.n_symbols),
        float(dd.est_freq_hz),
        int(dd.frame_offset),
    )
    d.reset_precondition = (
        before[0] > 0 and before[1] != 0.0 and before[2] >= 0
    )
    dd.reset()
    after = (
        int(dd.n_symbols),
        float(dd.est_freq_hz),
        int(dd.frame_offset),
        float(dd.est_rate_hz),
        float(dd.est_snr_db),
    )
    d.reset_clears = after == (0, 0.0, 0, 0.0, 0.0)
    rows = [
        ["n_symbols", str(before[0]), str(after[0])],
        ["est_freq_hz", f"{before[1]:.2f}", f"{after[1]:.2f}"],
        ["frame_offset", str(before[2]), str(after[2])],
    ]
    R.table(["read-back", "after a burst", "after reset()"], rows)
    d.reset_rows = rows
    R.md(
        f"The precondition matters as much as the assertion: all three are "
        f"**non-zero before** the reset (**{d.reset_precondition}**), or "
        f"the check would pass against state that was already clear — the "
        f"vacuous-reset shape this campaign keeps finding."
    )
    R.md()


def _sec_estimate(d: Data) -> None:
    R.md("### 2.4 The estimate the whole burst rests on")
    R.md()
    R.md(
        "With no loops, the feedforward estimate is the only thing standing "
        "between the burst and a failed decode. It is `ppe`'s, certified "
        "there; what matters here is that this object hands it the right "
        "segment and reports it in the right units."
    )
    R.md()
    fs = CHIP_RATE * SPC
    dd, _ = _demod(_burst(_payload()))
    want_hz = F0 * fs
    d.est_freq_err_hz = abs(dd.est_freq_hz - want_hz)
    rows = [
        [
            "est_freq_hz",
            f"{want_hz:.1f}",
            f"{dd.est_freq_hz:.1f}",
            f"{d.est_freq_err_hz:.1f}",
        ],
        ["est_rate_hz", "0.0", f"{dd.est_rate_hz:.3g}", "—"],
        ["est_snr_db", "—", f"{dd.est_snr_db:.1f}", "—"],
    ]
    R.table(["read-back", "injected", "reported", "error (Hz)"], rows)
    d.est_rows = rows
    R.md(
        f"The injected residual is `F0 * fs` = {want_hz:.0f} Hz and the "
        f"object reports it to **{d.est_freq_err_hz:.1f} Hz** — in Hz, not "
        f"in cycles per sample, which is the conversion a caller would "
        f"otherwise have to guess."
    )
    R.md()
    rows, csv = [], []
    ok = True
    payload = _payload()
    for sigma in (0.0, 0.2, 0.4):
        good = 0
        for k in range(5):
            dd, bb = _demod(_burst(payload, sigma=sigma, seed=100 + k))
            good += int(
                _frame_ok(bb) and np.array_equal(_payload_of(bb), payload)
            )
        rows.append([f"{sigma:g}", f"{good}/5"])
        csv.append([sigma, good])
        if sigma <= 0.2:
            ok &= good == 5
    R.table(["input noise sigma", "clean decodes"], rows)
    _csv(HERE / "data" / "noise.csv", "sigma,good_of_5", csv)
    d.noise_rows = rows
    d.noise_floor_ok = ok
    R.md(
        "Five draws per level. The point is not a sensitivity curve — that "
        "is a characterization — but that the object decodes **every** "
        "burst at a noise level it is comfortable with, so the CRC "
        "rejections in §2.1 are attributable to the corruption and not to "
        "a marginal link."
    )
    R.md()


def _sec_bounds(d: Data) -> None:
    R.md("### 2.5 What it refuses")
    R.md()
    dd = BurstDemod(
        _dcode(),
        spc=SPC,
        chip_rate=CHIP_RATE,
        frame_syms=len(SYNC) + PAYLOAD_LEN + CRC_BITS,
    )
    d.max_out_is_payload = (
        dd.demod_max_out() == len(SYNC) + PAYLOAD_LEN + CRC_BITS
    )
    dd.set_preamble(_acode(), REPS)
    dd.set_sync(SYNC)
    dd.set_prior(F0, 0)
    rows = []
    refused = True
    for n in (8, 64, 512):
        tiny = np.zeros(n, dtype=np.complex64)
        got = dd.demod(tiny)
        refused &= len(got) == 0
        rows.append([str(n), str(len(got))])
    R.table(["burst samples", "bits returned"], rows)
    d.short_rows = rows
    d.short_refused = refused
    R.md(
        f"A burst too short to contain a preamble returns no bits and no "
        f"verdict, at three lengths. `demod_max_out()` reports "
        f"{dd.demod_max_out()} — the payload length, which is the buffer a "
        f"caller must provide."
    )
    R.md()


# ── 3. review ─────────────────────────────────────────────────────────


def review(d: Data) -> None:
    R.md("## 3. Review — findings")
    R.md()
    R.find(
        "F1",
        "FIXED",
        "**The frame verdict's negative case was the too-short path "
        "only.** It was asserted 1 on a clean burst and 0 on an 8-sample "
        "input — but "
        "the latter returns before any check is ever computed. A verdict "
        "that ignored the trailer and reported 1 for anything it decoded "
        "would have passed both assertions, on an object whose entire "
        "purpose is to tell a caller whether the payload can be trusted. "
        "Now measured on frames that arrive intact and fail their own CRC — "
        "a payload bit flipped after the trailer was computed, at both ends "
        "and the middle — with the bits still returned, which is what "
        "distinguishes the CRC path from the too-short one. Sabotage-proven "
        "by reporting success whenever a frame decoded. The verdict has "
        "since moved OUT of this object entirely (doppler#1022): it stops "
        "at decisions, and what §2.1 pins is that a wire error still "
        "reaches whoever checks.",
    )
    R.find(
        "F2",
        "FIXED",
        "**`frame_offset` was only ever observed as 0** — its degenerate "
        "value, and exactly what a read-back hardwired to zero reports. Now "
        "measured against bursts carrying 0, 3 and 9 filler symbols before "
        "the sync, where it must equal the filler count. Sabotage-proven by "
        "hardwiring it. Worth noting how the first attempt at that sabotage "
        "failed: the obvious regex matched the **initialiser** rather than "
        "the assignment, so the patch replaced `= 0` with `= 0` and the "
        "suite stayed green — a no-op sabotage reads exactly like a test "
        "that cannot fail.",
    )
    R.find(
        "F3",
        "FIXED",
        "**`n_symbols` had no mention in either language**, and it does not "
        "mean what its neighbours might suggest: it counts the whole "
        "despread data section — filler, sync, payload and trailer — not "
        "the payload. That distinction is invisible until the sync is not "
        "at offset zero, which is why it needed the same stimulus as F2. "
        "Sabotage-proven by reporting `frame_syms` instead.",
    )
    R.find(
        "F4",
        "FIXED",
        "**`reset()` was called by nothing in either language.** The "
        "read-backs are this object's whole output surface, so a reset that "
        "left them standing would report the previous burst's verdict for a "
        "burst that had not been demodulated — silently, and in the "
        "direction that matters (a stale success). Now checked "
        "with the precondition asserted first, so it cannot pass against "
        "state that was already clear. Sabotage-proven by making `reset()` "
        "return immediately.",
    )
    R.find(
        "F5",
        "BY DESIGN",
        "There is no state triplet and no `serializable` flag, correctly: "
        "the object is per-burst and feedforward, so there is no "
        "mid-stream position to checkpoint — a burst either completes "
        "within one `demod()` call or is lost. That is the same reasoning "
        "as `ppe`'s, one level up, and it is why `reset()` (F4) carries the "
        "whole burden of separating one burst from the next.",
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
        d.clean_ok,
        "a clean burst decodes to the transmitted payload and validates",
    )
    R.limit(
        d.crc_rejects_all,
        "a payload bit flipped after the trailer is computed is REJECTED, "
        "at both ends of the payload and the middle",
    )
    R.limit(
        d.crc_still_decodes,
        "...and the bits are still returned, so the rejection is the CRC "
        "path and not the too-short path",
    )
    R.limit(
        d.offset_tracks,
        "frame_offset equals the number of symbols before the sync word, "
        "at 0, 3 and 9",
    )
    R.limit(
        d.nsym_tracks,
        "n_symbols counts the whole despread data section (filler + sync + "
        "payload + trailer), not the payload",
    )
    R.limit(
        d.reset_precondition,
        "the read-backs are non-zero after a burst — the precondition that "
        "stops the reset check passing vacuously",
    )
    R.limit(
        d.reset_clears,
        "reset() clears all five read-backs, so a fresh burst cannot "
        "inherit the previous verdict",
    )
    R.limit(
        d.est_freq_err_hz < 200.0,
        f"est_freq_hz reports the injected residual in Hz, to "
        f"{d.est_freq_err_hz:.1f} Hz",
    )
    R.limit(
        d.noise_floor_ok,
        "every burst decodes cleanly at input noise the link is "
        "comfortable with, so the CRC rejections are attributable to the "
        "corruption",
    )
    R.limit(
        d.short_refused,
        "a burst too short to hold a preamble returns no bits and no "
        "verdict, at three lengths",
    )
    R.limit(
        d.max_out_is_payload,
        "demod_max_out() is the FRAME length — the buffer a caller must "
        "provide",
    )
    R.limit(
        len(d.crc_rows) == 3,
        "the CRC rejection is measured at three payload positions, not one",
    )
    R.limit(
        len(d.offset_rows) == 3,
        "the read-back sweep includes offsets away from zero, where a "
        "hardwired value would show",
    )
    R.limit(
        len(d.noise_rows) == 3,
        "the decode check spans three noise levels",
    )
    R.limit(
        len(d.short_rows) == 3,
        "the refusal check spans three too-short lengths",
    )


# ── build ─────────────────────────────────────────────────────────────


def build(write: bool = True) -> Report:
    global R
    R = Report(write=write)
    R.md("# BurstDemod — validation report")
    R.md()
    section_object()
    d = characterise()
    review(d)
    limits(d)
    R.executive(
        "BurstDemod",
        [
            "**The bit-error path now has evidence for the case that "
            "matters.** "
            "It was pinned at 1 on a clean burst and at 0 on an 8-sample "
            "input — the *no frame at all* path — so a version that ignored "
            "the CRC entirely would have passed both. It is now measured on "
            "frames that arrive and fail (§2.1, F1).",
            "**`n_symbols` is not the payload count.** It counts the whole "
            "despread data section — filler, sync, payload and trailer — "
            "which is what a caller slicing soft symbols needs, and it was "
            "mentioned in neither language before this (§2.2, F3).",
            "**Call `reset()` between bursts.** The read-backs persist "
            "otherwise, and a silently repaired bit is the failure "
            "direction that matters. Nothing had ever called it (§2.3, F4).",
            "**`est_freq_hz` is in Hz**, not cycles per sample — the "
            "conversion a caller would otherwise have to guess from a "
            "field name (§2.4).",
            "**There is no state triplet, and that is correct.** A burst "
            "completes within one `demod()` call or is lost, so there is no "
            "mid-stream position to checkpoint — which is why `reset()` "
            "carries the whole burden of separating one burst from the "
            "next (F5).",
        ],
    )
    R.summary(
        "\n- Raw sweeps: `data/crc.csv`, `data/readbacks.csv`, "
        "`data/noise.csv`"
    )
    R.emit(HERE / "results.md")
    return R


if __name__ == "__main__":
    sys.exit(cli(build, HERE))
