"""Tests for ``doppler.spectral.obw_from_power`` — occupied bandwidth from a
DC-centred linear-power spectrum.

``obw_from_power(pwr, fs, frac)`` returns the width of the central interval
that holds ``frac`` of the total power, excluding ``(1-frac)/2`` of the power
from each tail symmetrically. Result is ``(ihi - ilo + 1) * fs / len(pwr)``.
"""

import numpy as np
import pytest

from doppler.spectral import obw_from_power


def test_flat_spectrum_is_fraction_of_fs():
    # A flat spectrum spreads power uniformly, so the central interval holding
    # `frac` of the power spans ~`frac` of the bins → obw ~ frac * fs (within a
    # bin or two from the inclusive cumulative-sum boundaries).
    n, fs = 1000, 1000.0
    pwr = np.ones(n, dtype=np.float64)
    for frac in (0.1, 0.5, 0.9):
        obw = obw_from_power(pwr, fs, frac)
        assert obw == pytest.approx(frac * fs, abs=2.0 * fs / n)


def test_obw_is_monotonic_in_frac():
    pwr = np.ones(256, dtype=np.float64)
    fs = 1.0
    widths = [obw_from_power(pwr, fs, f) for f in (0.2, 0.5, 0.8, 0.99)]
    assert widths == sorted(widths)
    assert all(w2 > w1 for w1, w2 in zip(widths, widths[1:]))


def test_single_spike_is_one_bin():
    # All power in one central bin → the occupied interval is that single bin,
    # whatever the fraction (< 1) → one bin width, fs / len(pwr).
    n, fs = 128, 128.0
    pwr = np.zeros(n, dtype=np.float64)
    pwr[n // 2] = 1.0
    assert obw_from_power(pwr, fs, 0.5) == pytest.approx(fs / n)


def test_zero_and_empty_power_return_zero():
    assert obw_from_power(np.zeros(64, dtype=np.float64), 1.0, 0.5) == 0.0
    assert obw_from_power(np.array([], dtype=np.float64), 1.0, 0.5) == 0.0


def test_per_bin_normalisation_cancels():
    # A constant scale on every bin cancels in the power ratio, so obw is
    # unchanged whether the spectrum is in W, mW, or arbitrary units.
    rng = np.random.default_rng(0)
    pwr = rng.random(512) + 0.1
    fs, frac = 2.0, 0.6
    assert obw_from_power(pwr, fs, frac) == pytest.approx(
        obw_from_power(pwr * 1e6, fs, frac)
    )
