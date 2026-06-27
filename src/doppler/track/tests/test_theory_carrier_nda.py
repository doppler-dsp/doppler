"""Theoretical-correctness tests for the NDA M-th-power carrier loop.

Validates the discriminator characteristic against closed form, for M in
{2, 4, 8}: ``phase_error`` is the scaled M-th-power detector ``Im(z^M)`` with
the gain-normalizing scale ``{1, ½, ¼}`` (S-curve slope 2 at lock for every M),
a sawtooth of period ``2π/M``.

The discriminator is probed through the loop: with a near-zero bandwidth and
the NCO at zero frequency the wipe-off is identity, so feeding one arm's worth
of a constant ``exp(jφ)`` makes ``last_error`` the discriminator output at φ.
The rigorous Monte-Carlo bounds (cold-start pull-in with no data/no timing,
jitter vs bn) live in ``native/validation/carrier_nda_{scurve,pullin}.c``.
"""

import numpy as np
import pytest

from doppler.track import CarrierNda

SPS, N = 8, 4
ARM = SPS // N  # arm_len


def _disc(m, phi):
    # near-zero bn freezes the loop; NCO at 0 => wipe-off is identity
    c = CarrierNda(bn=1e-9, zeta=0.707, init_norm_freq=0.0, sps=SPS, n=N, m=m)
    arm = np.full(ARM, np.exp(1j * phi), dtype=np.complex64)
    c.steps(arm)  # exactly one arm dump -> last_error = phase_error(phi)
    return c.last_error


@pytest.mark.parametrize("m", [2, 4, 8])
def test_phase_error_is_scaled_mth_power(m):
    scale = {2: 1.0, 4: 0.5, 8: 0.25}[m]
    seg = 2 * np.pi / m
    phis = np.linspace(-np.pi, np.pi, 361)
    meas = np.array([_disc(m, p) for p in phis])
    theory = scale * np.sin(m * phis)  # scale * Im(z^M)
    # compare on the linear branches (away from the ±seg/2 sawtooth snaps)
    wrapped = (phis + seg / 2) % seg - seg / 2
    guard = 3 * (phis[1] - phis[0])
    ok = np.abs(np.abs(wrapped) - seg / 2) > guard
    assert np.max(np.abs(meas[ok] - theory[ok])) < 1e-4


@pytest.mark.parametrize("m", [2, 4, 8])
def test_slope_is_two_at_lock(m):
    h = 1e-3 / m
    slope = (_disc(m, h) - _disc(m, -h)) / (2 * h)
    assert slope == pytest.approx(2.0, abs=1e-2)


@pytest.mark.parametrize("m", [2, 4, 8])
def test_sawtooth_period(m):
    seg = 2 * np.pi / m
    assert _disc(m, 0.05) == pytest.approx(_disc(m, 0.05 + seg), abs=1e-5)
    assert _disc(m, 0.0) == pytest.approx(0.0, abs=1e-6)
