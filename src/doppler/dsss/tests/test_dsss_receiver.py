"""DsssReceiver: the single-object form of Acquisition -> Dll(segments) ->
RateConverter -> MpskReceiver. Uses this repo's own validated CCSDS
Gold-code/SEED=6/CN0=97dB-Hz signal as the
known-good fixture -- if this test's DsssReceiver-based decode disagrees
with that validated operating point, something in the composed object's
wiring is wrong, not the underlying DSP (already covered by Acquisition/
Dll/MpskReceiver's own tests). Like `Acquisition` (a frame/push object,
not a simple block-`execute` shape), the state-serialization round trip is
bespoke here rather than in the generic `test_state_serialization.py`
matrix.
"""

import warnings

import numpy as np
import pytest

from doppler.dsss import DsssReceiver
from doppler.wfm import Gold

SF = 1023
CHIP_RATE = 3.0e6
SYM_RATE = 2100.0
SPC = 2
FS = CHIP_RATE * SPC
TE = SF * SPC
TSYM = FS / SYM_RATE
DOPPLER_HZ = 50.0
PRE_SILENCE = TE * 20 + 737
CN0_DBHZ = 97.0
SEED = 6
N_SYM = 3500

CODE = np.asarray(Gold().generate(SF)).astype(np.uint8)
_CSIGN = np.where(CODE & 1, -1.0, 1.0)


def _make_signal(cn0_dbhz, seed):
    rng = np.random.default_rng(seed)
    n = int(N_SYM * TSYM) + 2 * TE
    idx = np.arange(n)
    data = (rng.integers(0, 2, N_SYM + 4) * 2 - 1).astype(float)
    si = np.clip(np.floor(idx / TSYM).astype(int), 0, len(data) - 1)
    cph = (idx / SPC).astype(int) % SF
    sig = data[si] * _CSIGN[cph] * np.exp(2j * np.pi * (DOPPLER_HZ / FS) * idx)
    amp_snr = np.sqrt(10.0 ** (cn0_dbhz / 10.0) / FS)
    sigma = 1.0 / amp_snr
    total_n = int(PRE_SILENCE) + n
    noise = (sigma / np.sqrt(2.0)) * (
        rng.standard_normal(total_n) + 1j * rng.standard_normal(total_n)
    )
    x = np.concatenate([np.zeros(int(PRE_SILENCE)), sig]).astype(
        np.complex64
    ) + noise.astype(np.complex64)
    return x, data


def _new_receiver():
    with warnings.catch_warnings():
        warnings.simplefilter("ignore", UserWarning)
        return DsssReceiver(
            CODE,
            chip_rate=CHIP_RATE,
            symbol_rate=SYM_RATE,
            spc=SPC,
            cn0_dbhz=55.0,
            doppler_uncertainty=100.0,
            segments=4,
            sps=8,
        )


def test_create_defaults():
    rx = _new_receiver()
    assert rx.tracking == 0
    assert rx.segments == 4
    assert rx.sps == 8
    # `n` lands in MpskReceiver's `m_out` slot, and the cascade rebuild
    # changed what that means: terminal outputs per symbol now, not the
    # retired NDA arm's dumps per symbol. So it is derived as the
    # coherent-bound default (8), not the old "largest divisor of sps in
    # {4,2,1}" -- which gave 4 here and did not decode at all.
    assert rx.n == 8
    assert rx.chip_phase == 0.0


def test_context_manager():
    with _new_receiver() as rx:
        assert rx.tracking == 0


def test_only_signal_params_required():
    """code/chip_rate/symbol_rate are the only required constructor args --
    everything else defaults, matching this object's own "just works"
    design (spc defaults to 2x chip_rate, segments/sps to their own
    validated defaults)."""
    rx = DsssReceiver(CODE, chip_rate=CHIP_RATE, symbol_rate=SYM_RATE)
    assert rx.tracking == 0
    assert rx.segments == 4
    assert rx.sps == 8


