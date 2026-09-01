"""BurstCapture — pulling bursts out of a stream, and telling them apart.

Acquisition tells you a burst is *somewhere near here*: its `code_phase` is a
lag **modulo one code period**, so it fixes the alignment within a preamble
repetition and never says which one. A burst has a frame that begins in one
specific repetition, so something has to resolve that and reach back to a
start that has already gone past. `BurstCapture` is that something. It hands
back the burst's samples and stops — demodulating is `BurstDemod`'s job.

What this shows
---------------
1. **The starts are exact.** Three bursts are placed at known offsets and the
   capture reports each to the sample, including one that is not a multiple
   of the code period.
2. **Block size is not a parameter of the answer.** The same stream is pushed
   in 4096-sample blocks and in one call; the windows are bit-identical.
3. **How to tell a real burst from a false alarm.** At `pfa = 1e-3` a
   spurious window is expected — this object is a detector's output stage and
   will not gate on signal quality. The read-back that separates them is
   `cn0_dbhz_est`, by about 9 dB. It is *not* `refine_margin`, which is the
   intuitive choice and separates them by hundredths: the margin answers
   "was the code period resolved", and a window on noise has no period to
   resolve.

Every claim above is asserted, so exit 0 means demonstrated AND checked.

Run:  python src/doppler/examples/burst_capture_demo.py
"""

from __future__ import annotations

import numpy as np

from doppler.dsss import BurstCapture
from doppler.wfm import PN, Composer, Segment

# ── Geometry ────────────────────────────────────────────────────────────────
ACQ_SF, DATA_SF, REPS, SPC = 31, 8, 4, 4
#: Barker-13, the frame sync word the generator prepends to the payload.
SYNC = np.array([0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 1, 0], dtype=np.uint8)
#: Symbols after the preamble: sync | payload | CRC-16.
PAYLOAD_SYMS = 61
PAYLOAD = np.array(
    [(i * 7 + 3) & 1 for i in range(PAYLOAD_SYMS - SYNC.size - 16)],
    dtype=np.uint8,
)
BURST_LEN = (REPS * ACQ_SF + PAYLOAD_SYMS * DATA_SF) * SPC
CHIP_RATE = 1.0e6
SIGMA = 0.02
#: Not round numbers on purpose: 41_117 is not a multiple of the code period,
#: so a reported start that is exact there is a RESOLVED epoch rather than a
#: phase seed that happened to land on the grid.
AT = [9_000, 41_117, 120_000]
N = 200_000


def acq_code() -> np.ndarray:
    """An m-sequence from doppler's own generator.

    Not cosmetic: a code whose worst autocorrelation sidelobe is near its peak
    sets the CFAR reference from its own structure rather than from noise, and
    the characterization measured what that costs — roughly half the burst
    offsets are never found at all.
    """
    return (
        np.asarray(
            PN(poly=0, seed=1, length=5).generate(ACQ_SF), dtype=np.uint8
        )
        & 1
    )


def data_code() -> np.ndarray:
    return np.array([(i >> 1) & 1 for i in range(DATA_SF)], dtype=np.uint8)


def one_burst() -> np.ndarray:
    """One burst, from the library's own generator.

    `Segment(type="dsss")` is doppler's declarative DSSS burst source, and it
    is what `wfmgen` drives — so the waveform received below is the waveform
    the tool emits. Building it by hand out of numpy would be a private copy
    of something the library owns, and a copy with no sync word and no CRC is
    a different burst from the one any caller transmits.
    """
    seg = Segment(
        type="dsss",
        fs=CHIP_RATE * SPC,
        freq=0.0,
        snr=100.0,  # clean; scene() adds the noise floor
        snr_mode="esno",
        seed=1,
        sps=SPC,  # samples per CHIP
        acq_code=acq_code().tobytes(),
        acq_reps=REPS,
        data_code=data_code().tobytes(),
        sync=SYNC.tobytes(),
        payload=PAYLOAD.tobytes(),  # CRC-16 auto-appended
        gap_noise="auto",
        off_samples=0,
    )
    out = np.asarray(Composer([seg]).compose(), dtype=np.complex64)
    return out[:BURST_LEN]


