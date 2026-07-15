"""Tests for the Acquisition -> Dll seed conversion (``dsss.handoff``).

``Acquisition.push()`` reports ``code_phase`` as a correlation lag;
``Dll(init_chip=...)`` wants the code's own instantaneous phase — a sign
inversion modulo the spreading factor, not direct equality. These pin the
algebra in isolation, independent of a real Acquisition/Dll round trip
(that round trip is covered by
``src/doppler/track/tests/test_acq_dll_handoff.py``).
"""

import pytest

from doppler.dsss.handoff import dll_init_chip_from_acq

SF = 1023
SPC = 4


@pytest.mark.parametrize(
    "code_phase, expected",
    [
        (0.0, 0.0),  # no lag -> code phase is exactly at epoch start
        (SPC * (SF - 1), 1.0),  # lag of sf-1 chips -> phase = 1 chip in
        (SPC * 1, SF - 1),  # 1-chip lag -> phase = sf-1 chips in
        (SPC * SF, 0.0),  # a full-period lag wraps back to 0
    ],
)
def test_dll_init_chip_from_acq(code_phase, expected):
    assert dll_init_chip_from_acq(code_phase, SPC, SF) == pytest.approx(
        expected
    )


def test_dll_init_chip_from_acq_range():
    """Always lands in [0, sf) for any lag in the code's own period."""
    for code_phase in range(0, SPC * SF, SPC):
        chip = dll_init_chip_from_acq(float(code_phase), SPC, SF)
        assert 0.0 <= chip < SF


def test_dll_init_chip_from_acq_is_the_inverse_relationship():
    """Round-tripping a phase through the forward lag relationship and
    back recovers it -- the formula really is an inversion, not a
    reparametrization that happens to agree at a few points."""
    for true_phase_chips in [0, 3, 511, 1022]:
        lag_samples = SPC * ((SF - true_phase_chips) % SF)
        recovered = dll_init_chip_from_acq(float(lag_samples), SPC, SF)
        assert recovered == pytest.approx(float(true_phase_chips % SF))
