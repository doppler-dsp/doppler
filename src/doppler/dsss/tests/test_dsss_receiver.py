"""DsssReceiver: the single-object form of Acquisition -> Dll(segments) ->
RateConverter -> MpskReceiver. Uses this repo's own validated CCSDS
Gold-code/SEED=6/CN0=97dB-Hz signal (`async_dsss_receiver_demo.py`) as the
known-good fixture -- if this test's DsssReceiver-based decode disagrees
with that hand-composed reference, something in the composed object's
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
            reps=16,
            max_noncoh=8,
            segments=4,
            sps=8,
        )


def test_create_defaults():
    rx = _new_receiver()
    assert rx.tracking == 0
    assert rx.segments == 4
    assert rx.sps == 8
    assert rx.n == 4
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
    """Streaming the story's own validated signal through one DsssReceiver
    reproduces async_dsss_receiver_demo.py's hand-composed result: it
    locks and decodes cleanly."""
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
