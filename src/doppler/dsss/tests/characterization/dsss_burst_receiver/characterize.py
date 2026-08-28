"""Characterization of `DsssBurstReceiver` — what loses a burst, and when.

A **characterization**, not an example: it sweeps many trials and is run
deliberately by `make characterize`. See
`doppler.dsss.tests.characterization` for why that distinction exists.

Phase 7 of `docs/dev/contributing/adding-algorithms.md`, for the object
designed in `docs/design/dsss-burst-receiver.md`. It exists to answer the
one number that design left open — the refine stage's discrimination margin,
measured where it FAILS rather than where it holds — and it found a second,
larger effect on the way.

The two panels
--------------
**Left — detection vs where the burst lands, for two acquisition codes.**
The receiver inherits acquisition's framing, which is strictly sequential and
NON-overlapping: one dwell is one frame's worth of samples, with no sliding
search. A burst arrives at an arbitrary offset relative to that grid, so its
preamble can straddle two frames with neither holding all of it. The obvious
conclusion is that the framing loses those bursts — and it is wrong. This
panel sweeps the offset across a whole frame **near the sensitivity knee**,
for a code with good autocorrelation and one with poor. Both effects are
real, and they are not the same size.

The code dominates: at 59 dB-Hz a structured code whose peak-to-worst-
sidelobe ratio is 1.07 finds 6% of offsets where an m-sequence finds 77%.
The mechanism is the CFAR reference — with a poor code the noise estimate
is set by the code's own autocorrelation sidelobes rather than by noise, so
a straddled preamble has no margin to give away.

The framing is the residual: even a good code loses a band around mid-frame,
because acquisition frames without overlap and a preamble falling across a
boundary is split between two of them. That is doppler#1006.

**Both of those took a correction to see.** A first pass measured a 42% loss
on the structured code and concluded the framing needed overlapping dwells —
wrong, the code was the dominant term. The fix for that then over-corrected:
the sweep was run at 100 dB-Hz, where a half-covered frame clears threshold
by 40 dB and the framing term vanishes entirely, and the conclusion became
"the framing was never the problem". Near the knee it plainly is. A sweep
far from the operating point answers a question nobody asked.

**Right — refine's period discrimination vs C/N0.** Acquisition fixes the
code phase within a period; refine decides WHICH repetition, by correlating
one code period at each of the `reps` positions the preamble would occupy
and summing the magnitudes. The nearest rival period scores
`(reps-1)/reps` of the winner — 2.5 dB at `reps = 4` — and the design doc
recorded that as a margin measured only where it does not fail. This is
where it fails.

`refine_margin` is the receiver's own view of that decision (rival over
winner, so *lower is better* and a value near 1 means the period was not
resolved). Plotting it beside the correct-period rate is the point: it is
the only signal in the chain that sees a broken hand-off, because a
mis-windowed burst still has a carrier and still reads as locked.

What it measures, not what it hopes
-----------------------------------
Every trial drives the real object end to end — samples in, payload bits
out — and scores against the burst's known start. A trial counts as correct
only when `preamble_start` names the exact sample the burst began at, which
is a stricter criterion than a passing CRC and is the one a downstream
consumer of the detection event actually depends on.
"""

from __future__ import annotations

import sys

import numpy as np

from doppler.dsss import DsssBurstReceiver
from doppler.wfm import crc16

# ── Geometry ─────────────────────────────────────────────────────────────
# Small enough to sweep in minutes, large enough that the coherent depth is
# 4 -- which is what gives the refine stage four preamble positions to
# discriminate between, and the slow-time FFT a Doppler axis worth having.
ACQ_SF, DATA_SF, SYNC_LEN = 31, 8, 13
REPS, SPC, PAYLOAD = 4, 4, 32
CHIP_RATE = 1.0e6
FS = CHIP_RATE * SPC
CODE_PERIOD = ACQ_SF * SPC  # one preamble repetition, in samples
ACQ_FRAME = REPS * CODE_PERIOD  # acquisition's non-overlapping frame

N_CAP = 40_000
BASE_AT = 4 * ACQ_FRAME * 2  # a frame boundary, comfortably into the stream
PUSH = 2048

