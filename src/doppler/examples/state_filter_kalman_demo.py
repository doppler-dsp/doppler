"""state_filter_kalman_demo.py — a loop sized from the signal, a fast start.

:class:`doppler.track.StateFilter` is sized from a bandwidth. A caller who
knows the detector's noise and how fast the carrier's rate can change would
rather give those two figures and get the bandwidth that is optimal for them:
that is :class:`doppler.track.KalmanStateFilter`. It takes the rms detector
error when locked (``sigma_e``) and the rms change of the rate per update
(``sigma_a``), both in the loop's own units, or ``bn`` alone.

Two things are shown. First, the one-line conversion a caller with a
carrier-to-noise density and a jerk figure makes to get the two figures (an
example, not an API: the filter has no detector model in it). Second, the
high-gain start: an initial frequency uncertainty larger than the steady
state's makes the gains rise for a while and the loop settle to a frequency
step about 1.7 times as fast as the fixed-gain loop of the same final
bandwidth, inside the stability rectangle the constructor enforces.

Run it::

    python src/doppler/examples/state_filter_kalman_demo.py
"""

from __future__ import annotations

# --8<-- [start:figures]
import math

from doppler.track import KalmanStateFilter

CN0_DBHZ = 40.0  # carrier-to-noise density at the detector
T = 1e-3  # one loop update per millisecond
JERK = 500.0  # worst carrier jerk, cycles per second cubed

snr = 10 ** (CN0_DBHZ / 10) * T  # per-update carrier-to-noise ratio
sigma_e = 1 / (2 * math.pi * math.sqrt(2 * snr))  # cycles, an arctan detector
sigma_a = JERK * T**3  # cycles per update^2, per update

loop = KalmanStateFilter(sigma_e=sigma_e, sigma_a=sigma_a)  # per update
print(f"bn = {loop.bandwidth() / T:.1f} Hz  (bn*t = {loop.bandwidth():.4f})")
# --8<-- [end:figures]


# --8<-- [start:start]
import numpy as np

from doppler.track import KalmanStateFilter, StateFilter


def settle(f, f0=2e-3, n=4000):
    """Updates until the phase error stays under 5% of its peak after a
    frequency step of ``f0`` cycles per update."""
    phase_in = phase_osc = 0.0
    err = np.empty(n)
    for i in range(n):
        phase_in += f0
        err[i] = phase_in - phase_osc
        phase_osc += f.step(err[i])
    peak = np.abs(err).max()
    return int(np.nonzero(np.abs(err) > 0.05 * peak)[0].max()) + 1


fixed = StateFilter(bn=loop.bandwidth(), t=1.0)  # the same bandwidth
steady = KalmanStateFilter(sigma_e=sigma_e, sigma_a=sigma_a)
fast = KalmanStateFilter(
    sigma_e=sigma_e, sigma_a=sigma_a, sigma_f0=4.0 * steady.sigma_f
)
n_fixed, n_fast = settle(fixed), settle(fast)
# --8<-- [end:start]


def main() -> None:
    assert loop.stable(delay=2.0)
    assert 1e-6 < loop.bandwidth() < 0.12
    # the constructor refuses what the stability rectangle does not cover
    refused = False
    try:
        KalmanStateFilter(
            sigma_e=sigma_e, sigma_a=sigma_a, sigma_f0=50 * steady.sigma_f
        )
    except ValueError:
        refused = True
    assert refused
    print(f"settling to a frequency step: fixed {n_fixed}, fast {n_fast}")
    assert n_fast < 0.8 * n_fixed
    print("OK: sized from the signal, and the high-gain start settles faster")


if __name__ == "__main__":
    main()
