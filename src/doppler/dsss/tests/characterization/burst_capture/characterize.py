"""Characterization of `BurstCapture` — what a caller has to know to use it.

A **characterization**, not an example: it sweeps many trials and is run
deliberately by `make characterize`. See
`doppler.dsss.tests.characterization` for why that distinction exists.

Phase 7 of `docs/dev/contributing/adding-algorithms.md`, for the object
designed in `docs/design/burst-capture.md`. It deliberately does NOT re-measure
what `dsss_burst_receiver`'s characterization already covers — the framing
sweep and refine's period discrimination against C/N0 live there. What it
measures is the two things that are the CAPTURE's own and that nothing has
swept: how close two bursts may be, and how a caller separates a real window
from a spurious one.

The two panels
--------------
**Left — the spacing floor, against the number the header states.**
`refine_span`'s own documentation says the gap a caller must leave is
`max(0, refine_span - burst_len)`, which for a burst longer than the refine
reach is **zero**. That number was derived, not measured, and reading the
reach as required silence had already cost 9% of airtime once
(doppler#1085) — so the correction went the other way and has not been
checked. This sweeps dead air from zero upwards and asks how many of the
transmitted bursts come back.

The distinction that makes the panel readable is between the two ways a pair
can go wrong: the second burst can be **lost** (a merge, or a suppression
window swallowing it), or an **extra** window can appear where no burst was.
They are plotted separately because a caller defends against them
differently — the first is a geometry constraint, the second is a filter.

**Right — separating a real window from a spurious one.** At `pfa = 1e-3`
over a surface this size a false alarm is expected, not a defect: this object
is a detector's output stage, and gating it on signal quality would make it
lie about what it found. So the caller filters, and the two read-backs it
filters with are `refine_margin` and `cn0_dbhz_est`. This panel scatters both
for every window the capture emitted across the sweep, coloured by whether a
burst was actually transmitted there, and reports the separation each one
buys.

`refine_margin` is the runner-up code period's score over the winner's, so
**lower is better** and a value near 1 means the period was not resolved. A
window sitting on noise has nothing to resolve, which is exactly why the
statistic separates.

What it measures, not what it hopes
-----------------------------------
Every trial drives the real object — samples in, windows out — and scores a
window as REAL only if its `preamble_start` lands within one chip of a burst
that was actually transmitted. Nothing is scored against the object's own
opinion of itself.
"""

from __future__ import annotations

import numpy as np

from doppler.dsss import BurstCapture
from doppler.wfm import PN, Composer, Segment

# ── Geometry ────────────────────────────────────────────────────────────────
# The C suite's geometry, so the two bodies of evidence describe one object.
ACQ_SF, DATA_SF, REPS, SPC = 31, 8, 4, 4
#: Barker-13, the frame sync word the generator prepends to the payload.
SYNC = np.array([0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 1, 0], dtype=np.uint8)
#: Symbols after the preamble: sync | payload | CRC-16.
PAYLOAD_SYMS = 61
BURST_LEN = (REPS * ACQ_SF + PAYLOAD_SYMS * DATA_SF) * SPC
CHIP_RATE = 1.0e6
CODE_PERIOD = ACQ_SF * SPC

#: Noise floor for the sweeps. Well above the knee, so a burst that is missed
#: was missed by GEOMETRY rather than by sensitivity -- which is the question
#: the left panel asks. Sensitivity is the receiver characterization's.
SIGMA = 0.02
#: Trials per spacing. Each uses a different noise seed and a different
#: absolute position, so a result is not an artifact of one alignment.
TRIALS = 12
#: Dead air swept, in samples, from touching to well past a whole burst.
GAPS = [0, 32, 64, 128, 256, 512, 1024, 2048, 3072, 4096]


def acq_code() -> np.ndarray:
    """An m-sequence: a code whose worst autocorrelation sidelobe is near its
    peak sets the CFAR reference from its own structure rather than from
    noise, and measures the code instead of the object."""
    return (
        np.asarray(
            PN(poly=0, seed=1, length=5).generate(ACQ_SF), dtype=np.uint8
        )
        & 1
    )


