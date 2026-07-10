"""Tests for doppler.snr — data-aided and non-data-aided (M2M4) Es/N0.

Covers correctness at the Python API layer: agreement with a known
injected Es/N0 across a range, the documented degenerate cases (pure
noise, noiseless), scale/polarity invariance of the data-aided estimator,
sliding-window series shape/edge behavior, and mismatched soft/sign_bits
lengths.
"""

import numpy as np
import pytest

from doppler.snr import (
    snr_data_aided_db,
    snr_data_aided_db_series,
    snr_m2m4_db,
    snr_m2m4_db_series,
)


def _make_bpsk(n, esn0_db, *, seed=0):
    """n unit-energy BPSK symbols at the given Es/N0 (dB), plus the known
    transmitted bits."""
    rng = np.random.default_rng(seed)
    bits = rng.integers(0, 2, n).astype(np.uint8)
    sign = np.where(bits, -1.0, 1.0)
    sigma = 10.0 ** (-esn0_db / 20.0)
    noise = (sigma / np.sqrt(2.0)) * (
        rng.standard_normal(n) + 1j * rng.standard_normal(n)
    )
    soft = (sign + noise).astype(np.complex64)
    return soft, bits


# ── snr_data_aided_db ────────────────────────────────────────────────────────


@pytest.mark.parametrize("esn0_db", [0.0, 6.0, 10.0, 20.0])
def test_data_aided_matches_injected_esn0(esn0_db):
    soft, bits = _make_bpsk(20_000, esn0_db)
    assert snr_data_aided_db(soft, bits) == pytest.approx(esn0_db, abs=1.0)


def test_data_aided_pure_noise_is_low():
    rng = np.random.default_rng(1)
    noise = (
        rng.standard_normal(5000) + 1j * rng.standard_normal(5000)
    ).astype(np.complex64)
    bits = rng.integers(0, 2, 5000).astype(np.uint8)
    assert snr_data_aided_db(noise, bits) < -10.0


def test_data_aided_noiseless_is_nan():
    bits = np.array([0, 1, 0, 1, 1, 0], dtype=np.uint8)
    soft = np.where(bits, -1.0, 1.0).astype(np.complex64)
    assert np.isnan(snr_data_aided_db(soft, bits))


def test_data_aided_empty_is_nan():
    assert np.isnan(
        snr_data_aided_db(
            np.array([], dtype=np.complex64), np.array([], dtype=np.uint8)
        )
    )


def test_data_aided_scale_invariant():
    soft, bits = _make_bpsk(4000, 8.0)
    base = snr_data_aided_db(soft, bits)
    scaled = snr_data_aided_db((soft * 5.0).astype(np.complex64), bits)
    assert scaled == pytest.approx(base, abs=1e-3)


def test_data_aided_polarity_invariant():
    # A global sign flip (the ambiguity a Costas loop alone cannot
    # resolve) must not change the Es/N0 reading.
    soft, bits = _make_bpsk(4000, 8.0)
    base = snr_data_aided_db(soft, bits)
    flipped = snr_data_aided_db((-soft).astype(np.complex64), bits)
    assert flipped == pytest.approx(base, abs=1e-3)


def test_data_aided_clamps_to_shorter_array():
    soft, bits = _make_bpsk(4000, 8.0)
    # Extra unpaired samples past the known bits must be ignored, not
    # read out of bounds or bias the result.
    padded_soft = np.concatenate([soft, soft[:100]]).astype(np.complex64)
    assert snr_data_aided_db(padded_soft, bits) == pytest.approx(
        snr_data_aided_db(soft, bits), abs=1e-6
    )


# ── snr_m2m4_db ──────────────────────────────────────────────────────────────


@pytest.mark.parametrize("esn0_db", [0.0, 6.0, 10.0, 20.0])
def test_m2m4_matches_injected_esn0(esn0_db):
    soft, _bits = _make_bpsk(20_000, esn0_db)
    assert snr_m2m4_db(soft) == pytest.approx(esn0_db, abs=1.0)


def test_m2m4_pure_noise_is_very_negative():
    rng = np.random.default_rng(1)
    noise = (
        rng.standard_normal(5000) + 1j * rng.standard_normal(5000)
    ).astype(np.complex64)
    assert snr_m2m4_db(noise) < -10.0


def test_m2m4_noiseless_is_inf():
    bits = np.array([0, 1, 0, 1, 1, 0], dtype=np.uint8)
    soft = np.where(bits, -1.0, 1.0).astype(np.complex64)
    assert snr_m2m4_db(soft) == float("inf")


def test_m2m4_empty_is_nan():
    assert np.isnan(snr_m2m4_db(np.array([], dtype=np.complex64)))


def test_m2m4_needs_no_known_symbols():
    # The whole point of the non-data-aided estimator: it agrees with the
    # data-aided one without ever seeing `bits`.
    soft, bits = _make_bpsk(20_000, 10.0)
    assert snr_m2m4_db(soft) == pytest.approx(
        snr_data_aided_db(soft, bits), abs=1.0
    )


# ── sliding-window series ────────────────────────────────────────────────────


def test_data_aided_series_shape_and_center_matches_scalar():
    soft, bits = _make_bpsk(2000, 10.0)
    series = snr_data_aided_db_series(soft, bits, 2000)
    assert series.shape == (2000,)
    # A window covering the whole array should read close to the scalar
    # estimate everywhere it's not edge-clipped.
    assert series[1000] == pytest.approx(
        snr_data_aided_db(soft, bits), abs=0.5
    )


def test_data_aided_series_nan_past_short_sign_bits():
    soft, bits = _make_bpsk(200, 10.0)
    series = snr_data_aided_db_series(soft, bits[:50], 21)
    assert np.all(np.isnan(series[50:]))
    assert not np.any(np.isnan(series[:50]))


def test_m2m4_series_shape_and_center_matches_scalar():
    soft, _bits = _make_bpsk(2000, 10.0)
    series = snr_m2m4_db_series(soft, 2000)
    assert series.shape == (2000,)
    assert series[1000] == pytest.approx(snr_m2m4_db(soft), abs=0.5)
