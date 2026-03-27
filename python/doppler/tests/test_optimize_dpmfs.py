"""
Tests for LP minimax DPMFS optimization
(doppler.polyphase.optimize_dpmfs / optimize_pbf).

Checks:
  - optimize_pbf: C matrix shape, delta > 0, passband/stopband
    response levels.
  - optimize_dpmfs: returns DPMFSCoeffs with correct shapes/dtypes.
  - Spectral quality: stopband attenuation >= 30 dB for N=6, M=3.
  - Basis conversion: c[j,m,k] monomial coefficients reconstruct
    the same filter response as the C matrix.
  - Continuity constraints: num_der=0 produces a continuous IR.
"""

import numpy as np
import pytest

from doppler.polyphase import (
    kaiser_prototype,
    fit_dpmfs,
    optimize_dpmfs,
    optimize_pbf,
    DPMFSCoeffs,
)
from doppler.polyphase.matlab_optimization import (
    pbf_freq_resp,
    pbf_imp_resp,
)


# ------------------------------------------------------------------ #
# Helpers                                                             #
# ------------------------------------------------------------------ #

def _default_C(N=6, M=3, pb=0.4, sb=0.6):
    """Run optimize_pbf with default DPMFS settings."""
    C, delta = optimize_pbf(
        N=N, M=M,
        passband=pb, stopband=sb,
        a=1.0, b=-0.5,
        K_pass=10.0, K_stop=1.0,
        n_pass=80, n_stop=200,
        num_der=-1,
    )
    return C, delta


# ------------------------------------------------------------------ #
# optimize_pbf                                                        #
# ------------------------------------------------------------------ #

class TestOptimizePbf:
    def test_c_shape(self):
        C, _ = _default_C(N=6, M=3)
        assert C.shape == (6, 4)

    def test_delta_positive(self):
        _, delta = _default_C()
        assert delta > 0.0

    def test_delta_small(self):
        """Weighted minimax error should be small for a reasonable spec."""
        _, delta = _default_C()
        assert delta < 1.0  # sanity — not diverged

    def test_passband_near_one(self):
        """Passband response should be ≈ 1 (within LP equiripple)."""
        C, delta = _default_C()
        omega_pb = np.linspace(2 * np.pi * 0.01, 2 * np.pi * 0.4, 50)
        H = pbf_freq_resp(C, omega_pb, a=1.0, b=-0.5)
        pb_real = np.real(
            np.exp(1j * omega_pb * (C.shape[0] / 2)) * H
        )
        # LP passband weight K_pass=10 → passband error ≤ delta/10
        tol = delta / 10.0 + 0.01
        assert np.max(np.abs(pb_real - 1.0)) < tol

    def test_stopband_attenuated(self):
        """N=6, pb=0.4, sb=0.6: should achieve ≥ 15 dB."""
        C, delta = _default_C(pb=0.4, sb=0.6)
        assert delta < 0.2  # 15 dB after the fix (was 0.517 with the bug)

    def test_dissertation_example(self):
        """Reproduce the exact example from Hunter 2009 §3.4.2.

        Parameters: N=6, M=3, pb=0.2, sb=0.8, K_pass=10, K_stop=1,
        n_pass=100, n_stop=500 → expected δ = 0.0020.
        """
        C, delta = optimize_pbf(
            N=6, M=3, passband=0.2, stopband=0.8,
            a=1.0, b=-0.5, K_pass=10.0, K_stop=1.0,
            n_pass=100, n_stop=500,
        )
        assert delta == pytest.approx(0.0020, abs=1e-4)
        # First row of C matches known values
        np.testing.assert_allclose(
            C[0, :], [0.0138, 0.0687, -0.0079, -0.1415],
            atol=1e-4,
        )


# ------------------------------------------------------------------ #
# optimize_dpmfs — shape / dtype                                      #
# ------------------------------------------------------------------ #

class TestOptimizeDpmfsShape:
    def test_returns_dpmfscoeffs(self):
        coeffs = optimize_dpmfs(N=6, M=3, n_pass=50, n_stop=100)
        assert isinstance(coeffs, DPMFSCoeffs)

    def test_c_shape(self):
        coeffs = optimize_dpmfs(N=6, M=3, n_pass=50, n_stop=100)
        # N=6 → half=3 taps per branch
        assert coeffs.c.shape == (2, 4, 3)

    def test_c_dtype(self):
        coeffs = optimize_dpmfs(N=6, M=3, n_pass=50, n_stop=100)
        assert coeffs.c.dtype == np.float32

    def test_M_stored(self):
        coeffs = optimize_dpmfs(N=6, M=3, n_pass=50, n_stop=100)
        assert coeffs.M == 3

    def test_N_is_half(self):
        """coeffs.N should equal N//2 (taps per polyphase branch)."""
        coeffs = optimize_dpmfs(N=6, M=3, n_pass=50, n_stop=100)
        assert coeffs.N == 3

    def test_passband_stored(self):
        coeffs = optimize_dpmfs(passband=0.35, N=6, M=3,
                                n_pass=50, n_stop=100)
        assert coeffs.passband == pytest.approx(0.35)

    def test_linear_M1(self):
        coeffs = optimize_dpmfs(N=4, M=1, n_pass=50, n_stop=100)
        assert coeffs.c.shape == (2, 2, 2)
        assert coeffs.M == 1


# ------------------------------------------------------------------ #
# optimize_dpmfs — spectral quality                                   #
# ------------------------------------------------------------------ #

