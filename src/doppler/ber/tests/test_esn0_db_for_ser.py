"""Pin the NaN contract of ``ber_esn0_db_for_ser`` at the Python face.

The C core returns NaN, never a bracket end, for a rate with no Es/N0 in
[-10, 40] dB. The wrapper must hand that NaN straight through: a float NaN,
not an exception and not a clamped number.
"""

import math

import pytest

from doppler.ber import ber_esn0_db_for_ser, ber_theory_ser


@pytest.mark.parametrize("m", [2, 4, 8])
def test_zero_ser_is_nan_not_a_clamp(m):
    # A zero SER is a perfect link; no finite Es/N0 produces it. The old
    # return was -10 dB, which read as "very noisy".
    assert math.isnan(ber_esn0_db_for_ser(m, 0.0))


@pytest.mark.parametrize("m", [2, 4, 8])
def test_rate_above_bound_at_minus_10_db_is_nan(m):
    # Above the coherent bound at -10 dB: unreachable, so NaN, not -10.
    assert math.isnan(ber_esn0_db_for_ser(m, 0.9))


def test_in_bracket_anchor_unchanged():
    # The SER = 1e-3 BPSK anchor, inside the bracket, still bisects to the
    # same value the rest of the library quotes.
    assert ber_esn0_db_for_ser(2, 1e-3) == pytest.approx(6.79, abs=0.01)


def test_round_trip_through_theory():
    # Inverting the theory at a known Es/N0 recovers it.
    esn0_db = 12.0
    ser = ber_theory_ser(4, 10.0 ** (esn0_db / 10.0))
    assert ber_esn0_db_for_ser(4, ser) == pytest.approx(esn0_db, abs=0.01)


def test_nan_fails_a_loss_gate_written_as_not_less_equal():
    # The gate idiom the validation harnesses use: a NaN loss must fail.
    loss = 8.0 - ber_esn0_db_for_ser(2, 0.0)
    assert not (loss <= 0.5)


@pytest.mark.parametrize("m", [0, 1, 3, 5, 16])
def test_unsupported_m_is_nan_not_another_m(m):
    # gh-1913: 16 read as 8-PSK and 0/1 as BPSK. No closed form, no answer.
    assert math.isnan(ber_theory_ser(m, 10.0))
    assert math.isnan(ber_esn0_db_for_ser(m, 1e-3))
