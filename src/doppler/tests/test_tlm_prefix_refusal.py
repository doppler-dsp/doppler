"""Every ``set_telemetry`` refuses a prefix its probe names cannot hold.

A probe name is ``"<prefix>.<suffix>"`` in a fixed ``DP_TLM_NAME_MAX``-byte
buffer, and the prefix is the caller's. Before #1898 each instrumented
object built its names with a bare ``snprintf``, which truncates silently:
the probe registered under a shortened name, two probes whose names
differed only past the cut collapsed onto one registry entry, and the
attach still reported success (#676). #1898 routed every site through
``dp_tlm_name_join()`` and reordered each one to build *every* name before
its first ``dp_tlm_probe()``, so a refusal leaves the registry untouched.

That reordering is the part that can go wrong, and before this file only
``agc`` and ``costas`` pinned it (#1944). A composite is where it bites: the
M-PSK receiver attaches its timing loop, then its carrier loop, then the
AGC through the DDC's rate converter, and a child that refuses after a
sibling has registered leaves dead entries behind.

So for every Python class with a probe-prefix ``set_telemetry``:

1. **An overlong prefix raises** ``ValueError``, and nothing truncated is
   registered: every name in the registry afterwards is either one that was
   there before or the exact ``"<prefix>.<suffix>"`` asked for. That is
   #676's alias, and it holds on every face.
2. **The refusal leaves the registry untouched**: same count, same names.
   This holds on every leaf. The four composites still register their own
   probes before a child refuses; that is #1897, which #676 left out of
   scope deliberately, and those four cases are strict ``xfail``s. They turn
   red the day #1897 lands, so the marker cannot outlive the defect.
3. **The longest prefix that fits attaches**, every name in full, the same
   set of suffixes a short prefix registers.

The boundary is derived, not restated. The name limit is measured from
:class:`~doppler.telemetry.Telemetry` itself, and each object's longest
suffix comes from attaching it under a one-character prefix, so a new probe
moves the boundary with it. The prefix one character longer than the
longest that fits makes exactly one name a byte too long, so a silent
truncation fails (1) or (3), and an attach that registers before checking
fails (2).

**Complete by discovery.** :func:`test_every_face_is_covered` walks
doppler's public packages for every class with a ``set_telemetry`` and fails
on one missing from :data:`FACES`. A newly instrumented object is therefore
red here until it is added, which is the only way a parameter table stays
the whole surface rather than the part somebody remembered.

The C-side twin of the rule, that names are joined only by
``dp_tlm_name_join()``, is ``make lint-tlm-name-join``.
"""

from __future__ import annotations

import importlib
import inspect
import pkgutil
from typing import TYPE_CHECKING, Any

import numpy as np
import pytest

import doppler
from doppler.acquire import Acquisition
from doppler.agc import AGC
from doppler.dsss import Despreader
from doppler.telemetry import Telemetry
from doppler.track import (
    BpskReceiver,
    CarrierNda,
    Costas,
    Dll,
    MpskReceiver,
    MpskReceiverR,
    RateSync,
    SymbolSync,
)

if TYPE_CHECKING:
    from collections.abc import Callable

_CODE = (np.arange(31) % 2).astype(np.uint8)

# Public name -> a fresh instance: the default configuration, built as small
# as each class accepts. The probe SET can depend on configuration
# (MpskReceiver(agc=0) registers no agc.* names); the boundary below is read
# from whatever these instances register, and the agc=0 case is pinned
# separately in test_a_disabled_agc_still_bounds_the_receiver_prefix.
FACES: dict[str, Callable[[], Any]] = {
    "doppler.acquire.Acquisition": lambda: Acquisition(
        _CODE, spc=2, chip_rate=1e6, cn0_dbhz=50.0
    ),
    "doppler.agc.AGC": lambda: AGC(ref_db=0.0, loop_bw=0.0025, alpha=0.05),
    "doppler.dsss.Despreader": lambda: Despreader(code=_CODE, sps=4),
    "doppler.track.BpskReceiver": lambda: BpskReceiver(
        sample_rate_hz=8e6, symbol_rate_hz=1e6
    ),
    "doppler.track.CarrierNda": lambda: CarrierNda(bn=0.01, sps=8, n=4, m=4),
    "doppler.track.Costas": lambda: Costas(bn=0.05, zeta=0.707, tsamps=64),
    "doppler.track.Dll": lambda: Dll(code=_CODE, sps=2),
    "doppler.track.MpskReceiver": lambda: MpskReceiver(m=4, sps=4, m_out=2),
    "doppler.track.MpskReceiverR": lambda: MpskReceiverR(m=4, sps=8, m_out=2),
    "doppler.track.RateSync": lambda: RateSync(
        sps=8.0, pulse="iandd", m=4, bn=0.01
    ),
    "doppler.track.SymbolSync": lambda: SymbolSync(sps=4, bn=0.01, zeta=0.707),
}

