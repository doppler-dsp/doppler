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

import inspect
import re
from pathlib import Path

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


def _read(c: Correlator, name: str) -> int:
    args, dtype = _READOUTS[name]
    return getattr(c, name)(*args, out=np.zeros(1 << 16, dtype))


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


# ── Section 18's tables, against the code (#2958 review) ───────────────────
#
# The two tables in docs/design/correlator-python.md section 18 name which
# flavors serve each flavor-only member. They are read here, not restated:
# every public property of the class must have a row, so a new member is
# covered the moment it exists, and every (member, flavor) pair is checked
# against the code -- a served flavor reads or calls it, every other
# refuses with the member's own declared sentence.

_DESIGN = (
    Path(__file__).resolve().parents[4] / "docs/design/correlator-python.md"
)
#: Each "served by" phrase the tables use, as the flavors it names.
_PHRASES = {
    "every flavor": set(FLAVORS),
    "the maps and the detectors": {
        "1d",
        "2d",
        "delay_doppler",
        "detector_1d",
        "detector_2d",
    },
    "1-D, 2-D, delay-Doppler": {"1d", "2d", "delay_doppler"},
    "1-D, 2-D": {"1d", "2d"},
    "delay-Doppler": {"delay_doppler"},
    "1-D, both detectors": {"1d", "detector_1d", "detector_2d"},
    "both detectors": {"detector_1d", "detector_2d"},
    "the detectors and searches": {
        "detector_1d",
        "detector_2d",
        "continuous",
        "burst",
    },
    "both searches": {"continuous", "burst"},
    "the continuous search": {"continuous"},
    "the burst search": {"burst"},
}


def _section_18_table(header: str) -> dict[str, set[str]]:
    text = _DESIGN.read_text(encoding="utf-8")
    body = text[text.index("## 18. ") :]
    body = body[body.index(f"| {header} ") :]
    served: dict[str, set[str]] = {}
    for line in body.splitlines()[2:]:
        if not line.startswith("|"):
            break
        names, phrase = (c.strip() for c in line.strip("|").split("|"))
        for name in re.findall(r"`(\w+)`", names):
            # One row a member: a second would silently win over the first
            # (keep_surface once sat in two rows that disagreed).
            assert name not in served, f"{name} is in two rows of {header}"
            served[name] = _PHRASES[phrase]
    return served


_PROPS = _section_18_table("properties")
_METHODS = _section_18_table("methods")


def _telemetry():
    from doppler.telemetry import Telemetry

    return Telemetry(1 << 12)


#: How each table method is called: a served flavor must accept the call.
_CALLS = {
    "set_max_peaks": lambda c: c.set_max_peaks(2),
    "configure_search_raw": lambda c: c.configure_search_raw(1, 1),
    "set_threads": lambda c: c.set_threads(1),
    "set_carrier_freq_hz": lambda c: c.set_carrier_freq_hz(2.5e9),
    "set_telemetry": lambda c: c.set_telemetry(_telemetry(), "acq"),
    **{name: (lambda c, n=name: _read(c, n)) for name in _READOUTS},
}


def test_every_property_has_a_row():
    public = {
        n
        for n, v in inspect.getmembers(Correlator)
        if inspect.isdatadescriptor(v) and not n.startswith("_")
    }
    assert public == set(_PROPS), (
        f"not in section 18's table: {sorted(public - set(_PROPS))}; "
        f"in the table but not the class: {sorted(set(_PROPS) - public)}"
    )
    assert set(_METHODS) == set(_CALLS)


@pytest.mark.parametrize("name", sorted(_PROPS))
@pytest.mark.parametrize("flavor", FLAVORS)
def test_a_property_is_served_as_the_table_says(name, flavor):
    c = _build(flavor)
    if flavor in _PROPS[name]:
        getattr(c, name)
    else:
        with pytest.raises(AttributeError, match=f"^{name}: "):
            getattr(c, name)


@pytest.mark.parametrize("name", sorted(_METHODS))
@pytest.mark.parametrize("flavor", FLAVORS)
def test_a_method_is_served_as_the_table_says(name, flavor):
    c = _build(flavor)
    if flavor in _METHODS[name]:
        _CALLS[name](c)
    else:
        with pytest.raises(ValueError, match=f"^{name}: only "):
            _CALLS[name](c)
