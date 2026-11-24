"""The receiver and pool at the CCSDS 415.1-B-1 point are the full ones.

``Ccsds415Receiver`` and ``Ccsds415Pool`` are second constructors over the
async DSSS receiver and pool (``doppler/ccsds/ccsds_415.h``): the point's
numbers, and the binding's defaults for everything the point does not set.
These hold the views to the full classes built from those defaults, so a
view whose body drifts from the manifest's defaults, or from the point,
fails here; the C test (``_test_the_ccsds415_view_is_the_point``) holds the
same against the C create.
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
    """Same state, byte for byte. Rejects a view that sets a number the
    point does not (a pfa of 1e-2 in its body), or misses one it does."""
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


def test_the_chip_rate_is_the_formula_to_the_bit():
    """The Python face reads the C arithmetic: fc * 31 / 21216 as IEEE-754
    doubles give it. Rejects a binding that reimplements it, or C that
    rounds it another way."""
    for fc in CARRIERS:
        assert ccsds415_chip_rate(fc).hex() == (fc * 31.0 / 21216.0).hex()