def test_acquires_and_decodes():
    """Streaming the story's own validated signal through one
    DsssReceiver locks and decodes cleanly at the operating point the
    four-object chain was validated at."""
    x, data = _make_signal(CN0_DBHZ, SEED)
    rx = _new_receiver()

    syms = []
    chunk = TE
    for pos in range(0, len(x) - chunk, chunk):
        out = rx.steps(x[pos : pos + chunk])
        if len(out):
            syms.append(out)
    syms = np.concatenate(syms) if syms else np.zeros(0, dtype=np.complex64)

    assert rx.tracking == 1
    assert len(syms) > 100
    assert rx.cn0_dbhz_est > 0.0

    bits = np.where(syms.real > 0, 1.0, -1.0)
    lo, hi = len(bits) // 2, len(bits)
    best_ber = 1.0
    for lag in range(-100, 101):
        ti = lag + np.arange(lo, hi)
        if ti.min() < 0 or ti.max() >= len(data):
            continue
        truth = data[ti]
        best_ber = min(
            best_ber,
            float(np.mean(bits[lo:hi] != truth)),
            float(np.mean(bits[lo:hi] != -truth)),
        )
    assert best_ber < 0.01, f"expected a clean decode, got ber={best_ber}"


def test_reset_returns_to_searching():
    x, _data = _make_signal(CN0_DBHZ, SEED)
    rx = _new_receiver()
    chunk = TE
    for pos in range(0, len(x) - chunk, chunk):
        rx.steps(x[pos : pos + chunk])
        if rx.tracking:
            break
    assert rx.tracking == 1

    rx.reset()
    assert rx.tracking == 0
    assert rx.chip_phase == 0.0


def test_state_roundtrip_while_tracking():
    """Bespoke round trip (this is a frame/push composition, not a simple
    block-execute object -- same precedent as Acquisition's own bespoke
    test rather than the generic test_state_serialization.py matrix)."""
    x, _data = _make_signal(CN0_DBHZ, SEED)
    rx = _new_receiver()
    chunk = TE
    for pos in range(0, len(x) - chunk, chunk):
        rx.steps(x[pos : pos + chunk])
        if rx.tracking:
            break
    assert rx.tracking == 1

    blob = rx.get_state()
    rx2 = _new_receiver()
    rx2.set_state(blob)
    assert rx2.tracking == 1
    assert rx2.chip_phase == pytest.approx(rx.chip_phase)
    assert rx2.segments == rx.segments
    assert rx2.sps == rx.sps

    with pytest.raises(ValueError):
        rx2.set_state(b"\x00" * len(blob))

    with pytest.raises(TypeError):
        rx2.set_state("not bytes")


def test_state_roundtrip_while_searching():
    rx = _new_receiver()
    blob = rx.get_state()
    rx2 = _new_receiver()
    rx2.set_state(blob)
    assert rx2.tracking == 0


@pytest.mark.parametrize("sps", [0, 1])
def test_sps_below_two_is_refused_not_aborted(sps):
    """An unbuildable ``sps`` raises, and does not take the process with it.

    ``sps = 1`` used to reach ``dp_mpsk_receiver_create()``, whose
    argument-error NULL went through the abort-on-OOM helper ``dp_xnn()``
    and **SIGABRTed the interpreter** — exit 134, no exception, no
    traceback, nothing on stderr (gh-782). A library that aborts its host
    on a legal-looking argument leaves the caller no way to recover, so
    the regression worth pinning is not the message but the fact that
    control comes back at all.

    Two is the floor because it is the smallest legal ``m_out`` and
    ``MpskReceiver`` requires ``sps >= m_out``; below it there is no
    receiver to build, which is what the constructor now says.
    """
    with pytest.raises(ValueError, match="sps >= 2"):
        DsssReceiver(
            code=CODE, chip_rate=CHIP_RATE, symbol_rate=SYM_RATE, sps=sps
        )


def test_odd_sps_still_builds():
    """The gh-782 fix must not have widened into a parity rule.

    An earlier round of this same bug made every ODD ``sps`` abort, and it
    was fixed by flooring the derived ``m_out`` at 2. The guard added for
    ``sps = 1`` is a floor, not a parity test — so 5 must still build,
    and this is what says the two fixes did not collide.
    """
    rx = DsssReceiver(
        code=CODE, chip_rate=CHIP_RATE, symbol_rate=SYM_RATE, sps=5
    )
    assert rx is not None


# Each receiver at a configuration it builds. A case below changes ONE
# argument, so the refusal is that argument's; the base is built first, so a
# base the receiver refuses fails here rather than passing every case.
_RX_BASE = {
    "DsssReceiver": {"chip_rate": 1.023e6, "symbol_rate": 1e3},
    "AsyncDsssReceiver": {"chip_rate": 1.023e6, "symbol_rate": 1e3},
    "CellAsyncDsssReceiver": {"chip_rate": 1.023e6, "symbol_rate": 1e3},
    "AsyncDsssPool": {
        "chip_rate": 5e6,
        "symbol_rate": 2700.0,
        "cn0_dbhz": 47.0,
        "doppler_uncertainty": 6000.0,
        "n_slots": 2,
    },
}
_ALL = tuple(_RX_BASE)
_ASYNC = ("AsyncDsssReceiver", "CellAsyncDsssReceiver", "AsyncDsssPool")
_SEARCHING = ("DsssReceiver", "AsyncDsssReceiver", "AsyncDsssPool")
_NAN = float("nan")

