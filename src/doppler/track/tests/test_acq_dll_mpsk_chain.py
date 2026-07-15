"""Integration test: a real Acquisition hit seeding a real Dll seeding a
real MpskReceiver -- the full receive chain, not just the Acquisition->Dll
leg ``test_acq_dll_handoff.py`` already covers.

``test_async_dsss_receiver.py`` (despite its name) composes a genie-seeded
``Dll`` with ``Costas``/``SymbolSync``, a different architecture -- it does
not exercise ``MpskReceiver`` at all. This is the first test that drives
``Acquisition -> Dll(segments) -> MpskReceiver`` end to end with a real
acquisition search in front. See
``src/doppler/examples/async_dsss_receiver_demo.py`` (the gallery
counterpart, more epochs, plotted, plus the ``segments=4`` vs.
``segments=34`` comparison) for the full story this pins down as a fast,
always-run regression check.
"""

import warnings

import numpy as np
import pytest

from doppler.dsss import Acquisition
from doppler.dsss.handoff import dll_init_chip_from_acq
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
K_WORKING = 34


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


@pytest.mark.parametrize("segments", [34, 4])
def test_full_chain_decode(segments):
    """A real Acquisition hit, converted via dll_init_chip_from_acq into a
    Dll seed, despread and demodulated through MpskReceiver: ``segments=34``
    (sized for MpskReceiver's coherent matched filter) decodes cleanly;
    ``segments=4`` (Dll's own tracking sweet spot, too weak per partial for
    a downstream matched filter) does not -- the same finding the gallery
    example demonstrates with more epochs and a plotted convergence trace.
    """
    x, data = _make_signal(n_sym=1500, seed=6)
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

    partial_rate = FS * segments / TE
    norm_freq = doppler_hz_est / partial_rate
    sps = round(segments * TSYM / TE)
    n_arm = next(c for c in (4, 2, 1) if sps % c == 0)

    dll = Dll(CODE, SPC, chip_phase, 0.002, 0.707, 0.5, segments=segments)
    rest = x[s0:]
    nep = len(rest) // TE
    parts = [dll.steps(rest[e * TE : (e + 1) * TE]) for e in range(nep)]
    part = np.concatenate(parts)

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
    syms = rx.steps(part)
    ber = _decode_ber(syms, data, data_start)

    if segments == K_WORKING:
        assert ber < 0.01, (
            f"expected segments={segments} to decode cleanly (ber={ber:.4f})"
        )
    else:
        assert ber > 0.3, (
            f"expected segments={segments} (Dll's own sweet spot) to fail "
            f"to decode downstream of MpskReceiver (ber={ber:.4f})"
        )
