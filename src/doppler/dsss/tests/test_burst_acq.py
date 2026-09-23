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
from doppler.dsss.tests._preamble import code_preamble
from doppler.wfm import PN, mls_poly

SF = 31  # length-31 MLS (5-stage), matches test_acq_continuous.py's CODE
CODE = np.asarray(PN(poly=mls_poly(5), seed=1, length=5).generate(SF)).astype(
    np.uint8
)
SPC = 4
CHIP_RATE = 1.0e6
PFA = 1e-3
PD = 0.9
CN0_DBHZ = 55.0  # powered for both classes at this grid: burst D=6 (0.935),
# continuous via looks. 50 was "powered" only through looks a burst cannot
# fill (doppler#1181); coherently it predicts 0.47 at reps=8.


def _burst_acq(**kw):
    kw.setdefault("reps", 8)
    kw.setdefault("fs", CHIP_RATE * SPC)
    kw.setdefault("cn0_dbhz", CN0_DBHZ)
    kw.setdefault("doppler_uncertainty", 0.0)
    kw.setdefault("pfa", PFA)
    kw.setdefault("pd", PD)
    return BurstAcquisition(code_preamble(CODE, SPC), **kw)


def test_create():
    obj = _burst_acq()
    assert obj is not None
    assert obj.code_bins == SF * SPC
    assert obj.doppler_bins >= 1


def test_getter_setter():
    a = _burst_acq()
    assert a.code_bins == SF * SPC  # the preamble's samples
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


# ── the design C/N0: NaN is "none", every finite value is one (#1484) ──────


def test_no_design_point_is_the_default():
    """Omitted, cn0_dbhz is NaN: the whole preamble in one look, no target
    to be under, and so no warning."""
    import math
    import warnings

    with warnings.catch_warnings():
        warnings.simplefilter("error")
        b = BurstAcquisition(
            code_preamble(CODE, SPC), reps=8, fs=CHIP_RATE * SPC
        )
    assert math.isnan(b.cn0_dbhz) and math.isnan(b.pd_predicted)
    assert not b.underpowered and b.doppler_bins == 8


def test_a_negative_design_point_is_a_design_point():
    """0 used to mean "none", so a per-sample SNR -- negative wherever
    acquisition is hard -- could not be stated. Any finite value is now a
    real, if hopeless, design point: it sizes, predicts and warns."""
    with pytest.warns(UserWarning, match="under-powered"):
        b = BurstAcquisition(
            code_preamble(CODE, SPC), reps=8, fs=CHIP_RATE * SPC, cn0_dbhz=-5.0
        )
    assert b.cn0_dbhz == -5.0 and b.underpowered
    assert not np.isnan(b.pd_predicted)


def test_an_infinite_design_point_is_refused():
    """An infinite C/N0 is no design point: the constructor refuses it
    as the argument error it is, not a MemoryError (#1486)."""
    with pytest.raises(ValueError, match="cn0_dbhz finite or NaN"):
        BurstAcquisition(
            code_preamble(CODE, SPC),
            reps=8,
            fs=CHIP_RATE * SPC,
            cn0_dbhz=np.inf,
        )


# ── any repeated preamble (doppler#1470) ───────────────────────────────────
#
# The preamble is its samples, at fs; fs defaults to 1, normalized units.
# A PN code is one such preamble (code_preamble: bin_to_nrz, held spc).

_K = np.arange(127)
_ZC = np.exp(-1j * np.pi * 5 * _K * (_K + 1) / 127).astype(np.complex64)


def test_a_complex64_preamble_is_searched_by_its_samples():
    """One chip is one sample, and it is found at the delay it was put."""
    z = BurstAcquisition(_ZC, reps=8)
    assert z.code_bins == 127
    hits = z.push(np.tile(np.roll(_ZC, 40), 10))
    assert hits and hits[0][:2] == (0, 40)


def test_fs_defaults_to_normalized_units():
    """No fs is fs = 1: Doppler in cycles/sample, span +/- 1/(2n)."""
    z = BurstAcquisition(_ZC, reps=8)
    assert z.fs == 1.0
    assert z.doppler_span_hz == pytest.approx(1.0 / (2 * 127))
    hz = BurstAcquisition(_ZC, reps=8, fs=2.0e6)
    assert hz.fs == 2.0e6
    assert hz.doppler_span_hz == pytest.approx(2.0e6 / (2 * 127))


