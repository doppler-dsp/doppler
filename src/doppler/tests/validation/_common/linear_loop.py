"""The closed-loop reference every steered object is measured against.

A tracking loop in this library is always the same three pieces: a
detector that reports a phase or timing error, a `LoopFilter` that turns
that error into a control, and an oscillator the control steers. The
objects differ only in the detector — `costas` reports a carrier phase
error through a decision-directed S-curve, `dll` an early-late
correlation, `symsync` a Gardner or DTTL timing error, `carrier_nda` an
M-th-power law.

So the detector is the parameter here, and everything else is shared.
Substituting the IDEAL detector — plain subtraction, which reports the
error exactly, with no S-curve shape, no finite slope, no self-noise and
no pull-in range — gives the **limit on closed-loop behaviour**: the
best any loop built on this filter and this oscillator can do. A real
detector's performance is then a deduction from this reference rather
than a separate, unanchored measurement, which is what lets two objects'
numbers be compared at all.

This module owns the harness. Each object's `validate.py` supplies its
own detector and bandwidth and reports the difference.

Nothing here models the C. The loop filter and the oscillator are the
real objects, stepped one sample at a time through their own bindings.
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import TYPE_CHECKING

import numpy as np

from doppler.source import NCO
from doppler.track import LoopFilter

if TYPE_CHECKING:
    from collections.abc import Callable
    from pathlib import Path

W = 2.0**32

# Defaults for the standard drive set. An object may override any of
# them, but keeping the defaults identical is what makes two objects'
# reports comparable at a glance.
N = 4000
K0 = 500  # sample at which the disturbance arrives
STEP = 0.25  # cycles
RAMP = 1e-3  # cycles/sample of frequency offset
BN = 0.01
ZETA = 0.707


def wrap_cycles(c):
    """Wrap cycles to ``[-0.5, 0.5)``.

    A phase is modular, so the raw difference between two phases is only
    meaningful once folded into a half-cycle band. Inside that band the
    ideal detector is exactly linear, which is the regime this reference
    characterises; outside it, every detector including this one is
    reporting the wrong branch.
    """
    return c - np.floor(c + 0.5)


def ideal_detector(ref_phase: float, osc_phase: float) -> float:
    """Plain subtraction — the detector that cannot be improved on.

    Parameters
    ----------
    ref_phase, osc_phase : float
        Reference and oscillator phase for this update, in cycles.

    Returns
    -------
    float
        The phase error in cycles, wrapped to ``[-0.5, 0.5)``.
    """
    return float(wrap_cycles(ref_phase - osc_phase))


@dataclass
class LoopRun:
    """One closed-loop record: what went in, and every internal signal."""

    name: str
    drive: np.ndarray  # reference phase, cycles
    error: np.ndarray  # detector output = loop input, cycles
    control: np.ndarray  # loop filter output, cycles/sample
    phase: np.ndarray  # oscillator phase, cycles
    bn: float
    zeta: float
    k0: int


@dataclass
class Settling:
    """How a run converged, in the terms a loop is specified in."""

    peak: float  # largest |error| after the disturbance, cycles
    samples: int  # last sample above `frac` of peak, relative to k0
    residual: float  # largest |error| over the final tail, cycles


def run(
    drive: np.ndarray,
    *,
    name: str = "",
    bn: float = BN,
    zeta: float = ZETA,
    k0: int = K0,
    detector: Callable[[float, float], float] = ideal_detector,
) -> LoopRun:
    """Close the loop around a real `LoopFilter` and a real `NCO`.

    One update per sample: read the oscillator phase, form the error
    through `detector`, filter it, and steer the oscillator's control
    port with the result. The oscillator's own `phase_inc` stays zero
    throughout — the control carries the whole rate, so the record shows
    what the loop did rather than what it was initialised with.

    Parameters
    ----------
    drive : ndarray
        Reference phase per sample, in cycles.
    detector : callable, optional
        ``(ref_phase, osc_phase) -> error``, both in cycles. Defaults to
        `ideal_detector`; pass an object's own discriminator to measure
        how far short of the limit it falls.

    Returns
    -------
    LoopRun
    """
    lf = LoopFilter(bn=bn, zeta=zeta, t=1.0)
    nco = NCO(0.0, 0)
    n = drive.size
    err = np.empty(n)
    ctl = np.empty(n)
    pha = np.empty(n)
    one = np.zeros(1, dtype=np.float64)
    for k in range(n):
        pha[k] = nco.phase / W
        err[k] = detector(float(drive[k]), float(pha[k]))
        ctl[k] = lf.step(float(err[k]))
        one[0] = ctl[k]
        nco.steps_u32_ctrl(one)
    return LoopRun(name, drive, err, ctl, pha, bn, zeta, k0)


def step_drive(amp: float = STEP, *, n: int = N, k0: int = K0) -> np.ndarray:
    """A phase step of `amp` cycles at sample `k0`."""
    d = np.zeros(n)
    d[k0:] = amp
    return d


def ramp_drive(slope: float = RAMP, *, n: int = N, k0: int = K0) -> np.ndarray:
    """A frequency offset of `slope` cycles/sample, starting at `k0`.

    In phase this is a ramp, which is what a type-2 loop must absorb into
    its integrator to leave no steady-state phase error behind.
    """
    d = np.zeros(n)
    d[k0:] = slope * (np.arange(n)[k0:] - k0)
    return d


def standard_drives(
    *, n: int = N, k0: int = K0, step: float = STEP, ramp: float = RAMP
) -> dict[str, np.ndarray]:
    """Both signs of phase step, plus a frequency ramp.

    Both signs because a linear loop must be symmetric in sign, and an
    asymmetry is the first sign of a detector or a conversion that is
    not; the ramp because it is what separates a type-2 loop from a
    type-1 one.
    """
    return {
        f"+{step:g} cycle step": step_drive(step, n=n, k0=k0),
        f"-{step:g} cycle step": step_drive(-step, n=n, k0=k0),
        f"ramp, {ramp:g} cyc/sample": ramp_drive(ramp, n=n, k0=k0),
    }


def settle(r: LoopRun, *, frac: float = 0.01, tail: int = 200) -> Settling:
    """Peak error, settling sample and residual for one run."""
    seg = r.error[r.k0 :]
    peak = float(np.abs(seg).max())
    over = np.flatnonzero(np.abs(seg) > frac * peak)
    return Settling(
        peak=peak,
        samples=int(over[-1]) if over.size else 0,
        residual=float(np.abs(r.error[-tail:]).max()),
    )


def plot(
    runs: dict[str, LoopRun],
    path: Path | str,
    *,
    title: str | None = None,
    applied_ramp: float | None = RAMP,
) -> None:
    """Loop input, loop-filter output and oscillator phase, per drive.

    Three rows because those are the three places a loop can be wrong in
    a different way: the error says whether it converged, the control
    says what it converged ON, and the phase says whether it tracked the
    thing it was asked to.
    """
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    names = list(runs)
    fig, axes = plt.subplots(3, len(names), figsize=(15, 9), sharex=True)
    if len(names) == 1:
        axes = axes.reshape(3, 1)

    for j, nm in enumerate(names):
        r = runs[nm]
        k = np.arange(r.drive.size)

        ax = axes[0][j]
        ax.plot(k, r.drive, lw=1.4, label="reference phase")
        # The oscillator phase is modular; unwrap it so a ramp reads as a
        # ramp rather than a sawtooth against the reference.
        ax.plot(
            k,
            np.unwrap(r.phase * 2 * np.pi) / (2 * np.pi),
            lw=1.2,
            ls="--",
            label="NCO phase (unwrapped)",
        )
        ax.axvline(r.k0, color="0.6", lw=0.8)
        ax.set_title(nm)
        if j == 0:
            ax.set_ylabel("phase (cycles)")
        ax.grid(True, alpha=0.3)
        ax.legend(fontsize=7, loc="best")

        ax = axes[1][j]
        ax.plot(k, r.error, lw=1.3, color="tab:red")
        ax.axhline(0.0, color="0.6", lw=0.8)
        ax.axvline(r.k0, color="0.6", lw=0.8)
        if j == 0:
            ax.set_ylabel("loop input\n= detector output (cycles)")
        ax.grid(True, alpha=0.3)

        ax = axes[2][j]
        ax.plot(k, r.control, lw=1.3, color="tab:green")
        ax.axhline(0.0, color="0.6", lw=0.8)
        ax.axvline(r.k0, color="0.6", lw=0.8)
        if applied_ramp is not None and "ramp" in nm:
            ax.axhline(
                applied_ramp,
                color="tab:orange",
                ls=":",
                lw=1.2,
                label=f"applied offset {applied_ramp:g}",
            )
            ax.legend(fontsize=7, loc="best")
        if j == 0:
            ax.set_ylabel("loop filter output\n(cycles/sample)")
        ax.set_xlabel("sample")
        ax.grid(True, alpha=0.3)

    r0 = runs[names[0]]
    fig.suptitle(
        title
        or (
            f"Closed-loop reference: detector -> loop filter -> NCO "
            f"(bn = {r0.bn}, zeta = {r0.zeta})"
        ),
        fontsize=13,
    )
    fig.tight_layout()
    fig.savefig(str(path), dpi=110)
    plt.close(fig)
