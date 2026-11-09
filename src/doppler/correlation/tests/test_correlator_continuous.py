"""Coverage for the continuous search,
``doppler.correlation.Correlator(ref, framing="continuous", ...)`` (until
doppler#2752 the class ``Acquisition``), that isn't already exercised
elsewhere in this test suite.

``test_correlator_continuous_stress.py`` already characterizes this
flavor's search *accuracy* (Doppler/code-phase/CN0 sweep, reusing
the `continuous_search` characterization subject's own scene builder).
What's missing --
because it was only ever written for the burst search in
``test_correlator_burst_stream.py`` -- is
coverage of this flavor's own grid-configuration surface:
``configure_search_raw``'s bound (the fixed internal 256-look safety-valve
ceiling, no caller-supplied ``max_noncoh`` any more), the ``underpowered``
flag, and the state-serialization round trip (this is a frame/push object,
so its round trip is bespoke here rather than in the generic
``test_state_serialization.py`` matrix, same precedent as ``Acquisition``'s
own pre-split tests and ``DsssReceiver``'s).
"""

import re
import warnings

import numpy as np
import pytest

from doppler.acquire.tests._preamble import code_preamble
from doppler.correlation import Correlator
from doppler.tests._oom import linux_only, run_capped
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


def _acq(spc=SPC, **kw):
    # Samples a chip is the caller's sample rate over the chip rate.
    kw.setdefault("chip_rate", CHIP_RATE)
    kw.setdefault("sample_rate", kw["chip_rate"] * spc)
    kw.setdefault("symbol_rate", 1000.0)
    kw.setdefault("cn0_dbhz", CN0_DBHZ)
    kw.setdefault("doppler_uncertainty", 0.0)
    kw.setdefault("pfa", PFA)
    kw.setdefault("pd", PD)
    return Correlator(code_preamble(CODE, 1), framing="continuous", **kw)


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
    assert beyond.doppler_bins == 3  # 2*ceil((3*span - span)/(2*span)) + 1 = 3


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
    with pytest.warns(UserWarning, match="cannot meet pd"):
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
        [
            noise_pre.astype(np.complex64),
            (burst + noise_burst).astype(np.complex64),
        ]
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
        rng,
        n_epochs=a.n_noncoh * 6,
        code_bins=code_bins,
        sigma=_sigma(90.0, a.sample_rate),
        strong_sigma=_sigma(90.0, a.sample_rate) * 0.1,
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
        return [(int(h["row"]), int(h["col"])) for h in hs]

    assert cells(hits) == cells(ref)
    assert len(ref) >= 1, "scenario should produce at least one real hit"

    with pytest.raises(ValueError):  # size mismatch
        e2.set_state(b"\x00")
    with pytest.raises(TypeError):  # not bytes
        e2.set_state(42)


