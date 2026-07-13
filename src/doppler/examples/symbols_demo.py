"""symbols_demo.py — bring your own constellation with ``type="symbols"``.

The built-in `wfmgen` modulations (`bpsk`, `qpsk`, …) each pick a *fixed*
bit→symbol map. `type="symbols"` removes that ceiling: you hand the engine a
complex64 stream and **each element becomes an output point directly**. The
synth oversamples it by `sps`, cycles it to fill the request, and — with
`pulse="rrc"` — shapes it through the same matched-FIR path every other
modulation uses. That one hook expresses any constellation: pi/4-QPSK, QAM,
APSK, or something you invented this morning.

This demo showcases two constellations you cannot get from a modulation enum,
each shown two ways — the *rect* pulse (a boxcar matched filter recovers the
exact points) and the *RRC* pulse (the continuous shaped trajectory the radio
actually transmits):

Top-left — **pi/4-QPSK**, ideal points. Two QPSK rings offset by π/4 (eight
points total), used on alternate symbols. "Compute the symbols, pass them" —
there is no π/4-QPSK enum, and none is needed.

Top-right — **16-QAM**, ideal points. A 4×4 grid normalised to unit average
power. A random symbol stream through the rect pulse; the boxcar matched filter
lands exactly on the sixteen points.

Bottom-left — **why pi/4-QPSK exists.** The distribution of the RRC-shaped
envelope `|x(t)|` over a long run. Plain QPSK (all symbols from one ring → 180°
flips) drives the envelope clean through zero, so its histogram has mass at the
origin; pi/4-QPSK (alternating rings → the phase step is capped at ±135°) has a
hard floor near 0.15 and never collapses. That floor drops the peak-to-average
ratio by ~0.5 dB and lets a power amplifier run closer to saturation — the
entire reason the modulation exists.

Bottom-right — **16-QAM, shaped.** The RRC-shaped I/Q trajectory threading
between the sixteen points — the band-limited waveform actually on the wire,
contrasted with the crisp rect grid above it.

Run:
    python src/doppler/examples/symbols_demo.py
"""

from __future__ import annotations

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

from doppler.wfm import Synth

SPS = 8
NSYM = 48
RRC = {"pulse": "rrc", "rrc_beta": 0.35, "rrc_span": 6}


def shaped(syms: np.ndarray, **kw) -> np.ndarray:
    """Run a constellation stream through the synth and return the samples.

    ``**kw`` selects the pulse: nothing for the rect (boxcar) pulse, or the
    ``RRC`` dict for root-raised-cosine shaping. The stream is force-cast to
    complex64 by the binding, so an ``np.complex128`` array is fine.
    """
    s = Synth(
        type="symbols", symbols=np.asarray(syms, np.complex64), sps=SPS, **kw
    )
    return np.asarray(s.steps(SPS * len(syms)))


def matched(syms: np.ndarray) -> np.ndarray:
    """Rect pulse + boxcar matched filter → the exact symbol points back."""
    return shaped(syms).reshape(len(syms), SPS).mean(axis=1)


# ── pi/4-QPSK: two QPSK rings offset by pi/4, used on alternate symbols ──────
ring_a = np.exp(1j * (np.pi / 4 + np.arange(4) * np.pi / 2))  # 45,135,225,315
ring_b = np.exp(1j * (np.arange(4) * np.pi / 2))  # 0,90,180,270
rng = np.random.default_rng(0)
idx = rng.integers(0, 4, NSYM)
even = np.arange(NSYM) % 2 == 0
pi4 = np.where(even, ring_a[idx], ring_b[idx]).astype(np.complex64)
qpsk = ring_a[idx].astype(np.complex64)  # one ring only → 180° flips

# ── 16-QAM: 4×4 grid, unit average power ────────────────────────────────────
grid = np.array([a + 1j * b for a in (-3, -1, 1, 3) for b in (-3, -1, 1, 3)])
grid /= np.sqrt((np.abs(grid) ** 2).mean())  # normalise to E[|s|²] = 1
qam = grid[rng.integers(0, 16, NSYM)].astype(np.complex64)

# ── plot ────────────────────────────────────────────────────────────────────
fig, ax = plt.subplots(2, 2, figsize=(11, 9))
fig.suptitle(
    'type="symbols" — bring your own constellation',
    fontsize=13,
)