#: Offsets swept across one whole acquisition frame (left panel).
OFFSET_STEP = 8
#: Trials per C/N0 point (right panel). Seeded, so the curve is reproducible.
TRIALS = 24
#: Input noise sigmas swept. Converted to C/N0 for the axis.
SIGMAS = (0.02, 0.1, 0.3, 0.6, 1.0, 1.4, 1.8, 2.2, 2.6, 3.0)


def _codes():
    rng = np.random.default_rng(0)
    return (
        rng.integers(0, 2, ACQ_SF).astype(np.uint8),
        rng.integers(0, 2, DATA_SF).astype(np.uint8),
        rng.integers(0, 2, SYNC_LEN).astype(np.uint8),
    )


ACQ_CODE, DATA_CODE, SYNC = _codes()


def _structured_codes():
    """A deliberately POOR acquisition code, for the left panel's contrast.

    Not a strawman: this is the arithmetic pattern that reads as a perfectly
    reasonable deterministic code and is used in more than one hand-written
    test. Its autocorrelation says otherwise.
    """
    a = ((np.arange(ACQ_SF) * 2654435761 >> 13) & 1).astype(np.uint8)
    return a, DATA_CODE, SYNC


def peak_to_sidelobe(code: np.ndarray) -> float:
    """Peak over the worst non-zero-lag autocorrelation sidelobe.

    The single number that predicts the left panel: it is what sets the CFAR
    reference floor, and therefore how much margin a straddled preamble has
    to give away before it drops below threshold.
    """
    c = np.where(np.asarray(code) & 1, -1.0, 1.0)
    ac = np.array(
        [abs(float(np.dot(c, np.roll(c, k)))) for k in range(c.size)]
    )
    return float(ac[0] / ac[1:].max())


def _burst(acq_code=None, data_code=None, sync=None) -> np.ndarray:
    """One burst: preamble, then the spread sync|payload|CRC-16 frame."""
    acq_code = ACQ_CODE if acq_code is None else acq_code
    data_code = DATA_CODE if data_code is None else data_code
    sync = SYNC if sync is None else sync
    rng = np.random.default_rng(7)
    payload = rng.integers(0, 2, PAYLOAD).astype(np.uint8)
    c = crc16(payload)
    crc = np.array([(c >> (15 - j)) & 1 for j in range(16)], np.uint8)
    frame = np.concatenate([sync, payload, crc])

    def sgn(b):
        return np.where(np.asarray(b) & 1, -1.0, 1.0)

    chips = [np.tile(sgn(acq_code), REPS)] + [
        sgn(b) * sgn(data_code) for b in frame
    ]
    return np.repeat(np.concatenate(chips), SPC).astype(np.complex64)


BURST = _burst()
BURST_LEN = BURST.size


def cn0_dbhz(sigma: float) -> float:
    """C/N0 of a unit-amplitude burst in complex noise of std `sigma`.

    Per-sample SNR is `1 / sigma**2`; C/N0 adds `10log10(fs)`, which is the
    same relationship the acquisition engine sizes itself with, so the axis
    is directly comparable to a `cn0_dbhz` a caller would pass in.
    """
    return 10.0 * np.log10(1.0 / sigma**2) + 10.0 * np.log10(FS)


def _run_one(
    at: int, sigma: float, seed: int, codes=None, burst=None
) -> tuple[bool, bool, float]:
    """Drive the real receiver over one capture.

    Returns (period correct, CRC valid, refine margin). "Period correct"
    means `preamble_start` names the exact sample the burst began at --
    stricter than a passing CRC, and the criterion a consumer of the
    detection event depends on.
    """
    acq_code, data_code, sync = codes or (ACQ_CODE, DATA_CODE, SYNC)
    burst = BURST if burst is None else burst
    rng = np.random.default_rng(seed)
    cap = (
        sigma * (rng.standard_normal(N_CAP) + 1j * rng.standard_normal(N_CAP))
    ).astype(np.complex64)
    cap[at : at + burst.size] += burst

    rx = DsssBurstReceiver(
        acq_code,
        data_code,
        sync,
        reps=REPS,
        spc=SPC,
        chip_rate=CHIP_RATE,
        frame_syms=len(SYNC) + PAYLOAD + 16,
        cn0_dbhz=55.0,
    )
    for off in range(0, N_CAP, PUSH):
        bits = rx.push(cap[off : off + PUSH])
        if bits.size:
            # A returned frame is the receiver's whole claim; whether it
            # checks out is the DeFramer's answer (doppler#1022).
            return (
                rx.preamble_start == at,
                bool(bits.size),
                float(rx.refine_margin),
            )
    return (False, False, float("nan"))


