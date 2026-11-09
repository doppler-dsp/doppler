"""Who serves each member, from Python (docs/design/correlator-python.md
section 18).

One class reaches every member from every flavor, and the C refuses a
member off the flavors that serve it, never the binding. These tests read
section 18's two tables (properties and methods) from the design page and
hold every row against the code, on every flavor: a served flavor reads or
calls the member, and every other raises the member's own declared
off-flavor sentence (a property's AttributeError, a method's ValueError).

Both tables are complete by derivation: every public property of the class
is a row, every method whose manifest declares an off-flavor sentence
("name: only ...") is a row, and no member is in two rows. A refusal is
always checked beside a flavor that serves the member, so a binding that
raised everywhere fails too.
"""

import inspect
import re
import sys

import numpy as np
import pytest

from doppler.correlation import Correlator
from doppler.tests._repo import repo_root
from doppler.wfm import PN, mls_poly

if sys.version_info >= (3, 11):
    import tomllib
else:
    import tomli as tomllib

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

_DESIGN = repo_root() / "docs/design/correlator-python.md"
#: Each "served by" phrase the tables use, as the flavors it names.
_PHRASES = {
    "every flavor": set(FLAVORS),
    "the maps": {"1d", "2d", "delay_doppler"},
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


def _frame(c: Correlator) -> np.ndarray:
    """One frame for a map (ny x nx samples); any length off a map, where
    the flavor is refused before the length is read."""
    n = c.ny * c.nx if c.flavor in ("1d", "2d", "delay_doppler") else 64
    return np.ones(n, np.complex64)


_NO_RATES = np.empty(0, np.complex64)

#: How each table method is called: a served flavor must accept the call.
_CALLS = {
    "execute": lambda c: c.execute(_frame(c)),
    "execute_all": lambda c: c.execute_all(_frame(c)),
    "execute_coherent": lambda c: c.execute_coherent(_frame(c)),
    "execute_power_sum": lambda c: c.execute_power_sum(_frame(c)),
    "execute_power_max": lambda c: c.execute_power_max(_frame(c)),
    "execute_peaks": lambda c: c.execute_peaks(_frame(c)),
    "bank_power_max": lambda c: c.bank_power_max(
        _frame(c), 1, 0, 0, _NO_RATES
    ),
    "bank_detect": lambda c: c.bank_detect(
        _frame(c), 1, 1, 0, 0, 0.0, 0.0, _NO_RATES
    ),
    "push": lambda c: c.push(np.ones(64, np.complex64)),
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


def _declared_refusals() -> dict[str, str]:
    """Each method's declared off-flavor sentence, from its manifest: the
    DP_ERR_UNSUPPORTED status message, or a sentinel method's own message,
    when it is the off-flavor kind ("name: only ...")."""
    manifest = repo_root() / "objects/correlator.toml"
    with manifest.open("rb") as f:
        methods = tomllib.load(f)["correlator"]["methods"]
    out = {}
    for m in methods:
        msgs = [
            s["message"]
            for s in m.get("status_errors", [])
            if s["status"] == "DP_ERR_UNSUPPORTED"
        ] + ([m["error_message"]] if "error_message" in m else [])
        for msg in msgs:
            if msg.startswith(m["name"] + ": only "):
                out[m["name"]] = msg
    return out


_REFUSALS = _declared_refusals()


def test_every_flavor_refusing_method_has_a_row():
    """A method that declares an off-flavor refusal is a §18 row, and a row
    names a method that declares one: derived from the manifest, so a new
    flavor-only method is covered the moment it is declared."""
    assert set(_REFUSALS) == set(_METHODS), (
        f"declared but no row: {sorted(set(_REFUSALS) - set(_METHODS))}; "
        f"a row but nothing declared: {sorted(set(_METHODS) - set(_REFUSALS))}"
    )


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
        # The whole message, both ends: a method's refusal is exactly its
        # declared sentence (a property's read appends its rc).
        refusal = "^" + re.escape(_REFUSALS[name]) + "$"
        with pytest.raises(ValueError, match=refusal):
            _CALLS[name](c)
