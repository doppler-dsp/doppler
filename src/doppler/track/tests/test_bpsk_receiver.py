"""`BpskReceiver` — the same core, asked for in the units a caller holds.

This face exists because of what it does NOT take. `MpskReceiver` asks for
`sps`, which is `fs / Rs` — a ratio the library computes for its own use in
selecting a cascade, so requiring it makes the caller derive an internal
quantity. And because `sps` is in that constructor, `init_norm_freq` has to be
cycles per SAMPLE, so stating a carrier offset needs `sps` and `fs` together
while the loop bandwidth on the next line is normalised to the SYMBOL rate.

So these tests are mostly about ABSENCE, which is the hard thing to keep true:
a parameter that creeps back onto the signature will not break anything here
unless something asserts it is gone.
"""

from __future__ import annotations

import numpy as np
import pytest

from doppler.track import BpskReceiver, MpskReceiver


def test_the_type_carries_m_and_the_rates_carry_sps():
    """Two rates in Hz, and everything internal derived from them."""
    rx = BpskReceiver(sample_rate_hz=8e6, symbol_rate_hz=1e6)
    assert rx.m == 2  # the class name says it; not a parameter
    assert rx.sps == pytest.approx(8.0)  # fs/Rs, computed not asked for
    assert rx.m_out == 8  # DERIVED, and see below for why that matters


def test_m_out_derives_to_the_value_the_pulse_actually_needs():
    """The derived `m_out` is 8, and pinning 4 against I&D costs 3.11 dB.

    Not a style point. `m_out=4` with the default rectangular pulse samples
    the matched filter's integral too coarsely: measured 3.11 dB off the
    coherent bound at 18 dB Es/N0 against 0.41 dB for the derived 8. The
    telemetry capture demo shipped that pairing until this face removed the
    parameter that let it.
    """
    assert BpskReceiver(sample_rate_hz=8e6, symbol_rate_hz=1e6).m_out == 8


def test_the_carrier_is_stated_in_hz_not_cycles_per_sample():
    """`carrier_freq_hz / sample_rate_hz` is the only normalisation, and it
    happens inside."""
    rx = BpskReceiver(
        sample_rate_hz=8e6, symbol_rate_hz=1e6, carrier_freq_hz=2e6
    )
    assert rx.norm_freq == pytest.approx(0.25)


def test_sps_and_m_are_not_on_the_signature():
    """The absence is the feature, so it is asserted rather than assumed.

    A keyword this face does not take must be REFUSED, not quietly swallowed
    -- a constructor that accepts `sps=` and ignores it is worse than one that
    requires it, because the caller believes it took effect.
    """
    for bad in ({"sps": 8.0}, {"m": 2}, {"m_out": 4}, {"init_norm_freq": 0.1}):
        with pytest.raises(TypeError):
            BpskReceiver(sample_rate_hz=8e6, symbol_rate_hz=1e6, **bad)


@pytest.mark.parametrize(
    "kw",
    [
        {"sample_rate_hz": 0.0, "symbol_rate_hz": 1e6},
        {"sample_rate_hz": -8e6, "symbol_rate_hz": 1e6},
        {"sample_rate_hz": 8e6, "symbol_rate_hz": 0.0},
        {"sample_rate_hz": 8e6, "symbol_rate_hz": 1e6, "carrier_freq_hz": 9e6},
    ],
)
def test_impossible_geometry_is_refused_at_construction(kw):
    """A rate that cannot produce an `sps`, or a carrier outside Nyquist.

    Refused rather than approximated: a mis-stated capture is a caller error,
    and saying so at construction beats a first strobe that lands nowhere.
    """
    with pytest.raises(ValueError):
        BpskReceiver(**kw)


def test_it_is_the_same_receiver_as_the_equivalent_mpsk_receiver():
    """The whole claim of a view: one core, reached two ways.

    Constructed to the same geometry through both faces, the two must produce
    the SAME symbols bit for bit. If they diverge, this is a second
    implementation wearing a nicer signature, which is the thing the type/
    flavor rule exists to prevent.
    """
    fs, rs = 8e6, 1e6
    rng = np.random.default_rng(7)
    idx = rng.integers(0, 2, 2000)
    tx = np.repeat(np.exp(1j * np.pi * idx), 8).astype(np.complex64)
    x = (tx + 0.05 * rng.standard_normal(tx.size)).astype(np.complex64)

    a = BpskReceiver(sample_rate_hz=fs, symbol_rate_hz=rs).steps(x)
    b = MpskReceiver(m=2, sps=fs / rs, m_out=8).steps(x)
    assert np.array_equal(a, b)
