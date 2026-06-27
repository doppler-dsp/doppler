"""Integration tests for doppler.track.PartialDespreader.

The partial-correlation despreader splits each code epoch into k sub-epoch
partial prompts (so an asynchronous data-symbol clock is observable) and tracks
the code non-coherently. See docs/design/async-symbol-despreader.md.
"""

import numpy as np
import pytest

from doppler.track import PartialDespreader
from doppler.wfm import PN, mls_poly

SPS, SF, K = 8, 127, 4
TE = SF * SPS


def _code():
    c = np.asarray(PN(poly=mls_poly(7), seed=1, length=7).generate(SF))
    return c.astype(np.uint8), np.where(c.astype(int) & 1, -1.0, 1.0)


def _signal(csign, nsym, dsym, phi, dcode=0.0, snr_db=None, seed=0):
    """Carrier-free DSSS: code at rate (1+dcode), data on an independent clock
    Tsym=TE*(1+dsym), phase phi."""
    rng = np.random.default_rng(seed)
    tsym = TE * (1.0 + dsym)
    n = int(nsym * tsym) + 2 * TE
    data = (rng.integers(0, 2, nsym + 6) * 2 - 1).astype(float)
    idx = np.arange(n)
    si = np.clip(np.floor((idx - phi) / tsym).astype(int), 0, len(data) - 1)
    cph = (idx * (1.0 + dcode) / SPS).astype(int) % SF  # code chip per sample
    rx = data[si] * csign[cph]
    if snr_db is not None:
        p = np.sqrt(np.mean(rx**2))
        rx = rx + rng.normal(0, np.sqrt(10 ** (-snr_db / 10)) * p, n)
    return rx.astype(np.complex64), data, tsym


def _genie_ber(part, data, tsym, phi, nsym, dcode=0.0):
    """Symbol despread on the partials with known timing (no code Doppler)."""
    acc = np.zeros(nsym + 8)
    for pp in range(len(part)):
        t = TE * (pp + 0.5) / K
        s = int(np.floor((t - phi) / tsym))
        if 0 <= s < nsym:
            acc[s] += part[pp].real
    dec = np.where(acc[2 : nsym - 2] >= 0, 1, -1)
    e = np.mean(dec != data[2 : nsym - 2])
    return min(e, 1 - e)


def test_create_and_properties():
    code, _ = _code()
    p = PartialDespreader(code, SPS, K, 0.0, 0.002, 0.707, 0.5)
    assert p.k == K
    assert p.code_rate == pytest.approx(1.0)
    assert p.code_phase == pytest.approx(0.0)


def test_context_manager_and_destroy():
    code, _ = _code()
    with PartialDespreader(code, SPS, K, 0.0, 0.002, 0.707, 0.5):
        pass
    PartialDespreader(code, SPS, K, 0.0, 0.002, 0.707, 0.5).destroy()


def test_emits_k_partials_per_epoch():
    code, csign = _code()
    rx, _, _ = _signal(csign, 500, 0.0, 0.0)
    p = PartialDespreader(code, SPS, K, 0.0, 0.002, 0.707, 0.5)
    part = p.steps(rx)
    assert part.dtype == np.complex64
    nep = len(rx) // TE
    assert (nep - 1) * K <= len(part) <= (nep + 1) * K


def test_partials_carry_async_data():
    # the whole point: per-epoch coherent despread floors, but the k partials
    # let a known-timing symbol despread recover the async data error-free
    code, csign = _code()
    phi = 0.37 * TE
    rx, data, tsym = _signal(csign, 3000, 3e-3, phi)
    p = PartialDespreader(code, SPS, K, 0.0, 0.002, 0.707, 0.5)
    part = p.steps(rx)
    assert _genie_ber(part, data, tsym, phi, 3000) == 0.0


def test_noncoherent_code_tracking_under_doppler_and_async():
    code, csign = _code()
    dcode = 1e-4
    rx, _, _ = _signal(csign, 3000, 3e-3, 0.37 * TE, dcode=dcode, snr_db=6)
    p = PartialDespreader(code, SPS, K, 0.0, 0.002, 0.707, 0.5)
    part = p.steps(rx)
    assert p.code_rate == pytest.approx(1.0 + dcode, abs=5e-4)
    assert abs(p.last_error) < 0.3
    h = len(part) // 2
    assert np.mean(np.abs(part[h:].real)) > 0.3  # partials still despread
