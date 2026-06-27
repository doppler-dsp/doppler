"""Theoretical-correctness tests for the DLL code loop.

Validates the non-coherent early-minus-late code discriminator
(|E|-|L|)/(|E|+|L|):
  * the S-curve matches the triangular-autocorrelation E-L reference, is
    zero at the lock with a restoring slope, and — thanks to the
    fractional-boundary integrate-and-dump — is smooth and antisymmetric
    across sub-chip offsets at every sps (no integer-sample staircase);
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


def test_subchip_scurve_is_smooth_and_antisymmetric():
    # The fractional-boundary integrate-and-dump weights the lone sample that
    # straddles a chip transition by its overlap, so the correlation varies
    # continuously with sub-sample code phase. The old integer-sample
    # staircase (whose antisymmetry error shrank only as ~1/sps) is gone: at
    # every sps the S-curve is antisymmetric to round-off and free of jumps.
    taus = np.linspace(-1, 1, 41)
    for s in (8, 16, 32):
        m = _scurve(s, taus)
        assert (
            np.max(np.abs(m + m[::-1])) < 1e-3
        )  # antisymmetric, no staircase
        assert (
            np.max(np.abs(np.diff(m))) < 0.15
        )  # smooth, no sample-quantum jump


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