def data_code() -> np.ndarray:
    return np.array([(i >> 1) & 1 for i in range(DATA_SF)], dtype=np.uint8)


#: Payload bits, fixed so every trial transmits the same burst. The frame the
#: generator builds is `sync | payload | CRC-16`, which is what makes
#: BURST_LEN below the length it is.
PAYLOAD_BITS = np.array(
    [(i * 7 + 3) & 1 for i in range(PAYLOAD_SYMS - len(SYNC) - 16)],
    dtype=np.uint8,
)


def burst() -> np.ndarray:
    """One burst, from the library's own generator.

    `Segment(type="dsss")` is doppler's declarative DSSS burst source and it
    is what `wfmgen` drives, so the waveform measured here is the waveform the
    tool emits. Rolling it by hand with numpy would be a private fourth copy
    of a waveform the library already owns, and the copies drift: this file's
    first version built the preamble and payload itself and had no CRC at all,
    which is a different burst from the one any caller transmits.
    """
    seg = Segment(
        type="dsss",
        fs=CHIP_RATE * SPC,
        freq=0.0,
        snr=100.0,  # clean; the noise floor is added by scene()
        snr_mode="esno",
        seed=1,
        sps=SPC,
        acq_code=acq_code().tobytes(),
        acq_reps=REPS,
        data_code=data_code().tobytes(),
        sync=SYNC.tobytes(),
        payload=PAYLOAD_BITS.tobytes(),  # CRC-16 auto-appended
        gap_noise="auto",
        off_samples=0,
    )
    return np.asarray(Composer([seg]).compose(), dtype=np.complex64)[
        :BURST_LEN
    ]


def scene(at: list[int], n: int, seed: int, sigma: float = SIGMA):
    """Noise everywhere, bursts at `at`."""
    rng = np.random.default_rng(seed)
    x = (rng.normal(0.0, sigma, n) + 1j * rng.normal(0.0, sigma, n)).astype(
        np.complex64
    )
    b = burst()
    for a in at:
        m = min(b.size, n - a)
        x[a : a + m] += b[:m]
    return x


def capture() -> BurstCapture:
    return BurstCapture(
        acq_code(),
        burst_len=BURST_LEN,
        reps=REPS,
        spc=SPC,
        chip_rate=CHIP_RATE,
        cn0_dbhz=55.0,
    )


def run_pair(gap: int, seed: int) -> tuple[int, int, list[tuple]]:
    """One trial: two bursts `gap` samples apart, edge to edge.

    Returns ``(found, extra, rows)`` — how many of the two transmitted bursts
    came back, how many windows named a position no burst occupied, and one
    ``(margin, cn0, is_real)`` row per window emitted.
    """
    rng = np.random.default_rng(seed)
    first = int(rng.integers(6000, 12000))
    at = [first, first + BURST_LEN + gap]
    n = at[1] + 4 * BURST_LEN + 20_000
    x = scene(at, n, seed)

    cap = capture()
    cap.push(x)
    ev = cap.events()

    tol = SPC  # one chip
    starts = list(ev["preamble_start"])
    found = sum(1 for a in at if any(abs(int(s) - a) <= tol for s in starts))
    rows = []
    for k, s in enumerate(starts):
        real = any(abs(int(s) - a) <= tol for a in at)
        rows.append(
            (float(ev["refine_margin"][k]), float(ev["cn0_dbhz_est"][k]), real)
        )
    extra = sum(1 for _, _, real in rows if not real)
    return found, extra, rows


def sweep_spacing():
    """``(gaps, found_rate, extra_per_pair, rows)`` over `GAPS`."""
    found_rate, extra_rate, rows = [], [], []
    for gap in GAPS:
        f = e = 0
        for t in range(TRIALS):
            fi, ei, ri = run_pair(gap, seed=1000 + 97 * t + gap)
            f += fi
            e += ei
            rows.extend(ri)
        found_rate.append(f / (2.0 * TRIALS))
        extra_rate.append(e / float(TRIALS))
    return np.array(GAPS), np.array(found_rate), np.array(extra_rate), rows