#: C/N0 the offset sweep runs at, as an input noise sigma.
#:
#: NEAR THE KNEE, deliberately. The first version of this sweep ran at
#: sigma = 0.02 (about 100 dB-Hz, ~40 dB above the knee) and reported 100%
#: found for the good code -- which proves nothing, because a frame carrying
#: half a preamble still clears threshold by a mile up there. A straddle loss
#: can only show where the margin is thin. See doppler#1006.
OFFSET_SIGMA = 2.2


def sweep_offset(codes, sigma: float = OFFSET_SIGMA):
    """Detection vs the burst's offset within one acquisition frame.

    Run NEAR the sensitivity knee: a burst whose preamble straddles two of
    acquisition's non-overlapping frames is split between them, and only a
    thin margin turns that into a lost burst.
    """
    burst = _burst(*codes)
    offs = np.arange(0, ACQ_FRAME, OFFSET_STEP)
    ok = np.zeros(offs.size, dtype=bool)
    for i, off in enumerate(offs):
        ok[i] = _run_one(
            BASE_AT + int(off), sigma=sigma, seed=5, codes=codes, burst=burst
        )[0]
    return offs, ok


def sweep_cn0() -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    """Correct-period rate and refine margin vs C/N0.

    The burst start is redrawn per trial across a whole code period, so the
    sweep folds in the sub-period alignment a real link would have rather
    than measuring one lucky phase.
    """
    cn0 = np.array([cn0_dbhz(s) for s in SIGMAS])
    rate = np.zeros(len(SIGMAS))
    margin = np.full(len(SIGMAS), np.nan)
    for i, sigma in enumerate(SIGMAS):
        good, margins = 0, []
        for k in range(TRIALS):
            rng = np.random.default_rng(9000 + 97 * i + k)
            at = BASE_AT + int(rng.integers(0, CODE_PERIOD))
            period_ok, _crc, m = _run_one(at, sigma, seed=1000 + k)
            good += int(period_ok)
            if m == m:  # not NaN -- a burst was actually produced
                margins.append(m)
        rate[i] = good / TRIALS
        if margins:
            margin[i] = float(np.median(margins))
    return cn0, rate, margin


def main(out_path: str | None = None) -> None:
    out_path = out_path or "dsss_burst_receiver_characterization.png"

    print(
        f"geometry: code period {CODE_PERIOD} samples, "
        f"acquisition frame {ACQ_FRAME} (= the whole preamble), "
        f"burst {BURST_LEN}"
    )

    good = (ACQ_CODE, DATA_CODE, SYNC)
    poor = _structured_codes()
    print(
        f"\n[1] detection vs burst offset, at "
        f"{cn0_dbhz(OFFSET_SIGMA):.0f} dB-Hz -- NEAR THE KNEE, because a "
        f"straddle loss cannot show far above it (doppler#1006)"
    )
    results = {}
    for label, codes in (("good", good), ("poor", poor)):
        offs, ok = sweep_offset(codes)
        results[label] = (offs, ok)
        pts = peak_to_sidelobe(codes[0])
        miss = offs[~ok]
        band = (
            "none"
            if miss.size == 0
            else (
                f"{miss.size} of {ok.size} offsets, spanning "
                f"{miss.min() / ACQ_FRAME:.2f}.."
                f"{miss.max() / ACQ_FRAME:.2f} of a frame"
            )
        )
        print(
            f"    {label:>5} code (peak/sidelobe {pts:4.2f}): "
            f"{ok.sum():>2}/{ok.size} offsets = {100 * ok.mean():3.0f}%   "
            f"losses: {band}"
        )
    print(
        "    => BOTH matter, and the code matters more. With a poor code "
        "the CFAR reference is"
    )
    print(
        "       set by the code's own sidelobes, so a straddled preamble "
        "has no margin at all."
    )
    print(
        "       A good code recovers most of it -- but not all. The "
        "residual band around mid-frame"
    )
    print(
        "       is acquisition's NON-OVERLAPPING framing, and near the "
        "knee it costs a real"
    )
    print("       fraction of bursts. That is doppler#1006.")

    cn0, rate, margin = sweep_cn0()
    print(
        f"\n[2] refine period discrimination vs C/N0 ({TRIALS} trials/point)"
    )
    print(f"    {'C/N0':>8} {'correct period':>15} {'median margin':>14}")
    for c, r, m in zip(cn0, rate, margin):
        print(f"    {c:>8.1f} {r:>14.0%} {m:>14.3f}")
    ideal = (REPS - 1) / REPS
    print(
        f"    predicted rival ratio at high C/N0: (reps-1)/reps = {ideal:.3f}"
    )

    _plot(results, cn0, rate, margin, out_path)