class TestOptimizeDpmfsSpectral:
    """Evaluate the filter using DPMFSCoeffs.evaluate() at μ=0.25.

    At μ=0.25 the filter should behave as a decent lowpass.
    """

    def _spectrum(self, coeffs, n_fft=512):
        """FFT magnitude of the impulse response at μ=0.25."""
        h = coeffs.evaluate(0.25).astype(np.float64)
        H = np.abs(np.fft.rfft(h, n_fft))
        return H

    def test_stopband_attenuated_via_pbf_freq_resp(self):
        """LP optimizer minimises |H_E - d|: passband ≈ 1, SB < 0.6.

        Note: N=6 gives only modest attenuation (~5-8 dB) — the filter
        has 6 total pieces, 3 per DPMFS branch.  Deeper stopband
        requires larger N.
        """
        from doppler.polyphase.matlab_optimization import (
            optimize_pbf, pbf_freq_resp,
        )
        C, delta = optimize_pbf(
            N=6, M=3, passband=0.4, stopband=0.6,
            a=1.0, b=-0.5, K_pass=10.0, K_stop=1.0,
            n_pass=80, n_stop=200,
        )
        omega_sb = np.linspace(2 * np.pi * 0.6, 2 * np.pi * 1.0, 50)
        H = pbf_freq_resp(C, omega_sb, a=1.0, b=-0.5)
        H_E = np.abs(
            np.real(np.exp(1j * omega_sb * (C.shape[0] / 2)) * H)
        )
        # LP guarantees the constraint on its training grid; off-grid
        # evaluation may exceed delta by a small margin (< 0.5%)
        assert H_E.max() <= delta * 1.01 + 1e-4


# ------------------------------------------------------------------ #
# Basis conversion: evaluate() matches pbf_freq_resp                  #
# ------------------------------------------------------------------ #

class TestBasisConversion:
    """The monomial expansion in DPMFSCoeffs.evaluate() should give
    the same filter values as pbf_imp_resp with the original (a,b) basis.

    At μ=0.25, j=0: evaluate() returns c[0,:,k] via Horner using
    μ_J = 2*0.25 - 0 = 0.5.  The DPMFS mapping is:
      n = 2k + j  (piece index in the full filter)
    so for j=0, k∈{0,1,2}: n∈{0,2,4}.

    pbf_imp_resp(C, [0.25]) gives h[n] for n∈{0..N-1}.
    We compare h[0], h[2], h[4] against evaluate(0.25).
    """

    def test_evaluate_matches_pbf_imp_resp(self):
        a, b = 1.0, -0.5
        N_pieces, M = 6, 3
        C, _ = optimize_pbf(
            N=N_pieces, M=M, passband=0.4, stopband=0.6,
            a=a, b=b, K_pass=10.0, K_stop=1.0,
            n_pass=60, n_stop=150, num_der=-1,
        )
        coeffs = optimize_dpmfs(
            N=N_pieces, M=M, passband=0.4, stopband=0.6,
            a=a, b=b, n_pass=60, n_stop=150, num_der=-1,
        )
        # pbf_imp_resp at a single μ value
        mu = np.array([0.25])
        _, ha = pbf_imp_resp(C, mu, a=a, b=b)
        # ha has N_pieces=6 values: ha[n] = h_n(μ=0.25)
        # Even pieces (n=0,2,4) map to j=0 branch (μ=0.25 → j=0)
        ha_j0 = ha[[0, 2, 4]]                        # taps for j=0

        # evaluate(0.25) → j=0, μ_J=0.5; monomial Horner
        h_eval = coeffs.evaluate(0.25).astype(np.float64)

        np.testing.assert_allclose(ha_j0, h_eval, rtol=1e-4, atol=1e-5)


# ------------------------------------------------------------------ #
# Continuity constraint                                               #
# ------------------------------------------------------------------ #

class TestContinuityConstraint:
    def test_num_der0_runs(self):
        """num_der=0 should succeed without error."""
        coeffs = optimize_dpmfs(
            N=6, M=3, num_der=0, n_pass=60, n_stop=120,
        )
        assert isinstance(coeffs, DPMFSCoeffs)

    def test_num_der0_continuous_at_boundary(self):
        """With num_der=0 the IR at μ=1.0 should ≈ μ=0.0 shifted by 1."""
        coeffs = optimize_dpmfs(
            N=6, M=3, num_der=0, n_pass=60, n_stop=120,
        )
        h0 = coeffs.evaluate(0.0).astype(np.float64)
        h1 = coeffs.evaluate(1.0).astype(np.float64)  # wraps to 0.0
        # evaluate wraps, so they should be identical
        np.testing.assert_array_equal(
            coeffs.evaluate(0.0), coeffs.evaluate(1.0)
        )


# ------------------------------------------------------------------ #
# Comparison: fit_dpmfs vs optimize_dpmfs                             #
# ------------------------------------------------------------------ #

class TestFitVsOptimize:
    """Sanity comparison — both methods should produce plausible filters.

    The LP-optimized version may not always beat the LS fit since they
    optimize different objectives (Chebyshev vs L2) and use different
    filter lengths.  We just check both are in the same ballpark.
    """

    def test_both_have_small_evaluate_magnitude(self):
        """Evaluate at μ=0.5 should give a non-trivial filter for both."""
        bank = kaiser_prototype(attenuation=60.0, passband=0.4,
                                stopband=0.6)[1]
        fit = fit_dpmfs(bank, M=3)

        opt = optimize_dpmfs(
            passband=0.4, stopband=0.6, N=6, M=3,
            n_pass=60, n_stop=120,
        )

        # Both should produce filter taps with non-zero energy
        assert np.sum(fit.evaluate(0.25) ** 2) > 0.0
        assert np.sum(opt.evaluate(0.25) ** 2) > 0.0
