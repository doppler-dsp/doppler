"""The receiver and pool at the CCSDS 415.1-B-1 point are the full ones.

``Ccsds415Receiver`` and ``Ccsds415Pool`` are second constructors over the
async DSSS receiver and pool, built from ``dp_ccsds_415_rx()``'s table
(``doppler/ccsds/ccsds_415.h``). The table's arguments outside the point's
numbers equal the binding's defaults, so each view's state blob equals the
full class's built from the point's numbers alone. A blob does not carry
every argument (pd, lost_confirm_s and five more are config no blob holds):
the C test (``_test_the_ccsds415_view_is_the_point``) reads each argument
back from the built receiver. Here: the blob, and each refusal's sentence
at both sides of each edge.
"""

from __future__ import annotations

import numpy as np
import pytest

from doppler.ccsds import ccsds415_chip_rate, ccsds415_cn0
from doppler.dsss import (
    AsyncDsssPool,
    AsyncDsssReceiver,
    Ccsds415Pool,
    Ccsds415Receiver,
)
from doppler.wfm import Gold

#: The point's numbers as the design page's §1 table states them, typed
#: here once as the test's own expectation of the installed header.
SYM, FS, SPAN, R_ENV, CODE_ONLY, FLOOR = 3000.0, 11e6, 1500.0, 11.0, 275, 4.0
CARRIERS = (2.2e9, 2.8e9)


@pytest.fixture(scope="module")
def code() -> np.ndarray:
    return np.asarray(Gold().generate(1023), np.uint8)


def _point(fc: float, refine: bool) -> dict:
    """The full constructor's keywords at the point, the rest defaulted."""
    return {
        "chip_rate": ccsds415_chip_rate(fc),
        "symbol_rate": SYM,
        "sample_rate": FS,
        "cn0_dbhz": ccsds415_cn0(FLOOR),
        "doppler_uncertainty": SPAN,
        "carrier_freq_hz": fc,
        "lost_confirm_s": 0.4,
        "code_only_s": CODE_ONLY / SYM,
        "doppler_rate": R_ENV,
        "refine": refine,
    }


@pytest.mark.parametrize("refine", [True, False])
@pytest.mark.parametrize("fc", CARRIERS)
def test_the_receiver_view_is_the_full_receiver_at_the_point(code, fc, refine):
    """Same state, byte for byte: the blob's share of the arguments. Rejects
    a view whose table changes a number the blob carries (a pfa of 1e-2);
    the rest is the C test's."""
    v = Ccsds415Receiver(
        code, carrier_freq_hz=fc, lost_confirm_s=0.4, refine=refine
    )
    full = AsyncDsssReceiver(code, **_point(fc, refine))
    assert v.get_state() == full.get_state()


@pytest.mark.parametrize("refine", [True, False])
@pytest.mark.parametrize("fc", CARRIERS)
def test_the_pool_view_is_the_full_pool_at_the_point(code, fc, refine):
    """As the receiver's, for a pool of two on one thread."""
    v = Ccsds415Pool(
        code,
        n_receivers=2,
        carrier_freq_hz=fc,
        lost_confirm_s=0.4,
        threads=1,
        refine=refine,
    )
    full = AsyncDsssPool(code, n_receivers=2, threads=1, **_point(fc, refine))
    assert v.get_state() == full.get_state()


@pytest.mark.parametrize("cls", [Ccsds415Receiver, Ccsds415Pool])
def test_a_code_that_is_not_1023_chips_is_refused(code, cls):
    """The point's code is 1,023 chips; the message says so. Rejects a
    view that takes a 511-chip code and builds some other receiver."""
    with pytest.raises(ValueError, match="the code must be 1,023 chips"):
        cls(code[:511])


#: The carrier window's edges as ccsds_415.h derives them: a symbol is one
#: code period at the low edge, the chip rate is the capture rate at the high.
FC_MIN = 1023.0 * SYM * 21216.0 / 31.0
FC_MAX = FS * 21216.0 / 31.0


@pytest.mark.parametrize("cls", [Ccsds415Receiver, Ccsds415Pool])
def test_the_carrier_window_refuses_with_its_cause(code, cls):
    """Each edge from both sides: just inside builds, just outside raises
    the sentence naming why. Rejects a refusal that names the wrong cause
    (the old "above 5.5 MHz") or a window shifted off the receiver's."""
    kw = {"n_receivers": 1, "threads": 1} if cls is Ccsds415Pool else {}
    cls(code, carrier_freq_hz=FC_MIN * (1 + 1e-9), **kw)
    cls(code, carrier_freq_hz=FC_MAX * (1 - 1e-9), **kw)
    with pytest.raises(ValueError, match="a symbol is shorter than a code"):
        cls(code, carrier_freq_hz=FC_MIN * (1 - 1e-9), **kw)
    with pytest.raises(ValueError, match="passes the 11 MSa/s sample rate"):
        cls(code, carrier_freq_hz=FC_MAX * (1 + 1e-9), **kw)
    with pytest.raises(ValueError, match="carrier_freq_hz must be finite"):
        cls(code, carrier_freq_hz=float("nan"), **kw)
    with pytest.raises(ValueError, match="lost_confirm_s must be >= 0"):
        cls(code, lost_confirm_s=-1.0, **kw)


def test_the_pool_refuses_more_receivers_than_peaks(code):
    """1 to 16, one receiver a search peak: 16 builds, 17 and 0 raise the
    sentence that says so. Rejects the old message, which said only >= 1."""
    Ccsds415Pool(code, n_receivers=16, threads=1)
    for n in (0, 17):
        with pytest.raises(ValueError, match="n_receivers must be 1 to 16"):
            Ccsds415Pool(code, n_receivers=n, threads=1)


def test_the_chip_rate_is_the_formula_to_the_bit():
    """The Python face reads the C arithmetic: fc * 31 / 21216 as IEEE-754
    doubles give it. Rejects a binding that reimplements it, or C that
    rounds it another way."""
    for fc in CARRIERS:
        assert ccsds415_chip_rate(fc).hex() == (fc * 31.0 / 21216.0).hex()