def separation(rows) -> dict:
    """How well each read-back separates a real window from a spurious one.

    Reported as the gap between the two populations' medians, in the
    statistic's own units -- a difference a caller can put a threshold in.
    """
    real = [r for r in rows if r[2]]
    spur = [r for r in rows if not r[2]]
    out = {"n_real": len(real), "n_spurious": len(spur)}
    if real and spur:
        out["margin_real"] = float(np.median([r[0] for r in real]))
        out["margin_spurious"] = float(np.median([r[0] for r in spur]))
        out["cn0_real"] = float(np.median([r[1] for r in real]))
        out["cn0_spurious"] = float(np.median([r[1] for r in spur]))
    return out


def _plot(gaps, found, extra, rows, out_path: str) -> None:
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    fig, (ax0, ax1) = plt.subplots(1, 2, figsize=(11.5, 4.4))

    ax0.plot(gaps, 100.0 * found, "o-", label="transmitted bursts captured")
    ax0.plot(gaps, 100.0 * extra / 2.0, "s--", label="spurious windows / 2")
    claimed = max(0, 2480 - BURST_LEN)  # refine_span - burst_len at this geom
    ax0.axvline(
        claimed,
        color="crimson",
        ls=":",
        label=f"header's required gap ({claimed})",
    )
    ax0.set_xlabel("dead air between bursts (samples)")
    ax0.set_ylabel("percent")
    ax0.set_title(f"Spacing floor  (burst_len = {BURST_LEN})")
    ax0.set_ylim(-5, 115)
    ax0.grid(alpha=0.3)
    ax0.legend(fontsize=8)

    real = [r for r in rows if r[2]]
    spur = [r for r in rows if not r[2]]
    if real:
        ax1.scatter(
            [r[0] for r in real],
            [r[1] for r in real],
            s=14,
            alpha=0.6,
            label=f"at a real burst (n={len(real)})",
        )
    if spur:
        ax1.scatter(
            [r[0] for r in spur],
            [r[1] for r in spur],
            s=14,
            alpha=0.6,
            marker="x",
            label=f"spurious (n={len(spur)})",
        )
    ax1.axvline(
        (REPS - 1) / REPS,
        color="grey",
        ls=":",
        label=f"(reps-1)/reps = {(REPS - 1) / REPS:.2f}",
    )
    ax1.set_xlabel("refine_margin   (rival / winner — lower is better)")
    ax1.set_ylabel("cn0_dbhz_est (dB-Hz)")
    ax1.set_title("Separating a real window from a spurious one")
    ax1.grid(alpha=0.3)
    ax1.legend(fontsize=8)

    fig.suptitle(
        "BurstCapture — the spacing floor, and the two read-backs a caller "
        "filters with",
        fontsize=11,
    )
    fig.tight_layout()
    fig.savefig(out_path, dpi=110)
    print(f"  wrote {out_path}")


def main(out_path: str | None = None) -> None:
    out_path = out_path or "burst_capture_characterization.png"
    print("BurstCapture characterization")
    print(f"  burst_len={BURST_LEN}  code_period={CODE_PERIOD}  reps={REPS}")

    gaps, found, extra, rows = sweep_spacing()
    print("\n  dead air   captured   spurious/pair")
    for g, f, e in zip(gaps, found, extra):
        print(f"  {g:>8}   {100.0 * f:>7.1f}%   {e:>12.2f}")

    sep = separation(rows)
    print("\n  separation of real from spurious windows")
    for k, v in sep.items():
        print(f"    {k:<18} {v}")

    _plot(gaps, found, extra, rows, out_path)


if __name__ == "__main__":
    import sys

    main(sys.argv[1] if len(sys.argv) > 1 else None)
