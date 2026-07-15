"""Integration test: a real Acquisition hit seeding a real Dll seeding a
real MpskReceiver -- the full receive chain, not just the Acquisition->Dll
leg ``test_acq_dll_handoff.py`` already covers.

``test_async_dsss_receiver.py`` (despite its name) composes a genie-seeded
``Dll`` with ``Costas``/``SymbolSync``, a different architecture -- it does
not exercise ``MpskReceiver`` at all. This is the first test that drives
``Acquisition -> Dll(segments) -> RateConverter -> MpskReceiver`` end to
end with a real acquisition search in front. See
``src/doppler/examples/async_dsss_receiver_demo.py`` (the gallery
counterpart, more epochs, plotted, plus the with/without-resample
comparison) for the full story this pins down as a fast, always-run
regression check.

The despreader's job is just to remove the code; ``Dll(segments=K)``'s
partial-correlation stream is a sub-multiple of the chip rate, nothing
more. ``RateConverter`` is what turns that into a clean samples/symbol
grid for a "normal" ``MpskReceiver``. Both ``test_resampled_chain_decodes``
and ``test_unresampled_chain_fails`` use the *same* ``Dll(segments=4)``
config -- the only difference is whether ``RateConverter`` sits in
between -- to isolate that the resample stage is what matters, not the
choice of ``segments``.
"""

import warnings

import numpy as np

from doppler.dsss import Acquisition
from doppler.dsss.handoff import dll_init_chip_from_acq
from doppler.resample import RateConverter
from doppler.track import Dll, MpskReceiver
from doppler.wfm import Gold

SF = 1023  # CCSDS 415.0-G-1 command-link Gold code period
CHIP_RATE = 3.0e6  # Hz
SYM_RATE = 2100.0  # Hz -- chips/symbol = 1428.6, non-integer (asynchronous)
SPC = 2  # samples/chip
FS = CHIP_RATE * SPC
TE = SF * SPC  # samples per code epoch
TSYM = FS / SYM_RATE  # samples per symbol
DOPPLER_HZ = 50.0
PRE_SILENCE = TE * 5 + 311  # not a whole number of epochs

CODE = np.asarray(Gold().generate(SF)).astype(np.uint8)
_CSIGN = np.where(CODE & 1, -1.0, 1.0)

CN0_DBHZ = 97.0  # matches the gallery example's operating point
K = 4  # Dll's own tracking-optimal segments (Stage 2)
MPSK_SPS = 8  # MpskReceiver's own constructor default
MPSK_N = 4


def _make_signal(n_sym, seed):
    rng = np.random.default_rng(seed)
    n = int(n_sym * TSYM) + 2 * TE
    idx = np.arange(n)
    data = (rng.integers(0, 2, n_sym + 4) * 2 - 1).astype(float)
    si = np.clip(np.floor(idx / TSYM).astype(int), 0, len(data) - 1)
    cph = (idx / SPC).astype(int) % SF
    sig = data[si] * _CSIGN[cph] * np.exp(2j * np.pi * (DOPPLER_HZ / FS) * idx)
    amp_snr = np.sqrt(10.0 ** (CN0_DBHZ / 10.0) / FS)
    sigma = 1.0 / amp_snr
    total_n = int(PRE_SILENCE) + n
    noise = (sigma / np.sqrt(2.0)) * (
        rng.standard_normal(total_n) + 1j * rng.standard_normal(total_n)
    )
    x = np.concatenate([np.zeros(int(PRE_SILENCE)), sig]).astype(
        np.complex64
    ) + noise.astype(np.complex64)
    return x, data


def _acquire(x):
    with warnings.catch_warnings():
        warnings.simplefilter("ignore", UserWarning)
        acq = Acquisition(
            CODE,
            spc=SPC,
            chip_rate=CHIP_RATE,
            cn0_dbhz=55.0,
            doppler_uncertainty=100.0,
            reps=16,
            max_noncoh=8,
            symbol_rate=SYM_RATE,
            pfa=1e-3,
            pd=0.9,
        )
    frame = acq.code_bins * acq.doppler_bins
    pos = 0
    while pos + frame <= len(x):
        hits = acq.push(x[pos : pos + frame])
        if hits:
            return hits[0], pos, acq
        pos += frame
    return None, None, acq


