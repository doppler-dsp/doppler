"""Realized acquisition Pfa over a pure-noise run — measured in ONE place.

The certification (`validation/acq/`) and the characterization
(`characterization/burst_acquisition/`) both need to know what false-alarm
rate the detector actually delivers against the rate the caller asked for.
They need it at different scales — the certification wants a number fast
enough to run in `make validate-check`, the characterization wants enough
frames to resolve a small excursion — but that is a difference in `n_frames`,
not in the measurement. A second implementation of it would be a second
convention, and this repo has paid for that four times over.

**Why the number is not simply the target.** `acq_commit_thresholds()` sizes
the per-cell false-alarm probability from the NATIVE cell count:

    pfa_cell = 1 - (1 - pfa) ** (1 / (searched_bins * code_bins))

with no term for `ACQ_DOPPLER_INTERP`. That is deliberate and argued in
`native/src/acq/acq_core.c` — frequency-domain zero-padding is exact
band-limited interpolation, so the added rows carry no new *information*.

The argument is true and it is not the relevant one, because the detector
takes the **maximum** over the surface rather than integrating it. The
maximum of a band-limited process sampled finely is stochastically larger
than the maximum of the same process sampled coarsely: interpolation finds
noise peaks that previously fell between bins and were missed. That is the
same mechanism as the scalloping-loss win it was added for (doppler#1002,
~3.9 dB -> ~0.9 dB), applied to H0.

**Measured, and it decomposes into two errors of opposite sign.** 60,000
pure-noise frames at `pfa = 1e-2` (the highest-count point, so the tightest
bars), `doppler_bins = 8`, `n_noncoh = 1`, identical build and seed with only
`ACQ_DOPPLER_INTERP` changed:

    N_eff(1) / N            0.89 +/- 0.04   the native model is CONSERVATIVE
    N_eff(2) / N_eff(1)     1.86 +/- 0.09   what interpolation adds
    N_eff(2) / N            1.65 +/- 0.05   what a caller actually gets

The two partly cancel, which is why the delivered rate is 1.65x rather than
1.86x. Quoting 1.86 as "the" error would be quoting the ratio between two
BUILDS, not the rate anyone experiences -- and a correction of 1.86 applied to
a model that is already 11% conservative would land at 0.89 of target.

`N_eff(1) < N` because adjacent DFT bins of a white-noise transform are not
perfectly independent, so the Sidak count slightly overstates the looks. That
direction is safe: fewer false alarms than asked for.

**The form of the calculation is right; only `N` is wrong.** The ratio holds
across three decades of target -- 1.65 / 1.40 / 1.50 at 1e-2 / 1e-3 / 1e-4
with interpolation on, 0.89 / 0.77 / 0.83 with it off -- which is the
signature of a multiplicative cell-count error. A miscalibrated per-cell
threshold, or an unaccounted CFAR estimation loss from a finite reference
window, would DRIFT with the target instead. Neither does.

Tracked as doppler#1064. Until it is fixed, both callers ratchet against the
measured ratio rather than asserting the target, so the gap cannot widen
unnoticed and closing it is a ratchet that only shrinks.
"""

from __future__ import annotations

import math
from typing import TYPE_CHECKING

import numpy as np

if TYPE_CHECKING:
    from collections.abc import Callable

#: Ratchet on the realized-Pfa-to-target ratio with doppler#1002's Doppler
#: interpolation active. It may only SHRINK; 1.0 is the goal and closing
#: doppler#1064 earns it.
#:
#: Set at 2.2 against a best estimate of 1.65 +/- 0.05, which is deliberate
#: headroom rather than slack: the certification measures at `pfa = 1e-3`
#: over 20,000 frames, where 20 expected hits give the estimate a Poisson
#: spread of roughly +/-0.22 -- so a bound any tighter would go red on a
#: run that found nothing wrong. Tighten it when the measurement gets
#: cheaper, or when the defect is fixed and the ratio collapses to ~0.9.
PFA_RATIO_RATCHET = 2.2


def realized_pfa(
    make_engine: Callable[[], object],
    frame_len: int,
    n_frames: int,
    seed: int = 0,
    chunk: int = 64,
) -> float:
    """Achieved system Pfa over `n_frames` pure-noise frames.

    The CFAR test statistic is scale-invariant, so the rate does not depend
    on the noise level and one number characterises the threshold.

    Parameters
    ----------
    make_engine : callable
        Returns a fresh acquisition engine. Taken as a factory rather than an
        instance so the caller owns the configuration being characterised --
        the engine's grid is what sets the cell count the threshold is sized
        from, and measuring one engine's Pfa against another's target is the
        error this signature exists to prevent.
    frame_len : int
        Samples per decision (`code_bins * doppler_bins` for a
        one-push-one-decision grid).
    n_frames : int
        Frames to push. The estimate's own precision is Poisson: at a target
        of 1e-3 you need ~40,000 frames before a 1.85x excursion is more than
        two sigma, which is why the certification and the characterization
        pass different values.
    seed : int
        Noise seed.
    chunk : int
        Frames per push; a throughput knob only.

    Returns
    -------
    float
        Hits divided by frames pushed.
    """
    rng = np.random.default_rng(20240 + seed)
    a = make_engine()
    hits = 0
    pushed = 0
    while pushed < n_frames:
        f = min(chunk, n_frames - pushed)
        m = f * frame_len
        x = (1.0 / math.sqrt(2.0)) * (
            rng.standard_normal(m) + 1j * rng.standard_normal(m)
        )
        hits += len(a.push(x.astype(np.complex64)))
        pushed += f
    return hits / n_frames


def pfa_sigma(hat: float, target: float, n_frames: int) -> float:
    """How many Poisson sigma the realized rate sits from the target.

    Reported alongside the ratio because a ratio alone cannot say whether a
    run resolved anything: 1.85x on 200 frames is noise, and on 40,000 it is
    +5.4 sigma.
    """
    mu = target * n_frames
    if mu <= 0.0:
        return 0.0
    return (hat * n_frames - mu) / math.sqrt(mu)
