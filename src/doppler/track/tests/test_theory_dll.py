"""Theoretical-correctness tests for the DLL code loop.

Validates the non-coherent early-minus-late code discriminator
(|E|-|L|)/(|E|+|L|):
  * the S-curve matches the triangular-autocorrelation E-L reference, is
    zero at the lock with a restoring slope, and its sub-chip quantization
    asymmetry halves with each sps doubling (vanishing in the limit);
  * the code-error variance follows a 1/SNR law.
"""

import numpy as np

from doppler.track import Dll
from doppler.wfm import PN, mls_poly

N = 7
L = (1 << N) - 1


def _code():
    c = np.asarray(PN(poly=mls_poly(N), seed=1, length=N).generate(L))
    return c.astype(np.uint8), np.where(c.astype(int) & 1, -1.0, 1.0)


def _scurve(sps, taus, spacing=0.5):
    code, csign = _code()
    sig = np.tile(np.repeat(csign, sps).astype(np.complex64), 6)
    out = np.empty(len(taus))
    for i, t in enumerate(taus):
        d = Dll(code, sps, float(t), 1e-7, 0.707, spacing)
        d.steps(sig)
        out[i] = d.last_error
    return out


def _tri_ref(taus, s=0.5):
    def acorr(t):
        return np.maximum(0, 1 - np.abs(t))

    early, late = acorr(taus + s), acorr(taus - s)
    return (early - late) / (early + late + 1e-12)


def test_scurve_zero_at_lock_with_restoring_slope():
    taus = np.linspace(-1, 1, 81)
    m = _scurve(16, taus)
    i0 = len(taus) // 2
    assert abs(m[i0]) < 1e-6  # exact zero at the lock
    # restoring: negative slope through 0 (e>0 for tau<0, e<0 for tau>0)
    assert m[i0 - 4] > 0 and m[i0 + 4] < 0


def test_scurve_matches_triangular_reference():
    taus = np.linspace(-1, 1, 81)
    m = _scurve(16, taus)
    ref = _tri_ref(taus)
    assert np.corrcoef(m, ref)[0, 1] > 0.99


def test_subchip_asymmetry_halves_with_sps():
    taus = np.linspace(-1, 1, 41)
    asym = [
        np.max(np.abs(_scurve(s, taus) + _scurve(s, taus)[::-1]))
        for s in (8, 16, 32)
    ]
    # each sps doubling roughly halves the antisymmetry error
    assert asym[1] < 0.6 * asym[0]
    assert asym[2] < 0.6 * asym[1]


def _disc_var(snr_db, sps=16, nper=400):
    code, csign = _code()
    base = np.repeat(csign, sps).astype(np.complex64)
    rng = np.random.default_rng(7)
    x = np.tile(base, nper).copy()
    p = np.sqrt(np.mean(np.abs(x) ** 2))
    std = np.sqrt(10 ** (-snr_db / 10)) * p
    x = x + (
        rng.normal(0, std / np.sqrt(2), x.size)
        + 1j * rng.normal(0, std / np.sqrt(2), x.size)
    ).astype(np.complex64)
    d = Dll(code, sps, 0.0, 1e-7, 0.707, 0.5)
    e = []
    for i in range(0, len(x), L * sps):
        d.steps(x[i : i + L * sps])
        e.append(d.last_error)
    return np.var(e[len(e) // 2 :])


def test_code_error_variance_follows_one_over_snr():
    snr_db = np.array([20, 15, 10, 5, 0])
    var = np.array([_disc_var(s) for s in snr_db])
    snr = 10 ** (snr_db / 10)
    k = var * snr  # should be ~constant (var = k/SNR)
    assert np.max(k) / np.min(k) < 1.15
