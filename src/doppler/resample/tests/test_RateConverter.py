"""Tests for the C-backed doppler.resample.RateConverter class."""

import math

import numpy as np
import pytest

from doppler.resample import RateConverter, rate_convert


def _dc(n: int) -> np.ndarray:
    return np.ones(n, dtype=np.complex64)


def _csin(n: int, freq: float = 0.0) -> np.ndarray:
    t = np.arange(n, dtype=np.float64)
    return np.exp(1j * 2 * math.pi * freq * t).astype(np.complex64)


# ------------------------------------------------------------------ #
# Construction / teardown                                             #
# ------------------------------------------------------------------ #


def test_create_valid():
    rc = RateConverter(0.5)
    assert rc is not None


def test_invalid_rate_raises():
    with pytest.raises((ValueError, MemoryError)):
        RateConverter(0.0)
    with pytest.raises((ValueError, MemoryError)):
        RateConverter(-1.0)


def test_destroy():
    rc = RateConverter(0.5)
    rc.destroy()
    with pytest.raises(RuntimeError, match="destroyed"):
        rc.execute(_dc(4))


def test_context_manager():
    with RateConverter(0.5) as rc:
        y = rc.execute(_dc(64))
    assert len(y) == 32


# ------------------------------------------------------------------ #
# Stage selection                                                     #
# ------------------------------------------------------------------ #


def test_stages_interpolation():
    rc = RateConverter(2.0)
    assert len(rc.stages) == 1
    assert rc.stages[0].startswith("Resampler")


def test_stages_halfband_x1():
    rc = RateConverter(0.5)
    assert rc.stages == ["HalfbandDecimator"]


def test_stages_halfband_x2():
    rc = RateConverter(0.25)
    assert rc.stages == ["HalfbandDecimator", "HalfbandDecimator"]


def test_stages_cic():
    rc = RateConverter(0.125)
    assert rc.stages == ["CIC(8)"]


def test_stages_cic_comp():
    rc = RateConverter(0.125, compensate=1)
    assert rc.stages == ["CIC(8)+FIR"]


def test_stages_cic_resampler():
    rc = RateConverter(1.0 / 12.0)
    assert len(rc.stages) == 2
    assert rc.stages[0].startswith("CIC")
    assert rc.stages[1].startswith("Resampler")


def test_stages_small_non_integer():
    rc = RateConverter(1.0 / 3.0)
    assert len(rc.stages) == 1
    assert rc.stages[0].startswith("Resampler")


# ------------------------------------------------------------------ #
# Output length                                                       #
# ------------------------------------------------------------------ #


def test_execute_output_length_hb():
    rc = RateConverter(0.5)
    assert len(rc.execute(_dc(1024))) == 512


def test_execute_output_length_cic():
    rc = RateConverter(0.125)
    assert len(rc.execute(_dc(1024))) == 128


def test_execute_dtype():
    rc = RateConverter(0.5)
    y = rc.execute(_dc(64))
    assert y.dtype == np.complex64


# ------------------------------------------------------------------ #
# Properties                                                          #
# ------------------------------------------------------------------ #


def test_rate_readable():
    rc = RateConverter(0.25)
    assert rc.rate == pytest.approx(0.25)


def test_rate_set_rebuilds_cascade():
    rc = RateConverter(0.5)
    assert rc.stages == ["HalfbandDecimator"]
    rc.rate = 0.125
    assert rc.stages == ["CIC(8)"]
    assert len(rc.execute(_dc(1024))) == 128


# ------------------------------------------------------------------ #
# Reset                                                               #
# ------------------------------------------------------------------ #


def test_reset_reproducible():
    rc = RateConverter(0.5)
    x = _csin(64, freq=0.05)
    y1 = rc.execute(x)
    rc.reset()
    y2 = rc.execute(x)
    np.testing.assert_array_equal(y1, y2)


# ------------------------------------------------------------------ #
# Functional wrapper                                                  #
# ------------------------------------------------------------------ #


def test_rate_convert_returns_array_and_rc():
    y, rc = rate_convert(_dc(256), 0.5)
    assert isinstance(y, np.ndarray)
    assert isinstance(rc, RateConverter)
    assert len(y) == 128


def test_rate_convert_reuses_rc():
    _, rc1 = rate_convert(_dc(64), 0.5)
    _, rc2 = rate_convert(_dc(64), 0.5, rc1)
    assert rc2 is rc1


# ------------------------------------------------------------------ #
# Signal quality                                                      #
# ------------------------------------------------------------------ #


@pytest.mark.parametrize("rate", [0.5, 0.25, 0.125, 0.0625, 0.1, 2.0])
def test_dc_gain(rate):
    rc = RateConverter(rate)
    n_in = 4096 if rate <= 1.0 else 256
    y = rc.execute(_dc(n_in))
    settled = y[max(1, len(y) // 10) :]
    amp = float(np.mean(np.abs(settled)))
    assert 0.5 < amp < 2.0, f"rate={rate}: DC amplitude {amp:.3f}"


def test_alias_rejection_cic():
    rc = RateConverter(0.125)
    n = 8192
    x = _csin(n, freq=0.45)
    y = rc.execute(x)
    settled = y[len(y) // 4 :]
    in_power = float(np.mean(np.abs(_csin(len(settled), freq=0.45)) ** 2))
    out_power = float(np.mean(np.abs(settled) ** 2))
    rejection_db = 10 * math.log10(in_power / (out_power + 1e-30))
    assert rejection_db >= 20.0, f"alias rejection {rejection_db:.1f} dB"


def test_compensate_reduces_passband_droop():
    rate = 0.0625
    rc_plain = RateConverter(rate, compensate=0)
    rc_comp = RateConverter(rate, compensate=1)
    n = 8192
    f_in = 0.1 * rate
    x = _csin(n, freq=f_in)
    y_plain = rc_plain.execute(x)
    y_comp = rc_comp.execute(x)
    skip = len(y_plain) // 4
    amp_plain = float(np.mean(np.abs(y_plain[skip:])))
    amp_comp = float(np.mean(np.abs(y_comp[skip:])))
    assert abs(amp_comp - 1.0) <= abs(amp_plain - 1.0) + 0.02, (
        f"compensate=1 not flatter: plain={amp_plain:.4f} comp={amp_comp:.4f}"
    )
