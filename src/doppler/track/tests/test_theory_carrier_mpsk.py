"""Theoretical-correctness tests for the M-PSK carrier loop.

Validates the decision-directed M-PSK phase discriminator
``e = Im(P·conj(â))/|P|`` against closed form, for M in {2, 4, 8}:
  * the S-curve is a sawtooth of period 2π/M (the M-fold phase ambiguity),
    ``e = sin(φ)`` on the central branch with unit slope (Kd = 1) at lock and
    peak ±sin(π/M) at the decision boundary;
  * at M = 2 it is exactly the BPSK Costas S-curve.

The rigorous Monte-Carlo bounds (closed-loop phase jitter ~ G·σ_disc², the
M-tightening tracking threshold, FLL pull-in range) live in the C harnesses
``native/validation/carrier_mpsk_{scurve,jitter}.c`` (ctest ``--check``); these
Python tests cover the deterministic discriminator characteristic.
"""

import numpy as np
import pytest

from doppler.mpsk import mpsk_map
from doppler.track import CarrierMpsk


def _discriminator(m, phi):
    """Open-loop e(φ), averaged over the M constellation points (symmetric)."""
    pts = mpsk_map(np.arange(m, dtype=np.uint8), m)
    acc = 0.0
    rot = np.exp(1j * phi)
    for a in pts:
        c = CarrierMpsk(
            bn=1e-6, zeta=0.707, init_norm_freq=0.0, tsamps=1, bn_fll=0.0, m=m
        )
        c.steps(np.array([a * rot], np.complex64))
        acc += c.last_error
    return acc / m


@pytest.mark.parametrize("m", [2, 4, 8])
def test_scurve_sawtooth_period(m):
    seg = 2 * np.pi / m
    phis = np.linspace(-np.pi, np.pi, 361)
    meas = np.array([_discriminator(m, p) for p in phis])
    wrapped = (phis + seg / 2) % seg - seg / 2
    theory = np.sin(wrapped)
    # compare on the linear branches (away from the slicer-snap boundaries)
    guard = 3 * (phis[1] - phis[0])
    ok = np.abs(np.abs(wrapped) - seg / 2) > guard
    assert np.max(np.abs(meas[ok] - theory[ok])) < 1e-4


@pytest.mark.parametrize("m", [2, 4, 8])
def test_scurve_unit_slope_at_lock(m):
    h = 1e-3
    kd = (_discriminator(m, h) - _discriminator(m, -h)) / (2 * h)
    assert kd == pytest.approx(1.0, abs=1e-3)


@pytest.mark.parametrize("m", [2, 4, 8])
def test_scurve_lock_zero_and_peak(m):
    # stable zero at φ=0 ...
    assert _discriminator(m, 0.0) == pytest.approx(0.0, abs=1e-6)
    # ... and peak ±sin(π/M) just inside the ±π/M decision boundary
    peak = np.sin(np.pi / m)
    assert _discriminator(m, np.pi / m - 1e-3) == pytest.approx(peak, abs=5e-3)
    assert _discriminator(m, -(np.pi / m - 1e-3)) == pytest.approx(
        -peak, abs=5e-3
    )


def _single_point_disc(make, phi):
    """Discriminator for the single prompt exp(jφ) (constellation point +1)."""
    c = make()
    c.steps(np.array([np.exp(1j * phi)], np.complex64))
    return c.last_error


def test_m2_scurve_is_bpsk_costas():
    # at M=2 the M-PSK discriminator equals the BPSK Costas characteristic
    # e(φ) = sign(cosφ)·sinφ on the linear branches. The branch boundaries
    # (±π/2 unstable null, ±π data ambiguity) are exact ties whose tie-break
    # differs between the |P|-sign and nearest-point slicers; exclude them.
    from doppler.track import Costas

    def mk_mpsk():
        return CarrierMpsk(
            bn=1e-6, zeta=0.707, init_norm_freq=0.0, tsamps=1, bn_fll=0.0, m=2
        )

    def mk_costas():
        return Costas(
            bn=1e-6, zeta=0.707, init_norm_freq=0.0, tsamps=1, bn_fll=0.0
        )

    for phi in np.linspace(-np.pi, np.pi, 181):
        # skip a guard around the ±π/2 and ±π tie points
        if (
            min(abs(abs(phi) - np.pi / 2), abs(abs(phi) - np.pi), abs(phi))
            < 0.05
        ):
            if abs(phi) < 0.05:  # keep φ≈0 (clean lock, both 0)
                pass
            else:
                continue
        assert _single_point_disc(mk_mpsk, phi) == pytest.approx(
            _single_point_disc(mk_costas, phi), abs=1e-5
        )