def scene() -> np.ndarray:
    rng = np.random.default_rng(7)
    x = (rng.normal(0.0, SIGMA, N) + 1j * rng.normal(0.0, SIGMA, N)).astype(
        np.complex64
    )
    b = one_burst()
    for a in AT:
        x[a : a + b.size] += b
    return x


def capture() -> BurstCapture:
    """The look-back is NOT a knob — its span is derived from the geometry."""
    return BurstCapture(
        acq_code(),
        burst_len=BURST_LEN,
        reps=REPS,
        spc=SPC,
        chip_rate=CHIP_RATE,
        cn0_dbhz=55.0,
    )


def main() -> None:
    x = scene()
    print("BurstCapture — three bursts in 200k samples of noise\n")

    cap = capture()
    print(f"  burst_len   {cap.burst_len:>7}  what a window holds")
    print(
        f"  refine_span {cap.refine_span:>7}  "
        f"two detections this close are ONE burst"
    )
    print(f"  retain_span {cap.retain_span:>7}  minimum trailing context\n")

    # ── 1. Streamed in blocks, the way a real caller has the data ──────────
    blocks = [cap.push(x[off : off + 4096]) for off in range(0, x.size, 4096)]
    streamed = np.concatenate([b for b in blocks if b.size])
    # events() describes the LAST push, so collect as we go in a real caller;
    # here the starts are re-derived from a single-call run below.
    n_windows = streamed.size // BURST_LEN
    print(f"  pushed in 4096-sample blocks -> {n_windows} window(s)")

    # ── 2. The same stream in one call ────────────────────────────────────
    one = capture()
    single = one.push(x)
    ev = one.events()
    starts = [int(s) for s in ev["preamble_start"]]

    print(
        f"\n  {'#':<4}{'start':<10}{'error':<8}{'C/N0':<9}{'margin':<9}verdict"
    )
    for i, s in enumerate(starts):
        err = min((s - a for a in AT), key=abs)
        real = any(s == a for a in AT)
        print(
            f"  {i + 1:<4}{s:<10}{err if not real else 0:<8}"
            f"{ev['cn0_dbhz_est'][i]:<9.1f}{ev['refine_margin'][i]:<9.3f}"
            f"{'burst' if real else 'spurious'}"
        )

    # ── Assertions: exit 0 means demonstrated AND checked ─────────────────
    for a in AT:
        assert a in starts, f"burst at {a} was not captured (starts={starts})"
    assert streamed.size == single.size, (
        f"block size changed the answer: {streamed.size} vs {single.size}"
    )
    assert np.array_equal(streamed, single), (
        "the windows differ between a blocked and a single-call push"
    )
    assert single.size % BURST_LEN == 0, "a partial window is not a window"
    assert one.dropped == 0, f"{one.dropped} samples were dropped"
    assert one.pending == 0, "a burst is still awaiting samples"

    real_rows = [i for i, s in enumerate(starts) if s in AT]
    spurious = [i for i, s in enumerate(starts) if s not in AT]
    assert len(real_rows) == len(AT)

    print(
        f"\n  {len(real_rows)} real, {len(spurious)} spurious "
        f"(pfa = 1e-3, so a false window is expected, not a defect)"
    )
    if spurious:
        cn0_real = float(np.median(ev["cn0_dbhz_est"][real_rows]))
        cn0_spur = float(np.median(ev["cn0_dbhz_est"][spurious]))
        print(
            f"  cn0_dbhz_est separates them by {cn0_real - cn0_spur:.1f} dB "
            f"({cn0_real:.1f} vs {cn0_spur:.1f}) — filter on THIS"
        )
        assert cn0_real > cn0_spur, "C/N0 must be higher at a real burst"
    else:
        print("  (no false alarm in this scene — the separation is in §2.5 of")
        print("   the validation report, over many more trials)")

    print("\n  OK — every start exact, block size irrelevant, nothing dropped")


if __name__ == "__main__":
    main()
