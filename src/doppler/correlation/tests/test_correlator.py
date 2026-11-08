"""Who serves each member, from Python (docs/design/correlator-python.md
section 18).

One class reaches every member from every flavor, and the C refuses a
member off the flavors that serve it, never the binding. These tests hold
the Python face of that table for the members whose refusal is a sentinel
or a status the binding translates: the six read-out methods (SIZE_MAX,
raised as ValueError), ``keep_surface``'s read (a getter status raised as
AttributeError) and ``last_corr``'s view (a status asked before the view is
built). Each refusal is checked beside a flavor that serves the member, so
a binding that raised everywhere would fail too.
"""

import numpy as np
import pytest

from doppler.correlation import Correlator
from doppler.wfm import PN, mls_poly

_CHIPS = np.asarray(PN(poly=mls_poly(5), seed=1, length=5).generate(31))
_NRZ = np.where(_CHIPS & 1, -1, 1).astype(np.complex64)
_K = np.arange(127)
_ZC = np.exp(-1j * np.pi * 5 * _K * (_K + 1) / 127).astype(np.complex64)


def _build(flavor: str) -> Correlator:
    ref2 = np.ones((4, 8), np.complex64)
    return {
        "1d": lambda: Correlator(_NRZ),
        "2d": lambda: Correlator(ref2),
        "delay_doppler": lambda: Correlator(_NRZ, doppler_rows=8),
        "detector_1d": lambda: Correlator(_NRZ, detect=1),
        "detector_2d": lambda: Correlator(ref2, detect=1),
        "continuous": lambda: Correlator(
            _NRZ,
            framing="continuous",
            sample_rate=4.0e6,
            chip_rate=1.0e6,
            cn0_dbhz=55.0,
            doppler_uncertainty=5.0e3,
        ),
        "burst": lambda: Correlator(
            _ZC, framing="burst", reps=8, sample_rate=1.0
        ),
    }[flavor]()


FLAVORS = [
    "1d",
    "2d",
    "delay_doppler",
    "detector_1d",
    "detector_2d",
    "continuous",
    "burst",
]


def test_each_flavor_builds_and_names_itself():
    for f in FLAVORS:
        assert _build(f).flavor == f


# Each read-out, the arguments it takes before its out= buffer, and the
# out= dtype. Big buffers, so a served flavor never fails on room.
_READOUTS = {
    "surface": ((), np.float32),
    "surface_doppler_hz": ((), np.float64),
    "surface_chip_phase": ((), np.float64),
    "surface_complex": ((), np.complex64),
    "block_prompt": ((0, 0), np.complex64),
    "block_raw": ((), np.complex64),
}
# Section 18: surface_complex serves both searches (the burst search's map
# equivalence reads it); the rest serve the continuous search only.
_SERVED_BY = {
    name: {"continuous", "burst"}
    if name == "surface_complex"
    else {"continuous"}
    for name in _READOUTS
}


def _read(c: Correlator, name: str) -> int:
    args, dtype = _READOUTS[name]
    return getattr(c, name)(*args, out=np.zeros(1 << 16, dtype))


@pytest.mark.parametrize("name", sorted(_READOUTS))
@pytest.mark.parametrize("flavor", FLAVORS)
def test_a_readout_refuses_off_its_flavors(name, flavor):
    c = _build(flavor)
    if flavor in _SERVED_BY[name]:
        # Served: nothing decided yet, so "nothing yet" (0), never a raise.
        assert _read(c, name) >= 0
    else:
        with pytest.raises(ValueError, match=f"^{name}: only "):
            _read(c, name)


@pytest.mark.parametrize("flavor", FLAVORS)
def test_keep_surface_is_the_continuous_searchs(flavor):
    c = _build(flavor)
    if flavor == "continuous":
        assert c.keep_surface == 0
        c.keep_surface = 1
        assert c.keep_surface == 1
        return
    only = "^keep_surface: only the continuous"
    with pytest.raises(AttributeError, match=only):
        c.keep_surface  # noqa: B018 -- the read is the call under test
    with pytest.raises(AttributeError, match=only):
        c.keep_surface = 1
    assert not hasattr(c, "keep_surface")


@pytest.mark.parametrize("flavor", FLAVORS)
def test_last_corr_is_a_detectors(flavor):
    c = _build(flavor)
    if flavor.startswith("detector"):
        assert c.last_corr is None  # served: no push has produced one yet
        return
    only = "^last_corr: only a detector"
    with pytest.raises(AttributeError, match=only):
        c.last_corr  # noqa: B018 -- the read is the call under test
    assert not hasattr(c, "last_corr")
