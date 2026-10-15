"""Realized acquisition Pfa over a pure-noise run — measured in ONE place.

The certification (`validation/acq/`) and the characterization
(`characterization/burst_acquisition/`) both need to know what false-alarm
rate the detector actually delivers against the rate the caller asked for.
They need it at different scales — the certification wants a number fast
enough to run in `make validate-check`, the characterization wants enough
frames to resolve a small excursion — but that is a difference in `n_frames`,
not in the measurement. A second implementation of it would be a second
convention, and this repo has paid for that four times over.

**Why it is worth measuring at all.** The gate is a maximum over the
Doppler-INTERPOLATED surface, and `acq_commit_thresholds()` prices it with a
Sidak split over the cells that maximum ranges over (`acq_band_cells()` in
`native/src/acq/acq_core.c`: every searched row of the interpolated surface,
times `code_bins`, times the rate hypotheses). That is the one definition;
this module only measures what it delivers.

Until doppler#2218 the split counted the NATIVE cells, with no term for
`ACQ_DOPPLER_INTERP`, on the grounds that zero-padding adds no new
*information*. True, and not the relevant property: a maximum over a finer
sampling of the same band-limited process is stochastically larger, so
interpolation found noise peaks that used to fall between bins (the
scalloping-loss win of doppler#1002, applied to H0). Every D > 1 engine
delivered about twice its target: 1.86-1.93x at 1e-3 on the burst engine,
1e6 dwells a row, against 0.94-0.99x for a maximum over the native rows only.
(The 0.89 "conservative native model" once quoted beside it was doppler#1183,
an even depth never searching its outermost bin, not cell correlation.)

Priced over the interpolated cells the rate is a never-exceed bound: each
interpolated row is correlated with its native neighbours, so counting it as
a full look over-counts slightly. Measured 0.95-0.97x at 1e-3, which is
about 0.015 dB of sensitivity. The measured per-geometry `N_eff` that would
recover it is doppler#1064, still open. Both callers ratchet the realized
ratio so that a regression back toward 2x is caught.
"""

from __future__ import annotations

import math
from typing import TYPE_CHECKING

import numpy as np

if TYPE_CHECKING:
    from collections.abc import Callable

#: Ratchet on the realized-Pfa-to-target ratio. It may only SHRINK.
#:
#: 1.4 since doppler#2218 put the delivered rate on target (it was 2.2
#: against a delivered ~1.65-1.85). A BACKSTOP: the exact guard is the C pin,
#: test_acq_core.c's _acq_pfa_split_check, which holds pfa_cell to the band
#: mask's cell count. This catches what slips past it, at the frames that
#: resolve it: the certification (the caller CI runs, through
#: test_validation_limits) measures 60,000 frames at `pfa = 1e-3`. An
#: on-target detector expects 60 hits and exceeds 84 by chance about 0.1% of
#: the time. The native pricing, put back, measured 1.76x on this engine
#: (21 seeds, mean 105 hits; the certification's seed 102), which stays at
#: or under 84 about 2% of the time (none of the 21 did). At 20,000 frames no
#: bound does both: that run read 1.45x, inside a 1.5 bound. The
#: characterization still measures 20,000 frames for its figure, where the
#: same bound is a looser check (about 3% false red).
PFA_RATIO_RATCHET = 1.4


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
