"""``BurstAcquisition`` is a thin composing object: a genuinely separate jm
object (``objects/burst_acq.toml``) whose C core (``burst_acq_core.c``) is a
pure forwarder onto the SAME shared ``acq_state_t``/``acq_core.c`` engine
``Acquisition`` (continuous) uses -- the two public front doors onto one
engine that ``SPEC.md``'s Acquisition/BurstAcquisition split calls for.
``test_acq.py`` already exhaustively covers this class's own streaming/
physics behavior (it predates the split and was renamed wholesale, since its
reps/coherent-depth scenario matches ``BurstAcquisition``, not continuous
``Acquisition``). This file covers what's specific to the split itself: the
composing object's own construction/reset/state-triplet round trip, and the
cross-class relationship with ``Acquisition`` -- same config, divergent
grids, and state blobs that reject across classes.
"""

import numpy as np
import pytest

from doppler.dsss import Acquisition, BurstAcquisition
from doppler.wfm import PN, mls_poly

SF = 31  # length-31 MLS (5-stage), matches test_acq_continuous.py's CODE
CODE = np.asarray(PN(poly=mls_poly(5), seed=1, length=5).generate(SF)).astype(
    np.uint8
)
SPC = 4
CHIP_RATE = 1.0e6
PFA = 1e-3
PD = 0.9
CN0_DBHZ = 50.0  # comfortably powered for both classes at this grid


def _burst_acq(**kw):
    kw.setdefault("reps", 8)
    kw.setdefault("spc", SPC)
    kw.setdefault("chip_rate", CHIP_RATE)
    kw.setdefault("cn0_dbhz", CN0_DBHZ)
    kw.setdefault("doppler_uncertainty", 0.0)
    kw.setdefault("pfa", PFA)
    kw.setdefault("pd", PD)
    return BurstAcquisition(CODE, **kw)


def test_create():
    obj = _burst_acq()
    assert obj is not None
    assert obj.code_bins == SF * SPC
    assert obj.doppler_bins >= 1


def test_getter_setter():
    a = _burst_acq()
    assert a.sf == SF
    assert a.spc == SPC
    assert a.fs == pytest.approx(CHIP_RATE * SPC)
    assert a.pd == pytest.approx(PD)
    assert a.n_noncoh >= 1


def test_reset():
    """reset() clears in-flight accumulation without needing a fresh object:
    pushing the same stream after a mid-stream reset reproduces a totally
    fresh engine's hits on that same stream."""
    np.random.default_rng(3)
    s0 = np.repeat(np.where(CODE & 1, -1.0, 1.0), SPC).astype(np.complex64)
    burst = np.tile(s0, 8).astype(np.complex64)

    a = _burst_acq()
    a.push(burst[: len(burst) // 2])  # partial dwell, in-flight state
    a.reset()
    hits_after_reset = a.push(burst)

    fresh = _burst_acq()
    hits_fresh = fresh.push(burst)

    def cells(hs):
        return [(h[0], h[1]) for h in hs]

    assert cells(hits_after_reset) == cells(hits_fresh)


def test_context_manager():
    with _burst_acq() as obj:
        assert obj.code_bins == SF * SPC


def test_destroy():
    obj = _burst_acq()
    obj.destroy()


def test_state_roundtrip_resume():
    """Serialize mid-stream, restore into a fresh engine, resume -- the
    pod-handoff guarantee, bespoke here (frame/push object) same as
    Acquisition's own in test_acq_continuous.py and BurstAcquisition's
    streaming variant in test_acq.py's test_state_roundtrip_resume."""
    rng = np.random.default_rng(5)
    a = _burst_acq(cn0_dbhz=90.0)  # strong -> few reps needed, fast hits
    s0 = np.repeat(np.where(CODE & 1, -1.0, 1.0), SPC).astype(np.complex64)
    burst = np.tile(s0, a.doppler_bins * 4).astype(np.complex64)
    noise = 0.05 * (
        rng.standard_normal(len(burst)) + 1j * rng.standard_normal(len(burst))
    )
    stream = (burst + noise).astype(np.complex64)
    cut = len(stream) // 2

    ref = _burst_acq(cn0_dbhz=90.0).push(stream)

    e1 = _burst_acq(cn0_dbhz=90.0)
    hits = list(e1.push(stream[:cut]))
    blob = e1.get_state()
    assert isinstance(blob, bytes) and len(blob) == e1.state_bytes()

    e2 = _burst_acq(cn0_dbhz=90.0)
    e2.set_state(blob)
    hits += list(e2.push(stream[cut:]))

    def cells(hs):
        return [(h[0], h[1]) for h in hs]

    assert cells(hits) == cells(ref)
    assert len(ref) >= 1, "scenario should produce at least one real hit"

    with pytest.raises(ValueError):
        e2.set_state(b"\x00")
    with pytest.raises(TypeError):
        e2.set_state(42)


# ── Cross-class: Acquisition and BurstAcquisition share one engine ─────────


def _continuous_acq(**kw):
    kw.setdefault("spc", SPC)
    kw.setdefault("chip_rate", CHIP_RATE)
    kw.setdefault("symbol_rate", 1000.0)
    kw.setdefault("cn0_dbhz", CN0_DBHZ)
    kw.setdefault("doppler_uncertainty", 0.0)
    kw.setdefault("pfa", PFA)
    kw.setdefault("pd", PD)
    return Acquisition(CODE, **kw)


def test_same_config_diverges_by_design():
    """At an identical (code/spc/chip_rate/cn0/pfa/pd) config within one
    native Doppler span, the two classes size DIFFERENT grids -- intended,
    not a bug: BurstAcquisition coherently combines up to its reps ceiling
    (doppler_bins > 1 possible), Acquisition never does (doppler_bins == 1
    always in-span, sensitivity purely from n_noncoh). This is the split's
    whole point (SPEC.md's Acquisition/BurstAcquisition rationale)."""
    cont = _continuous_acq()
    burst = _burst_acq()

    assert cont.doppler_bins == 1  # continuous: window_bins==1 in-span
    assert burst.doppler_bins > 1  # burst: coherently combined
    assert not cont.underpowered and not burst.underpowered
    # Different mechanisms buy sensitivity differently -> different
    # non-coherent look counts for the same target pd at the same cn0.
    assert cont.n_noncoh != burst.n_noncoh


def test_cross_class_state_rejection():
    """A state blob from one class's engine is rejected by the other's --
    same size/magic/version validation the C envelope already enforces,
    now exercised across the two Python front doors onto that ONE engine."""
    cont = _continuous_acq()
    burst = _burst_acq()
    assert cont.state_bytes() != burst.state_bytes()  # different grids

    blob_cont = cont.get_state()
    blob_burst = burst.get_state()

    with pytest.raises(ValueError):
        burst.set_state(blob_cont)
    with pytest.raises(ValueError):
        cont.set_state(blob_burst)

    # Each still round-trips into a FRESH instance of its OWN class.
    cont2 = _continuous_acq()
    cont2.set_state(blob_cont)
    assert cont2.doppler_bins == cont.doppler_bins

    burst2 = _burst_acq()
    burst2.set_state(blob_burst)
    assert burst2.doppler_bins == burst.doppler_bins
