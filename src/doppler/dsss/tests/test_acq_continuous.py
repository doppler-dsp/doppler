"""Coverage for ``doppler.dsss.Acquisition`` (the continuous front door onto
``acq_core.c``) that isn't already exercised elsewhere in this test suite.

``test_dsss_acquisition_stress.py`` already characterizes this class's
search *accuracy* (Doppler/code-phase/CN0 sweep, reusing
``dsss_acquisition_stress.py``'s own scene builder). What's missing --
because it was only ever written for ``BurstAcquisition`` in
``test_acq.py`` before the Acquisition/BurstAcquisition split -- is
coverage of this class's own grid-configuration surface:
``configure_search_raw``'s bound (the fixed internal 256-look safety-valve
ceiling, no caller-supplied ``max_noncoh`` any more), the ``underpowered``
flag, and the state-serialization round trip (this is a frame/push object,
so its round trip is bespoke here rather than in the generic
``test_state_serialization.py`` matrix, same precedent as ``Acquisition``'s
own pre-split tests and ``DsssReceiver``'s).
"""

import warnings

import numpy as np
import pytest

from doppler.dsss import Acquisition
from doppler.wfm import PN, mls_poly

SF = 31  # length-31 MLS (5-stage) -- small and fast, matches test_acq.py's
# own DATA_CODE convention.
CODE = np.asarray(PN(poly=mls_poly(5), seed=1, length=5).generate(SF)).astype(
    np.uint8
)
SPC = 4
CHIP_RATE = 1.0e6
PFA = 1e-3
PD = 0.9
CN0_DBHZ = 50.0  # comfortably powered (not underpowered) at doppler_bins=1


def _acq(**kw):
    kw.setdefault("spc", SPC)
    kw.setdefault("chip_rate", CHIP_RATE)
    kw.setdefault("symbol_rate", 1000.0)
    kw.setdefault("cn0_dbhz", CN0_DBHZ)
    kw.setdefault("doppler_uncertainty", 0.0)
    kw.setdefault("pfa", PFA)
    kw.setdefault("pd", PD)
    return Acquisition(CODE, **kw)


def test_always_window_tiles():
    """Continuous Acquisition never coherently combines: doppler_bins stays
    1 within the native span and only grows via window-tiling once
    doppler_uncertainty exceeds it -- unconditionally, regardless of how
    weak cn0_dbhz is (there is no reps/coherent-depth axis at all on this
    class to fall back to)."""
    span = _acq().doppler_span_hz
    with warnings.catch_warnings():
        warnings.simplefilter("ignore", UserWarning)
        within = _acq(doppler_uncertainty=span * 0.5, cn0_dbhz=25.0)
    assert within.doppler_bins == 1

    with warnings.catch_warnings():
        warnings.simplefilter("ignore", UserWarning)
        beyond = _acq(doppler_uncertainty=span * 3.0)
    assert beyond.doppler_bins == 3  # ceil(3*span/span) window tiles


def test_configure_search_raw_bounds():
    """Out-of-range pins raise ValueError and leave the grid untouched; the
    n_noncoh ceiling is now the fixed internal safety valve (256), not a
    caller-suppliable max_noncoh."""
    a = _acq()
    orig = (a.doppler_bins, a.n_noncoh)

    for db, nc in [(0, 1), (1, 0), (1, 257)]:
        with pytest.raises(ValueError):
            a.configure_search_raw(db, nc)
    assert (a.doppler_bins, a.n_noncoh) == orig

    a.configure_search_raw(1, 2)
    assert (a.doppler_bins, a.n_noncoh) == (1, 2)


def test_underpowered_warns():
    """An infeasible operating point still builds, but warns + self-flags,
    hitting the internal 256-look ceiling without meeting pd."""
    with pytest.warns(UserWarning, match="under-powered"):
        a = _acq(cn0_dbhz=20.0)
    assert a.underpowered
    assert a.n_noncoh == 256  # rode the ceiling, never met pd
    assert a.pd_predicted < a.pd

    with warnings.catch_warnings():
        warnings.simplefilter("error")
        b = _acq(cn0_dbhz=CN0_DBHZ)
    assert not b.underpowered


def _sigma(cn0_dbhz, fs):
    amp_snr = np.sqrt(10.0 ** (cn0_dbhz / 10.0) / fs)
    return 1.0 / amp_snr


def _burst_stream(rng, n_epochs, code_bins, sigma, strong_sigma):
    """A repeating, zero-Doppler BPSK code burst (strong_sigma) preceded by
    pure noise (sigma) -- enough non-coherent looks for a hit to fire
    partway through, so a mid-stream state split has real in-flight
    accumulation to carry across."""
    s0 = np.repeat(np.where(CODE & 1, -1.0, 1.0), SPC).astype(np.complex64)
    noise_pre = (sigma / np.sqrt(2.0)) * (
        rng.standard_normal(code_bins) + 1j * rng.standard_normal(code_bins)
    )
    burst = np.tile(s0, n_epochs)
    noise_burst = (strong_sigma / np.sqrt(2.0)) * (
        rng.standard_normal(len(burst)) + 1j * rng.standard_normal(len(burst))
    )
    return np.concatenate(
        [noise_pre.astype(np.complex64), (burst + noise_burst).astype(np.complex64)]
    )


def test_state_roundtrip_resume():
    """Serialize mid-stream, restore into a fresh engine, and resume -- the
    concatenated detections must match an uninterrupted run (the pod-handoff
    guarantee), mirroring BurstAcquisition's own bespoke round trip in
    test_acq.py and DsssReceiver's in test_dsss_receiver.py."""
    rng = np.random.default_rng(2024)
    a = _acq(cn0_dbhz=90.0)  # strong sizing -> n_noncoh small, fast hits
    code_bins = a.code_bins
    stream = _burst_stream(
        rng, n_epochs=a.n_noncoh * 6, code_bins=code_bins,
        sigma=_sigma(90.0, a.fs), strong_sigma=_sigma(90.0, a.fs) * 0.1,
    )
    cut = len(stream) // 2

    ref = _acq(cn0_dbhz=90.0).push(stream)  # uninterrupted reference

    e1 = _acq(cn0_dbhz=90.0)
    hits = list(e1.push(stream[:cut]))
    blob = e1.get_state()
    assert isinstance(blob, bytes) and len(blob) == e1.state_bytes()

    e2 = _acq(cn0_dbhz=90.0)
    e2.set_state(blob)
    hits += list(e2.push(stream[cut:]))

    def cells(hs):
        return [(h[0], h[1]) for h in hs]

    assert cells(hits) == cells(ref)
    assert len(ref) >= 1, "scenario should produce at least one real hit"

    with pytest.raises(ValueError):  # size mismatch
        e2.set_state(b"\x00")
    with pytest.raises(TypeError):  # not bytes
        e2.set_state(42)