# Classes whose `set_telemetry` is not a probe-prefix attach, with the reason.
NOT_A_PROBE_PREFIX: dict[str, str] = {
    "doppler.telemetry.EventLog": (
        "names the dp_tlm record file a run's SigMF sidecar indexes; its "
        "argument is a path, and it registers no probes"
    ),
}

# The composites that register their own probes, then forward to a child
# whose longer names refuse: the dead registry slots of #1897. Every name they
# leave behind is exact, so (1) and (3) hold for them; only (2) does not.
_DEAD_SLOTS_1897 = {
    "doppler.dsss.Despreader",
    "doppler.track.BpskReceiver",
    "doppler.track.MpskReceiver",
    "doppler.track.MpskReceiverR",
}

# A probe that is already in the registry when the attach is tried, so
# "unchanged" is checked against a non-empty table, not just a zero count.
_RESIDENT = "resident"


def _discovered() -> set[str]:
    """Every public doppler class that has a ``set_telemetry``.

    Walks each top-level package's re-exported names, which is the public
    surface; ``tests`` is skipped. An import failure is not caught, so the
    walk cannot shrink silently.
    """
    found: set[str] = set()
    for info in pkgutil.iter_modules(doppler.__path__):
        if info.name == "tests":
            continue
        mod = importlib.import_module(f"doppler.{info.name}")
        for attr, obj in vars(mod).items():
            if (
                not attr.startswith("_")
                and inspect.isclass(obj)
                and hasattr(obj, "set_telemetry")
            ):
                found.add(f"doppler.{info.name}.{attr}")
    return found


def _name_max() -> int:
    """The longest probe name the registry accepts, measured.

    One less than ``DP_TLM_NAME_MAX``, which counts the terminator.
    """
    n = 1
    while True:
        try:
            Telemetry(1 << 4).probe("n" * (n + 1))
        except ValueError:
            return n
        n += 1


def _suffixes(make: Callable[[], Any]) -> set[str]:
    """The probe suffixes an object registers, read under prefix ``"p"``."""
    tlm = Telemetry(1 << 12)
    make().set_telemetry(tlm, "p")
    names = set(tlm.probe_names)
    assert names, "attached, but registered no probes to test"
    assert all(n.startswith("p.") for n in names), sorted(names)
    return {n[2:] for n in names}


def _fits(suffixes: set[str]) -> int:
    """The longest prefix length under which every suffix still fits."""
    return _name_max() - 1 - max(len(s) for s in suffixes)


def _feed(obj: Any) -> None:
    """Run one block through ``obj``: enough for every face to emit."""
    if hasattr(obj, "push"):  # Acquisition: a few dwells of its 31-chip code
        obj.push(np.ones(obj.n_noncoh * _CODE.size * 2 * 4, np.complex64))
        return
    real = type(obj).__name__ == "MpskReceiverR"
    obj.steps(np.ones(8192, np.float32 if real else np.complex64))


def _fresh() -> Telemetry:
    tlm = Telemetry(1 << 12)
    tlm.probe(_RESIDENT)
    return tlm


def test_every_face_is_covered() -> None:
    """A class with ``set_telemetry`` that is in neither table fails."""
    found = _discovered()
    listed = set(FACES) | set(NOT_A_PROBE_PREFIX)
    assert found - listed == set(), (
        "add these to FACES (or NOT_A_PROBE_PREFIX, with the reason)"
    )
    assert listed - found == set(), "listed, but no longer public"
    assert set(FACES) >= _DEAD_SLOTS_1897, "a #1897 xfail names no face"


def _refuse(face: str) -> tuple[Telemetry, str, set[str]]:
    """Attach ``face`` under a prefix one byte too long; return the wreck.

    Asserts the refusal itself. Returns the registry afterwards, the prefix
    tried, and the suffixes a fitting prefix would have registered.
    """
    make = FACES[face]
    suffixes = _suffixes(make)
    prefix = "q" * (_name_max() - max(len(s) for s in suffixes))
    tlm = _fresh()
    with pytest.raises(ValueError, match="set_telemetry failed"):
        make().set_telemetry(tlm, prefix)
    return tlm, prefix, suffixes


