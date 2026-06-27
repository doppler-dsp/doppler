"""Integration tests for doppler.track.Dll (code-tracking loop)."""

import numpy as np
import pytest

from doppler.track import Dll

SF, SPS = 63, 4


def _code(seed=1):
    return np.random.default_rng(seed).integers(0, 2, SF).astype(np.uint8)


def _signal(code, delta, nper, seed=9, const_data=False):
    """Carrier-free spread signal at code rate (1+delta), BPSK data/period.

    With ``const_data`` the per-period BPSK symbol is held at +1, isolating
    code tracking from the data-symbol vs code-period timing (a separate
    symbol-sync concern): under a code-rate offset the code period slowly
    slides against the data-symbol clock, so a per-period data flip can split
    a prompt integration across two opposite symbols and cut the despread.
    """
    rng = np.random.default_rng(seed)
    n = SF * SPS * nper
    rx = np.empty(n, np.complex64)
    cph = 0.0
    for p in range(nper):
        data = 1 if const_data else (1 if rng.integers(0, 2) else -1)
        for i in range(SF * SPS):
            idx = int(cph % SF)
            rx[p * SF * SPS + i] = data * (-1.0 if code[idx] & 1 else 1.0)
            cph += (1 + delta) / SPS
    return rx


def test_create_and_guard():
    assert Dll(_code(), SPS, 0.0, 0.01, 0.707, 0.5) is not None


def test_context_manager_and_destroy():
    with Dll(_code(), SPS, 0.0, 0.01, 0.707, 0.5):
        pass
    d = Dll(_code(), SPS, 0.0, 0.01, 0.707, 0.5)
    d.destroy()


def test_properties():
    d = Dll(_code(), SPS, 0.0, bn=0.01, zeta=0.707, spacing=0.5)
    assert d.bn == pytest.approx(0.01)
    assert d.code_rate == pytest.approx(1.0)
    d.bn = 0.005
    assert d.bn == pytest.approx(0.005)


def test_one_prompt_per_period():
    code = _code()
    rx = _signal(code, 0.0, 100)
    d = Dll(code, SPS, 0.0, 0.005, 0.707, 0.5)
    sym = d.steps(rx)
    assert sym.dtype == np.complex64
    assert len(sym) == 100


def test_tracks_code_doppler():
    code = _code(11)
    delta = 5e-4
    # const_data isolates code-Doppler tracking from data-symbol/code-period
    # async; the replica speeds up to match the incoming chip rate and the
    # fractional-boundary discriminator holds sub-chip lock (clean despread).
    rx = _signal(code, delta, 1500, seed=9, const_data=True)
    d = Dll(code, SPS, 0.0, 0.005, 0.707, 0.5)
    sym = d.steps(rx)
    assert d.code_rate == pytest.approx(1.0 + delta, abs=1e-4)
    assert np.mean(np.abs(sym[len(sym) // 2 :].real)) > 0.9


def test_pulls_in_static_offset():
    code = _code(13)
    rx = _signal(code, 0.0, 800, seed=17)
    d = Dll(code, SPS, 0.4, 0.005, 0.707, 0.5)  # 0.4-chip offset
    d.steps(rx[: SF * SPS * 3])
    assert abs(d.last_error) > 0.05  # starts misaligned
    d.steps(rx[SF * SPS * 3 :])
    assert abs(d.last_error) < 0.05  # pulled in


def test_reset_reproducible():
    code = _code(21)
    rx = _signal(code, 3e-4, 300, seed=5)
    d = Dll(code, SPS, 0.0, 0.005, 0.707, 0.5)
    d.steps(rx)
    r1, e1 = d.code_rate, d.last_error
    d.reset()
    d.steps(rx)
    assert (r1, e1) == (d.code_rate, d.last_error)


# --- segments > 1: sub-epoch partial despreading for an async symbol clock ---
TE = SF * SPS


def _async_signal(code, nsym, dsym, phi, seed=0):
    """Carrier-free DSSS, code-aligned, data on an independent clock
    Tsym=TE*(1+dsym), phase phi (samples)."""
    rng = np.random.default_rng(seed)
    csign = np.where(code & 1, -1.0, 1.0)
    chip = np.repeat(csign, SPS)
    tsym = TE * (1.0 + dsym)
    n = int(nsym * tsym) + 2 * TE
    data = (rng.integers(0, 2, nsym + 6) * 2 - 1).astype(float)
    idx = np.arange(n)
    si = np.clip(np.floor((idx - phi) / tsym).astype(int), 0, len(data) - 1)
    return (data[si] * chip[idx % TE]).astype(np.complex64), data, tsym


def test_segments_default_is_one():
    assert Dll(_code(), SPS, 0.0, 0.005, 0.707, 0.5).segments == 1


def test_segments_emits_partials_per_epoch():
    code = _code(11)
    rx, _, _ = _async_signal(code, 400, 0.0, 0.0)
    d = Dll(code, SPS, 0.0, 0.002, 0.707, 0.5, segments=4)
    part = d.steps(rx)
    nep = len(rx) // TE
    assert d.segments == 4
    assert (nep - 1) * 4 <= len(part) <= (nep + 1) * 4


def test_segments_recover_async_data():
    # the partials carry an async symbol clock that a coherent full-epoch
    # despread would collapse; a known-timing symbol despread recovers it
    code = _code(11)
    phi = 0.37 * TE
    rx, data, tsym = _async_signal(code, 2000, 3e-3, phi)
    d = Dll(code, SPS, 0.0, 0.002, 0.707, 0.5, segments=4)
    part = d.steps(rx)
    acc = np.zeros(2000 + 8)
    for pp in range(len(part)):
        s = int(np.floor((TE * (pp + 0.5) / 4 - phi) / tsym))
        if 0 <= s < 2000:
            acc[s] += part[pp].real
    dec = np.where(acc[2:1998] >= 0, 1, -1)
    assert min(np.mean(dec != data[2:1998]), np.mean(dec == data[2:1998])) == 0