def test_a_code_is_its_samples():
    """A PN code is a preamble like any other: its samples, at
    chip_rate * spc, find the burst at the delay it was put."""
    b = BurstAcquisition(
        code_preamble(CODE, SPC), reps=8, fs=CHIP_RATE * SPC, cn0_dbhz=60.0
    )
    burst = np.tile(np.roll(code_preamble(CODE, SPC), 17), 10)
    hits = b.push(burst)
    assert hits and hits[0][:2] == (0, 17)


def test_a_complex128_preamble_is_refused():
    """complex64 only: numpy's default complex128 is refused loudly by the
    safe cast rather than silently narrowed."""
    with pytest.raises(TypeError):
        BurstAcquisition(_ZC.astype(np.complex128), reps=8)


def test_a_preamble_engine_resumes_from_its_state():
    """The blob is the engine's either way: split mid-stream, resume."""
    x = np.tile(np.roll(_ZC, 11), 12).astype(np.complex64)
    whole = BurstAcquisition(_ZC, reps=8).push(x)
    a = BurstAcquisition(_ZC, reps=8)
    first = a.push(x[:500])
    b = BurstAcquisition(_ZC, reps=8)
    b.set_state(a.get_state())
    assert first + b.push(x[500:]) == whole


def test_an_underpowered_preamble_warns():
    """A preamble that cannot meet pd at its design C/N0 says so."""
    with pytest.warns(UserWarning, match="under-powered"):
        z = BurstAcquisition(_ZC, reps=2, fs=1.0e6, cn0_dbhz=30.0)
    assert z.underpowered


@pytest.mark.parametrize(
    "make",
    [
        lambda: BurstAcquisition(_ZC, reps=8, pfa=1.0),
        lambda: BurstAcquisition(_ZC, reps=0),
        lambda: BurstAcquisition(np.zeros(127, np.complex64), reps=8),
    ],
    ids=["pfa-of-one", "zero-reps", "silent-preamble"],
)
def test_an_argument_error_is_a_value_error(make):
    """Every NULL from the constructor is a refused argument, never an
    allocation failure, so it raises ValueError (#1486)."""
    with pytest.raises(ValueError, match="BurstAcquisition: invalid"):
        make()


# ── a Doppler rate caps the coherent depth (doppler#1482) ─────────────────


def test_doppler_rate_defaults_to_no_bound():
    """Omitted, the rate is 0 and the whole preamble is integrated."""
    b = BurstAcquisition(_ZC, reps=8)
    assert b.doppler_rate == 0.0 and b.doppler_bins == 8


@pytest.mark.parametrize("fs", [1.0, 1.0e6])
def test_doppler_rate_caps_the_depth(fs):
    """At most floor(f_epoch / sqrt(2 * rate)) repetitions, f_epoch = fs/n,
    so the carrier drifts less than half a slow-time row per block. Stated
    as a fraction of f_epoch^2 so it reads the same in any units."""
    f_epoch = fs / _ZC.size
    rate = f_epoch**2 / (2 * 3.5**2)  # a ceiling of 3
    b = BurstAcquisition(_ZC, reps=8, fs=fs, doppler_rate=rate)
    assert b.doppler_rate == rate and b.doppler_bins == 3


def test_a_code_reads_the_same_rule():
    """A code's f_epoch is chip_rate / sf."""
    f_epoch = CHIP_RATE / CODE.size
    rate = f_epoch**2 / (2 * 2.5**2)  # a ceiling of 2
    b = BurstAcquisition(
        code_preamble(CODE, SPC), reps=8, fs=CHIP_RATE * SPC, doppler_rate=rate
    )
    assert b.doppler_bins == 2


@pytest.mark.parametrize("rate", [-1.0, np.nan, np.inf])
def test_a_bad_doppler_rate_is_a_value_error(rate):
    with pytest.raises(ValueError, match="doppler_rate >= 0"):
        BurstAcquisition(_ZC, reps=8, doppler_rate=rate)
