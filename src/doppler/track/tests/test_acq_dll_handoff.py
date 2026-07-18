"""Integration test: a real Acquisition hit seeding a real Dll.

Unlike ``test_dll.py`` (a genie-seeded ``Dll``, no ``Acquisition`` involved)
and ``test_async_dsss_receiver.py`` (a genie-seeded ``Dll`` feeding
``Costas``/``SymbolSync``), this drives the actual hand-off: a real
``Acquisition`` search over a continuous, asynchronous-data-modulated,
carrier-present signal, converted via ``dll_init_chip_from_acq`` into a
real ``Dll`` seed, then verifies that seed is exact and that tracking
holds up over an extended run. See
``src/doppler/examples/dsss_despread_async_data_demo.py`` (the gallery
counterpart, same construction, more epochs, plotted) for the full story
this pins down as a fast, always-run regression check.
"""

import warnings

import numpy as np
import pytest

from doppler.dsss import Acquisition
from doppler.dsss.handoff import dll_init_chip_from_acq
from doppler.track import Dll
from doppler.wfm import Gold

SF = 1023  # CCSDS 415.0-G-1 command-link Gold code period
CHIP_RATE = 3.0e6  # Hz
SYM_RATE = 2100.0  # Hz -- chips/symbol = 1428.6, non-integer (asynchronous)
SPC = 2  # samples/chip
FS = CHIP_RATE * SPC
TE = SF * SPC  # samples per code epoch
TSYM = FS / SYM_RATE  # samples per symbol
DOPPLER_HZ = 50.0  # residual carrier -- never removed; Dll is carrier-blind
PRE_SILENCE = TE * 5 + 311  # not a whole number of epochs

CODE = np.asarray(Gold().generate(SF)).astype(np.uint8)
_CSIGN = np.where(CODE & 1, -1.0, 1.0)


def _make_signal(cn0_dbhz, n_sym, seed):
    """Continuous, asynchronous-symbol-clock DSSS capture at real units --
    same construction as the gallery example's ``make_signal``."""
    rng = np.random.default_rng(seed)
    n = int(n_sym * TSYM) + 2 * TE
    idx = np.arange(n)
    data = (rng.integers(0, 2, n_sym + 4) * 2 - 1).astype(float)
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
    return x


def _acquire(x):
    with warnings.catch_warnings():
        warnings.simplefilter("ignore", UserWarning)
        acq = Acquisition(
            CODE,
            spc=SPC,
            chip_rate=CHIP_RATE,
            cn0_dbhz=55.0,
            doppler_uncertainty=100.0,
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


def test_acquisition_hit_seeds_dll_exactly():
    """dll_init_chip_from_acq's seed matches the true instantaneous code
    phase at hand-off exactly (well within the phase-inversion formula's
    intrinsic half-chip resolution -- a wrong sign inversion would miss by
    roughly half a code period instead)."""
    x = _make_signal(cn0_dbhz=75.0, n_sym=1500, seed=6)
    hit, hitpos, acq = _acquire(x)
    assert hit is not None, "acquisition failed to find the continuous code"
    _dop_bin, code_phase, _pk, _n, _ts, _c, *_rest = hit

    chip_phase = dll_init_chip_from_acq(code_phase, SPC, SF)
    s0 = hitpos + acq.code_bins * acq.doppler_bins
    ridx0 = s0 - int(PRE_SILENCE)
    true_phase0 = (ridx0 / SPC) % SF
    err = ((chip_phase - true_phase0 + SF / 2) % SF) - SF / 2
    assert abs(err) < 0.1, (
        f"hand-off phase disagrees with the true code phase by {err:.2f} "
        "chips -- should be exact"
    )


@pytest.mark.parametrize("segments", [1, 4])
def test_dll_tracks_after_handoff(segments):
    """A Dll seeded from a real Acquisition hit tracks the true code phase
    (small circular error, low code-rate bias) over an extended run --
    the hand-off produces a working despreader, not just a correctly
    -valued but otherwise untested seed."""
    x = _make_signal(cn0_dbhz=80.0, n_sym=900, seed=6)
    hit, hitpos, acq = _acquire(x)
    assert hit is not None
    _dop_bin, code_phase, _pk, _n, _ts, _c, *_rest = hit

    chip_phase = dll_init_chip_from_acq(code_phase, SPC, SF)
    s0 = hitpos + acq.code_bins * acq.doppler_bins

    dll = Dll(CODE, SPC, chip_phase, 0.002, 0.707, 0.5, segments=segments)
    dll.configure_lock(1e-3, 20)

    n_epochs = min(500, (len(x) - s0) // TE)
    assert n_epochs > 100, "signal too short for a meaningful tracking run"
    pos = s0
    errs = np.full(n_epochs, np.nan)
    for i in range(n_epochs):
        dll.steps(x[pos : pos + TE])
        pos += TE
        ridx = pos - int(PRE_SILENCE)
        true_phase = (ridx / SPC) % SF
        errs[i] = ((dll.code_phase - true_phase + SF / 2) % SF) - SF / 2

    back = errs[n_epochs // 2 :]
    rms = float(np.sqrt(np.nanmean(back**2)))
    assert rms < 2.0, (
        f"expected the despreader to track within a couple chips at this "
        f"strong margin (segments={segments}, rms={rms:.3f})"
    )
    assert dll.locked, (
        f"expected the lock detector to have latched by the end of a "
        f"{n_epochs}-epoch run at this margin (segments={segments})"
    )