# (classes, the one argument changed, what the message must name). Before
# doppler#2103 most of these took the interpreter down (SIGABRT), from a
# dp_xnn build under create or later in the stream; the PR body lists each.
_REFUSALS = [
    (_ALL, {"code": np.array([1], np.uint8)}, "a code of at least 2 chips"),
    (_ALL, {"chip_rate": _NAN}, "a finite chip_rate > 0"),
    (_ALL, {"symbol_rate": _NAN}, "a finite symbol_rate > 0"),
    (_ALL, {"sps": 1}, "sps >= 2"),
    (_SEARCHING, {"pfa": 2.0}, "0 < pfa < 1"),
    (_SEARCHING, {"pd": 1.5}, "0 < pd < 1"),
    (_SEARCHING, {"cn0_dbhz": _NAN}, "a finite cn0_dbhz"),
    (
        _SEARCHING,
        {"doppler_uncertainty": _NAN},
        "a finite doppler_uncertainty >= 0",
    ),
    (_ASYNC, {"carrier_freq_hz": 1e-305}, "above chip_rate \\* spc / 2"),
    (_ASYNC, {"carrier_freq_hz": 5e-324}, "above chip_rate \\* spc / 2"),
    (_ASYNC, {"symbol_rate": 1e-6}, "at most 2\\^20 Dll partials"),
    (
        ("AsyncDsssReceiver",),
        {"refine_n_fft": 0},
        "refine_n_fft >= 1",
    ),
    (
        ("AsyncDsssReceiver",),
        {"refine_zero_pad": 0},
        "refine_zero_pad >= 1",
    ),
    (
        ("AsyncDsssReceiver",),
        {"refine_samples_per_symbol": 0},
        "refine_samples_per_symbol >= 1",
    ),
    (
        ("CellAsyncDsssReceiver",),
        {"correct_periods": 0},
        "correct_periods >= 1",
    ),
    (
        ("CellAsyncDsssReceiver", "AsyncDsssPool"),
        {"gain": 0.0},
        "0 < gain <= 1",
    ),
]


@pytest.mark.parametrize(
    "cls, change, names",
    [
        pytest.param(cls, change, names, id=f"{cls}-{next(iter(change))}")
        for classes, change, names in _REFUSALS
        for cls in classes
    ],
)
def test_create_refuses_and_names_the_condition(cls, change, names):
    """A bad argument is a ValueError whose message names the condition it
    broke, where most used to abort the process (doppler#2103). The message
    is one string per class, so the condition it names proves only that the
    message lists it; that this argument is the one refused is proved by the
    same receiver building with it at a good value."""
    import doppler.dsss as dsss

    make = getattr(dsss, cls)
    kw = dict(_RX_BASE[cls], code=CODE)
    make(**kw)
    with pytest.raises(ValueError, match=names):
        make(**dict(kw, **change))


@pytest.mark.parametrize("cls", ["AsyncDsssReceiver", "CellAsyncDsssReceiver"])
def test_configure_chain_raw_refuses_a_grid_create_would(cls):
    """configure_chain_raw() rebuilds the tracker on a new grid. It now checks
    that grid as create() checks the first, before anything is rebuilt, so a
    grid the chain cannot take is a ValueError and the receiver keeps its own
    (doppler#2103). The cases are what reached an abort mid-rebuild: a symbol
    of more Dll partials than the aid takes, and sps = 1, which carries no
    m_out. A 1023-chip code at 1.023 Mchip/s and 1 kbaud has a symbol of one
    code period, so 2^20 + 1 segments is one partial past the cap."""
    import doppler.dsss as dsss

    rx = getattr(dsss, cls)(CODE, chip_rate=1.023e6, symbol_rate=1e3)
    rx.configure_chain_raw(4, 8, 4)
    held = rx.get_state()
    for grid in ((2**20 + 1, 8, 4), (4, 1, 1)):
        with pytest.raises(ValueError, match="configure_chain_raw failed"):
            rx.configure_chain_raw(*grid)
        assert rx.get_state() == held
