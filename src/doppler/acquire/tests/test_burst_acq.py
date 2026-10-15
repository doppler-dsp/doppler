"""``BurstAcquisition`` is a thin composing object: a genuinely separate jm
object (``objects/burst_acq.toml``) whose C core (``burst_acq_core.c``) is a
pure forwarder onto the SAME shared ``dp_acq_state_t``/``acq_core.c`` engine
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

from doppler.acquire import Acquisition, BurstAcquisition
from doppler.acquire.tests._preamble import code_preamble
from doppler.wfm import PN, mls_poly, rotate

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


def _burst_acq(spc=SPC, **kw):
    kw.setdefault("reps", 8)
    kw.setdefault("fs", CHIP_RATE * spc)
    kw.setdefault("cn0_dbhz", CN0_DBHZ)
    kw.setdefault("doppler_uncertainty", 0.0)
    kw.setdefault("pfa", PFA)
    kw.setdefault("pd", PD)
    return BurstAcquisition(code_preamble(CODE, spc), **kw)


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


@pytest.mark.parametrize("spc", [1, SPC])
def test_hit_chip_phase_matches_acquisition(spc):
    """A BurstAcquisition hit carries the same Dll seed as an Acquisition
    hit on the same capture: ``hit[7]``, ``chip_phase`` (doppler#1257).

    Both classes run ``dp_acq_push`` -> ``acq_report_peaks`` on one engine,
    so the C hit is the same; this pins that both Python faces return it.
    The units differ by construction: a burst engine is built from a
    sampled template, "one chip = one sample" (``dp_acq_create_burst``),
    so its ``chip_phase`` counts template samples over a period of
    ``code_bins``, while Acquisition counts code chips over ``SF``. At one
    sample per chip the two are equal; at ``spc`` the burst value is
    ``spc`` times the chip value. Neither class is coupled to a carrier
    here, so ``chip_phase`` is the inverted lag alone.
    """
    rng = np.random.default_rng(7)
    period = np.asarray(code_preamble(CODE, spc))
    lag = 5 * spc + 1  # off the chip grid when spc > 1
    sig = np.tile(np.roll(period, lag), 64)
    noise = 0.05 * (
        rng.standard_normal(len(sig)) + 1j * rng.standard_normal(len(sig))
    )
    x = (sig + noise).astype(np.complex64)

    burst = _burst_acq(spc=spc, cn0_dbhz=90.0).push(x)
    cont = _continuous_acq(spc=spc, cn0_dbhz=90.0).push(x)
    assert burst and cont, "both classes must detect the strong code"
    b, c = burst[0], cont[0]

    assert len(b) == len(c) == 9
    assert b[8] == c[8] == 0.0, "neither searches rate hypotheses here"
    assert b[1] == c[1], "the two front doors disagree on the lag"
    n = SF * spc
    assert b[7] == pytest.approx((n - b[1]) % n, abs=1e-9)
    assert b[7] == pytest.approx(spc * c[7], abs=1e-9)


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
    assert math.isnan(b.pd_burst)
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
    assert not np.isnan(b.pd_burst)


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


# ── doppler_bins is the grid a hit's bin indexes ───────────────────────────


def test_a_tiled_search_with_depth_reports_the_whole_grid():
    """Tiled (doppler_uncertainty past the native span) AND coherent inside
    each tile: a hit's bin indexes window_bins * depth rows, and that product
    is what `doppler_bins` must be -- it is the count the documented recipe
    folds over, `bin_to_signed(bin, doppler_bins) * doppler_res_hz`. It used
    to read the tile count alone, so BurstCapture (which reads the product)
    and BurstAcquisition disagreed, and the recipe gave the wrong Doppler."""
    from doppler.acquire import BurstCapture

    span = 1.0 / (2.0 * _ZC.size)
    kw = {
        "reps": 8,
        "fs": 1.0,
        "cn0_dbhz": -10.0,
        "doppler_uncertainty": 3.5 * span,
    }
    acq = BurstAcquisition(_ZC, **kw)
    cap = BurstCapture(_ZC, burst_len=2048, **kw)
    assert acq.doppler_bins == cap.doppler_bins
    # More than the tile count: a depth was chosen inside the tiles.
    tiles = 2 * int(np.ceil((3.5 * span - span) / (2 * span))) + 1
    assert acq.doppler_bins > tiles and acq.doppler_bins % tiles == 0


@pytest.mark.parametrize("native_spans", [1.2, -1.2, -1.9])
def test_the_documented_recipe_reads_an_off_centre_tile(native_spans):
    """`bin_to_signed(bin, doppler_bins) * doppler_res_hz` -- the recipe in
    the acquisition guides -- is the signed Doppler of a tiled search with a
    coherent depth, in a tile off the centre and in a negative one. A PN
    preamble: a Zadoff-Chu one cannot be told apart across tiles (its
    delay-Doppler ridge repeats at every tile). Whole dwells of input, as the
    engine frames them."""
    from doppler.acquire import bin_to_signed

    chips = PN(poly=mls_poly(7), seed=1, length=7).generate(127)
    code = np.asarray(chips).astype(np.uint8) & 1
    pre = code_preamble(code, 1)
    fs = 1.0e6
    native = fs / code.size  # one tile's width, Hz
    acq = BurstAcquisition(
        pre,
        reps=8,
        fs=fs,
        cn0_dbhz=45.0,
        doppler_uncertainty=1.75 * native,
    )
    res = acq.doppler_res_hz
    depth = round(native / res)
    assert depth > 1 and acq.doppler_bins == 5 * depth  # 5 tiles x depth
    f = round(native_spans * native / res) * res  # exactly on a bin
    k = np.arange(6 * depth * pre.size)
    x = (np.tile(pre, 6 * depth) * np.exp(2j * np.pi * f * k / fs)).astype(
        np.complex64
    )
    hits = acq.push(x)
    assert hits, "the tone is detected"
    got = bin_to_signed(hits[0][0], acq.doppler_bins) * res
    assert got == pytest.approx(f, abs=1e-6)


# ── a Doppler rate compensated, not just capped ────────────────────────────


def _sweeping_stream(pre, reps, f_mid, rate):
    """`reps` repetitions of `pre` whose carrier sweeps at `rate`, about the
    stream's middle (normalized: fs = 1, so rates are cycles/sample^2)."""
    n = pre.size
    return rotate(
        np.tile(pre, reps).astype(np.complex64),
        f_mid,
        rate,
        1.0,
        center=0.5 * reps * n,
    )


def test_rate_hypotheses_default_to_none():
    b = BurstAcquisition(_ZC, reps=8, doppler_rate=1e-6)
    assert b.rate_hypotheses == 1


def test_rate_hypotheses_buy_depth_over_the_cap():
    """The drift bound applies to the RESIDUAL, doppler_rate / (n - 1): the
    same rate that capped the depth at 5 allows 15 once nine rates are
    searched: the residual is 1/8 of the span, so the cap grows by
    sqrt(8)."""
    f_epoch = 1.0 / _ZC.size
    rate = f_epoch**2 / (2 * 5.5**2)  # a ceiling of 5
    assert BurstAcquisition(_ZC, reps=16, doppler_rate=rate).doppler_bins == 5
    b = BurstAcquisition(_ZC, reps=16, doppler_rate=rate, rate_hypotheses=9)
    assert b.rate_hypotheses == 9 and b.doppler_bins == 15


@pytest.mark.parametrize("n", [65, 1000])
def test_rate_hypotheses_out_of_range_is_a_value_error(n):
    with pytest.raises(ValueError, match="rate_hypotheses"):
        BurstAcquisition(_ZC, reps=8, doppler_rate=1e-6, rate_hypotheses=n)


def test_rate_hypotheses_need_a_rate_to_span():
    """doppler_rate is the SPAN of the hypotheses: with none given there is
    nothing to compensate."""
    with pytest.raises(ValueError, match="rate_hypotheses"):
        BurstAcquisition(_ZC, reps=8, rate_hypotheses=3)
    with pytest.raises(ValueError, match="rate_hypotheses"):
        BurstAcquisition(_ZC, reps=8, rate_hypotheses=0)  # auto, too
    assert BurstAcquisition(_ZC, reps=8, rate_hypotheses=1)  # one is fine


def test_rate_hypotheses_zero_is_auto_and_minimal():
    """0 picks the fewest hypotheses whose residual stops limiting the
    depth, readable back as ``rate_hypotheses``: the engine it builds is the
    explicit one of that size, and one fewer is shallower."""
    f_epoch = 1.0 / _ZC.size
    rate = f_epoch**2 / (2 * 5.5**2)  # a ceiling of 5 when capped
    auto = BurstAcquisition(_ZC, reps=16, doppler_rate=rate, rate_hypotheses=0)
    n = auto.rate_hypotheses
    assert 1 < n <= 64
    same = BurstAcquisition(_ZC, reps=16, doppler_rate=rate, rate_hypotheses=n)
    assert same.doppler_bins == auto.doppler_bins
    fewer = BurstAcquisition(
        _ZC, reps=16, doppler_rate=rate, rate_hypotheses=n - 1
    )
    assert fewer.doppler_bins < auto.doppler_bins


@pytest.mark.parametrize("sign", [-1, 1])
def test_a_hit_names_the_rate_hypothesis_that_won(sign):
    """The hit's last field is the winning rate; the truth is on a hypothesis
    here (hypotheses are -R, -R/2, 0, R/2, R), so it is found exactly."""
    span = 9e-7
    b = BurstAcquisition(_ZC, reps=16, doppler_rate=span, rate_hypotheses=5)
    x = _sweeping_stream(_ZC, 48, 5e-4 * sign, 0.5 * span * sign)
    hits = b.push(x)
    assert hits, "the swept burst is detected"
    assert hits[0][-1] == pytest.approx(0.5 * span * sign)


def test_a_hit_without_hypotheses_reports_no_rate():
    b = BurstAcquisition(_ZC, reps=8)
    hits = b.push(np.tile(_ZC, 16).astype(np.complex64))
    assert hits and hits[0][-1] == 0.0