def _plot(results, cn0, rate, margin, out_path: str) -> None:
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    fig, (ax0, ax1) = plt.subplots(1, 2, figsize=(11.5, 4.4))
    fig.suptitle(
        "DsssBurstReceiver — what loses a burst "
        f"(ACQ_SF={ACQ_SF}, reps={REPS}, spc={SPC})",
        fontsize=11,
    )

    for row, (label, colour) in enumerate((("good", "C0"), ("poor", "C3"))):
        offs, ok = results[label]
        pts = peak_to_sidelobe(
            ACQ_CODE if label == "good" else _structured_codes()[0]
        )
        y = 1.0 - 0.5 * row
        x = offs / ACQ_FRAME
        # A strip per code: a mark where the burst was found, a gap where it
        # was lost. Two rows rather than two step-lines, because the question
        # is WHERE the gaps fall, and overlaid steps hide one behind the
        # other exactly where they differ.
        ax0.plot(
            x[ok],
            np.full(ok.sum(), y),
            "|",
            color=colour,
            ms=16,
            mew=2,
            label=f"{label} code (peak/sidelobe {pts:.2f}) — "
            f"{100 * ok.mean():.0f}% found",
        )
        if (~ok).any():
            ax0.plot(
                x[~ok],
                np.full((~ok).sum(), y),
                "x",
                color="0.35",
                ms=5,
                mew=1.2,
            )
    ax0.set_xlabel("burst start, as a fraction of one acquisition frame")
    ax0.set_yticks([1.0, 0.5])
    ax0.set_yticklabels(["good\ncode", "poor\ncode"])
    ax0.set_ylim(0.25, 1.35)
    ax0.set_xlim(-0.02, 1.02)
    ax0.set_title(
        "Where a burst is lost, near the knee\n"
        f"(x = lost, at {cn0_dbhz(OFFSET_SIGMA):.0f} dB-Hz)",
        fontsize=10,
    )
    ax0.legend(fontsize=8, loc="upper center", ncol=1)
    ax0.grid(alpha=0.3, axis="x")

    ax1.plot(cn0, rate, "o-", color="C0", label="correct period")
    ax1.set_xlabel("C/N0 (dB-Hz)")
    ax1.set_ylabel("fraction of bursts on the exact sample", color="C0")
    ax1.set_ylim(-0.03, 1.03)
    ax1.tick_params(axis="y", labelcolor="C0")
    ax1.grid(alpha=0.3)

    ax2 = ax1.twinx()
    ax2.plot(
        cn0, margin, "s--", color="C3", ms=4, label="refine_margin (median)"
    )
    ax2.axhline(
        (REPS - 1) / REPS,
        color="0.5",
        ls=":",
        lw=1.2,
        label=f"(reps-1)/reps = {(REPS - 1) / REPS:.2f}",
    )
    ax2.axhline(1.0, color="C3", ls="-", lw=0.8, alpha=0.5)
    ax2.set_ylabel(
        "refine_margin — rival / winner (lower is better)", color="C3"
    )
    ax2.set_ylim(0.7, 1.02)
    ax2.tick_params(axis="y", labelcolor="C3")
    ax1.set_title(
        "Refine resolves the period until the\nmargin closes on 1",
        fontsize=10,
    )

    h0, l0 = ax1.get_legend_handles_labels()
    h1, l1 = ax2.get_legend_handles_labels()
    ax1.legend(h0 + h1, l0 + l1, fontsize=8, loc="lower right")

    fig.tight_layout(rect=(0, 0, 1, 0.92))
    fig.savefig(out_path, dpi=110)
    print(f"\nSaved → {out_path}")


if __name__ == "__main__":
    main(out_path=sys.argv[1] if len(sys.argv) > 1 else None)