def _emitter(acq, tile: int, code_phase: int, dwells: int):
    """A noise-free emitter on window tile ``tile`` at ``code_phase``
    samples: the rolled replica on the carrier that is ``tile`` bins of the
    epoch-length FFT, for ``dwells`` decided dwells."""
    nx = acq.code_bins
    n = dwells * acq.n_noncoh * nx
    k = np.arange(n)
    q = k % nx
    src = (q + nx - code_phase % nx) % nx
    chips = CODE[(src // SPC) % SF]
    return (
        np.where(chips, -1.0, 1.0) * np.exp(2j * np.pi * tile * k / nx)
    ).astype(np.complex64)


def test_telemetry_probes_and_the_surface_tap():
    """docs/design/async-dsss-receiver.md §2.4: ten probes per decided dwell;
    the surface in the gate's units, its maximum the dwell's test statistic at
    the reported cell; the axes the hand-off's numbers; nothing kept until
    ``keep_surface``."""
    from doppler.telemetry import Telemetry

    a = _acq(doppler_uncertainty=3 * CHIP_RATE / (2 * SF))
    assert a.doppler_bins > 1
    tlm = Telemetry(1 << 12)
    a.set_telemetry(tlm, "acq")
    names = sorted(tlm.probe_names)
    assert names == sorted(
        "acq." + s
        for s in [
            "stat",
            "gate",
            "noise",
            "peak",
            "row",
            "col",
            "n_peaks",
            "n_held",
            "conc",
            "hit",
        ]
    )
    assert a.keep_surface == 0
    x = _emitter(a, tile=1, code_phase=5, dwells=3)
    a.keep_surface = 1
    hits = a.push(x)
    assert len(hits) >= 3
    recs = tlm.read()
    assert len(recs) == 10 * 3
    hit_id = tlm.probe_id("acq.hit")
    assert np.all(recs["value"][recs["probe"] == hit_id] == 1.0)

    s = np.empty(a.surface_rows * a.lag_bins, dtype=np.float32)
    assert a.surface(s) == s.size
    # a hit is a record: row (doppler_bin), col (code_phase), ...,
    # test_stat, ..., samples_consumed
    assert a.surface_at == hits[-1]["samples_consumed"]
    surf = s.reshape(a.surface_rows, a.lag_bins)
    row, col = np.unravel_index(np.argmax(surf), surf.shape)
    assert (row, col) == (hits[-1]["row"], hits[-1]["col"])
    # to a float rounding: the SIMD build's fast-math may normalise the
    # surface with a reciprocal where the statistic took a divide
    assert np.isclose(surf[row, col], hits[-1]["test_stat"], rtol=4e-7, atol=0)
    assert a.peak_conc > 0.5

    hz = np.empty(a.surface_rows)
    ch = np.empty(a.lag_bins)
    assert a.surface_doppler_hz(hz) == hz.size
    assert a.surface_chip_phase(ch) == ch.size
    assert hz[0] == 0.0 and hz[1] == CHIP_RATE / SF and hz[-1] == -hz[1]
    # column c is a lag of c * code_bins / lag_bins samples
    assert ch[0] == 0.0
    assert ch[col] == pytest.approx((SF - col * SF / a.lag_bins) % SF)

    # too small a buffer, and nothing kept once the tap is off
    assert a.surface(np.empty(3, dtype=np.float32)) == 0
    a.keep_surface = 0
    a.reset()
    assert a.surface_at == 0
    a.push(_emitter(a, tile=1, code_phase=5, dwells=1))
    assert a.surface(s) == 0


def test_block_coherent_depth_inside_the_tiles():
    """docs/design/async-dsss-receiver.md §2.3: a pure-code window buys a
    coherent depth D inside every tile; the Doppler axis becomes one grid of
    ``doppler_bins`` native bins of ``doppler_res_hz = chip_rate / (sf * D)``;
    an emitter one row above tile +1 is reported at bin ``1 * D + 1``."""
    du = 3 * CHIP_RATE / (2 * SF)
    a = _acq(doppler_uncertainty=du, code_only_epochs=7)
    assert a.coherent_bins == 4
    assert a.doppler_bins == 3 * 4
    assert a.doppler_res_hz == pytest.approx(CHIP_RATE / (SF * 4))
    r = _acq(doppler_uncertainty=du, code_only_epochs=7, doppler_rate=1e9)
    assert r.coherent_bins == 1  # the rate bound floors at one epoch
    with pytest.raises(
        ValueError, match="code_only_epochs must be at least 1"
    ):
        _acq(doppler_uncertainty=du, code_only_epochs=0)

    nx = a.code_bins
    k = np.arange(3 * a.n_noncoh * 4 * nx)
    src = (k % nx + nx - 5) % nx
    chips = CODE[(src // SPC) % SF]
    x = (
        np.where(chips, -1.0, 1.0) * np.exp(2j * np.pi * 1.25 * k / nx)
    ).astype(np.complex64)
    hits = a.push(x)
    assert len(hits) >= 1
    assert (hits[0]["row"], hits[0]["col"]) == (1 * 4 + 1, 5)
    # one row BELOW tile +1: bin 1 * D - 1, adjacent on the one folded grid
    a.reset()
    x = (
        np.where(chips, -1.0, 1.0) * np.exp(2j * np.pi * 0.75 * k / nx)
    ).astype(np.complex64)
    hits = a.push(x)
    assert (hits[0]["row"], hits[0]["col"]) == (1 * 4 - 1, 5)
    # tile -1, one row below: bin -4 - 1 = -5 -> 12 - 5 = 7 in FFT order
    a.reset()
    x = (
        np.where(chips, -1.0, 1.0) * np.exp(-2j * np.pi * 1.25 * k / nx)
    ).astype(np.complex64)
    hits = a.push(x)
    assert (hits[0]["row"], hits[0]["col"]) == (3 * 4 - 4 - 1, 5)


def test_the_roll_per_thread_is_bit_identical():
    """docs/design/async-dsss-receiver.md §2.3: a tiled engine fans its tiles
    across a pool created with it; the surface and the hits are byte-identical
    at any thread count, and ``set_threads`` re-sizes the pool (0 = the
    machine's cores, 1 = serial)."""
    du = 3 * CHIP_RATE / (2 * SF)
    a = _acq(doppler_uncertainty=du, code_only_epochs=7)
    assert a.threads >= 1
    a.keep_surface = 1
    nx = a.code_bins
    k = np.arange(3 * a.n_noncoh * 4 * nx)
    src = (k % nx + nx - 5) % nx
    chips = CODE[(src // SPC) % SF]
    x = (
        np.where(chips, -1.0, 1.0) * np.exp(2j * np.pi * 1.25 * k / nx)
    ).astype(np.complex64)
    a.set_threads(1)
    assert a.threads == 1
    ref_hits = a.push(x)
    ref = np.empty(a.surface_rows * a.lag_bins, np.float32)
    assert a.surface(ref) == ref.size
    for n in (2, 4, 8, 0):
        a.set_threads(n)
        assert a.threads >= 1
        a.reset()
        hits = a.push(x)
        got = np.empty_like(ref)
        assert a.surface(got) == got.size
        assert np.array_equal(ref, got), n
        assert np.array_equal(hits, ref_hits), n
    # a single-tile engine has nothing to fan
    s = _acq(doppler_uncertainty=0.0)
    assert s.threads == 1
    s.set_threads(4)
    assert s.threads == 1


def test_carrier_and_prior_refusals_are_raised():
    """The carrier rule and the finite prior (doppler#2103), from Python.
    set_carrier_freq_hz refuses a carrier at or below half the sample rate
    (here 1e-305, which overflowed doppler / carrier) and keeps the one set.
    A non-finite doppler_uncertainty is refused at create, as a ValueError
    naming the domain (Acquisition's create_error, doppler#2112 item 7)."""
    from doppler.wfm import Gold

    code = np.asarray(Gold().generate(1023)).astype(np.uint8)
    kw = {
        "sample_rate": 2 * 5e6,
        "chip_rate": 5e6,
        "symbol_rate": 2700.0,
        "cn0_dbhz": 45.0,
    }
    a = Correlator(
        code_preamble(code, 1),
        framing="continuous",
        doppler_uncertainty=50e3,
        **kw,
    )
    a.set_carrier_freq_hz(2.5e9)
    # On the continuous search the carrier is served: an out-of-range one is
    # a bad value (DP_ERR_INVALID), with its own sentence, not the
    # off-flavor one (DP_ERR_UNSUPPORTED).
    refused = re.escape(
        "set_carrier_freq_hz: must be 0 (no carrier) or finite and above "
        "sample_rate / 2"
    )
    with pytest.raises(ValueError, match=refused):
        a.set_carrier_freq_hz(1e-305)
    with pytest.raises(ValueError, match=refused):
        a.set_carrier_freq_hz(5e6)  # exactly fs / 2
    assert a.carrier_freq_hz == 2.5e9
    finite = "doppler_uncertainty must be finite and >= 0"
    with pytest.raises(ValueError, match=finite):
        Correlator(
            code_preamble(code, 1),
            framing="continuous",
            doppler_uncertainty=float("nan"),
            **kw,
        )


# An allocation failure raises MemoryError, not a refused argument's
# ValueError (doppler#2846, just-makeit#2470). The carrier rebuilds the
# delay-Doppler map, about 9 MB more on this engine, so a child process caps
# its address space just above its current size and makes the change: the
# rebuild's allocation fails, and nothing else in the call allocates that
# much. The message is the one the manifest declares for DP_ERR_MEMORY, so
# Python running out of memory itself cannot pass the test. With the cap
# lifted, the engine's blob is byte for byte the one before.
_OOM_SETUP = """
import numpy as np
from doppler.acquire.tests._preamble import code_preamble
from doppler.correlation import Correlator
from doppler.wfm import Gold

code = np.asarray(Gold().generate(1023)).astype(np.uint8)
kw = dict(sample_rate=10.0e6, chip_rate=5.0e6, symbol_rate=2700.0,
          cn0_dbhz=37.31, doppler_uncertainty=50000.0, pfa=1e-3, pd=0.9,
          code_only_epochs=150, doppler_rate=500.0)
a = Correlator(code_preamble(code, 1), framing="continuous", **kw)
b = Correlator(code_preamble(code, 1), framing="continuous", **kw)
b.set_carrier_freq_hz(2.2e9)
blob = b.get_state()
before = a.get_state()
"""
_OOM_CALLS = {
    "set_carrier_freq_hz": "a.set_carrier_freq_hz(2.2e9)",
    "set_state": "a.set_state(blob)",
}
_OOM_AFTER = """
print("unchanged" if a.get_state() == before else "CHANGED")
print("carrier", a.carrier_freq_hz)
"""


@linux_only
@pytest.mark.parametrize(
    ("entry", "message"),
    [
        (
            "set_carrier_freq_hz",
            "set_carrier_freq_hz: rebuilding the delay-Doppler map for the "
            "new carrier could not allocate; the engine is unchanged",
        ),
        (
            "set_state",
            "set_state: the blob's carrier needs the delay-Doppler map "
            "rebuilt, and the rebuild could not allocate; the engine is "
            "unchanged",
        ),
    ],
)
def test_a_carrier_rebuild_that_cannot_allocate_raises_memoryerror(
    entry: str, message: str
) -> None:
    r = run_capped(_OOM_SETUP, _OOM_CALLS[entry], after=_OOM_AFTER)
    lines = r.stdout.splitlines()
    assert r.returncode == 0, r.stderr[-2000:]
    assert lines == [f"MemoryError: {message}", "unchanged", "carrier 0.0"]
