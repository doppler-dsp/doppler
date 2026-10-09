"""Tests for the Acquisition hit's own ``chip_phase`` (the Dll seed).

``Acquisition.push()`` reports ``code_phase`` as a correlation lag; each hit
also carries ``chip_phase``, the code's own instantaneous phase at the
hand-off instant, which C computes once (``dp_acq_build_handoff``'s helper)
and both faces read. Uncoupled, that phase is the lag's sign inversion
modulo the spreading factor. Coupled to a carrier, it is further advanced by
the chip drift over half the dwell -- the term the retired Python
``dll_init_chip_from_acq`` could not carry (doppler#1257).

The drifted case runs at 20 ppm: a 50 kHz Doppler on a 2.5 GHz carrier.
"""

import warnings

from doppler.acquire import Acquisition, bin_to_signed
from doppler.track.tests import test_acq_dll_handoff as _h

SF = _h.SF
SPC = _h.SPC
CARRIER_HZ = 2.5e9  # coupled carrier the drifted hit is searched against
DRIFT_HZ = 20e-6 * CARRIER_HZ  # 20 ppm of the carrier: 50 kHz


def _inversion(code_phase):
    """The uncoupled phase: the lag inverted modulo the code period."""
    return (SF - code_phase / SPC) % SF


def _search(x, carrier_hz):
    """First hit of a continuous search, with the carrier coupling set."""
    with warnings.catch_warnings():
        warnings.simplefilter("ignore", UserWarning)
        acq = Acquisition(
            _h.CODE,
            spc=SPC,
            chip_rate=_h.CHIP_RATE,
            cn0_dbhz=55.0,
            doppler_uncertainty=100e3,
            symbol_rate=_h.SYM_RATE,
            pfa=1e-3,
            pd=0.9,
        )
    acq.set_carrier_freq_hz(carrier_hz)
    frame = acq.code_bins * acq.doppler_bins
    pos = 0
    while pos + frame <= len(x):
        hits = acq.push(x[pos : pos + frame])
        if hits:
            return hits[0], acq
        pos += frame
    return None, acq


def test_drifted_hit_chip_phase_carries_the_dwell_advance(monkeypatch):
    """At 20 ppm the hit's chip_phase is the inverted lag plus the drift over
    half the dwell, to the C formula's own precision."""
    monkeypatch.setattr(_h, "DOPPLER_HZ", DRIFT_HZ)
    x = _h._make_signal(cn0_dbhz=75.0, n_sym=1500, seed=6)
    hit, acq = _search(x, CARRIER_HZ)
    assert hit is not None, "acquisition failed on the drifted signal"

    doppler_hz = bin_to_signed(hit[0], acq.doppler_bins) * (acq.doppler_res_hz)
    dwell_chips = acq.n_noncoh * acq.coherent_bins * SF
    advance = doppler_hz / CARRIER_HZ * 0.5 * dwell_chips
    assert abs(advance) > 1e-3, "the drift must be material to test"

    expect = (_inversion(hit[1]) + advance) % SF
    err = ((hit[7] - expect + SF / 2) % SF) - SF / 2
    assert abs(err) < 1e-6, (
        f"hit[7] disagrees with the C dwell advance by {err:.3e} "
        f"chips (advance={advance:.4f})"
    )


def test_uncoupled_hit_chip_phase_is_the_inverted_lag(monkeypatch):
    """With no carrier set there is no drift to advance by: the hit's
    chip_phase is the lag inverted, exactly."""
    monkeypatch.setattr(_h, "DOPPLER_HZ", 50.0)
    x = _h._make_signal(cn0_dbhz=75.0, n_sym=1500, seed=6)
    hit, _acq = _search(x, 0.0)
    assert hit is not None, "acquisition failed on the uncoupled signal"
    err = ((hit[7] - _inversion(hit[1]) + SF / 2) % SF) - SF / 2
    assert abs(err) < 1e-9