def _decode_ber(syms, data, data_start):
    bits = np.where(syms.real > 0, 1.0, -1.0)
    lo, hi = len(bits) // 3, 2 * len(bits) // 3
    best_ber = 1.0
    for lag in range(-100, 101):
        ti = data_start + lag + np.arange(lo, hi)
        if ti.min() < 0 or ti.max() >= len(data):
            continue
        truth = data[ti]
        best_ber = min(
            best_ber,
            float(np.mean(bits[lo:hi] != truth)),
            float(np.mean(bits[lo:hi] != -truth)),
        )
    return best_ber


def _chain(x, s0, chip_phase, doppler_hz_est, resample: bool):
    dll = Dll(CODE, SPC, chip_phase, 0.002, 0.707, 0.5, segments=K)
    rest = x[s0:]
    nep = len(rest) // TE
    parts = [dll.steps(rest[e * TE : (e + 1) * TE]) for e in range(nep)]
    part = np.concatenate(parts)

    partial_rate = FS * K / TE
    if resample:
        target_rate = MPSK_SPS * SYM_RATE
        rc = RateConverter(rate=target_rate / partial_rate)
        stream = rc.execute(part)
        norm_freq = doppler_hz_est / target_rate
        sps, n_arm = MPSK_SPS, MPSK_N
    else:
        stream = part
        norm_freq = doppler_hz_est / partial_rate
        sps = round(K * TSYM / TE)
        n_arm = next(c for c in (4, 2, 1) if sps % c == 0)

    rx = MpskReceiver(
        m=2,
        sps=sps,
        n=n_arm,
        pulse="iandd",
        bn_carrier=0.01,
        bn_timing=0.01,
        acq_to_track=1,
        lock_thresh=0.3,
        init_norm_freq=norm_freq,
        warmup_syms=30,
    )
    syms = rx.steps(stream)
    return rx, syms


def _handoff(x):
    hit, hitpos, acq = _acquire(x)
    assert hit is not None, "acquisition failed to find the continuous code"
    dop_bin, code_phase, _pk, _n, _ts, _c = hit
    doppler_bins = acq.doppler_bins
    k_fold = (dop_bin + doppler_bins // 2) % doppler_bins - doppler_bins // 2
    doppler_hz_est = k_fold * acq.doppler_res_hz
    chip_phase = dll_init_chip_from_acq(code_phase, SPC, SF)
    frame = acq.code_bins * acq.doppler_bins
    s0 = hitpos + frame
    data_start = round((s0 - PRE_SILENCE) / TSYM)
    return s0, chip_phase, doppler_hz_est, data_start


def test_resampled_chain_decodes():
    """Dll(segments=4) -> RateConverter -> MpskReceiver(sps=8, n=4) --
    the correct architecture -- decodes cleanly."""
    x, data = _make_signal(n_sym=1500, seed=6)
    s0, chip_phase, doppler_hz_est, data_start = _handoff(x)
    rx, syms = _chain(x, s0, chip_phase, doppler_hz_est, resample=True)
    ber = _decode_ber(syms, data, data_start)
    assert ber < 0.01, f"expected the resampled chain to decode (ber={ber})"
    assert rx.tracking == 1, "MpskReceiver never handed over to tracking"


def test_unresampled_chain_fails():
    """The same Dll(segments=4) fed directly into MpskReceiver, with no
    resample stage (segments forced to double as sps) -- the earlier bug.
    Kept as a regression guard: proves the fix above is the resample
    stage, not the choice of segments."""
    x, data = _make_signal(n_sym=1500, seed=6)
    s0, chip_phase, doppler_hz_est, data_start = _handoff(x)
    _rx, syms = _chain(x, s0, chip_phase, doppler_hz_est, resample=False)
    ber = _decode_ber(syms, data, data_start)
    assert ber > 0.3, (
        f"expected the un-resampled direct-wiring to fail to decode "
        f"(near-chance BER) at segments={K}; got ber={ber:.4f}"
    )