# ideal constellations (rect pulse, matched-filtered)
for a, syms, title, lim in (
    (ax[0, 0], pi4, "pi/4-QPSK — ideal points (8)", 1.4),
    (ax[0, 1], qam, "16-QAM — ideal points (16)", 1.6),
):
    pts = matched(syms)
    a.scatter(pts.real, pts.imag, s=28, color="C0", zorder=3)
    a.axhline(0, color="k", lw=0.5)
    a.axvline(0, color="k", lw=0.5)
    a.set_title(title)
    a.set_xlabel("I")
    a.set_ylabel("Q")
    a.set_xlim(-lim, lim)
    a.set_ylim(-lim, lim)
    a.set_aspect("equal")
    a.grid(alpha=0.3)

# pi/4-QPSK vs QPSK: the RRC-shaped envelope distribution. A long run gives
# smooth statistics; the FIR warm-up/tail (rrc_span symbols each end) is
# trimmed so the transient ramp doesn't pollute the histogram.
a = ax[1, 0]
warm = RRC["rrc_span"] * SPS
long_idx = rng.integers(0, 4, 4000)
long_even = np.arange(4000) % 2 == 0
env_qpsk = np.abs(shaped(ring_a[long_idx].astype(np.complex64), **RRC))
env_pi4 = np.abs(
    shaped(
        np.where(long_even, ring_a[long_idx], ring_b[long_idx]).astype(
            np.complex64
        ),
        **RRC,
    )
)
env_qpsk, env_pi4 = env_qpsk[warm:-warm], env_pi4[warm:-warm]


def papr(e):
    return 10 * np.log10(e.max() ** 2 / (e**2).mean())


bins = np.linspace(0, 1.6, 60)
a.hist(
    env_qpsk,
    bins=bins,
    density=True,
    histtype="step",
    color="C3",
    label=f"QPSK  (PAPR {papr(env_qpsk):.1f} dB)",
)
a.hist(
    env_pi4,
    bins=bins,
    density=True,
    histtype="step",
    color="C0",
    label=f"pi/4-QPSK  (PAPR {papr(env_pi4):.1f} dB)",
)
a.axvline(0, color="k", lw=0.5)
a.set_title("RRC envelope |x(t)| — why pi/4-QPSK exists")
a.set_xlabel("|x(t)|")
a.set_ylabel("density")
a.legend(loc="upper left", fontsize=8)
a.grid(alpha=0.3)

# 16-QAM shaped trajectory over its ideal grid (short window for legibility)
a = ax[1, 1]
tj_qam = shaped(qam[:20], **RRC)
a.plot(tj_qam.real, tj_qam.imag, lw=0.7, alpha=0.8, color="C0")
ideal = matched(qam)
a.scatter(ideal.real, ideal.imag, s=24, color="C1", zorder=3)
a.axhline(0, color="k", lw=0.5)
a.axvline(0, color="k", lw=0.5)
a.set_title("16-QAM — RRC-shaped waveform on the wire")
a.set_xlabel("I")
a.set_ylabel("Q")
a.set_xlim(-1.7, 1.7)
a.set_ylim(-1.7, 1.7)
a.set_aspect("equal")
a.grid(alpha=0.3)

fig.tight_layout(rect=(0, 0, 1, 0.96))
fig.savefig("symbols_demo.png", dpi=110)
print("wrote symbols_demo.png")

# ── validate ─────────────────────────────────────────────────────────────────
# Rect pulse + boxcar matched filter is exact: every recovered point must
# land back on the transmitted constellation symbol (float32 round-off).
pi4_err = float(np.max(np.abs(matched(pi4) - pi4)))
qam_err = float(np.max(np.abs(matched(qam) - qam)))
assert pi4_err < 1e-6, f"pi/4-QPSK symbol recovery off by {pi4_err:.1e}"
assert qam_err < 1e-6, f"16-QAM symbol recovery off by {qam_err:.1e}"
# The envelope physics of panel 3: pi/4-QPSK's ±135° phase cap keeps the
# RRC envelope off the origin (hard floor near 0.15), while plain QPSK's
# 180° flips drive it clean through zero.
assert env_pi4.min() > 0.1, f"pi/4 envelope floor {env_pi4.min():.3f}"
assert env_qpsk.min() < 0.02, f"QPSK envelope min {env_qpsk.min():.3f}"
# ... which is worth ~0.5 dB of PAPR — the reason the modulation exists.
papr_gain = papr(env_qpsk) - papr(env_pi4)
assert papr_gain > 0.3, f"PAPR gain only {papr_gain:.2f} dB"
print(
    f"validated: exact recovery (|err| < 1e-6), envelope floors "
    f"{env_qpsk.min():.3f}/{env_pi4.min():.3f}, "
    f"PAPR gain {papr_gain:.2f} dB"
)