@pytest.mark.parametrize("face", sorted(FACES))
def test_an_overlong_prefix_is_refused_without_truncating(face: str) -> None:
    tlm, prefix, suffixes = _refuse(face)
    exact = {f"{prefix}.{s}" for s in suffixes} | {_RESIDENT}
    assert set(tlm.probe_names) <= exact, "a truncated name was registered"
    assert tlm.probe_id(_RESIDENT) == 0


@pytest.mark.parametrize(
    "face",
    [
        pytest.param(
            f,
            marks=pytest.mark.xfail(
                strict=True,
                raises=AssertionError,
                reason="#1897: a composite registers its own probes before "
                "a child refuses",
            ),
        )
        if f in _DEAD_SLOTS_1897
        else f
        for f in sorted(FACES)
    ],
)
def test_a_refusal_leaves_the_registry_untouched(face: str) -> None:
    tlm, _, _ = _refuse(face)
    assert tlm.probe_count == 1
    assert tlm.probe_names == {_RESIDENT: 0}


@pytest.mark.parametrize("face", sorted(FACES))
def test_the_longest_prefix_that_fits_attaches_in_full(face: str) -> None:
    make = FACES[face]
    suffixes = _suffixes(make)
    prefix = "q" * (_name_max() - 1 - max(len(s) for s in suffixes))

    tlm = _fresh()
    make().set_telemetry(tlm, prefix)
    want = {f"{prefix}.{s}" for s in suffixes} | {_RESIDENT}
    assert set(tlm.probe_names) == want
    assert max(len(n) for n in tlm.probe_names) == _name_max()


@pytest.mark.parametrize("face", sorted(FACES))
def test_every_overlong_prefix_is_refused_cleanly(face: str) -> None:
    """Sweep every prefix length past the boundary, to the name limit.

    One length exercises one refusal point. A composite's chain refuses at
    a different stage at each length: the M-PSK receiver's own names, then
    its `sync` loop, then the RateConverter's validate-before-record; the
    despreader's `car` and `code`. At every length the attach raises and
    registers nothing truncated. It leaves the registry untouched at every
    length except the one #1897 names, one past a composite's boundary,
    which the strict xfail above pins. A wider dead-slot window fails here.
    """
    make = FACES[face]
    suffixes = _suffixes(make)
    fits = _fits(suffixes)
    for length in range(fits + 1, _name_max() + 2):
        prefix = "q" * length
        tlm = _fresh()
        with pytest.raises(ValueError, match="set_telemetry failed"):
            make().set_telemetry(tlm, prefix)
        exact = {f"{prefix}.{s}" for s in suffixes} | {_RESIDENT}
        assert set(tlm.probe_names) <= exact, f"truncated at {length}"
        if face in _DEAD_SLOTS_1897 and length == fits + 1:
            continue  # #1897's window
        assert tlm.probe_names == {_RESIDENT: 0}, f"dead slots at {length}"


@pytest.mark.parametrize("face", sorted(FACES))
def test_a_refused_object_stays_detached(face: str) -> None:
    """After a refusal nothing emits, dead slots or not.

    Fed the same block, a successful attach emits records; that is the
    control, so "nothing emitted" cannot pass because the feed is idle.
    """
    make = FACES[face]
    fits = _fits(_suffixes(make))

    good, attached = Telemetry(1 << 14), make()
    attached.set_telemetry(good, "q" * fits)
    _feed(attached)
    assert len(good.read()) > 0, "the feed emits nothing even when attached"

    tlm, refused = _fresh(), make()
    with pytest.raises(ValueError, match="set_telemetry failed"):
        refused.set_telemetry(tlm, "q" * (fits + 1))
    _feed(refused)
    assert len(tlm.read()) == 0


def test_a_disabled_agc_still_bounds_the_receiver_prefix() -> None:
    """Pinned current behaviour, #1983: MpskReceiver(agc=0) registers no
    agc.* probe, but its RateConverter still validates `<p>.agc.level_db`.

    So a prefix every registered name fits is refused, after the loops have
    registered (the #1897 dead slots). When #1983 is fixed the agc=0
    receiver attaches at the boundary its own names set, and this goes red.
    """

    def make() -> MpskReceiver:
        return MpskReceiver(m=4, sps=4, m_out=2, agc=0)

    suffixes = _suffixes(make)
    assert not any(s.startswith("agc.") for s in suffixes)
    fits = _fits(suffixes)

    make().set_telemetry(_fresh(), "q" * (fits - 1))  # one short: attaches
    tlm = _fresh()
    with pytest.raises(ValueError, match="set_telemetry failed"):
        make().set_telemetry(tlm, "q" * fits)  # every own name fits
    assert tlm.probe_count > 1  # and the loops registered first (#1897)
